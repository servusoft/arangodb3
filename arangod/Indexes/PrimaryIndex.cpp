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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "PrimaryIndex.h"
#include "Aql/AstNode.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/hashes.h"
#include "Basics/tri-strings.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Utils/Transaction.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief hard-coded vector of the index attributes
/// note that the attribute names must be hard-coded here to avoid an init-order
/// fiasco with StaticStrings::FromString etc.
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::vector<arangodb::basics::AttributeName>> const IndexAttributes
    {{arangodb::basics::AttributeName("_id", false)},
     {arangodb::basics::AttributeName("_key", false)}};

static inline uint64_t HashKey(void* userData, uint8_t const* key) {
  // can use fast hash-function here, as index values are restricted to strings
  return VPackSlice(key).hashString();
}

static inline uint64_t HashElement(void*, TRI_doc_mptr_t const* element) {
  return element->getHash();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if a key corresponds to an element
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyElement(void*, uint8_t const* key,
                              uint64_t const hash,
                              TRI_doc_mptr_t const* element) {
  if (hash != element->getHash()) {
    return false;
  }
 
  return Transaction::extractKeyFromDocument(VPackSlice(element->vpack())).equals(VPackSlice(key)); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if two elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementElement(void*, TRI_doc_mptr_t const* left,
                                  TRI_doc_mptr_t const* right) {
  if (left->getHash() != right->getHash()) {
    return false;
  }

  VPackSlice l = Transaction::extractKeyFromDocument(VPackSlice(left->vpack()));
  VPackSlice r = Transaction::extractKeyFromDocument(VPackSlice(right->vpack()));
  return l.equals(r);
}

TRI_doc_mptr_t* PrimaryIndexIterator::next() {
  while (_iterator.valid()) {
    auto result = _index->lookup(_trx, _iterator.value());
    _iterator.next();

    if (result != nullptr) {
      // found a result
      return result;
    }

    // found no result. now go to next lookup value in _keys
  }

  return nullptr;
}

void PrimaryIndexIterator::reset() { _iterator.reset(); }

TRI_doc_mptr_t* AllIndexIterator::next() {
  if (_reverse) {
    return _index->findSequentialReverse(_trx, _position);
  }
  return _index->findSequential(_trx, _position, _total);
};

void AllIndexIterator::nextBabies(std::vector<TRI_doc_mptr_t*>& buffer, size_t limit) {
  size_t atMost = limit;

  buffer.clear();

  while (atMost > 0) {
    auto result = next();

    if (result == nullptr) {
      return;
    }

    buffer.emplace_back(result);
    --atMost;
  }
}

void AllIndexIterator::reset() { _position.reset(); }

TRI_doc_mptr_t* AnyIndexIterator::next() {
  return _index->findRandom(_trx, _initial, _position, _step, _total);
}

void AnyIndexIterator::reset() {
  _step = 0;
  _total = 0;
  _position = _initial;
}


PrimaryIndex::PrimaryIndex(TRI_document_collection_t* collection)
    : Index(0, collection,
            std::vector<std::vector<arangodb::basics::AttributeName>>(
                {{{StaticStrings::KeyString, false}}}),
            true, false),
      _primaryIndex(nullptr) {
  uint32_t indexBuckets = 1;

  if (collection != nullptr) {
    // document is a nullptr in the coordinator case
    indexBuckets = collection->_info.indexBuckets();
  }

  _primaryIndex = new TRI_PrimaryIndex_t(
      HashKey, HashElement, IsEqualKeyElement, IsEqualElementElement,
      IsEqualElementElement, indexBuckets,
      []() -> std::string { return "primary"; });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an index stub with a hard-coded selectivity estimate
/// this is used in the cluster coordinator case
////////////////////////////////////////////////////////////////////////////////

PrimaryIndex::PrimaryIndex(VPackSlice const& slice)
    : Index(slice), _primaryIndex(nullptr) {}

PrimaryIndex::~PrimaryIndex() { delete _primaryIndex; }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of documents from the index
////////////////////////////////////////////////////////////////////////////////

size_t PrimaryIndex::size() const { return _primaryIndex->size(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the memory usage of the index
////////////////////////////////////////////////////////////////////////////////

size_t PrimaryIndex::memory() const { return _primaryIndex->memoryUsage(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a VelocyPack representation of the index
////////////////////////////////////////////////////////////////////////////////

void PrimaryIndex::toVelocyPack(VPackBuilder& builder, bool withFigures) const {
  Index::toVelocyPack(builder, withFigures);
  // hard-coded
  builder.add("unique", VPackValue(true));
  builder.add("sparse", VPackValue(false));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a VelocyPack representation of the index figures
////////////////////////////////////////////////////////////////////////////////

void PrimaryIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  Index::toVelocyPackFigures(builder);
  _primaryIndex->appendToVelocyPack(builder);
}

int PrimaryIndex::insert(arangodb::Transaction*, TRI_doc_mptr_t const*, bool) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

int PrimaryIndex::remove(arangodb::Transaction*, TRI_doc_mptr_t const*, bool) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given a key
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookup(arangodb::Transaction* trx,
                                     VPackSlice const& slice) const {
  TRI_ASSERT(slice.isArray() && slice.length() == 1);
  
  VPackSlice tmp = slice.at(0);
  TRI_ASSERT(tmp.isObject() && tmp.hasKey(TRI_SLICE_KEY_EQUAL));
  tmp = tmp.get(TRI_SLICE_KEY_EQUAL);
  return _primaryIndex->findByKey(trx, tmp.begin());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given a key
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupKey(arangodb::Transaction* trx,
                                        VPackSlice const& key) const {
  TRI_ASSERT(key.isString());
  return _primaryIndex->findByKey(trx, key.begin());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        a random order.
///        Returns nullptr if all documents have been returned.
///        Convention: step === 0 indicates a new start.
///        DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupRandom(
    arangodb::Transaction* trx,
    arangodb::basics::BucketPosition& initialPosition,
    arangodb::basics::BucketPosition& position, uint64_t& step,
    uint64_t& total) {
  return _primaryIndex->findRandom(trx, initialPosition, position, step, total);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        a sequential order.
///        Returns nullptr if all documents have been returned.
///        Convention: position === 0 indicates a new start.
///        DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupSequential(
    arangodb::Transaction* trx, arangodb::basics::BucketPosition& position,
    uint64_t& total) {
  return _primaryIndex->findSequential(trx, position, total);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief request an iterator over all elements in the index in
///        a sequential order.
//////////////////////////////////////////////////////////////////////////////

IndexIterator* PrimaryIndex::allIterator(arangodb::Transaction* trx,
                                         bool reverse) const {
  return new AllIndexIterator(trx, _primaryIndex, reverse);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief request an iterator over all elements in the index in
///        a random order. It is guaranteed that each element is found
///        exactly once unless the collection is modified.
//////////////////////////////////////////////////////////////////////////////

IndexIterator* PrimaryIndex::anyIterator(arangodb::Transaction* trx) const {
  return new AnyIndexIterator(trx, _primaryIndex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        reversed sequential order.
///        Returns nullptr if all documents have been returned.
///        Convention: position === UINT64_MAX indicates a new start.
///        DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupSequentialReverse(
    arangodb::Transaction* trx, arangodb::basics::BucketPosition& position) {
  return _primaryIndex->findSequentialReverse(trx, position);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// returns a status code, and *found will contain a found element (if any)
////////////////////////////////////////////////////////////////////////////////

int PrimaryIndex::insertKey(arangodb::Transaction* trx, TRI_doc_mptr_t* header,
                            void const** found) {
  *found = nullptr;
  int res = _primaryIndex->insert(trx, header);

  if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
    *found = _primaryIndex->find(trx, header);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// this is a special, optimized version that receives the target slot index
/// from a previous lookupKey call
////////////////////////////////////////////////////////////////////////////////

int PrimaryIndex::insertKey(arangodb::Transaction* trx, TRI_doc_mptr_t* header,
                            arangodb::basics::BucketPosition const& position) {
  return _primaryIndex->insertAtPosition(trx, header, position);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element from the index
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::removeKey(arangodb::Transaction* trx,
                                        VPackSlice const& slice) {
  return _primaryIndex->removeByKey(trx, slice.begin());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the index
////////////////////////////////////////////////////////////////////////////////

int PrimaryIndex::resize(arangodb::Transaction* trx, size_t targetSize) {
  return _primaryIndex->resize(trx, targetSize);
}

uint64_t PrimaryIndex::calculateHash(arangodb::Transaction* trx,
                                     VPackSlice const& slice) {
  // can use fast hash-function here, as index values are restricted to strings
  return slice.hashString();
}

uint64_t PrimaryIndex::calculateHash(arangodb::Transaction* trx,
                                     uint8_t const* key) {
  return HashKey(trx, key);
}

void PrimaryIndex::invokeOnAllElements(
    std::function<bool(TRI_doc_mptr_t*)> work) {
  _primaryIndex->invokeOnAllElements(work);
}

void PrimaryIndex::invokeOnAllElementsForRemoval(
    std::function<bool(TRI_doc_mptr_t*)> work) {
  _primaryIndex->invokeOnAllElementsForRemoval(work);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether the index supports the condition
////////////////////////////////////////////////////////////////////////////////

bool PrimaryIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {

  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IndexIterator for the given Condition
////////////////////////////////////////////////////////////////////////////////

IndexIterator* PrimaryIndex::iteratorForCondition(
    arangodb::Transaction* trx, IndexIteratorContext* context,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) const {
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);
  TRI_ASSERT(node->numMembers() == 1);

  auto comp = node->getMember(0);

  // assume a.b == value
  auto attrNode = comp->getMember(0);
  auto valNode = comp->getMember(1);

  if (attrNode->type != aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    // value == a.b  ->  flip the two sides
    attrNode = comp->getMember(1);
    valNode = comp->getMember(0);
  }
  TRI_ASSERT(attrNode->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
    // a.b == value
    return createIterator(
        trx, context, attrNode,
        std::vector<arangodb::aql::AstNode const*>({valNode}));
  } else if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // a.b IN values
    if (!valNode->isArray()) {
      return nullptr;
    }

    std::vector<arangodb::aql::AstNode const*> valNodes;
    size_t const n = valNode->numMembers();
    valNodes.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      valNodes.emplace_back(valNode->getMemberUnchecked(i));
      TRI_IF_FAILURE("PrimaryIndex::iteratorValNodes") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }

    return createIterator(trx, context, attrNode, valNodes);
  }

  // operator type unsupported
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IndexIterator for the given slice
////////////////////////////////////////////////////////////////////////////////

IndexIterator* PrimaryIndex::iteratorForSlice(
    arangodb::Transaction* trx, IndexIteratorContext* ctxt,
    arangodb::velocypack::Slice const searchValues, bool) const {
  if (!searchValues.isArray()) {
    // Invalid searchValue
    return nullptr;
  }
  auto builder = std::make_unique<VPackBuilder>();
  builder->add(searchValues);
  return new PrimaryIndexIterator(trx, this, builder);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief specializes the condition for use with the index
////////////////////////////////////////////////////////////////////////////////

arangodb::aql::AstNode* PrimaryIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {

  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.specializeOne(this, node, reference);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the iterator
////////////////////////////////////////////////////////////////////////////////

IndexIterator* PrimaryIndex::createIterator(
    arangodb::Transaction* trx, IndexIteratorContext* context,
    arangodb::aql::AstNode const* attrNode,
    std::vector<arangodb::aql::AstNode const*> const& valNodes) const {
  // _key or _id?
  bool const isId =
      (strcmp(attrNode->getStringValue(), TRI_VOC_ATTRIBUTE_ID) == 0);

  // only leave the valid elements in the vector
  auto keys = std::make_unique<VPackBuilder>();
  keys->openArray();

  for (auto const& valNode : valNodes) {
    if (!valNode->isStringValue()) {
      continue;
    }
    if (valNode->getStringLength() == 0) {
      continue;
    }

    if (isId) {
      // lookup by _id. now validate if the lookup is performed for the
      // correct collection (i.e. _collection)
      TRI_voc_cid_t cid;
      char const* key;
      int res = context->resolveId(valNode->getStringValue(), cid, key);

      if (res != TRI_ERROR_NO_ERROR) {
        continue;
      }

      TRI_ASSERT(cid != 0);
      TRI_ASSERT(key != nullptr);

      if (!context->isCluster() && cid != _collection->_info.id()) {
        // only continue lookup if the id value is syntactically correct and
        // refers to "our" collection, using local collection id
        continue;
      }

      if (context->isCluster() && cid != _collection->_info.planId()) {
        // only continue lookup if the id value is syntactically correct and
        // refers to "our" collection, using cluster collection id
        continue;
      }

      // use _key value from _id
      keys->openArray();
      keys->openObject();
      keys->add(TRI_SLICE_KEY_EQUAL, VPackValue(key));
      keys->close();
      keys->close();
    } else {
      keys->openArray();
      keys->openObject();
      keys->add(TRI_SLICE_KEY_EQUAL, VPackValue(valNode->getStringValue()));
      keys->close();
      keys->close();
    }
  }

  TRI_IF_FAILURE("PrimaryIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys->close();
  return new PrimaryIndexIterator(trx, this, keys);
}
