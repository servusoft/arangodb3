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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterMethods.h"
#include "Basics/conversions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Indexes/Index.h"
#include "VocBase/Traverser.h"
#include "VocBase/server.h"

#include <velocypack/Buffer.h>
#include <velocypack/Helpers.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
using namespace arangodb::rest;

static double const CL_DEFAULT_TIMEOUT = 60.0;

namespace arangodb {

static int handleGeneralCommErrors(ClusterCommResult const* res) {
  // This function creates an error code from a ClusterCommResult, 
  // but only if it is a communication error. If the communication
  // was successful and there was an HTTP error code, this function
  // returns TRI_ERROR_NO_ERROR.
  // If TRI_ERROR_NO_ERROR is returned, then the result was CL_COMM_RECEIVED
  // and .answer can safely be inspected.
  if (res->status == CL_COMM_TIMEOUT) {
    // No reply, we give up:
    return TRI_ERROR_CLUSTER_TIMEOUT;
  } else if (res->status == CL_COMM_ERROR) {
    return TRI_ERROR_CLUSTER_CONNECTION_LOST;
  } else if (res->status == CL_COMM_BACKEND_UNAVAILABLE) {
    if (res->result == nullptr) {
      return TRI_ERROR_CLUSTER_CONNECTION_LOST;
    }
    if (!res->result->isComplete()) {
      // there is no result
      return TRI_ERROR_CLUSTER_CONNECTION_LOST;
    }
    return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a numeric value from an hierarchical VelocyPack
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static T ExtractFigure(VPackSlice const& slice, char const* group,
                       char const* name) {
  TRI_ASSERT(slice.isObject());
  VPackSlice g = slice.get(group);

  if (!g.isObject()) {
    return static_cast<T>(0);
  }
  return arangodb::basics::VelocyPackHelper::getNumericValue<T>(g, name, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts answer from response into a VPackBuilder.
///        If there was an error extracting the answer the builder will be
///        empty.
///        No Error can be thrown.
////////////////////////////////////////////////////////////////////////////////

static std::shared_ptr<VPackBuilder> ExtractAnswer(
    ClusterCommResult const& res) {
  try {
    return VPackParser::fromJson(res.answer->body());
  } catch (...) {
    // Return an empty Builder
    return std::make_shared<VPackBuilder>();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge the baby-object results.
///        The shard map contains the ordering of elements, the vector in this
///        Map is expected to be sorted from front to back.
///        The second map contains the answers for each shard.
///        The builder in the third parameter will be cleared and will contain
///        the resulting array. It is guaranteed that the resulting array indexes
///        are equal to the original request ordering before it was destructured
///        for babies.
////////////////////////////////////////////////////////////////////////////////

static void mergeResults(
    std::vector<std::pair<ShardID, VPackValueLength>> const& reverseMapping,
    std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> const& resultMap,
    std::shared_ptr<VPackBuilder>& resultBody) {
  resultBody->clear();
  resultBody->openArray();
  for (auto const& pair : reverseMapping) {
    VPackSlice arr = resultMap.find(pair.first)->second->slice();
    resultBody->add(arr.at(pair.second));
  }
  resultBody->close();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge the baby-object results. (all shards version)
///        results contians the result from all shards in any order.
///        resultBody will be cleared and contains the merged result after this function
///        errorCounter will correctly compute the NOT_FOUND counter, all other
///        codes remain unmodified.
///        
///        The merge is executed the following way:
///        FOR every expected document we scan iterate over the corresponding response
///        of each shard. If any of them returned sth. different than NOT_FOUND
///        we take this result as correct.
///        If none returned sth different than NOT_FOUND we return NOT_FOUND as well
////////////////////////////////////////////////////////////////////////////////

static void mergeResultsAllShards(
    std::vector<std::shared_ptr<VPackBuilder>> const& results,
    std::shared_ptr<VPackBuilder>& resultBody,
    std::unordered_map<int, size_t>& errorCounter,
    VPackValueLength const expectedResults) {
  // errorCounter is not allowed to contain any NOT_FOUND entry.
  TRI_ASSERT(errorCounter.find(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) == errorCounter.end());
  size_t realNotFound = 0;
  VPackBuilder cmp;
  cmp.openObject();
  cmp.add("error", VPackValue(true));
  cmp.add("errorNum", VPackValue(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND));
  cmp.close();
  VPackSlice notFound = cmp.slice();
  resultBody->clear();
  resultBody->openArray();
  for (VPackValueLength currentIndex = 0; currentIndex < expectedResults; ++currentIndex) {
    bool foundRes = false;
    for (auto const& it: results) {
      VPackSlice oneRes = it->slice();
      TRI_ASSERT(oneRes.isArray());
      oneRes = oneRes.at(currentIndex);
      if (!oneRes.equals(notFound)) {
        // This is the correct result
        // Use it
        resultBody->add(oneRes);
        foundRes = true;
        break;
      }
    }
    if (!foundRes) {
      // Found none, use NOT_FOUND
      resultBody->add(notFound);
      realNotFound++;
    }
  }
  resultBody->close();
  if (realNotFound > 0) {
    errorCounter.emplace(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, realNotFound);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Extract all error baby-style error codes and store them in a map
////////////////////////////////////////////////////////////////////////////////

static void extractErrorCodes(ClusterCommResult const& res,
                              std::unordered_map<int, size_t>& errorCounter,
                              bool includeNotFound) {
  auto resultHeaders = res.answer->headers();
  auto codes = resultHeaders.find(StaticStrings::ErrorCodes);
  if (codes != resultHeaders.end()) {
    auto parsedCodes = VPackParser::fromJson(codes->second);
    VPackSlice codesSlice = parsedCodes->slice();
    TRI_ASSERT(codesSlice.isObject());
    for (auto const& code : VPackObjectIterator(codesSlice)) {
      VPackValueLength codeLength;
      char const* codeString = code.key.getString(codeLength);
      int codeNr = static_cast<int>(
          arangodb::basics::StringUtils::int64(codeString, static_cast<size_t>(codeLength)));
      if (includeNotFound || codeNr != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        errorCounter[codeNr] += code.value.getNumericValue<size_t>();
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Distribute one document onto a shard map. If this returns
///        TRI_ERROR_NO_ERROR the correct shard could be determined, if
///        it returns sth. else this document is NOT contained in the shardMap
////////////////////////////////////////////////////////////////////////////////

static int distributeBabyOnShards(
    std::unordered_map<ShardID, std::vector<VPackValueLength>>& shardMap,
    ClusterInfo* ci, std::string const& collid,
    std::shared_ptr<CollectionInfo> collinfo,
    std::vector<std::pair<ShardID, VPackValueLength>>& reverseMapping,
    VPackSlice const node, VPackValueLength const index) {
  // Now find the responsible shard:
  bool usesDefaultShardingAttributes;
  ShardID shardID;
  int error = ci->getResponsibleShard(collid, node, false, shardID,
                                      usesDefaultShardingAttributes);
  if (error == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
    return TRI_ERROR_CLUSTER_SHARD_GONE;
  }
  if (error != TRI_ERROR_NO_ERROR) {
    // We can not find a responsible shard
    return error;
  }

  // We found the responsible shard. Add it to the list.
  auto it = shardMap.find(shardID);
  if (it == shardMap.end()) {
    std::vector<VPackValueLength> counter({index});
    shardMap.emplace(shardID, counter);
    reverseMapping.emplace_back(shardID, 0);
  } else {
    it->second.emplace_back(index);
    reverseMapping.emplace_back(shardID, it->second.size() - 1);
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Distribute one document onto a shard map. If this returns
///        TRI_ERROR_NO_ERROR the correct shard could be determined, if
///        it returns sth. else this document is NOT contained in the shardMap.
///        Also generates a key if necessary.
////////////////////////////////////////////////////////////////////////////////

static int distributeBabyOnShards(
    std::unordered_map<ShardID, std::vector<std::pair<VPackValueLength, std::string>>>& shardMap,
    ClusterInfo* ci, std::string const& collid,
    std::shared_ptr<CollectionInfo> collinfo,
    std::vector<std::pair<ShardID, VPackValueLength>>& reverseMapping,
    VPackSlice const node, VPackValueLength const index) {


  ShardID shardID;
  bool userSpecifiedKey = false;
  std::string _key = "";

  if (!node.isObject()) {
    // We have invalid input at this point.
    // However we can work with the other babies.
    // This is for compatibility with single server
    // We just asign it to any shard and pretend the user has given a key
    std::shared_ptr<std::vector<ShardID>> shards = ci->getShardList(collid);
    shardID = shards->at(0);
    userSpecifiedKey = true;
  } else {

    // Sort out the _key attribute:
    // The user is allowed to specify _key, provided that _key is the one
    // and only sharding attribute, because in this case we can delegate
    // the responsibility to make _key attributes unique to the responsible
    // shard. Otherwise, we ensure uniqueness here and now by taking a
    // cluster-wide unique number. Note that we only know the sharding
    // attributes a bit further down the line when we have determined
    // the responsible shard.

    VPackSlice keySlice = node.get(StaticStrings::KeyString);
    if (keySlice.isNone()) {
      // The user did not specify a key, let's create one:
      uint64_t uid = ci->uniqid();
      _key = arangodb::basics::StringUtils::itoa(uid);
    } else {
      userSpecifiedKey = true;
    }

    // Now find the responsible shard:
    bool usesDefaultShardingAttributes;
    int error = TRI_ERROR_NO_ERROR;
    if (userSpecifiedKey) {
      error = ci->getResponsibleShard(collid, node, true, shardID,
                                      usesDefaultShardingAttributes);
    } else {
      error = ci->getResponsibleShard(collid, node, true, shardID,
                                      usesDefaultShardingAttributes, _key);
    }
    if (error == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
      return TRI_ERROR_CLUSTER_SHARD_GONE;
    }

    // Now perform the above mentioned check:
    if (userSpecifiedKey &&
        (!usesDefaultShardingAttributes || !collinfo->allowUserKeys())) {
      return TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY;
    }
  }

  // We found the responsible shard. Add it to the list.
  auto it = shardMap.find(shardID);
  if (it == shardMap.end()) {
    std::vector<std::pair<VPackValueLength, std::string>> counter(
        {{index, _key}});
    shardMap.emplace(shardID, counter);
    reverseMapping.emplace_back(shardID, 0);
  } else {
    it->second.emplace_back(index, _key);
    reverseMapping.emplace_back(shardID, it->second.size() - 1);
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Collect the results from all shards (fastpath variant)
///        All result bodies are stored in resultMap
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static void collectResultsFromAllShards(
    std::unordered_map<ShardID, std::vector<T>> const& shardMap,
    std::vector<ClusterCommRequest>& requests,
    std::unordered_map<int, size_t>& errorCounter,
    std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>>& resultMap,
    GeneralResponse::ResponseCode& responseCode) {
  // If none of the shards responds we return a SERVER_ERROR;
  responseCode = GeneralResponse::ResponseCode::SERVER_ERROR;
  for (auto const& req : requests) {
    auto res = req.result;

    int commError = handleGeneralCommErrors(&res);
    if (commError != TRI_ERROR_NO_ERROR) {
      auto tmpBuilder = std::make_shared<VPackBuilder>();
      // If there was no answer whatsoever, we cannot rely on the shardId
      // being present in the result struct:
      ShardID sId = req.destination.substr(6);
      auto weSend = shardMap.find(sId);
      TRI_ASSERT(weSend != shardMap.end());  // We send sth there earlier.
      size_t count = weSend->second.size();
      for (size_t i = 0; i < count; ++i) {
        tmpBuilder->openObject();
        tmpBuilder->add("error", VPackValue(true));
        tmpBuilder->add("errorNum", VPackValue(commError));
        tmpBuilder->close();
      }
      resultMap.emplace(sId, tmpBuilder);
    } else {
      TRI_ASSERT(res.answer != nullptr);
      resultMap.emplace(res.shardID,
                        res.answer->toVelocyPack(&VPackOptions::Defaults));
      extractErrorCodes(res, errorCounter, true);
      responseCode = res.answer_code;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a copy of all HTTP headers to forward
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<std::string, std::string> getForwardableRequestHeaders(
    arangodb::HttpRequest* request) {
  std::unordered_map<std::string, std::string> const& headers = request->headers();
  std::unordered_map<std::string, std::string>::const_iterator it = headers.begin();

  std::unordered_map<std::string, std::string> result;

  while (it != headers.end()) {
    std::string const& key = (*it).first;

    // ignore the following headers
    if (key != "x-arango-async" && key != "authorization" &&
        key != "content-length" && key != "connection" && key != "expect" &&
        key != "host" && key != "origin" &&
        key != StaticStrings::ErrorCodes &&
        key.substr(0, 14) != "access-control") {
      result.emplace(key, (*it).second);
    }
    ++it;
  }

  result["content-length"] = StringUtils::itoa(request->contentLength());

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a list of attributes have the same values in two vpack
/// documents
////////////////////////////////////////////////////////////////////////////////

bool shardKeysChanged(std::string const& dbname, std::string const& collname,
                      VPackSlice const& oldValue, VPackSlice const& newValue,
                      bool isPatch) {
  if (!oldValue.isObject() || !newValue.isObject()) {
    // expecting two objects. everything else is an error
    return true;
  }

  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<CollectionInfo> c = ci->getCollection(dbname, collname);
  std::vector<std::string> const& shardKeys = c->shardKeys();

  for (size_t i = 0; i < shardKeys.size(); ++i) {
    if (shardKeys[i] == StaticStrings::KeyString) {
      continue;
    }

    VPackSlice n = newValue.get(shardKeys[i]);

    if (n.isNone() && isPatch) {
      // attribute not set in patch document. this means no update
      continue;
    }
   
    // a temporary buffer to hold a null value 
    char buffer[1];
    VPackSlice nullValue = arangodb::velocypack::buildNullValue(&buffer[0], sizeof(buffer));

    VPackSlice o = oldValue.get(shardKeys[i]);

    if (o.isNone()) {
      // if attribute is undefined, use "null" instead
      o = nullValue;
    }

    if (n.isNone()) {
      // if attribute is undefined, use "null" instead
      n = nullValue;
    }

    if (arangodb::basics::VelocyPackHelper::compare(n, o, false) != 0) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns revision for a sharded collection
////////////////////////////////////////////////////////////////////////////////

int revisionOnCoordinator(std::string const& dbname,
                          std::string const& collname, TRI_voc_rid_t& rid) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);

  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  rid = 0;

  // If we get here, the sharding attributes are not only _key, therefore
  // we have to contact everybody:
  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();

  for (auto const& p : *shards) {
    auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
    cc->asyncRequest(
        "", coordTransactionID, "shard:" + p.first,
        arangodb::GeneralRequest::RequestType::GET,
        "/_db/" + StringUtils::urlEncode(dbname) + "/_api/collection/" +
            StringUtils::urlEncode(p.first) + "/revision",
        std::shared_ptr<std::string const>(), headers, nullptr, 300.0);
  }

  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::GeneralResponse::ResponseCode::OK) {
        std::shared_ptr<VPackBuilder> answerBuilder = ExtractAnswer(res);
        VPackSlice answer = answerBuilder->slice();

        if (answer.isObject()) {
          VPackSlice r = answer.get("revision");

          if (r.isString()) {
            TRI_voc_rid_t cmp = StringUtils::uint64(r.copyString());

            if (cmp > rid) {
              // get the maximum value
              rid = cmp;
            }
          }
          nrok++;
        }
      }
    }
  }

  if (nrok != (int)shards->size()) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                              // the DBserver could have reported an error.
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns figures for a sharded collection
////////////////////////////////////////////////////////////////////////////////

int figuresOnCoordinator(std::string const& dbname, std::string const& collname,
                         TRI_doc_collection_info_t*& result) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);

  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  // prefill with 0s
  result = (TRI_doc_collection_info_t*)TRI_Allocate(
      TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_collection_info_t), true);

  if (result == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // If we get here, the sharding attributes are not only _key, therefore
  // we have to contact everybody:
  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();

  for (auto const& p : *shards) {
    auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
    cc->asyncRequest(
        "", coordTransactionID, "shard:" + p.first,
        arangodb::GeneralRequest::RequestType::GET,
        "/_db/" + StringUtils::urlEncode(dbname) + "/_api/collection/" +
            StringUtils::urlEncode(p.first) + "/figures",
        std::shared_ptr<std::string const>(), headers, nullptr, 300.0);
  }

  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::GeneralResponse::ResponseCode::OK) {
        std::shared_ptr<VPackBuilder> answerBuilder = ExtractAnswer(res);
        VPackSlice answer = answerBuilder->slice();

        if (answer.isObject()) {
          VPackSlice figures = answer.get("figures");
          if (figures.isObject()) {
            // add to the total
            result->_numberAlive +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "alive", "count");
            result->_numberDead +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "dead", "count");
            result->_numberDeletions +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "dead", "deletion");
            result->_numberIndexes +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "indexes", "count");

            result->_sizeAlive +=
                ExtractFigure<int64_t>(figures, "alive", "size");
            result->_sizeDead +=
                ExtractFigure<int64_t>(figures, "dead", "size");
            result->_sizeIndexes +=
                ExtractFigure<int64_t>(figures, "indexes", "size");

            result->_numberDatafiles +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "datafiles", "count");
            result->_numberJournalfiles +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "journals", "count");
            result->_numberCompactorfiles +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "compactors", "count");

            result->_datafileSize +=
                ExtractFigure<int64_t>(figures, "datafiles", "fileSize");
            result->_journalfileSize +=
                ExtractFigure<int64_t>(figures, "journals", "fileSize");
            result->_compactorfileSize +=
                ExtractFigure<int64_t>(figures, "compactors", "fileSize");

            result->_numberDocumentDitches +=
                arangodb::basics::VelocyPackHelper::getNumericValue<uint64_t>(
                    figures, "documentReferences", 0);
          }
          nrok++;
        }
      }
    }
  }

  if (nrok != (int)shards->size()) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, result);
    result = 0;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                              // the DBserver could have reported an error.
}

////////////////////////////////////////////////////////////////////////////////
/// @brief counts number of documents in a coordinator
////////////////////////////////////////////////////////////////////////////////

int countOnCoordinator(std::string const& dbname, std::string const& collname,
                       uint64_t& result) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  result = 0;

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);

  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  auto shards = collinfo->shardIds();
  std::vector<ClusterCommRequest> requests;
  auto body = std::make_shared<std::string>();
  for (auto const& p : *shards) {
    requests.emplace_back("shard:" + p.first,
                          arangodb::GeneralRequest::RequestType::GET,
                          "/_db/" + StringUtils::urlEncode(dbname) + 
                          "/_api/collection/" +
                          StringUtils::urlEncode(p.first) + "/count", body);
  }
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::QUERIES);
  for (auto& req : requests) {
    auto& res = req.result;
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::GeneralResponse::ResponseCode::OK) {
        std::shared_ptr<VPackBuilder> answerBuilder = ExtractAnswer(res);
        VPackSlice answer = answerBuilder->slice();

        if (answer.isObject()) {
          // add to the total
          result +=
              arangodb::basics::VelocyPackHelper::getNumericValue<uint64_t>(
                  answer, "count", 0);
        } else {
          return TRI_ERROR_INTERNAL;
        }
      } else {
        return static_cast<int>(res.answer_code);
      }
    } else {
      return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
    }
  }
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates one or many documents in a coordinator
///
/// In case of many documents (slice is a VPackArray) it will send to each
/// shard all the relevant documents for this shard only.
/// If one of them fails, this error is reported.
/// There is NO guarantee for the stored documents of all other shards, they may
/// be stored or not. All answers of these shards are dropped.
/// If we return with NO_ERROR it is guaranteed that all shards reported success
/// for their documents.
////////////////////////////////////////////////////////////////////////////////

int createDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    arangodb::OperationOptions const& options, VPackSlice const& slice,
    arangodb::GeneralResponse::ResponseCode& responseCode,
    std::unordered_map<int, size_t>& errorCounter,
    std::shared_ptr<VPackBuilder>& resultBody) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);

  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  std::string const collid = StringUtils::itoa(collinfo->id());
  std::unordered_map<
      ShardID, std::vector<std::pair<VPackValueLength, std::string>>> shardMap;
  std::vector<std::pair<ShardID, VPackValueLength>> reverseMapping;
  bool useMultiple = slice.isArray();

  int res = TRI_ERROR_NO_ERROR;
  if (useMultiple) {
    VPackValueLength length = slice.length();
    for (VPackValueLength idx = 0; idx < length; ++idx) {
      res = distributeBabyOnShards(shardMap, ci, collid, collinfo,
                                   reverseMapping, slice.at(idx), idx);
      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }
  } else {
    res = distributeBabyOnShards(shardMap, ci, collid, collinfo, reverseMapping,
                                 slice, 0);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  std::string const baseUrl =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document?collection=";

  std::string const optsUrlPart =
      std::string("&waitForSync=") + (options.waitForSync ? "true" : "false") +
      "&returnNew=" + (options.returnNew ? "true" : "false") + "&returnOld=" +
      (options.returnOld ? "true" : "false");

  VPackBuilder reqBuilder;

  // Now prepare the requests:
  std::vector<ClusterCommRequest> requests;
  auto body = std::make_shared<std::string>();
  for (auto const& it : shardMap) {
    if (!useMultiple) {
      TRI_ASSERT(it.second.size() == 1);
      auto idx = it.second.front();
      if (idx.second.empty()) {
        body = std::make_shared<std::string>(slice.toJson());
      } else {
        reqBuilder.clear();
        reqBuilder.openObject();
        reqBuilder.add(StaticStrings::KeyString, VPackValue(idx.second));
        TRI_SanitizeObject(slice, reqBuilder);
        reqBuilder.close();
        body = std::make_shared<std::string>(reqBuilder.slice().toJson());
      }
    } else {
      reqBuilder.clear();
      reqBuilder.openArray();
      for (auto const& idx : it.second) {
        if (idx.second.empty()) {
          reqBuilder.add(slice.at(idx.first));
        } else {
          reqBuilder.openObject();
          reqBuilder.add(StaticStrings::KeyString, VPackValue(idx.second));
          TRI_SanitizeObject(slice.at(idx.first), reqBuilder);
          reqBuilder.close();
        }
      }
      reqBuilder.close();
      body = std::make_shared<std::string>(reqBuilder.slice().toJson());
    }

    requests.emplace_back(
        "shard:" + it.first, arangodb::GeneralRequest::RequestType::POST,
        baseUrl + StringUtils::urlEncode(it.first) + optsUrlPart, body);
  }

  // Perform the requests
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::REQUESTS);

  // Now listen to the results:
  if (!useMultiple) {
    TRI_ASSERT(requests.size() == 1);
    auto const& req = requests[0];
    auto& res = req.result;

    int commError = handleGeneralCommErrors(&res);
    if (commError != TRI_ERROR_NO_ERROR) {
      return commError;
    }

    responseCode = res.answer_code;
    TRI_ASSERT(res.answer != nullptr);
    auto parsedResult = res.answer->toVelocyPack(&VPackOptions::Defaults);
    resultBody.swap(parsedResult);
    return TRI_ERROR_NO_ERROR;
  }

  std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> resultMap;

  collectResultsFromAllShards<std::pair<VPackValueLength, std::string>>(
      shardMap, requests, errorCounter, resultMap, responseCode);

  responseCode =
      (options.waitForSync ? GeneralResponse::ResponseCode::CREATED
                           : GeneralResponse::ResponseCode::ACCEPTED);
  mergeResults(reverseMapping, resultMap, resultBody);

  // the cluster operation was OK, however,
  // the DBserver could have reported an error.
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int deleteDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    VPackSlice const slice,
    arangodb::OperationOptions const& options,
    arangodb::GeneralResponse::ResponseCode& responseCode,
    std::unordered_map<int, size_t>& errorCounter,
    std::shared_ptr<arangodb::velocypack::Builder>& resultBody) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);
  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  bool useDefaultSharding = collinfo->usesDefaultShardKeys();
  std::string collid = StringUtils::itoa(collinfo->id());
  bool useMultiple = slice.isArray();

  std::string const baseUrl =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document/";

  std::string const optsUrlPart =
      std::string("?waitForSync=") + (options.waitForSync ? "true" : "false") +
      "&returnOld=" + (options.returnOld ? "true" : "false") +
      "&ignoreRevs=" + (options.ignoreRevs ? "true" : "false");


  VPackBuilder reqBuilder;

  if (useDefaultSharding) {
    // fastpath we know which server is responsible.

    // decompose the input into correct shards.
    // Send the correct documents to the correct shards
    // Merge the results with static merge helper

    std::unordered_map<ShardID, std::vector<VPackValueLength>> shardMap;
    std::vector<std::pair<ShardID, VPackValueLength>> reverseMapping;
    auto workOnOneNode = [&shardMap, &ci, &collid, &collinfo, &reverseMapping](
        VPackSlice const node, VPackValueLength const index) -> int {
      // Sort out the _key attribute and identify the shard responsible for it.

      std::string _key(Transaction::extractKeyPart(node));
      ShardID shardID;
      if (_key.empty()) {
        // We have invalid input at this point.
        // However we can work with the other babies.
        // This is for compatibility with single server
        // We just asign it to any shard and pretend the user has given a key
        std::shared_ptr<std::vector<ShardID>> shards = ci->getShardList(collid);
        shardID = shards->at(0);
      } else {
        // Now find the responsible shard:
        bool usesDefaultShardingAttributes;
        int error = ci->getResponsibleShard(
            collid, arangodb::basics::VelocyPackHelper::EmptyObjectValue(), true,
            shardID, usesDefaultShardingAttributes, _key);

        if (error == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
          return TRI_ERROR_CLUSTER_SHARD_GONE;
        }
      }

      // We found the responsible shard. Add it to the list.
      auto it = shardMap.find(shardID);
      if (it == shardMap.end()) {
        std::vector<VPackValueLength> counter({index});
        shardMap.emplace(shardID, counter);
        reverseMapping.emplace_back(shardID, 0);
      } else {
        it->second.emplace_back(index);
        reverseMapping.emplace_back(shardID, it->second.size() - 1);
      }
      return TRI_ERROR_NO_ERROR;
    };

    if (useMultiple) {
      for (VPackValueLength idx = 0; idx < slice.length(); ++idx) {
        int res = workOnOneNode(slice.at(idx), idx);
        if (res != TRI_ERROR_NO_ERROR) {
          // Is early abortion correct?
          return res;
        }
      }
    } else {
      int res = workOnOneNode(slice, 0);
      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }

    // We sorted the shards correctly.

    // Now prepare the requests:
    std::vector<ClusterCommRequest> requests;
    auto body = std::make_shared<std::string>();
    for (auto const& it : shardMap) {
      if (!useMultiple) {
        TRI_ASSERT(it.second.size() == 1);
        body = std::make_shared<std::string>(slice.toJson());
      } else {
        reqBuilder.clear();
        reqBuilder.openArray();
        for (auto const& idx : it.second) {
          reqBuilder.add(slice.at(idx));
        }
        reqBuilder.close();
        body = std::make_shared<std::string>(reqBuilder.slice().toJson());
      }
      requests.emplace_back(
          "shard:" + it.first,
          arangodb::GeneralRequest::RequestType::DELETE_REQ,
          baseUrl + StringUtils::urlEncode(it.first) + optsUrlPart, body);
    }

    // Perform the requests
    size_t nrDone = 0;
    cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::REQUESTS);

    // Now listen to the results:
    if (!useMultiple) {
      TRI_ASSERT(requests.size() == 1);
      auto const& req = requests[0];
      auto& res = req.result;
      
      int commError = handleGeneralCommErrors(&res);
      if (commError != TRI_ERROR_NO_ERROR) {
        return commError;
      }

      responseCode = res.answer_code;
      TRI_ASSERT(res.answer != nullptr);
      auto parsedResult = res.answer->toVelocyPack(&VPackOptions::Defaults);
      resultBody.swap(parsedResult);
      return TRI_ERROR_NO_ERROR;
    }

    std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> resultMap;
    collectResultsFromAllShards<VPackValueLength>(
        shardMap, requests, errorCounter, resultMap, responseCode);
    mergeResults(reverseMapping, resultMap, resultBody);
    return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                                // the DBserver could have reported an error.
  }

  // slowpath we do not know which server is responsible ask all of them.

  // We simply send the body to all shards and await their results.
  // As soon as we have the results we merge them in the following way:
  // For 1 .. slice.length()
  //    for res : allResults
  //      if res != NOT_FOUND => insert this result. skip other results
  //    end
  //    if (!skipped) => insert NOT_FOUND
 
  auto body = std::make_shared<std::string>(slice.toJson());
  std::vector<ClusterCommRequest> requests;
  auto shardList = ci->getShardList(collid);
  for (auto const& shard : *shardList) {
    requests.emplace_back(
        "shard:" + shard, arangodb::GeneralRequest::RequestType::DELETE_REQ,
        baseUrl + StringUtils::urlEncode(shard) + optsUrlPart, body);
  }

  // Perform the requests
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::REQUESTS);

  // Now listen to the results:
  if (!useMultiple) {
    // Only one can answer, we react a bit differently
    size_t count;
    int nrok = 0;
    for (count = requests.size(); count > 0; count--) {
      auto const& req = requests[count - 1];
      auto res = req.result;
      if (res.status == CL_COMM_RECEIVED) {
        if (res.answer_code !=
                arangodb::GeneralResponse::ResponseCode::NOT_FOUND ||
            (nrok == 0 && count == 1)) {
          nrok++;

          responseCode = res.answer_code;
          TRI_ASSERT(res.answer != nullptr);
          auto parsedResult = res.answer->toVelocyPack(&VPackOptions::Defaults);
          resultBody.swap(parsedResult);
        }
      }
    }

    // Note that nrok is always at least 1!
    if (nrok > 1) {
      return TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS;
    }
    return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                                // the DBserver could have reported an error.
  }

  // We select all results from all shards an merge them back again.
  std::vector<std::shared_ptr<VPackBuilder>> allResults;
  allResults.reserve(shardList->size());
  // If no server responds we return 500
  responseCode = GeneralResponse::ResponseCode::SERVER_ERROR;
  for (auto const& req : requests) {
    auto res = req.result;
    int error = handleGeneralCommErrors(&res);
    if (error != TRI_ERROR_NO_ERROR) {
      // Local data structures are automatically freed
      return error;
    }
    if (res.answer_code == GeneralResponse::ResponseCode::OK ||
        res.answer_code == GeneralResponse::ResponseCode::ACCEPTED) {
      responseCode = res.answer_code;
    }
    TRI_ASSERT(res.answer != nullptr);
    allResults.emplace_back(res.answer->toVelocyPack(&VPackOptions::Defaults));
    extractErrorCodes(res, errorCounter, false);
  }
  // If we get here we get exactly one result for every shard.
  TRI_ASSERT(allResults.size() == shardList->size());
  mergeResultsAllShards(allResults, resultBody, errorCounter, static_cast<size_t>(slice.length()));
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate a cluster collection on a coordinator
////////////////////////////////////////////////////////////////////////////////

int truncateCollectionOnCoordinator(std::string const& dbname,
                                    std::string const& collname) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);

  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  // Some stuff to prepare cluster-intern requests:
  // We have to contact everybody:
  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  for (auto const& p : *shards) {
    auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
    cc->asyncRequest("", coordTransactionID, "shard:" + p.first,
                     arangodb::GeneralRequest::RequestType::PUT,
                     "/_db/" + StringUtils::urlEncode(dbname) +
                         "/_api/collection/" + p.first + "/truncate",
                     std::shared_ptr<std::string>(), headers, nullptr,
                     60.0);
  }
  // Now listen to the results:
  unsigned int count;
  unsigned int nrok = 0;
  for (count = (unsigned int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::GeneralResponse::ResponseCode::OK) {
        nrok++;
      }
    }
  }

  // Note that nrok is always at least 1!
  if (nrok < shards->size()) {
    return TRI_ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION;
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int getDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    VPackSlice const slice, OperationOptions const& options,
    std::unique_ptr<std::unordered_map<std::string, std::string>>& headers,
    arangodb::GeneralResponse::ResponseCode& responseCode,
    std::unordered_map<int, size_t>& errorCounter,
    std::shared_ptr<VPackBuilder>& resultBody) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);
  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  std::string collid = StringUtils::itoa(collinfo->id());

  // If _key is the one and only sharding attribute, we can do this quickly,
  // because we can easily determine which shard is responsible for the
  // document. Otherwise we have to contact all shards and ask them to
  // delete the document. All but one will not know it.
  // Now find the responsible shard(s)
  
  ShardID shardID;

  std::unordered_map<ShardID, std::vector<VPackValueLength>> shardMap;
  std::vector<std::pair<ShardID, VPackValueLength>> reverseMapping;
  bool useMultiple = slice.isArray();

  int res = TRI_ERROR_NO_ERROR;
  bool canUseFastPath = true;
  if (useMultiple) {
    VPackValueLength length = slice.length();
    for (VPackValueLength idx = 0; idx < length; ++idx) {
      res = distributeBabyOnShards(shardMap, ci, collid, collinfo,
                                   reverseMapping, slice.at(idx), idx);
      if (res != TRI_ERROR_NO_ERROR) {
        canUseFastPath = false;
        shardMap.clear();
        reverseMapping.clear();
        break;
      }
    }
  } else {
    res = distributeBabyOnShards(shardMap, ci, collid, collinfo, reverseMapping,
                                 slice, 0);
    if (res != TRI_ERROR_NO_ERROR) {
      canUseFastPath = false;
    }
  }

  // Some stuff to prepare cluster-internal requests:

  std::string baseUrl = "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document/";
  std::string optsUrlPart = std::string("?ignoreRevs=") + (options.ignoreRevs ? "true" : "false");
 
  arangodb::GeneralRequest::RequestType reqType;
  if (!useMultiple) {
    if (options.silent) {
      reqType = arangodb::GeneralRequest::RequestType::HEAD;
    } else {
      reqType = arangodb::GeneralRequest::RequestType::GET;
    }
  } else {
    reqType = arangodb::GeneralRequest::RequestType::PUT;
    if (options.silent) {
      optsUrlPart += std::string("&silent=true");
    }
    optsUrlPart += std::string("&onlyget=true");
  }

  if (canUseFastPath) {
    // All shard keys are known in all documents.
    // Contact all shards directly with the correct information.
 
    VPackBuilder reqBuilder;

    // Now prepare the requests:
    std::vector<ClusterCommRequest> requests;
    auto body = std::make_shared<std::string>();
    for (auto const& it : shardMap) {
      if (!useMultiple) {
        TRI_ASSERT(it.second.size() == 1);
        if (!options.ignoreRevs && slice.hasKey(StaticStrings::RevString)) {
          headers->emplace("if-match", slice.get(StaticStrings::RevString).copyString());
        }
      
        VPackSlice keySlice = slice;
        if (slice.isObject()) {
          keySlice = slice.get(StaticStrings::KeyString);
        }

        // We send to single endpoint
        requests.emplace_back(
            "shard:" + it.first, reqType,
            baseUrl + StringUtils::urlEncode(it.first) + "/" +
                StringUtils::urlEncode(keySlice.copyString()) +
                optsUrlPart,
            body);
        requests[0].setHeaders(headers);
      } else {
        reqBuilder.clear();
        reqBuilder.openArray();
        for (auto const& idx : it.second) {
          reqBuilder.add(slice.at(idx));
        }
        reqBuilder.close();
        body = std::make_shared<std::string>(reqBuilder.slice().toJson());
        // We send to Babies endpoint
        requests.emplace_back(
            "shard:" + it.first, reqType,
            baseUrl + StringUtils::urlEncode(it.first) + optsUrlPart, body);
      }
    }

    // Perform the requests
    size_t nrDone = 0;
    cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::REQUESTS);

    // Now listen to the results:
    if (!useMultiple) {
      TRI_ASSERT(requests.size() == 1);
      auto const& req = requests[0];
      auto res = req.result;

      int commError = handleGeneralCommErrors(&res);
      if (commError != TRI_ERROR_NO_ERROR) {
        return commError;
      }

      responseCode = res.answer_code;
      TRI_ASSERT(res.answer != nullptr);
      auto parsedResult = res.answer->toVelocyPack(&VPackOptions::Defaults);
      resultBody.swap(parsedResult);
      return TRI_ERROR_NO_ERROR;
    }

    std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> resultMap;
    collectResultsFromAllShards<VPackValueLength>(
        shardMap, requests, errorCounter, resultMap, responseCode);

    mergeResults(reverseMapping, resultMap, resultBody);

    // the cluster operation was OK, however,
    // the DBserver could have reported an error.
    return TRI_ERROR_NO_ERROR;
  }

  // Not all shard keys are known in all documents.
  // We contact all shards with the complete body and ignore NOT_FOUND

  std::vector<ClusterCommRequest> requests;
  auto shardList = ci->getShardList(collid);
  if (!useMultiple) {

    if (!options.ignoreRevs && slice.hasKey(StaticStrings::RevString)) {
      headers->emplace("if-match", slice.get(StaticStrings::RevString).copyString());
    }
    for (auto const& shard : *shardList) {
      VPackSlice keySlice = slice;
      if (slice.isObject()) {
        keySlice = slice.get(StaticStrings::KeyString);
      }
      ClusterCommRequest req(
          "shard:" + shard, reqType,
          baseUrl + StringUtils::urlEncode(shard) + "/" +
              StringUtils::urlEncode(keySlice.copyString()) +
              optsUrlPart,
          nullptr);
      auto headersCopy =
          std::make_unique<std::unordered_map<std::string, std::string>>(*headers);
      req.setHeaders(headersCopy);
      requests.emplace_back(std::move(req));
    }
  } else {
    auto body = std::make_shared<std::string>(slice.toJson());
    for (auto const& shard : *shardList) {
      requests.emplace_back(
          "shard:" + shard, reqType,
          baseUrl + StringUtils::urlEncode(shard) + optsUrlPart, body);
    }
  }

  // Perform the requests
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::REQUESTS);

  // Now listen to the results:
  if (!useMultiple) {
    // Only one can answer, we react a bit differently
    size_t count;
    int nrok = 0;
    int commError = TRI_ERROR_NO_ERROR;
    for (count = requests.size(); count > 0; count--) {
      auto const& req = requests[count - 1];
      auto res = req.result;
      if (res.status == CL_COMM_RECEIVED) {
        if (res.answer_code !=
                arangodb::GeneralResponse::ResponseCode::NOT_FOUND ||
            (nrok == 0 && count == 1 && commError == TRI_ERROR_NO_ERROR)) {
          nrok++;
          responseCode = res.answer_code;
          TRI_ASSERT(res.answer != nullptr);
          auto parsedResult = res.answer->toVelocyPack(&VPackOptions::Defaults);
          resultBody.swap(parsedResult);
        }
      } else {
        commError = handleGeneralCommErrors(&res);
      }
    }
    if (nrok == 0) {
      // This can only happen, if a commError was encountered!
      return commError;
    }
    if (nrok > 1) {
      return TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS;
    }
    return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                                // the DBserver could have reported an error.

  }

  // We select all results from all shards and merge them back again.
  std::vector<std::shared_ptr<VPackBuilder>> allResults;
  allResults.reserve(shardList->size());
  // If no server responds we return 500
  responseCode = GeneralResponse::ResponseCode::SERVER_ERROR;
  for (auto const& req : requests) {
    auto& res = req.result;
    int error = handleGeneralCommErrors(&res);
    if (error != TRI_ERROR_NO_ERROR) {
      // Local data structores are automatically freed
      return error;
    }
    if (res.answer_code == GeneralResponse::ResponseCode::OK ||
        res.answer_code == GeneralResponse::ResponseCode::ACCEPTED) {
      responseCode = res.answer_code;
    }
    TRI_ASSERT(res.answer != nullptr);
    allResults.emplace_back(res.answer->toVelocyPack(&VPackOptions::Defaults));
    extractErrorCodes(res, errorCounter, false);
  }
  // If we get here we get exactly one result for every shard.
  TRI_ASSERT(allResults.size() == shardList->size());
  mergeResultsAllShards(allResults, resultBody, errorCounter, static_cast<size_t>(slice.length()));
  return TRI_ERROR_NO_ERROR;
}

static void insertIntoShardMap(
    ClusterInfo* ci, std::string const& dbname, std::string const& documentId,
    std::unordered_map<ShardID, std::vector<std::string>>& shardMap) {
  std::vector<std::string> splitId =
      arangodb::basics::StringUtils::split(documentId, '/');
  TRI_ASSERT(splitId.size() == 2);

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, splitId[0]);
  if (collinfo->empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                   "Collection not found: " + splitId[0]);
  }
  std::string collid = StringUtils::itoa(collinfo->id());
  if (collinfo->usesDefaultShardKeys()) {
    // We only need add one resp. shard
    VPackBuilder partial;
    partial.openObject();
    partial.add(StaticStrings::KeyString, VPackValue(splitId[1]));
    partial.close();
    bool usesDefaultShardingAttributes;
    ShardID shardID;

    int error = ci->getResponsibleShard(collid, partial.slice(), true, shardID,
                                        usesDefaultShardingAttributes);
    if (error != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(error);
    }
    TRI_ASSERT(usesDefaultShardingAttributes);  // If this is false the if
                                                // condition should be false in
                                                // the first place
    auto it = shardMap.find(shardID);
    if (it == shardMap.end()) {
      shardMap.emplace(shardID, std::vector<std::string>({splitId[1]}));
    } else {
      it->second.push_back(splitId[1]);
    }
  } else {
    // Sorry we do not know the responsible shard yet
    // Ask all of them
    auto shardList = ci->getShardList(collid);
    for (auto const& shard : *shardList) {
      auto it = shardMap.find(shard);
      if (it == shardMap.end()) {
        shardMap.emplace(shard, std::vector<std::string>({splitId[1]}));
      } else {
        it->second.push_back(splitId[1]);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a list of filtered documents in a coordinator
///        All found documents will be inserted into result.
///        After execution documentIds will contain all id's of documents
///        that could not be found.
////////////////////////////////////////////////////////////////////////////////

int getFilteredDocumentsOnCoordinator(
    std::string const& dbname,
    std::vector<traverser::TraverserExpression*> const& expressions,
    std::unordered_set<std::string>& documentIds,
    std::unordered_map<std::string, std::shared_ptr<VPackBuffer<uint8_t>>>& result) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  std::unordered_map<ShardID, std::vector<std::string>> shardRequestMap;
  for (auto const& doc : documentIds) {
    try {
      insertIntoShardMap(ci, dbname, doc, shardRequestMap);
    } catch (...) {
    }
  }

  // Now start the request.
  // We do not have to care for shard attributes esp. shard by key.
  // If it is by key the key was only added to one key list, if not
  // it is contained multiple times.
  std::vector<ClusterCommRequest> requests;
  VPackBuilder bodyBuilder;
  for (auto const& shard : shardRequestMap) {
    bodyBuilder.clear();
    bodyBuilder.openObject();
    bodyBuilder.add("collection", VPackValue(shard.first));
    bodyBuilder.add("keys", VPackValue(VPackValueType::Array));
    for (auto const& key : shard.second) {
      bodyBuilder.add(VPackValue(key));
    }
    bodyBuilder.close(); // keys
    if (!expressions.empty()) {
      bodyBuilder.add("filter", VPackValue(VPackValueType::Array));
      for (auto const& e : expressions) {
        e->toVelocyPack(bodyBuilder);
      }
      bodyBuilder.close(); // filter
    }
    bodyBuilder.close(); // Object

    auto bodyString = std::make_shared<std::string>(bodyBuilder.toJson());
    requests.emplace_back("shard:" + shard.first,
                          arangodb::GeneralRequest::RequestType::PUT,
                          "/_db/" + StringUtils::urlEncode(dbname) +
                              "/_api/simple/lookup-by-keys",
                          bodyString);
  }

  // Perform the requests
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::REQUESTS);

  // All requests send, now collect results.
  for (auto const& req : requests) {
    auto& res = req.result;
    if (res.status == CL_COMM_RECEIVED) {
      std::shared_ptr<VPackBuilder> resultBody = res.answer->toVelocyPack(&VPackOptions::Defaults);
      VPackSlice resSlice = resultBody->slice();

      if (!resSlice.isObject()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "Received an invalid result in cluster.");
      }
      bool isError = arangodb::basics::VelocyPackHelper::getBooleanValue(
          resSlice, "error", false);
      if (isError) {
        return arangodb::basics::VelocyPackHelper::getNumericValue<int>(
            resSlice, "errorNum", TRI_ERROR_INTERNAL);
      }
      VPackSlice documents = resSlice.get("documents");
      if (!documents.isArray()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "Received an invalid result in cluster.");
      }
      for (auto const& element : VPackArrayIterator(documents)) {
        std::string id = arangodb::basics::VelocyPackHelper::getStringValue(
            element, StaticStrings::IdString, "");
        VPackBuilder tmp;
        tmp.add(element);
        result.emplace(id, tmp.steal());
      }
      VPackSlice filtered = resSlice.get("filtered");
      if (filtered.isArray()) {
        for (auto const& element : VPackArrayIterator(filtered)) {
          if (element.isString()) {
            std::string id = element.copyString();
            documentIds.erase(id);
          }
        }
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all edges on coordinator using a Traverser Filter
////////////////////////////////////////////////////////////////////////////////

int getFilteredEdgesOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& vertex, TRI_edge_direction_e const& direction,
    std::vector<traverser::TraverserExpression*> const& expressions,
    arangodb::GeneralResponse::ResponseCode& responseCode,
    VPackBuilder& result) {
  TRI_ASSERT(result.isOpenObject());

  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);
  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  auto shards = collinfo->shardIds();
  std::string queryParameters = "?vertex=" + StringUtils::urlEncode(vertex);
  if (direction == TRI_EDGE_IN) {
    queryParameters += "&direction=in";
  } else if (direction == TRI_EDGE_OUT) {
    queryParameters += "&direction=out";
  }
  auto reqBodyString = std::make_shared<std::string>();
  if (!expressions.empty()) {
    VPackBuilder bodyBuilder;
    bodyBuilder.openArray();
    for (auto& e : expressions) {
      e->toVelocyPack(bodyBuilder);
    }
    bodyBuilder.close();
    reqBodyString->append(bodyBuilder.toJson());
  }

  std::vector<ClusterCommRequest> requests;
  std::string baseUrl = "/_db/" + StringUtils::urlEncode(dbname) + "/_api/edges/";

  for (auto const& p : *shards) {
    requests.emplace_back(
        "shard:" + p.first, arangodb::GeneralRequest::RequestType::PUT,
        baseUrl + StringUtils::urlEncode(p.first) + queryParameters,
        reqBodyString);
  }

  // Perform the requests
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::REQUESTS);

  size_t filtered = 0;
  size_t scannedIndex = 0;
  responseCode = arangodb::GeneralResponse::ResponseCode::OK;

  result.add("edges", VPackValue(VPackValueType::Array));

  // All requests send, now collect results.
  for (auto const& req : requests) {
    auto& res = req.result;
    int error = handleGeneralCommErrors(&res);
    if (error != TRI_ERROR_NO_ERROR) {
      // Cluster is in bad state. Report.
      return error;
    }
    TRI_ASSERT(res.answer != nullptr);
    std::shared_ptr<VPackBuilder> shardResult = res.answer->toVelocyPack(&VPackOptions::Defaults);

    if (shardResult == nullptr) {
      return TRI_ERROR_INTERNAL;
    }

    VPackSlice shardSlice = shardResult->slice();
    if (!shardSlice.isObject()) {
      return TRI_ERROR_INTERNAL;
    }

    bool const isError = arangodb::basics::VelocyPackHelper::getBooleanValue(
        shardSlice, "error", false);

    if (isError) {
      // shard returned an error
      return arangodb::basics::VelocyPackHelper::getNumericValue<int>(
          shardSlice, "errorNum", TRI_ERROR_INTERNAL);
    }

    VPackSlice docs = shardSlice.get("edges");

    if (!docs.isArray()) {
      return TRI_ERROR_INTERNAL;
    }

    for (auto const& doc : VPackArrayIterator(docs)) {
      result.add(doc);
    }

    VPackSlice stats = shardSlice.get("stats");
    if (stats.isObject()) {
      filtered += arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
          stats, "filtered", 0);
      scannedIndex += arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
          stats, "scannedIndex", 0);
    }
  }
  result.close(); // edges

  result.add("stats", VPackValue(VPackValueType::Object));
  result.add("scannedIndex", VPackValue(scannedIndex));
  result.add("filtered", VPackValue(filtered));
  result.close(); // stats

  // Leave outer Object open
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief modify a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int modifyDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    VPackSlice const& slice,
    arangodb::OperationOptions const& options, bool isPatch,
    std::unique_ptr<std::unordered_map<std::string, std::string>>& headers,
    arangodb::GeneralResponse::ResponseCode& responseCode,
    std::unordered_map<int, size_t>& errorCounter,
    std::shared_ptr<VPackBuilder>& resultBody) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);
  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  std::string collid = StringUtils::itoa(collinfo->id());

  // We have a fast path and a slow path. The fast path only asks one shard
  // to do the job and the slow path asks them all and expects to get
  // "not found" from all but one shard. We have to cover the following
  // cases:
  //   isPatch == false    (this is a "replace" operation)
  //     Here, the complete new document is given, we assume that we
  //     can read off the responsible shard, therefore can use the fast
  //     path, this is always true if _key is the one and only sharding
  //     attribute, however, if there is any other sharding attribute,
  //     it is possible that the user has changed the values in any of
  //     them, in that case we will get a "not found" or a "sharding
  //     attributes changed answer" in the fast path. In the first case
  //     we have to delegate to the slow path.
  //   isPatch == true     (this is an "update" operation)
  //     In this case we might or might not have all sharding attributes
  //     specified in the partial document given. If _key is the one and
  //     only sharding attribute, it is always given, if not all sharding
  //     attributes are explicitly given (at least as value `null`), we must
  //     assume that the fast path cannot be used. If all sharding attributes
  //     are given, we first try the fast path, but might, as above,
  //     have to use the slow path after all.

  ShardID shardID;

  std::unordered_map<ShardID, std::vector<VPackValueLength>> shardMap;
  std::vector<std::pair<ShardID, VPackValueLength>> reverseMapping;
  bool useMultiple = slice.isArray();

  int res = TRI_ERROR_NO_ERROR;
  bool canUseFastPath = true;
  if (useMultiple) {
    VPackValueLength length = slice.length();
    for (VPackValueLength idx = 0; idx < length; ++idx) {
      res = distributeBabyOnShards(shardMap, ci, collid, collinfo,
                                   reverseMapping, slice.at(idx), idx);
      if (res != TRI_ERROR_NO_ERROR) {
        if (!isPatch) {
          return res;
        }
        canUseFastPath = false;
        shardMap.clear();
        reverseMapping.clear();
        break;
      }
    }
  } else {
    res = distributeBabyOnShards(shardMap, ci, collid, collinfo, reverseMapping,
                                 slice, 0);
    if (res != TRI_ERROR_NO_ERROR) {
      if (!isPatch) {
        return res;
      }
      canUseFastPath = false;
    }
  }


  // Some stuff to prepare cluster-internal requests:

  std::string baseUrl = "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document/";
  std::string optsUrlPart = std::string("?waitForSync=") + (options.waitForSync ? "true" : "false");
  optsUrlPart += std::string("&ignoreRevs=") + (options.ignoreRevs ? "true" : "false");

  arangodb::GeneralRequest::RequestType reqType;
  if (isPatch) {
    reqType = arangodb::GeneralRequest::RequestType::PATCH;
    if (!options.keepNull) {
      optsUrlPart += "&keepNull=false";
    }
    if (options.mergeObjects) {
      optsUrlPart += "&mergeObjects=true";
    } else {
      optsUrlPart += "&mergeObjects=false";
    }
  } else {
    reqType = arangodb::GeneralRequest::RequestType::PUT;
  }
  if (options.returnNew) {
    optsUrlPart += "&returnNew=true";
  }

  if (options.returnOld) {
    optsUrlPart += "&returnOld=true";
  }

  if (canUseFastPath) {
    // All shard keys are known in all documents.
    // Contact all shards directly with the correct information.
    std::vector<ClusterCommRequest> requests;
    VPackBuilder reqBuilder;
    auto body = std::make_shared<std::string>();
    for (auto const& it : shardMap) {
      if (!useMultiple) {
        TRI_ASSERT(it.second.size() == 1);
        body = std::make_shared<std::string>(slice.toJson());

        // We send to single endpoint
        requests.emplace_back(
            "shard:" + it.first, reqType,
            baseUrl + StringUtils::urlEncode(it.first) + "/" +
                slice.get(StaticStrings::KeyString).copyString() + optsUrlPart,
            body);
      } else {
        reqBuilder.clear();
        reqBuilder.openArray();
        for (auto const& idx : it.second) {
          reqBuilder.add(slice.at(idx));
        }
        reqBuilder.close();
        body = std::make_shared<std::string>(reqBuilder.slice().toJson());
        // We send to Babies endpoint
        requests.emplace_back(
            "shard:" + it.first, reqType,
            baseUrl + StringUtils::urlEncode(it.first) + optsUrlPart, body);
      }
    }

    // Perform the requests
    size_t nrDone = 0;
    cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::REQUESTS);

    // Now listen to the results:
    if (!useMultiple) {
      TRI_ASSERT(requests.size() == 1);
      auto res = requests[0].result;

      int commError = handleGeneralCommErrors(&res);
      if (commError != TRI_ERROR_NO_ERROR) {
        return commError;
      }

      responseCode = res.answer_code;
      TRI_ASSERT(res.answer != nullptr);
      auto parsedResult = res.answer->toVelocyPack(&VPackOptions::Defaults);
      resultBody.swap(parsedResult);
      return TRI_ERROR_NO_ERROR;
    }

    std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> resultMap;
    collectResultsFromAllShards<VPackValueLength>(
        shardMap, requests, errorCounter, resultMap, responseCode);

    mergeResults(reverseMapping, resultMap, resultBody);

    // the cluster operation was OK, however,
    // the DBserver could have reported an error.
    return TRI_ERROR_NO_ERROR;
  }

  // Not all shard keys are known in all documents.
  // We contact all shards with the complete body and ignore NOT_FOUND

  std::vector<ClusterCommRequest> requests;
  auto body = std::make_shared<std::string>(slice.toJson());
  auto shardList = ci->getShardList(collid);
  if (!useMultiple) {
    std::string key = slice.get(StaticStrings::KeyString).copyString();
    for (auto const& shard : *shardList) {
      requests.emplace_back(
          "shard:" + shard, reqType,
          baseUrl + StringUtils::urlEncode(shard) + "/" + key + optsUrlPart,
          body);
    }
  } else {
    for (auto const& shard : *shardList) {
      requests.emplace_back(
          "shard:" + shard, reqType,
          baseUrl + StringUtils::urlEncode(shard) + optsUrlPart, body);
    }
  }

  // Perform the requests
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::REQUESTS);

  // Now listen to the results:
  if (!useMultiple) {
    // Only one can answer, we react a bit differently
    int nrok = 0;
    int commError = TRI_ERROR_NO_ERROR;
    for (size_t count = shardList->size(); count > 0; count--) {
      auto const& req = requests[count - 1];
      auto res = req.result;
      if (res.status == CL_COMM_RECEIVED) {
        if (res.answer_code !=
                arangodb::GeneralResponse::ResponseCode::NOT_FOUND ||
            (nrok == 0 && count == 1 && commError == TRI_ERROR_NO_ERROR)) {
          nrok++;
          responseCode = res.answer_code;
          TRI_ASSERT(res.answer != nullptr);
          auto parsedResult = res.answer->toVelocyPack(&VPackOptions::Defaults);
          resultBody.swap(parsedResult);
        }
      } else {
        commError = handleGeneralCommErrors(&res);
      }
    }
    if (nrok == 0) {
      // This can only happen, if a commError was encountered!
      return commError;
    }
    if (nrok > 1) {
      return TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS;
    }
    return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                                // the DBserver could have reported an error.
  }

  responseCode = GeneralResponse::ResponseCode::SERVER_ERROR;
  // We select all results from all shards an merge them back again.
  std::vector<std::shared_ptr<VPackBuilder>> allResults;
  allResults.reserve(requests.size());
  for (auto const& req : requests) {
    auto res = req.result;
    int error = handleGeneralCommErrors(&res);
    if (error != TRI_ERROR_NO_ERROR) {
      // Cluster is in bad state. Just report.
      // Local data structores are automatically freed
      return error;
    }
    if (res.answer_code == GeneralResponse::ResponseCode::OK ||
        res.answer_code == GeneralResponse::ResponseCode::ACCEPTED) {
      responseCode = res.answer_code;
    }
    TRI_ASSERT(res.answer != nullptr);
    allResults.emplace_back(res.answer->toVelocyPack(&VPackOptions::Defaults));
    extractErrorCodes(res, errorCounter, false);
  }
  // If we get here we get exactly one result for every shard.
  TRI_ASSERT(allResults.size() == shardList->size());
  mergeResultsAllShards(allResults, resultBody, errorCounter, static_cast<size_t>(slice.length()));
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush Wal on all DBservers
////////////////////////////////////////////////////////////////////////////////

int flushWalOnAllDBServers(bool waitForSync, bool waitForCollector) {
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();
  std::vector<ServerID> DBservers = ci->getCurrentDBServers();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  std::string url = std::string("/_admin/wal/flush?waitForSync=") +
                    (waitForSync ? "true" : "false") + "&waitForCollector=" +
                    (waitForCollector ? "true" : "false");
  auto body = std::make_shared<std::string const>();
  for (auto it = DBservers.begin(); it != DBservers.end(); ++it) {
    auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
    // set collection name (shard id)
    cc->asyncRequest("", coordTransactionID, "server:" + *it,
                     arangodb::GeneralRequest::RequestType::PUT, url, body,
                     headers, nullptr, 120.0);
  }

  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)DBservers.size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::GeneralResponse::ResponseCode::OK) {
        nrok++;
      }
    }
  }

  if (nrok != (int)DBservers.size()) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

}  // namespace arangodb
