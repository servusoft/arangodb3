////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef APPLICATION_FEATURES_REST_SERVER_FEATURE_H
#define APPLICATION_FEATURES_REST_SERVER_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include "Actions/RestActionHandler.h"
#include "VocBase/AuthInfo.h"

namespace arangodb {
namespace rest {
class AsyncJobManager;
class HttpHandlerFactory;
class HttpServer;
}

class RestServerThread;

class RestServerFeature final
    : public application_features::ApplicationFeature {
 public:
  static AuthInfo AUTH_INFO;

 public:
  static bool authenticationEnabled() {
    return RESTSERVER != nullptr && RESTSERVER->authentication();
  }
  
  static bool hasProxyCheck() {
    return RESTSERVER != nullptr && RESTSERVER->proxyCheck();
  }
  
  static std::vector<std::string> getTrustedProxies() {
    if (RESTSERVER == nullptr) {
      return std::vector<std::string>();
    }
    return RESTSERVER->trustedProxies();
  }

  static std::string getJwtSecret() {
    if (RESTSERVER == nullptr) {
      return std::string();
    }
    return RESTSERVER->jwtSecret();
  }

 private:
  static RestServerFeature* RESTSERVER;
  static const size_t _maxSecretLength = 64;

 public:
  explicit RestServerFeature(application_features::ApplicationServer*);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;
  void unprepare() override final;

 private:
  double _keepAliveTimeout;
  bool _allowMethodOverride;
  bool _authentication;
  bool _authenticationUnixSockets;
  bool _authenticationSystemOnly;

  bool _proxyCheck;
  std::vector<std::string> _trustedProxies;
  std::vector<std::string> _accessControlAllowOrigins;
  
  std::string _jwtSecret;

 public:
  bool authentication() const { return _authentication; }
  bool authenticationUnixSockets() const { return _authenticationUnixSockets; }
  bool authenticationSystemOnly() const { return _authenticationSystemOnly; }
  bool proxyCheck() const { return _proxyCheck; }
  std::vector<std::string> trustedProxies() const { return _trustedProxies; }
  std::string jwtSecret() const { return _jwtSecret; }
  void generateNewJwtSecret();
  void setJwtSecret(std::string const& jwtSecret) { _jwtSecret = jwtSecret; }

 private:
  void buildServers();
  void defineHandlers();

 private:
  std::unique_ptr<rest::HttpHandlerFactory> _handlerFactory;
  std::unique_ptr<rest::AsyncJobManager> _jobManager;
  std::vector<rest::HttpServer*> _servers;
  RestActionHandler::action_options_t _httpOptions;
};
}

#endif
