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

#include "CheckVersionFeature.h"

#include "Logger/LoggerFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabaseServerFeature.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-query.h"
#include "V8Server/v8-vocbase.h"
#include "VocBase/server.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

CheckVersionFeature::CheckVersionFeature(
    ApplicationServer* server, int* result,
    std::vector<std::string> const& nonServerFeatures)
    : ApplicationFeature(server, "CheckVersion"),
      _checkVersion(false),
      _result(result),
      _nonServerFeatures(nonServerFeatures) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Database");
  startsAfter("V8Dealer");
}

void CheckVersionFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("database", "Configure the database");

  options->addOldOption("check-version", "database.check-version");

  options->addHiddenOption("--database.check-version",
                           "checks the versions of the database and exit",
                           new BooleanParameter(&_checkVersion));
}

void CheckVersionFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  if (!_checkVersion) {
    return;
  }

  ApplicationServer::forceDisableFeatures(_nonServerFeatures);

  LoggerFeature* logger =
      ApplicationServer::getFeature<LoggerFeature>("Logger");
  logger->disableThreaded();

  DatabaseFeature* database =
      ApplicationServer::getFeature<DatabaseFeature>("Database");
  database->disableReplicationApplier();
  database->disableCompactor();
  database->enableCheckVersion();

  V8DealerFeature* v8dealer =
      ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
  v8dealer->setNumberContexts(1);
}

void CheckVersionFeature::start() {
  if (!_checkVersion) {
    return;
  }

  // check the version
  if (DatabaseFeature::DATABASE->isInitiallyEmpty()) {
    *_result = EXIT_SUCCESS;
  } else {
    checkVersion();
  }

  // and force shutdown
  server()->beginShutdown();
}

void CheckVersionFeature::checkVersion() {
  *_result = 1;

  // run version check
  LOG(TRACE) << "starting version check";

  auto* vocbase = DatabaseFeature::DATABASE->vocbase();

  // enter context and isolate
  {
    V8Context* context =
        V8DealerFeature::DEALER->enterContext(vocbase, true, 0);

    if (context == nullptr) {
      LOG(FATAL) << "could not enter context #0";
      FATAL_ERROR_EXIT();
    }

    TRI_DEFER(V8DealerFeature::DEALER->exitContext(context));

    {
      v8::HandleScope scope(context->_isolate);
      auto localContext =
          v8::Local<v8::Context>::New(context->_isolate, context->_context);
      localContext->Enter();

      {
        v8::Context::Scope contextScope(localContext);

        // run version-check script
        LOG(DEBUG) << "running database version check";

        // can do this without a lock as this is the startup
        auto server = DatabaseServerFeature::SERVER;
        auto unuser = server->_databasesProtector.use();
        auto theLists = server->_databasesLists.load();

        for (auto& p : theLists->_databases) {
          TRI_vocbase_t* vocbase = p.second;

          // special check script to be run just once in first thread (not in
          // all)
          // but for all databases

          int status = TRI_CheckDatabaseVersion(vocbase, localContext);

          LOG(DEBUG) << "version check return status " << status;

          if (status < 0) {
            LOG(FATAL) << "Database version check failed for '"
                       << vocbase->_name
                       << "'. Please inspect the logs for any errors";
            FATAL_ERROR_EXIT();
          } else if (status == 3) {
            *_result = 3;
          } else if (status == 2 && *_result == 1) {
            *_result = 2;
          }
        }
      }

      // issue #391: when invoked with --database.auto-upgrade, the server will
      // not always shut
      // down
      localContext->Exit();
    }
  }

  if (*_result == 1) {
    *_result = EXIT_SUCCESS;
  }
}
