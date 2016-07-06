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

#ifndef ARANGOD_REST_SERVER_VOCBASE_CONTEXT_H
#define ARANGOD_REST_SERVER_VOCBASE_CONTEXT_H 1

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Common.h"

#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Rest/RequestContext.h"

struct TRI_server_t;
struct TRI_vocbase_t;

namespace arangodb {
class VocbaseContext : public arangodb::RequestContext {
 public:
  static double ServerSessionTtl;

 public:
  VocbaseContext(HttpRequest*, TRI_vocbase_t*, std::string const&);
  ~VocbaseContext();

 public:
  TRI_vocbase_t* vocbase() const { return _vocbase; }

 public:
  GeneralResponse::ResponseCode authenticate() override final;

 private:
  bool useClusterAuthentication() const;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks the authentication (basic)
  //////////////////////////////////////////////////////////////////////////////

  GeneralResponse::ResponseCode basicAuthentication(const char*);
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks the authentication (jwt)
  //////////////////////////////////////////////////////////////////////////////

  GeneralResponse::ResponseCode jwtAuthentication(std::string const&);

 private: 
  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks the authentication header and sets user if successful
  //////////////////////////////////////////////////////////////////////////////

  GeneralResponse::ResponseCode authenticateRequest(bool* forceOpen);

 private:
  TRI_vocbase_t* _vocbase;
  std::string const _jwtSecret;
};
}

#endif
