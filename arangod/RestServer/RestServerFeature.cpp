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

#include "RestServerFeature.h"

#include "Agency/AgencyFeature.h"
#include "Agency/RestAgencyHandler.h"
#include "Agency/RestAgencyPrivHandler.h"
#include "Aql/RestAqlHandler.h"
#include "Basics/StringUtils.h"
#include "Cluster/AgencyCallbackRegistry.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/RestAgencyCallbacksHandler.h"
#include "Cluster/RestShardHandler.h"
#include "Dispatcher/DispatcherFeature.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "HttpServer/HttpServer.h"
#include "HttpServer/HttpsServer.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Random/RandomGenerator.h"
#include "Rest/Version.h"
#include "RestHandler/RestAdminLogHandler.h"
#include "RestHandler/RestAqlFunctionsHandler.h"
#include "RestHandler/RestAuthHandler.h"
#include "RestHandler/RestBatchHandler.h"
#include "RestHandler/RestCursorHandler.h"
#include "RestHandler/RestDebugHandler.h"
#include "RestHandler/RestDocumentHandler.h"
#include "RestHandler/RestEchoHandler.h"
#include "RestHandler/RestEdgesHandler.h"
#include "RestHandler/RestExportHandler.h"
#include "RestHandler/RestHandlerCreator.h"
#include "RestHandler/RestImportHandler.h"
#include "RestHandler/RestJobHandler.h"
#include "RestHandler/RestPleaseUpgradeHandler.h"
#include "RestHandler/RestQueryCacheHandler.h"
#include "RestHandler/RestQueryHandler.h"
#include "RestHandler/RestReplicationHandler.h"
#include "RestHandler/RestShutdownHandler.h"
#include "RestHandler/RestSimpleHandler.h"
#include "RestHandler/RestSimpleQueryHandler.h"
#include "RestHandler/RestUploadHandler.h"
#include "RestHandler/RestVersionHandler.h"
#include "RestHandler/WorkMonitorHandler.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabaseServerFeature.h"
#include "RestServer/EndpointFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ServerFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Ssl/SslServerFeature.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/server.h"

using namespace arangodb;
using namespace arangodb::rest;
using namespace arangodb::options;

AuthInfo RestServerFeature::AUTH_INFO;
RestServerFeature* RestServerFeature::RESTSERVER = nullptr;

RestServerFeature::RestServerFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "RestServer"),
      _keepAliveTimeout(300.0),
      _allowMethodOverride(false),
      _authentication(true),
      _authenticationUnixSockets(true),
      _authenticationSystemOnly(true),
      _proxyCheck(true),
      _jwtSecret(""),
      _handlerFactory(nullptr),
      _jobManager(nullptr) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Agency");
  startsAfter("CheckVersion");
  startsAfter("Database");
  startsAfter("Dispatcher");
  startsAfter("Endpoint");
  startsAfter("FoxxQueues");
  startsAfter("LogfileManager");
  startsAfter("Random");
  startsAfter("Scheduler");
  startsAfter("Server");
  startsAfter("Upgrade");
}

void RestServerFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addOldOption("server.disable-authentication",
                        "server.authentication");
  options->addOldOption("server.disable-authentication-unix-sockets",
                        "server.authentication-unix-sockets");
  options->addOldOption("server.authenticate-system-only",
                        "server.authentication-system-only");
  options->addOldOption("server.allow-method-override",
                        "http.allow-method-override");
  options->addOldOption("server.hide-product-header",
                        "http.hide-product-header");
  options->addOldOption("server.keep-alive-timeout", "http.keep-alive-timeout");
  options->addOldOption("server.default-api-compatibility", "");
  options->addOldOption("no-server", "server.rest-server");

  options->addOption("--server.authentication",
                     "enable or disable authentication for ALL client requests",
                     new BooleanParameter(&_authentication));

  options->addOption(
      "--server.authentication-system-only",
      "use HTTP authentication only for requests to /_api and /_admin",
      new BooleanParameter(&_authenticationSystemOnly));

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
  options->addOption("--server.authentication-unix-sockets",
                     "authentication for requests via UNIX domain sockets",
                     new BooleanParameter(&_authenticationUnixSockets));
#endif

  options->addOption("--server.jwt-secret",
                     "secret to use when doing jwt authentication",
                     new StringParameter(&_jwtSecret));

  options->addSection("http", "HttpServer features");

  options->addHiddenOption("--http.allow-method-override",
                           "allow HTTP method override using special headers",
                           new BooleanParameter(&_allowMethodOverride));

  options->addOption("--http.keep-alive-timeout",
                     "keep-alive timeout in seconds",
                     new DoubleParameter(&_keepAliveTimeout));

  options->addOption(
      "--http.hide-product-header",
      "do not expose \"Server: ArangoDB\" header in HTTP responses",
      new BooleanParameter(&HttpResponse::HIDE_PRODUCT_HEADER));

  options->addOption(
      "--http.trusted-origin",
      "trusted origin URLs for CORS requests with credentials",
      new VectorParameter<StringParameter>(&_accessControlAllowOrigins));

  options->addSection("frontend", "Frontend options");

  options->addOption("--frontend.proxy-request-check",
                     "enable or disable proxy request checking",
                     new BooleanParameter(&_proxyCheck));

  options->addOption("--frontend.trusted-proxy",
                     "list of proxies to trust (may be IP or network). Make "
                     "sure --frontend.proxy-request-check is enabled",
                     new VectorParameter<StringParameter>(&_trustedProxies));
}

void RestServerFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
  if (!_accessControlAllowOrigins.empty()) {
    // trim trailing slash from all members
    for (auto& it : _accessControlAllowOrigins) {
      if (it == "*" || it == "all") {
        // special members "*" or "all" means all origins are allowed
        _accessControlAllowOrigins.clear();
        _accessControlAllowOrigins.push_back("*");
        break;
      } else if (it == "none") {
        // "none" means no origins are allowed
        _accessControlAllowOrigins.clear();
        break;
      } else if (it[it.size() - 1] == '/') {
        // strip trailing slash
        it = it.substr(0, it.size() - 1);
      }
    }

    // remove empty members
    _accessControlAllowOrigins.erase(
        std::remove_if(_accessControlAllowOrigins.begin(),
                       _accessControlAllowOrigins.end(),
                       [](std::string const& value) {
                         return basics::StringUtils::trim(value).empty();
                       }),
        _accessControlAllowOrigins.end());
  }

  if (!_jwtSecret.empty()) {
    if (_jwtSecret.length() > RestServerFeature::_maxSecretLength) {
      LOG(ERR) << "Given JWT secret too long. Max length is "
               << RestServerFeature::_maxSecretLength;
      FATAL_ERROR_EXIT();
    }
  }
}

static TRI_vocbase_t* LookupDatabaseFromRequest(HttpRequest* request,
                                                TRI_server_t* server) {
  // get database name from request
  std::string const& dbName = request->databaseName();

  char const* p;
  if (dbName.empty()) {
    // if no databases was specified in the request, use system database name
    // as a fallback
    request->setDatabaseName(StaticStrings::SystemDatabase);
    p = StaticStrings::SystemDatabase.c_str();
  } else {
    p = dbName.c_str();
  }

  if (ServerState::instance()->isCoordinator()) {
    return TRI_UseCoordinatorDatabaseServer(server, p);
  }

  return TRI_UseDatabaseServer(server, p);
}

static bool SetRequestContext(HttpRequest* request, void* data) {
  TRI_server_t* server = static_cast<TRI_server_t*>(data);
  TRI_vocbase_t* vocbase = LookupDatabaseFromRequest(request, server);

  // invalid database name specified, database not found etc.
  if (vocbase == nullptr) {
    return false;
  }

  // database needs upgrade
  if (vocbase->_state == (sig_atomic_t)TRI_VOCBASE_STATE_FAILED_VERSION) {
    request->setRequestPath("/_msg/please-upgrade");
    return false;
  }

  VocbaseContext* ctx = new arangodb::VocbaseContext(
      request, vocbase, RestServerFeature::getJwtSecret());
  request->setRequestContext(ctx, true);

  // the "true" means the request is the owner of the context
  return true;
}

void RestServerFeature::generateNewJwtSecret() {
  _jwtSecret = "";
  uint16_t m = 254;

  for (size_t i = 0; i < RestServerFeature::_maxSecretLength; i++) {
    _jwtSecret += (1 + RandomGenerator::interval(m));
  }
}

void RestServerFeature::prepare() {
  if (_jwtSecret.empty()) {
    generateNewJwtSecret();
  }

  HttpHandlerFactory::setMaintenance(true);
}

void RestServerFeature::start() {
  RESTSERVER = this;

  _jobManager.reset(new AsyncJobManager(ClusterCommRestCallback));

  _httpOptions._vocbase = DatabaseFeature::DATABASE->vocbase();

  _handlerFactory.reset(new HttpHandlerFactory(
      _allowMethodOverride, &SetRequestContext, DatabaseServerFeature::SERVER));

  defineHandlers();
  buildServers();

  for (auto& server : _servers) {
    server->startListening();
  }

  LOG(INFO) << "Authentication is turned " << (_authentication ? "on" : "off");

  if (_authentication) {
    if (_authenticationSystemOnly) {
      LOG(INFO) << "Authentication system only";
    }

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
    LOG(INFO) << "Authentication for unix sockets is turned "
              << (_authenticationUnixSockets ? "on" : "off");
#endif
  }

  // populate the authentication cache. otherwise no one can access the new
  // database
  RestServerFeature::AUTH_INFO.outdate();
}

void RestServerFeature::stop() {
  for (auto& server : _servers) {
    server->stopListening();
  }

  for (auto& server : _servers) {
    server->stop();
  }
}

void RestServerFeature::unprepare() {
  for (auto& server : _servers) {
    delete server;
  }

  _httpOptions._vocbase = nullptr;
  RESTSERVER = nullptr;
}

void RestServerFeature::buildServers() {
  TRI_ASSERT(_jobManager != nullptr);

  EndpointFeature* endpoint =
      application_features::ApplicationServer::getFeature<EndpointFeature>(
          "Endpoint");

  // unencrypted HTTP endpoints
  HttpServer* httpServer =
      new HttpServer(SchedulerFeature::SCHEDULER, DispatcherFeature::DISPATCHER,
                     _handlerFactory.get(), _jobManager.get(),
                     _keepAliveTimeout, _accessControlAllowOrigins);

  // YYY #warning FRANK filter list
  auto const& endpointList = endpoint->endpointList();
  httpServer->setEndpointList(&endpointList);
  _servers.push_back(httpServer);

  // ssl endpoints
  if (endpointList.hasSsl()) {
    SslServerFeature* ssl =
        application_features::ApplicationServer::getFeature<SslServerFeature>(
            "SslServer");

    // check the ssl context
    if (ssl->sslContext() == nullptr) {
      LOG(FATAL) << "no ssl context is known, cannot create https server, "
                    "please use the '--ssl.keyfile' option";
      FATAL_ERROR_EXIT();
    }

    SSL_CTX* sslContext = ssl->sslContext();

    // https
    httpServer = new HttpsServer(
        SchedulerFeature::SCHEDULER, DispatcherFeature::DISPATCHER,
        _handlerFactory.get(), _jobManager.get(), _keepAliveTimeout,
        _accessControlAllowOrigins, sslContext);

    httpServer->setEndpointList(&endpointList);
    _servers.push_back(httpServer);
  }
}

void RestServerFeature::defineHandlers() {
  TRI_ASSERT(_jobManager != nullptr);

  AgencyFeature* agency =
      application_features::ApplicationServer::getFeature<AgencyFeature>(
          "Agency");
  TRI_ASSERT(agency != nullptr);

  ClusterFeature* cluster =
      application_features::ApplicationServer::getFeature<ClusterFeature>(
          "Cluster");
  TRI_ASSERT(cluster != nullptr);

  auto queryRegistry = QueryRegistryFeature::QUERY_REGISTRY;

  // ...........................................................................
  // /_msg
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      "/_msg/please-upgrade",
      RestHandlerCreator<RestPleaseUpgradeHandler>::createNoData);

  // ...........................................................................
  // /_api
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::BATCH_PATH,
      RestHandlerCreator<RestBatchHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::CURSOR_PATH,
      RestHandlerCreator<RestCursorHandler>::createData<aql::QueryRegistry*>,
      queryRegistry);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::DOCUMENT_PATH,
      RestHandlerCreator<RestDocumentHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::EDGES_PATH,
      RestHandlerCreator<RestEdgesHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::EXPORT_PATH,
      RestHandlerCreator<RestExportHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::IMPORT_PATH,
      RestHandlerCreator<RestImportHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::REPLICATION_PATH,
      RestHandlerCreator<RestReplicationHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH,
      RestHandlerCreator<RestSimpleQueryHandler>::createData<
          aql::QueryRegistry*>,
      queryRegistry);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_KEYS_PATH,
      RestHandlerCreator<RestSimpleQueryHandler>::createData<
          aql::QueryRegistry*>,
      queryRegistry);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<aql::QueryRegistry*>,
      queryRegistry);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<aql::QueryRegistry*>,
      queryRegistry);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::UPLOAD_PATH,
      RestHandlerCreator<RestUploadHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      "/_api/shard-comm", RestHandlerCreator<RestShardHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      "/_api/aql",
      RestHandlerCreator<aql::RestAqlHandler>::createData<aql::QueryRegistry*>,
      queryRegistry);

  _handlerFactory->addPrefixHandler(
      "/_api/aql-builtin",
      RestHandlerCreator<RestAqlFunctionsHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      "/_api/query", RestHandlerCreator<RestQueryHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      "/_api/query-cache",
      RestHandlerCreator<RestQueryCacheHandler>::createNoData);

  if (agency->isEnabled()) {
    _handlerFactory->addPrefixHandler(
        RestVocbaseBaseHandler::AGENCY_PATH,
        RestHandlerCreator<RestAgencyHandler>::createData<consensus::Agent*>,
        agency->agent());

    _handlerFactory->addPrefixHandler(
        RestVocbaseBaseHandler::AGENCY_PRIV_PATH,
        RestHandlerCreator<RestAgencyPrivHandler>::createData<
            consensus::Agent*>,
        agency->agent());
  }

  if (cluster->isEnabled()) {
    // add "/agency-callbacks" handler
    _handlerFactory->addPrefixHandler(
        cluster->agencyCallbacksPath(),
        RestHandlerCreator<RestAgencyCallbacksHandler>::createData<
            AgencyCallbackRegistry*>,
        cluster->agencyCallbackRegistry());
  }

  // And now some handlers which are registered in both /_api and /_admin
  _handlerFactory->addPrefixHandler(
      "/_api/job", RestHandlerCreator<arangodb::RestJobHandler>::createData<
                       AsyncJobManager*>,
      _jobManager.get());

  _handlerFactory->addHandler(
      "/_api/version", RestHandlerCreator<RestVersionHandler>::createNoData);

  // ...........................................................................
  // /_admin
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      "/_admin/job", RestHandlerCreator<arangodb::RestJobHandler>::createData<
                         AsyncJobManager*>,
      _jobManager.get());

  _handlerFactory->addHandler(
      "/_admin/version", RestHandlerCreator<RestVersionHandler>::createNoData);

  // further admin handlers
  _handlerFactory->addHandler(
      "/_admin/log",
      RestHandlerCreator<arangodb::RestAdminLogHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      "/_admin/work-monitor",
      RestHandlerCreator<WorkMonitorHandler>::createNoData);

  _handlerFactory->addHandler(
      "/_admin/json-echo", RestHandlerCreator<RestEchoHandler>::createNoData);

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  // This handler is to activate SYS_DEBUG_FAILAT on DB servers
  _handlerFactory->addPrefixHandler(
      "/_admin/debug", RestHandlerCreator<RestDebugHandler>::createNoData);
#endif

  _handlerFactory->addPrefixHandler(
      "/_admin/shutdown",
      RestHandlerCreator<arangodb::RestShutdownHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      "/_open/auth", RestHandlerCreator<arangodb::RestAuthHandler>::createData<
                         std::string const*>,
      &_jwtSecret);

  // ...........................................................................
  // /_admin
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      "/", RestHandlerCreator<RestActionHandler>::createData<
               RestActionHandler::action_options_t*>,
      (void*)&_httpOptions);
}
