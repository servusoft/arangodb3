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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "Agent.h"
#include "State.h"

#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

#include "Aql/Query.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/collection.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::consensus;
using namespace arangodb::velocypack;
using namespace arangodb::rest;


/// Construct with endpoint
State::State(std::string const& endpoint)
    : _agent(nullptr),
      _vocbase(nullptr),
      _endpoint(endpoint),
      _collectionsChecked(false),
      _collectionsLoaded(false),
      _cur(0) {
  std::shared_ptr<Buffer<uint8_t>> buf = std::make_shared<Buffer<uint8_t>>();
  VPackSlice value = arangodb::basics::VelocyPackHelper::EmptyObjectValue();
  buf->append(value.startAs<char const>(), value.byteSize());
  if (!_log.size()) {
    _log.push_back(log_t(arangodb::consensus::index_t(0), term_t(0),
                         arangodb::consensus::id_t(0), buf));
  }
}


/// Default dtor
State::~State() {}


/// Persist one entry
bool State::persist(arangodb::consensus::index_t index, term_t term,
                    arangodb::consensus::id_t lid,
                    arangodb::velocypack::Slice const& entry) {
  Builder body;
  body.add(VPackValue(VPackValueType::Object));
  std::ostringstream i_str;
  i_str << std::setw(20) << std::setfill('0') << index;
  body.add("_key", Value(i_str.str()));
  body.add("term", Value(term));
  body.add("leader", Value((uint32_t)lid));
  body.add("request", entry);
  body.close();

  TRI_ASSERT(_vocbase != nullptr);
  auto transactionContext =
      std::make_shared<StandaloneTransactionContext>(_vocbase);
  SingleCollectionTransaction trx(transactionContext, "log",
                                  TRI_TRANSACTION_WRITE);

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  OperationResult result;
  try {
    result = trx.insert("log", body.slice(), _options);
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to persist log entry:" << e.what();
  }
  res = trx.finish(result.code);

  return (res == TRI_ERROR_NO_ERROR);
}


/// Log transaction (leader)
std::vector<arangodb::consensus::index_t> State::log(
    query_t const& transaction, std::vector<bool> const& appl, term_t term,
    arangodb::consensus::id_t lid) {
  
  std::vector<arangodb::consensus::index_t> idx(appl.size());
  std::vector<bool> good = appl;
  size_t j = 0;
  auto const& slice = transaction->slice();

  TRI_ASSERT(slice.isArray());
  TRI_ASSERT(slice.length() == good.size());
    
  MUTEX_LOCKER(mutexLocker, _logLock);  // log entries must stay in order
  for (auto const& i : VPackArrayIterator(slice)) {
    TRI_ASSERT(i.isArray());
    if (good[j]) {
      std::shared_ptr<Buffer<uint8_t>> buf =
          std::make_shared<Buffer<uint8_t>>();
      buf->append((char const*)i[0].begin(), i[0].byteSize());
      idx[j] = _log.back().index + 1;
      _log.push_back(log_t(idx[j], term, lid, buf)); // log to RAM
      persist(idx[j], term, lid, i[0]);              // log to disk
      ++j;
    }
  }

  return idx;
}


/// Log transactions (follower)
arangodb::consensus::index_t State::log(
  query_t const& transactions, term_t term, arangodb::consensus::id_t lid,
  arangodb::consensus::index_t prevLogIndex, term_t prevLogTerm) {

  if (transactions->slice().type() != VPackValueType::Array) {
    return false;
  }

  MUTEX_LOCKER(mutexLocker, _logLock);  // log entries must stay in order
  
  arangodb::consensus::index_t highest = (_log.empty()) ? 0 : _log.back().index;
  for (auto const& i : VPackArrayIterator(transactions->slice())) {
    try {
      auto idx = i.get("index").getUInt();
      if (highest < idx) {
        highest = idx;
      }
      std::shared_ptr<Buffer<uint8_t>> buf = std::make_shared<Buffer<uint8_t>>();
      buf->append((char const*)i.get("query").begin(),i.get("query").byteSize());
      _log.push_back(log_t(idx, term, lid, buf));
      persist(idx, term, lid, i.get("query"));  // to disk
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::AGENCY) << e.what() << " " << __FILE__ << __LINE__;
    }
  }
  
  return highest;
  
}


/// Get log entries from indices "start" to "end"
std::vector<log_t> State::get(arangodb::consensus::index_t start,
                              arangodb::consensus::index_t end) const {

  std::vector<log_t> entries;
  MUTEX_LOCKER(mutexLocker, _logLock);
  
  if (_log.empty()) {
    return entries;
  }
  
  if (end == (std::numeric_limits<uint64_t>::max)() || end > _log.size() - 1) {
    end = _log.size() - 1;
  }
  
  if (start < _log[0].index) {
    start = _log[0].index;
  }

  for (size_t i = start - _cur; i <= end; ++i) {
    entries.push_back(_log[i]);
  }

  return entries;
  
}


/// Get vector of past transaction from 'start' to 'end'
std::vector<VPackSlice> State::slices(
  arangodb::consensus::index_t start, arangodb::consensus::index_t end) const {
  
  std::vector<VPackSlice> slices;
  MUTEX_LOCKER(mutexLocker, _logLock);

  if (_log.empty()) {
    return slices;
  }

  if (start < _log.front().index) { // no start specified
    start = _log.front().index;
  }

  if (start > _log.back().index) {  // no end specified
    return slices;
  }

  if (end == (std::numeric_limits<uint64_t>::max)() || end > _log.back().index) {
    end = _log.back().index;
  }

  for (size_t i = start - _cur; i <= end - _cur; ++i) {
    try {
      slices.push_back(VPackSlice(_log.at(i).entry->data()));
    } catch (std::exception const&) {
      break;
    } 
  }
  
  return slices;
}


/// Get log entry by log index
log_t const& State::operator[](arangodb::consensus::index_t index) const {
  MUTEX_LOCKER(mutexLocker, _logLock);
  TRI_ASSERT(index - _cur < _log.size());
  return _log.at(index - _cur);
}


/// Get last log entry
log_t const& State::lastLog() const {
  MUTEX_LOCKER(mutexLocker, _logLock);
  TRI_ASSERT(!_log.empty());
  return _log.back();
}


/// Configure with agent
bool State::configure(Agent* agent) {
  _agent = agent;
  _endpoint = agent->endpoint();
  _collectionsChecked = false;
  return true;
}


/// Check if collections exist otherwise create them
bool State::checkCollections() {
  if (!_collectionsChecked) {
    _collectionsChecked = checkCollection("log") && checkCollection("election");
  }
  return _collectionsChecked;
}


/// Create agency collections
bool State::createCollections() {
  if (!_collectionsChecked) {
    return (createCollection("log") && createCollection("election") &&
            createCollection("compact"));
  }
  return _collectionsChecked;
}


/// Check collection by name
bool State::checkCollection(std::string const& name) {
  if (!_collectionsChecked) {
    return (TRI_LookupCollectionByNameVocBase(_vocbase, name) != nullptr);
  }
  return true;
}


/// Create collection by name
bool State::createCollection(std::string const& name) {
  Builder body;
  body.add(VPackValue(VPackValueType::Object));
  body.close();

  VocbaseCollectionInfo parameters(_vocbase, name.c_str(),
                                   TRI_COL_TYPE_DOCUMENT, body.slice(), false);
  TRI_vocbase_col_t const* collection =
      TRI_CreateCollectionVocBase(_vocbase, parameters, parameters.id(), true);

  if (collection == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_errno(), "cannot create collection");
  }

  return true;
}


/// Load collections
bool State::loadCollections(TRI_vocbase_t* vocbase, bool waitForSync) {

  _vocbase = vocbase;
  _options.waitForSync = false;
  _options.silent = true;

  if (loadPersisted()) {
    if (_log.empty()) {
      std::shared_ptr<Buffer<uint8_t>> buf = std::make_shared<Buffer<uint8_t>>();
      VPackSlice value = arangodb::basics::VelocyPackHelper::EmptyObjectValue();
      buf->append(value.startAs<char const>(), value.byteSize());
      _log.push_back(log_t(arangodb::consensus::index_t(0), term_t(0),
                           arangodb::consensus::id_t(0), buf));
      persist(
        0, 0, (std::numeric_limits<arangodb::consensus::id_t>::max)(), value);
    }
    return true;
  }

  return false;
  
}


/// Load actually persisted collections
bool State::loadPersisted() {
  TRI_ASSERT(_vocbase != nullptr);

  if (checkCollection("log") && checkCollection("compact")) {
    return (loadCompacted() && loadRemaining());
  }

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Couldn't find persisted log";
  createCollections();

  return true;
}


/// Load compaction collection
bool State::loadCompacted() {

  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->close();

  std::string const aql(
      std::string("FOR c IN compact SORT c._key DESC LIMIT 1 RETURN c"));

  arangodb::aql::Query query(false, _vocbase, aql.c_str(), aql.size(), bindVars,
                             nullptr, arangodb::aql::PART_MAIN);

  auto queryResult = query.execute(QueryRegistryFeature::QUERY_REGISTRY);

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }

  VPackSlice result = queryResult.result->slice();

  if (result.isArray() && result.length()) {
    for (auto const& i : VPackArrayIterator(result)) {
      buffer_t tmp = std::make_shared<arangodb::velocypack::Buffer<uint8_t>>();
      (*_agent) = i;
      try {
        _cur = std::stoul(i.get("_key").copyString());
      } catch (std::exception const& e) {
        LOG_TOPIC(ERR, Logger::AGENCY) << e.what() << " " << __FILE__ << __LINE__;
      }
    }
  }

  return true;
  
}


/// Load beyond last compaction
bool State::loadRemaining() {
  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->close();

  std::string const aql(std::string("FOR l IN log SORT l._key RETURN l"));
  arangodb::aql::Query query(false, _vocbase, aql.c_str(), aql.size(), bindVars,
                             nullptr, arangodb::aql::PART_MAIN);

  auto queryResult = query.execute(QueryRegistryFeature::QUERY_REGISTRY);

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }

  auto result = queryResult.result->slice();

  if (result.isArray()) {
    _log.clear();
    for (auto const& i : VPackArrayIterator(result)) {
      buffer_t tmp = std::make_shared<arangodb::velocypack::Buffer<uint8_t>>();
      auto req = i.get("request");
      tmp->append(req.startAs<char const>(), req.byteSize());
      try {
        _log.push_back(
          log_t(
            std::stoi(i.get(StaticStrings::KeyString).copyString()),
            static_cast<term_t>(i.get("term").getUInt()),
            static_cast<arangodb::consensus::id_t>(i.get("leader").getUInt()),
            tmp));
      } catch (std::exception const& e) {
        LOG_TOPIC(ERR, Logger::AGENCY) <<
          "Failed to convert " + i.get(StaticStrings::KeyString).copyString() +
          " to integer via std::stoi." << e.what(); 
      }
    }
  }

  return true;
  
}


/// Find entry by index and term
bool State::find(arangodb::consensus::index_t prevIndex, term_t prevTerm) {
  MUTEX_LOCKER(mutexLocker, _logLock);
  if (prevIndex > _log.size()) {
    return false;
  }
  return _log.at(prevIndex).term == prevTerm;
}


/// Log compaction
bool State::compact(arangodb::consensus::index_t cind) {
  bool saved = persistReadDB(cind);

  if (saved) {
    compactVolatile(cind);

    try {
      compactPersisted(cind);
      removeObsolete(cind);
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to compact persisted store.";
      LOG_TOPIC(ERR, Logger::AGENCY) << e.what();
    }
    return true;
  } else {
    return false;
  }
}


/// Compact volatile state
bool State::compactVolatile(arangodb::consensus::index_t cind) {
  if (!_log.empty() && cind > _cur && cind - _cur < _log.size()) {
    MUTEX_LOCKER(mutexLocker, _logLock);
    _log.erase(_log.begin(), _log.begin() + (cind - _cur));
    _cur = _log.begin()->index;
  }
  return true;
}


/// Compact persisted state
bool State::compactPersisted(arangodb::consensus::index_t cind) {
  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->close();

  std::stringstream i_str;
  i_str << std::setw(20) << std::setfill('0') << cind;

  std::string const aql(std::string("FOR l IN log FILTER l._key < \"") +
                        i_str.str() + "\" REMOVE l IN log");

  arangodb::aql::Query query(false, _vocbase, aql.c_str(), aql.size(), bindVars,
                             nullptr, arangodb::aql::PART_MAIN);

  auto queryResult = query.execute(QueryRegistryFeature::QUERY_REGISTRY);

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }

  return true;
  
}


/// Remove outdate compactions
bool State::removeObsolete(arangodb::consensus::index_t cind) {
  if (cind > 3 * _agent->config().compactionStepSize) {
    auto bindVars = std::make_shared<VPackBuilder>();
    bindVars->openObject();
    bindVars->close();

    std::stringstream i_str;
    i_str << std::setw(20) << std::setfill('0')
          << -3 * _agent->config().compactionStepSize + cind;

    std::string const aql(std::string("FOR c IN compact FILTER c._key < \"") +
                          i_str.str() + "\" REMOVE c IN compact");

    arangodb::aql::Query query(false, _vocbase, aql.c_str(), aql.size(),
                               bindVars, nullptr, arangodb::aql::PART_MAIN);

    auto queryResult = query.execute(QueryRegistryFeature::QUERY_REGISTRY);

    if (queryResult.code != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
    }

    if (queryResult.code != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
    }
  }
  return true;
}


/// Persist the globally commited truth
bool State::persistReadDB(arangodb::consensus::index_t cind) {
  
  if (checkCollection("compact")) {
    Builder store;
    store.openObject();
    store.add("readDB", VPackValue(VPackValueType::Array));
    _agent->readDB().dumpToBuilder(store);
    store.close();
    std::stringstream i_str;
    i_str << std::setw(20) << std::setfill('0') << cind;
    store.add("_key", VPackValue(i_str.str()));
    store.close();

    TRI_ASSERT(_vocbase != nullptr);
    auto transactionContext =
        std::make_shared<StandaloneTransactionContext>(_vocbase);
    SingleCollectionTransaction trx(transactionContext, "compact",
                                    TRI_TRANSACTION_WRITE);

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    auto result = trx.insert("compact", store.slice(), _options);
    res = trx.finish(result.code);

    return (res == TRI_ERROR_NO_ERROR);
  }

  LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to persist read DB for compaction!";
  return false;
}
