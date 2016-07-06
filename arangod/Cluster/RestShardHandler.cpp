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

#include "RestShardHandler.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterComm.h"
#include "Dispatcher/Dispatcher.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

using namespace arangodb;
using namespace arangodb::rest;

RestShardHandler::RestShardHandler(arangodb::HttpRequest* request)
    : RestBaseHandler(request) {
}

bool RestShardHandler::isDirect() const { return true; }

arangodb::rest::HttpHandler::status_t RestShardHandler::execute() {
  bool found;
  std::string const& _coordinator = _request->header(StaticStrings::Coordinator, found);

  if (!found) {
    generateError(arangodb::GeneralResponse::ResponseCode::BAD,
                  (int)arangodb::GeneralResponse::ResponseCode::BAD,
                  "header 'X-Arango-Coordinator' is missing");
    return status_t(HANDLER_DONE);
  }

  std::string coordinatorHeader = _coordinator;
  std::string result =
      ClusterComm::instance()->processAnswer(coordinatorHeader, stealRequest());

  if (result == "") {
    createResponse(arangodb::GeneralResponse::ResponseCode::ACCEPTED);
  } else {
    generateError(arangodb::GeneralResponse::ResponseCode::BAD,
                  (int)arangodb::GeneralResponse::ResponseCode::BAD, result.c_str());
  }

  return status_t(HANDLER_DONE);
}
