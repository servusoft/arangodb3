/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief administration actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("@arangodb/actions");
var internal = require("internal");
var users = require("@arangodb/users");


////////////////////////////////////////////////////////////////////////////////
/// @brief main routing action
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "",
  prefix : true,

  callback : function (req, res) {
    req.absoluteUrl = function (url) {
      var protocol = req.headers["x-forwarded-proto"] || req.protocol;
      var address = req.headers["x-forwarded-host"] || req.headers.host;
      if (!address) {
        address = (req.server.address === '0.0.0.0' ? 'localhost' : req.server.address) + ':' + req.server.port;
      }
      return protocol + "://" + address + '/_db/' + encodeURIComponent(req.database) + (url || req.url);
    };
    try {
      actions.routeRequest(req, res);
    }
    catch (err) {
      var msg = 'A runtime error occurred while executing an action: '
                + String(err) + " " + String(err.stack);

      if (err.hasOwnProperty("route")) {
        actions.errorFunction(err.route, msg)(req, res);
      }
      else {
        actions.resultError(req, res, actions.HTTP_SERVER_ERROR, actions.HTTP_SERVER_ERROR, msg);
      }
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the server authentication information
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/auth/reload",
  prefix : false,

  callback : function (req, res) {
    users.reload();
    actions.resultOk(req, res, actions.HTTP_OK);
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the AQL user functions
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/aql/reload",
  prefix : false,

  callback : function (req, res) {
    internal.reloadAqlFunctions();
    actions.resultOk(req, res, actions.HTTP_OK);
  }
});


