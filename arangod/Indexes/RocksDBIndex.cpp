////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBIndex.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/debugging.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/PrimaryIndex.h"
#include "Indexes/RocksDBFeature.h"
#include "Indexes/RocksDBKeyComparator.h"
#include "Utils/Transaction.h"
#include "VocBase/document-collection.h"

#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

static size_t sortWeight(arangodb::aql::AstNode const* node) {
  switch (node->type) {
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      return 1;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN:
      return 2;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
      return 3;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
      return 4;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
      return 5;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
      return 6;
    default:
      return 42;
  }
}

// .............................................................................
// recall for all of the following comparison functions:
//
// left < right  return -1
// left > right  return  1
// left == right return  0
//
// furthermore:
//
// the following order is currently defined for placing an order on documents
// undef < null < boolean < number < strings < lists < hash arrays
// note: undefined will be treated as NULL pointer not NULL JSON OBJECT
// within each type class we have the following order
// boolean: false < true
// number: natural order
// strings: lexicographical
// lists: lexicographically and within each slot according to these rules.
// ...........................................................................
  
RocksDBIterator::RocksDBIterator(arangodb::Transaction* trx, 
                                 arangodb::RocksDBIndex const* index,
                                 arangodb::PrimaryIndex* primaryIndex,
                                 rocksdb::OptimisticTransactionDB* db,
                                 bool reverse, 
                                 VPackSlice const& left,
                                 VPackSlice const& right)
    : _trx(trx),
      _primaryIndex(primaryIndex),
      _db(db),
      _reverse(reverse),
      _probe(false) {
  
  TRI_idx_iid_t const id = index->id();
  std::string const prefix = RocksDBIndex::buildPrefix(trx->vocbase()->_id, _primaryIndex->collection()->_info.id(), id);
  TRI_ASSERT(prefix.size() == RocksDBIndex::keyPrefixSize());

  _leftEndpoint.reset(new arangodb::velocypack::Buffer<char>());
  _leftEndpoint->reserve(RocksDBIndex::keyPrefixSize() + left.byteSize());
  _leftEndpoint->append(prefix.c_str(), prefix.size());
  _leftEndpoint->append(left.startAs<char const>(), left.byteSize());
  
  _rightEndpoint.reset(new arangodb::velocypack::Buffer<char>());
  _rightEndpoint->reserve(RocksDBIndex::keyPrefixSize() + right.byteSize());
  _rightEndpoint->append(prefix.c_str(), prefix.size());
  _rightEndpoint->append(right.startAs<char const>(), right.byteSize());

  TRI_ASSERT(_leftEndpoint->size() > 8);
  TRI_ASSERT(_rightEndpoint->size() > 8);
    
  // LOG(TRACE) << "prefix: " << fasthash64(prefix.c_str(), prefix.size(), 0);
  // LOG(TRACE) << "iterator left key: " << left.toJson();
  // LOG(TRACE) << "iterator right key: " << right.toJson();
    
  _cursor.reset(_db->GetBaseDB()->NewIterator(rocksdb::ReadOptions()));

  reset();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Reset the cursor
////////////////////////////////////////////////////////////////////////////////

void RocksDBIterator::reset() {
  if (_reverse) {
    _probe = true;
    _cursor->Seek(rocksdb::Slice(_rightEndpoint->data(), _rightEndpoint->size()));
    if (!_cursor->Valid()) {
      _cursor->SeekToLast();
    }
  } else {
    _cursor->Seek(rocksdb::Slice(_leftEndpoint->data(), _leftEndpoint->size()));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the next element in the index
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* RocksDBIterator::next() {
  auto comparator = RocksDBFeature::instance()->comparator();
    
  while (true) {
    if (!_cursor->Valid()) {
      // We are exhausted already, sorry
      return nullptr;
    }
  
    rocksdb::Slice key = _cursor->key();
    // LOG(TRACE) << "cursor key: " << VPackSlice(key.data() + RocksDBIndex::keyPrefixSize()).toJson();
  
    int res = comparator->Compare(key, rocksdb::Slice(_leftEndpoint->data(), _leftEndpoint->size()));
    // LOG(TRACE) << "comparing: " << VPackSlice(key.data() + RocksDBIndex::keyPrefixSize()).toJson() << " with " << VPackSlice((char const*) _leftEndpoint->data() + RocksDBIndex::keyPrefixSize()).toJson() << " - res: " << res;

    if (res < 0) {
      if (_reverse) {
        return nullptr;
      } else {
        _cursor->Next();
      }
      continue;
    } 
  
    res = comparator->Compare(key, rocksdb::Slice(_rightEndpoint->data(), _rightEndpoint->size()));
    // LOG(TRACE) << "comparing: " << VPackSlice(key.data() + RocksDBIndex::keyPrefixSize()).toJson() << " with " << VPackSlice((char const*) _rightEndpoint->data() + RocksDBIndex::keyPrefixSize()).toJson() << " - res: " << res;
   
    TRI_doc_mptr_t* doc = nullptr;
     
    if (res <= 0) {
      // get the value for _key, which is the last entry in the key array
      VPackSlice const keySlice = comparator->extractKeySlice(key);
      TRI_ASSERT(keySlice.isArray());
      VPackValueLength const n = keySlice.length();
      TRI_ASSERT(n > 1); // one value + _key
    
      // LOG(TRACE) << "looking up document with key: " << keySlice.toJson();
      // LOG(TRACE) << "looking up document with primary key: " << keySlice[n - 1].toJson();

      // use primary index to lookup the document
      doc = _primaryIndex->lookupKey(_trx, keySlice[n - 1]);
    }
    
    if (_reverse) {
      _cursor->Prev();
    } else {
      _cursor->Next();
    }

    if (res > 0) {
      if (!_probe) {
        return nullptr;
      }
      _probe = false;
      continue;
    }

    if (doc != nullptr) {
      return doc;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the index
////////////////////////////////////////////////////////////////////////////////

RocksDBIndex::RocksDBIndex(
    TRI_idx_iid_t iid, TRI_document_collection_t* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    bool unique, bool sparse)
    : PathBasedIndex(iid, collection, fields, unique, sparse, true),
      _db(RocksDBFeature::instance()->db()) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an index stub with a hard-coded selectivity estimate
/// this is used in the cluster coordinator case
////////////////////////////////////////////////////////////////////////////////

RocksDBIndex::RocksDBIndex(VPackSlice const& slice)
    : PathBasedIndex(slice, true),
      _db(nullptr) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the index
////////////////////////////////////////////////////////////////////////////////

RocksDBIndex::~RocksDBIndex() {}

size_t RocksDBIndex::memory() const {
  return 0; // TODO
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a VelocyPack representation of the index
////////////////////////////////////////////////////////////////////////////////

void RocksDBIndex::toVelocyPack(VPackBuilder& builder,
                                bool withFigures) const {
  Index::toVelocyPack(builder, withFigures);
  builder.add("unique", VPackValue(_unique));
  builder.add("sparse", VPackValue(_sparse));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a VelocyPack representation of the index figures
////////////////////////////////////////////////////////////////////////////////

void RocksDBIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add("memory", VPackValue(memory()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document into the index
////////////////////////////////////////////////////////////////////////////////

int RocksDBIndex::insert(arangodb::Transaction* trx, TRI_doc_mptr_t const* doc,
                         bool) {
  auto comparator = RocksDBFeature::instance()->comparator();
  std::vector<TRI_index_element_t*> elements;

  int res;
  try {
    res = fillElement(elements, doc);
  } catch (...) {
    res = TRI_ERROR_OUT_OF_MEMORY;
  }

  // make sure we clean up before we leave this method
  auto cleanup = [&elements] {
    for (auto& it : elements) {
      TRI_index_element_t::freeElement(it);
    }
  };

  TRI_DEFER(cleanup());
  
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  
  VPackSlice const key = Transaction::extractKeyFromDocument(VPackSlice(doc->vpack()));
  std::string const prefix = buildPrefix(trx->vocbase()->_id, _collection->_info.id(), _iid);

  VPackBuilder builder;
  std::vector<std::string> values;
  values.reserve(elements.size());

  // lower and upper bounds, only required if the index is unique
  std::vector<std::pair<std::string, std::string>> bounds;
  if (_unique) {
    bounds.reserve(elements.size());
  }

  for (auto& it : elements) {
    builder.clear();
    builder.openArray();
    for (size_t i = 0; i < _fields.size(); ++i) {
      builder.add(it->subObjects()[i].slice(doc));
    }
    builder.add(key); // always append _key value to the end of the array
    builder.close();

    VPackSlice const s = builder.slice();
    std::string value;
    value.reserve(keyPrefixSize() + s.byteSize());
    value += prefix;
    value.append(s.startAs<char const>(), s.byteSize());
    values.emplace_back(std::move(value));

    if (_unique) {
      builder.clear();
      builder.openArray();
      for (size_t i = 0; i < _fields.size(); ++i) {
        builder.add(it->subObjects()[i].slice(doc));
      }
      builder.add(VPackSlice::minKeySlice());
      builder.close();
    
      VPackSlice s = builder.slice();
      std::string value;
      value.reserve(keyPrefixSize() + s.byteSize());
      value += prefix;
      value.append(s.startAs<char const>(), s.byteSize());
      
      std::pair<std::string, std::string> p;
      p.first = value;
      
      builder.clear();
      builder.openArray();
      for (size_t i = 0; i < _fields.size(); ++i) {
        builder.add(it->subObjects()[i].slice(doc));
      }
      builder.add(VPackSlice::maxKeySlice());
      builder.close();
    
      s = builder.slice();
      value.clear();
      value += prefix;
      value.append(s.startAs<char const>(), s.byteSize());
      
      p.second = value;
      bounds.emplace_back(std::move(p));
    }
  }

  auto rocksTransaction = trx->rocksTransaction();
  TRI_ASSERT(rocksTransaction != nullptr);

  rocksdb::ReadOptions readOptions;

  size_t const count = elements.size();
  for (size_t i = 0; i < count; ++i) {
    if (_unique) {
      bool uniqueConstraintViolated = false;
      auto iterator = rocksTransaction->GetIterator(readOptions);

      if (iterator != nullptr) {
        auto& bound = bounds[i];
        iterator->Seek(rocksdb::Slice(bound.first.c_str(), bound.first.size()));

        while (iterator->Valid()) {
          int res = comparator->Compare(iterator->key(), rocksdb::Slice(bound.second.c_str(), bound.second.size()));

          if (res > 0) {
            break;
          }

          uniqueConstraintViolated = true;
          break;
        }

        delete iterator;
      }

      if (uniqueConstraintViolated) {
        // duplicate key
        res = TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
        if (!_collection->useSecondaryIndexes()) {
          // suppress the error during recovery
          res = TRI_ERROR_NO_ERROR;
        }
      }
    }

    if (res == TRI_ERROR_NO_ERROR) {
      auto status = rocksTransaction->Put(values[i], std::string());
      
      if (! status.ok()) {
        res = TRI_ERROR_INTERNAL;
      }
    }

    if (res != TRI_ERROR_NO_ERROR) {
      for (size_t j = 0; j < i; ++j) {
        rocksTransaction->Delete(values[i]);
      }
    
      if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED && !_unique) {
        // We ignore unique_constraint violated if we are not unique
        res = TRI_ERROR_NO_ERROR;
      }
      break;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from the index
////////////////////////////////////////////////////////////////////////////////

int RocksDBIndex::remove(arangodb::Transaction* trx, TRI_doc_mptr_t const* doc,
                         bool) {
  std::vector<TRI_index_element_t*> elements;

  int res;
  try {
    res = fillElement(elements, doc);
  } catch (...) {
    res = TRI_ERROR_OUT_OF_MEMORY;
  }
  
  // make sure we clean up before we leave this method
  auto cleanup = [&elements] {
    for (auto& it : elements) {
      TRI_index_element_t::freeElement(it);
    }
  };

  TRI_DEFER(cleanup());

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  
  VPackSlice const key = Transaction::extractKeyFromDocument(VPackSlice(doc->vpack()));
  
  VPackBuilder builder;
  std::vector<std::string> values;
  for (auto& it : elements) {
    builder.clear();
    builder.openArray();
    for (size_t i = 0; i < _fields.size(); ++i) {
      builder.add(it->subObjects()[i].slice(doc));
    }
    builder.add(key); // always append _key value to the end of the array
    builder.close();

    VPackSlice const s = builder.slice();
    std::string value;
    value.reserve(keyPrefixSize() + s.byteSize());
    value.append(buildPrefix(trx->vocbase()->_id, _collection->_info.id(), _iid));
    value.append(s.startAs<char const>(), s.byteSize());
    values.emplace_back(std::move(value));
  }
  
  auto rocksTransaction = trx->rocksTransaction();
  TRI_ASSERT(rocksTransaction != nullptr);

  size_t const count = elements.size();

  for (size_t i = 0; i < count; ++i) {
    // LOG(TRACE) << "removing key: " << VPackSlice(values[i].c_str() + keyPrefixSize()).toJson();
    auto status = rocksTransaction->Delete(values[i]);

    // we may be looping through this multiple times, and if an error
    // occurs, we want to keep it
    if (! status.ok()) {
      res = TRI_ERROR_INTERNAL;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief called when the index is dropped
////////////////////////////////////////////////////////////////////////////////

int RocksDBIndex::drop() {
  return RocksDBFeature::instance()->dropIndex(_collection->_vocbase->_id, _collection->_info.id(), _iid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to locate an entry in the index
///
/// Warning: who ever calls this function is responsible for destroying
/// the RocksDBIterator* results
////////////////////////////////////////////////////////////////////////////////

RocksDBIterator* RocksDBIndex::lookup(arangodb::Transaction* trx,
                                      VPackSlice const searchValues,
                                      bool reverse) const {
  TRI_ASSERT(searchValues.isArray());
  TRI_ASSERT(searchValues.length() <= _fields.size());

  VPackBuilder leftSearch;
  VPackBuilder rightSearch;

  VPackSlice lastNonEq;
  leftSearch.openArray();
  for (auto const& it : VPackArrayIterator(searchValues)) {
    TRI_ASSERT(it.isObject());
    VPackSlice eq = it.get(TRI_SLICE_KEY_EQUAL);
    if (eq.isNone()) {
      lastNonEq = it;
      break;
    }
    leftSearch.add(eq);
  }

  VPackSlice leftBorder;
  VPackSlice rightBorder;

  if (lastNonEq.isNone()) {
    // We only have equality!
    rightSearch = leftSearch;

    leftSearch.add(VPackSlice::minKeySlice());
    leftSearch.close();
    
    rightSearch.add(VPackSlice::maxKeySlice());
    rightSearch.close();

    leftBorder = leftSearch.slice();
    rightBorder = rightSearch.slice();
  } else {
    // Copy rightSearch = leftSearch for right border
    rightSearch = leftSearch;

    // Define Lower-Bound 
    VPackSlice lastLeft = lastNonEq.get(TRI_SLICE_KEY_GE);
    if (!lastLeft.isNone()) {
      TRI_ASSERT(!lastNonEq.hasKey(TRI_SLICE_KEY_GT));
      leftSearch.add(lastLeft);
      leftSearch.add(VPackSlice::minKeySlice());
      leftSearch.close();
      VPackSlice search = leftSearch.slice();
      leftBorder = search;
    } else {
      lastLeft = lastNonEq.get(TRI_SLICE_KEY_GT);
      if (!lastLeft.isNone()) {
        leftSearch.add(lastLeft);
        leftSearch.add(VPackSlice::maxKeySlice());
        leftSearch.close();
        VPackSlice search = leftSearch.slice();
        leftBorder = search;
      } else {
        // No lower bound set default to (null <= x)
        leftSearch.add(VPackSlice::minKeySlice());
        leftSearch.close();
        VPackSlice search = leftSearch.slice();
        leftBorder = search;
      }
    }

    // Define upper-bound
    VPackSlice lastRight = lastNonEq.get(TRI_SLICE_KEY_LE);
    if (!lastRight.isNone()) {
      TRI_ASSERT(!lastNonEq.hasKey(TRI_SLICE_KEY_LT));
      rightSearch.add(lastRight);
      rightSearch.add(VPackSlice::maxKeySlice());
      rightSearch.close();
      VPackSlice search = rightSearch.slice();
      rightBorder = search;
    } else {
      lastRight = lastNonEq.get(TRI_SLICE_KEY_LT);
      if (!lastRight.isNone()) {
        rightSearch.add(lastRight);
        rightSearch.add(VPackSlice::minKeySlice());
        rightSearch.close();
        VPackSlice search = rightSearch.slice();
        rightBorder = search;
      } else {
        // No upper bound set default to (x <= INFINITY)
        rightSearch.add(VPackSlice::maxKeySlice());
        rightSearch.close();
        VPackSlice search = rightSearch.slice();
        rightBorder = search;
      }
    }
  }

  auto iterator = std::make_unique<RocksDBIterator>(trx, this, _collection->primaryIndex(), _db, reverse, leftBorder, rightBorder);

  return iterator.release();
}

bool RocksDBIndex::accessFitsIndex(
    arangodb::aql::AstNode const* access, arangodb::aql::AstNode const* other,
    arangodb::aql::AstNode const* op, arangodb::aql::Variable const* reference,
    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&
        found,
    bool isExecution) const {
  if (!this->canUseConditionPart(access, other, op, reference, isExecution)) {
    return false;
  }

  arangodb::aql::AstNode const* what = access;
  std::pair<arangodb::aql::Variable const*,
            std::vector<arangodb::basics::AttributeName>> attributeData;

  if (op->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    if (!what->isAttributeAccessForVariable(attributeData) ||
        attributeData.first != reference) {
      // this access is not referencing this collection
      return false;
    }
    if (arangodb::basics::TRI_AttributeNamesHaveExpansion(
            attributeData.second)) {
      // doc.value[*] == 'value'
      return false;
    }
    if (isAttributeExpanded(attributeData.second)) {
      // doc.value == 'value' (with an array index)
      return false;
    }
  } else {
    // ok, we do have an IN here... check if it's something like 'value' IN
    // doc.value[*]
    TRI_ASSERT(op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN);
    bool canUse = false;

    if (what->isAttributeAccessForVariable(attributeData) &&
        attributeData.first == reference &&
        !arangodb::basics::TRI_AttributeNamesHaveExpansion(
            attributeData.second) &&
        attributeMatches(attributeData.second)) {
      // doc.value IN 'value'
      // can use this index
      canUse = true;
    } else {
      // check for  'value' IN doc.value  AND  'value' IN doc.value[*]
      what = other;
      if (what->isAttributeAccessForVariable(attributeData) &&
          attributeData.first == reference &&
          isAttributeExpanded(attributeData.second) &&
          attributeMatches(attributeData.second)) {
        canUse = true;
      }
    }

    if (!canUse) {
      return false;
    }
  }

  std::vector<arangodb::basics::AttributeName> const& fieldNames =
      attributeData.second;

  for (size_t i = 0; i < _fields.size(); ++i) {
    if (_fields[i].size() != fieldNames.size()) {
      // attribute path length differs
      continue;
    }

    if (this->isAttributeExpanded(i) &&
        op->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
      // If this attribute is correct or not, it could only serve for IN
      continue;
    }

    bool match = arangodb::basics::AttributeName::isIdentical(_fields[i],
                                                              fieldNames, true);

    if (match) {
      // mark ith attribute as being covered
      auto it = found.find(i);

      if (it == found.end()) {
        found.emplace(i, std::vector<arangodb::aql::AstNode const*>{op});
      } else {
        (*it).second.emplace_back(op);
      }
      TRI_IF_FAILURE("RocksDBIndex::accessFitsIndex") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      return true;
    }
  }

  return false;
}

void RocksDBIndex::matchAttributes(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&
        found,
    size_t& values, bool isExecution) const {
  for (size_t i = 0; i < node->numMembers(); ++i) {
    auto op = node->getMember(i);

    switch (op->type) {
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
        TRI_ASSERT(op->numMembers() == 2);
        accessFitsIndex(op->getMember(0), op->getMember(1), op, reference,
                        found, isExecution);
        accessFitsIndex(op->getMember(1), op->getMember(0), op, reference,
                        found, isExecution);
        break;

      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN:
        if (accessFitsIndex(op->getMember(0), op->getMember(1), op, reference,
                            found, isExecution)) {
          auto m = op->getMember(1);
          if (m->isArray() && m->numMembers() > 1) {
            // attr IN [ a, b, c ]  =>  this will produce multiple items, so
            // count them!
            values += m->numMembers() - 1;
          }
        }
        break;

      default:
        break;
    }
  }
}

bool RocksDBIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>> found;
  size_t values = 0;
  matchAttributes(node, reference, found, values, false);

  bool lastContainsEquality = true;
  size_t attributesCovered = 0;
  size_t attributesCoveredByEquality = 0;
  double equalityReductionFactor = 20.0;
  estimatedCost = static_cast<double>(itemsInIndex);

  for (size_t i = 0; i < _fields.size(); ++i) {
    auto it = found.find(i);

    if (it == found.end()) {
      // index attribute not covered by condition
      break;
    }

    // check if the current condition contains an equality condition
    auto const& nodes = (*it).second;
    bool containsEquality = false;
    for (size_t j = 0; j < nodes.size(); ++j) {
      if (nodes[j]->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
          nodes[j]->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
        containsEquality = true;
        break;
      }
    }

    if (!lastContainsEquality) {
      // unsupported condition. must abort
      break;
    }

    ++attributesCovered;
    if (containsEquality) {
      ++attributesCoveredByEquality;
      estimatedCost /= equalityReductionFactor;

      // decrease the effect of the equality reduction factor
      equalityReductionFactor *= 0.25;
      if (equalityReductionFactor < 2.0) {
        // equalityReductionFactor shouldn't get too low
        equalityReductionFactor = 2.0;
      }
    } else {
      // quick estimate for the potential reductions caused by the conditions
      if (nodes.size() >= 2) {
        // at least two (non-equality) conditions. probably a range with lower
        // and upper bound defined
        estimatedCost /= 7.5;
      } else {
        // one (non-equality). this is either a lower or a higher bound
        estimatedCost /= 2.0;
      }
    }

    lastContainsEquality = containsEquality;
  }

  if (values == 0) {
    values = 1;
  }

  if (attributesCoveredByEquality == _fields.size() && unique()) {
    // index is unique and condition covers all attributes by equality
    if (estimatedItems >= values) {
      // reduce costs due to uniqueness
      estimatedItems = values;
      estimatedCost = static_cast<double>(estimatedItems);
    } else {
      // cost is already low... now slightly prioritize the unique index
      estimatedCost *= 0.995;
    }
    return true;
  }

  if (attributesCovered > 0 &&
      (!_sparse || attributesCovered == _fields.size())) {
    // if the condition contains at least one index attribute and is not sparse,
    // or the index is sparse and all attributes are covered by the condition,
    // then it can be used (note: additional checks for condition parts in
    // sparse indexes are contained in Index::canUseConditionPart)
    estimatedItems = static_cast<size_t>((std::max)(
        static_cast<size_t>(estimatedCost * values), static_cast<size_t>(1)));
    estimatedCost *= static_cast<double>(values);
    return true;
  }

  // no condition
  estimatedItems = itemsInIndex;
  estimatedCost = static_cast<double>(estimatedItems);
  return false;
}

bool RocksDBIndex::supportsSortCondition(
    arangodb::aql::SortCondition const* sortCondition,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    double& estimatedCost, size_t& coveredAttributes) const {
  TRI_ASSERT(sortCondition != nullptr);

  if (!_sparse) {
    // only non-sparse indexes can be used for sorting
    if (!_useExpansion && sortCondition->isUnidirectional() &&
        sortCondition->isOnlyAttributeAccess()) {
      coveredAttributes = sortCondition->coveredAttributes(reference, _fields);

      if (coveredAttributes >= sortCondition->numAttributes()) {
        // sort is fully covered by index. no additional sort costs!
        // forward iteration does not have high costs
        estimatedCost = itemsInIndex * 0.001;
        if (sortCondition->isDescending()) {
          // reverse iteration has higher costs than forward iteration
          estimatedCost *= 4;
        }
        return true;
      } else if (coveredAttributes > 0) {
        estimatedCost = (itemsInIndex / coveredAttributes) *
                        std::log2(static_cast<double>(itemsInIndex));
        if (sortCondition->isAscending()) {
          // reverse iteration is more expensive
          estimatedCost *= 4;
        }
        return true;
      }
    }
  }

  coveredAttributes = 0;
  // by default no sort conditions are supported
  if (itemsInIndex > 0) {
    estimatedCost = itemsInIndex * std::log2(static_cast<double>(itemsInIndex));
    // slightly penalize this type of index against other indexes which
    // are in memory
    estimatedCost *= 1.05;
  } else {
    estimatedCost = 0.0;
  }
  return false;
}

IndexIterator* RocksDBIndex::iteratorForCondition(
    arangodb::Transaction* trx, IndexIteratorContext* context,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) const {
  VPackBuilder searchValues;
  searchValues.openArray();
  bool needNormalize = false;
  if (node == nullptr) {
    // We only use this index for sort. Empty searchValue
    VPackArrayBuilder guard(&searchValues);

    TRI_IF_FAILURE("RocksDBIndex::noSortIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  } else {
    // Create the search Values for the lookup
    VPackArrayBuilder guard(&searchValues);

    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>> found;
    size_t unused = 0;
    matchAttributes(node, reference, found, unused, true);

    // found contains all attributes that are relevant for this node.
    // It might be less than fields().
    //
    // Handle the first attributes. They can only be == or IN and only
    // one node per attribute

    auto getValueAccess = [&](arangodb::aql::AstNode const* comp,
                              arangodb::aql::AstNode const*& access,
                              arangodb::aql::AstNode const*& value) -> bool {
      access = comp->getMember(0);
      value = comp->getMember(1);
      std::pair<arangodb::aql::Variable const*,
                std::vector<arangodb::basics::AttributeName>> paramPair;
      if (!(access->isAttributeAccessForVariable(paramPair) &&
            paramPair.first == reference)) {
        access = comp->getMember(1);
        value = comp->getMember(0);
        if (!(access->isAttributeAccessForVariable(paramPair) &&
              paramPair.first == reference)) {
          // Both side do not have a correct AttributeAccess, this should not
          // happen and indicates
          // an error in the optimizer
          TRI_ASSERT(false);
        }
        return true;
      }
      return false;
    };

    size_t usedFields = 0;
    for (; usedFields < _fields.size(); ++usedFields) {
      auto it = found.find(usedFields);
      if (it == found.end()) {
        // We are either done
        // or this is a range.
        // Continue with more complicated loop
        break;
      }

      auto comp = it->second[0];
      TRI_ASSERT(comp->numMembers() == 2);
      arangodb::aql::AstNode const* access = nullptr;
      arangodb::aql::AstNode const* value = nullptr;
      getValueAccess(comp, access, value);
      // We found an access for this field
      
      if (comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
        searchValues.openObject();
        searchValues.add(VPackValue(TRI_SLICE_KEY_EQUAL));
        TRI_IF_FAILURE("RocksDBIndex::permutationEQ") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
      } else if (comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
        if (isAttributeExpanded(usedFields)) {
          searchValues.openObject();
          searchValues.add(VPackValue(TRI_SLICE_KEY_EQUAL));
          TRI_IF_FAILURE("RocksDBIndex::permutationArrayIN") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
        } else {
          needNormalize = true;
          searchValues.openObject();
          searchValues.add(VPackValue(TRI_SLICE_KEY_IN));
        }
      } else {
        // This is a one-sided range
        break;
      }
      // We have to add the value always, the key was added before
      value->toVelocyPackValue(searchValues);
      searchValues.close();
    }

    // Now handle the next element, which might be a range
    if (usedFields < _fields.size()) {
      auto it = found.find(usedFields);
      if (it != found.end()) {
        auto rangeConditions = it->second;
        TRI_ASSERT(rangeConditions.size() <= 2);
        VPackObjectBuilder searchElement(&searchValues);
        for (auto& comp : rangeConditions) {
          TRI_ASSERT(comp->numMembers() == 2);
          arangodb::aql::AstNode const* access = nullptr;
          arangodb::aql::AstNode const* value = nullptr;
          bool isReverseOrder = getValueAccess(comp, access, value);
          // Add the key
          switch (comp->type) {
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
              if (isReverseOrder) {
                searchValues.add(VPackValue(TRI_SLICE_KEY_GT));
              } else {
                searchValues.add(VPackValue(TRI_SLICE_KEY_LT));
              }
              break;
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
              if (isReverseOrder) {
                searchValues.add(VPackValue(TRI_SLICE_KEY_GE));
              } else {
                searchValues.add(VPackValue(TRI_SLICE_KEY_LE));
              }
              break;
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
              if (isReverseOrder) {
                searchValues.add(VPackValue(TRI_SLICE_KEY_LT));
              } else {
                searchValues.add(VPackValue(TRI_SLICE_KEY_GT));
              }
              break;
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
              if (isReverseOrder) {
                searchValues.add(VPackValue(TRI_SLICE_KEY_LE));
              } else {
                searchValues.add(VPackValue(TRI_SLICE_KEY_GE));
              }
              break;
          default:
            // unsupported right now. Should have been rejected by
            // supportsFilterCondition
            TRI_ASSERT(false);
            return nullptr;
          }
          value->toVelocyPackValue(searchValues);
        }
      }
    }
  }
  searchValues.close();

  TRI_IF_FAILURE("RocksDBIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (needNormalize) {
    VPackBuilder expandedSearchValues;
    expandInSearchValues(searchValues.slice(), expandedSearchValues);
    VPackSlice expandedSlice = expandedSearchValues.slice();
    std::vector<IndexIterator*> iterators;
    try {
      for (auto const& val : VPackArrayIterator(expandedSlice)) {
        auto iterator = lookup(trx, val, reverse);
        try {
          iterators.push_back(iterator);
        } catch (...) {
          // avoid leak
          delete iterator;
          throw;
        }
      }
      if (reverse) {
        std::reverse(iterators.begin(), iterators.end());
      }
    }
    catch (...) {
      for (auto& it : iterators) {
        delete it;
      }
      throw; 
    }
    return new MultiIndexIterator(iterators);
  }

  VPackSlice searchSlice = searchValues.slice();
  TRI_ASSERT(searchSlice.length() == 1);
  searchSlice = searchSlice.at(0);
  return lookup(trx, searchSlice, reverse);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief specializes the condition for use with the index
////////////////////////////////////////////////////////////////////////////////

arangodb::aql::AstNode* RocksDBIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>> found;
  size_t values = 0;
  matchAttributes(node, reference, found, values, false);

  std::vector<arangodb::aql::AstNode const*> children;
  bool lastContainsEquality = true;

  for (size_t i = 0; i < _fields.size(); ++i) {
    auto it = found.find(i);

    if (it == found.end()) {
      // index attribute not covered by condition
      break;
    }

    // check if the current condition contains an equality condition
    auto& nodes = (*it).second;
    bool containsEquality = false;
    for (size_t j = 0; j < nodes.size(); ++j) {
      if (nodes[j]->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
          nodes[j]->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
        containsEquality = true;
        break;
      }
    }

    if (!lastContainsEquality) {
      // unsupported condition. must abort
      break;
    }

    std::sort(
        nodes.begin(), nodes.end(),
        [](arangodb::aql::AstNode const* lhs, arangodb::aql::AstNode const* rhs)
            -> bool { return sortWeight(lhs) < sortWeight(rhs); });

    lastContainsEquality = containsEquality;
    std::unordered_set<int> operatorsFound;
    for (auto& it : nodes) {
      // do not let duplicate or related operators pass
      if (isDuplicateOperator(it, operatorsFound)) {
        continue;
      }
      operatorsFound.emplace(static_cast<int>(it->type));
      children.emplace_back(it);
    }
  }

  while (node->numMembers() > 0) {
    node->removeMemberUnchecked(0);
  }

  for (auto& it : children) {
    node->addMember(it);
  }
  return node;
}

bool RocksDBIndex::isDuplicateOperator(
    arangodb::aql::AstNode const* node,
    std::unordered_set<int> const& operatorsFound) const {
  auto type = node->type;
  if (operatorsFound.find(static_cast<int>(type)) != operatorsFound.end()) {
    // duplicate operator
    return true;
  }

  if (operatorsFound.find(
          static_cast<int>(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ)) !=
          operatorsFound.end() ||
      operatorsFound.find(
          static_cast<int>(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN)) !=
          operatorsFound.end()) {
    return true;
  }

  bool duplicate = false;
  switch (type) {
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
      duplicate = operatorsFound.find(static_cast<int>(
                      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE)) !=
                  operatorsFound.end();
      break;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
      duplicate = operatorsFound.find(static_cast<int>(
                      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT)) !=
                  operatorsFound.end();
      break;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
      duplicate = operatorsFound.find(static_cast<int>(
                      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE)) !=
                  operatorsFound.end();
      break;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
      duplicate = operatorsFound.find(static_cast<int>(
                      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT)) !=
                  operatorsFound.end();
      break;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      duplicate = operatorsFound.find(static_cast<int>(
                      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN)) !=
                  operatorsFound.end();
      break;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN:
      duplicate = operatorsFound.find(static_cast<int>(
                      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ)) !=
                  operatorsFound.end();
      break;
    default: {
      // ignore
    }
  }

  return duplicate;
}
