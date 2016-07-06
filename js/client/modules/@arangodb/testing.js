/*jshint strict: false, sub: true */
/*global print, arango */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2014 triagens GmbH, Cologne, Germany
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

const functionsDocumentation = {
  "all": "run all tests (marked with [x])",
  "agency": "run agency tests",
  "arangobench": "arangobench tests",
  "arangosh": "arangosh exit codes tests",
  "authentication": "authentication tests",
  "authentication_parameters": "authentication parameters tests",
  "boost": "boost test suites",
  "config": "checks the config file parsing",
  "dump": "dump tests",
  "dump_authentication": "dump tests with authentication",
  "dfdb": "start test",
  "foxx_manager": "foxx manager tests",
  "http_replication": "http replication tests",
  "http_server": "http server tests in Ruby",
  "server_http": "http server tests in Mocha",
  "importing": "import tests",
  "recovery": "run recovery tests",
  "replication_ongoing": "replication ongoing tests",
  "replication_static": "replication static tests",
  "replication_sync": "replication sync tests",
  "resilience": "resilience tests",
  "shell_client": "shell client tests",
  "shell_replication": "shell replication tests",
  "shell_server": "shell server tests",
  "shell_server_aql": "AQL tests in the server",
  "shell_server_only": "server specific tests",
  "shell_server_perf": "bulk tests intended to get an overview of executiontime needed",
  "single_client": "run one test suite isolated via the arangosh; options required\n" +
    "            run without arguments to get more detail",
  "single_server": "run one test suite on the server; options required\n" +
    "            run without arguments to get more detail",
  "ssl_server": "https server tests",
  "upgrade": "upgrade tests"
};

const optionsDocumentation = [
  '',
  ' The following properties of `options` are defined:',
  '',
  '   - `jsonReply`: if set a json is returned which the caller has to ',
  '        present the user',
  '   - `force`: if set to true the tests are continued even if one fails',
  '',
  '   - `skipAql`: if set to true the AQL tests are skipped',
  '   - `skipArangoBenchNonConnKeepAlive`: if set to true benchmark do not use keep-alive',
  '   - `skipArangoBench`: if set to true benchmark tests are skipped',
  '   - `skipAuthentication : testing authentication and authentication_paramaters will be skipped.',
  '   - `skipBoost`: if set to true the boost unittests are skipped',
  '   - `skipConfig`: omit the noisy configuration tests',
  '   - `skipFoxxQueues`: omit the test for the foxx queues',
  '   - `skipGeo`: if set to true the geo index tests are skipped',
  '   - `skipGraph`: if set to true the graph tests are skipped',
  '   - `skipLogAnalysis`: don\'t try to crawl the server logs',
  '   - `skipMemoryIntense`: tests using lots of resources will be skipped.',
  '   - `skipNightly`: omit the nightly tests',
  '   - `skipRanges`: if set to true the ranges tests are skipped',
  '   - `skipSsl`: omit the ssl_server rspec tests.',
  '   - `skipTimeCritical`: if set to true, time critical tests will be skipped.',
  '   - `skipNondeterministic`: if set, nondeterministic tests are skipped.',
  '   - `skipShebang`: if set, the shebang tests are skipped.',
  '   - `testBuckets`: split tests in to buckets and execute on, for example',
  '       10/2 will split into 10 buckets and execute the third bucket.',
  '',
  '   - `onlyNightly`: execute only the nightly tests',
  '   - `loopEternal`: to loop one test over and over.',
  '   - `loopSleepWhen`: sleep every nth iteration',
  '   - `loopSleepSec`: sleep seconds between iterations',
  '',
  '   - `cluster`: if set to true the tests are run with the coordinator',
  '     of a small local cluster',
  '   - `clusterNodes`: number of DB-Servers to use',
  '   - `agency`: if set to true agency tests are done',
  '   - `agencySize`: number of agents in agency',
  '   - `test`: path to single test to execute for "single" test target',
  '   - `cleanup`: if set to true (the default), the cluster data files',
  '     and logs are removed after termination of the test.',
  '',
  '   - `benchargs`: additional commandline arguments to arangobench',
  '',
  '   - `build`: the directory containing the binaries',
  '   - `buildType`: Windows build type (Debug, Release), leave empty on linux',
  '   - `rspec`: the location of rspec program',
  '   - `ruby`: the location of ruby program; if empty start rspec directly',
  '',
  '   - `rr`: if set to true arangod instances are run with rr',
  '',
  '   - `sanitizer`: if set the programs are run with enabled sanitizer',
  '     and need longer tomeouts',
  '',
  '   - `valgrind`: if set the programs are run with the valgrind',
  '     memory checker; should point to the valgrind executable',
  '   - `valgrindFileBase`: string to prepend to the report filename',
  '   - `valgrindArgs`: commandline parameters to add to valgrind',
  '   - valgrindHosts  - configure which clustercomponents to run using valgrind',
  '        Coordinator - flag to run Coordinator with valgrind',
  '        DBServer    - flag to run DBServers with valgrind',
  '',
  '   - `extraArgs`: list of extra commandline arguments to add to arangod',
  '',
  '   - `verbose`: if set to true, be more verbose',
  '   - `extremeVerbosity`: if set to true, then there will be more test run',
  '     output, especially for cluster tests.',
  ''
];

const optionsDefaults = {
  "agencySize": 3,
  "build": "",
  "buildType": "",
  "cleanup": true,
  "cluster": false,
  "clusterNodes": 2,
  "concurrency": 3,
  "coreDirectory": "/var/tmp",
  "duration": 10,
  "extraArgs": {},
  "extremeVerbosity": false,
  "force": true,
  "jsonReply": false,
  "loopEternal": false,
  "loopSleepSec": 1,
  "loopSleepWhen": 1,
  "onlyNightly": false,
  "password": "",
  "replication": false,
  "rr": false,
  "rspec": "rspec",
  "ruby": "",
  "sanitizer": false,
  "skipAql": false,
  "skipArangoBench": false,
  "skipArangoBenchNonConnKeepAlive": true,
  "skipAuthentication": false,
  "skipBoost": false,
  "skipGeo": false,
  "skipLogAnalysis": false,
  "skipMemoryIntense": false,
  "skipNightly": true,
  "skipNondeterministic": false,
  "skipRanges": false,
  "skipShebang": false,
  "skipSsl": false,
  "skipTimeCritical": false,
  "test": undefined,
  "testBuckets": undefined,
  "username": "root",
  "valgrind": false,
  "valgrindFileBase": "",
  "valgrindArgs": {},
  "valgrindHosts": false,
  "verbose": false,
  "writeXmlReport": true
};

const _ = require("lodash");
const fs = require("fs");
const yaml = require("js-yaml");

const base64Encode = require("internal").base64Encode;
const download = require("internal").download;
const executeExternal = require("internal").executeExternal;
const executeExternalAndWait = require("internal").executeExternalAndWait;
const killExternal = require("internal").killExternal;
const sleep = require("internal").sleep;
const statusExternal = require("internal").statusExternal;
const testPort = require("internal").testPort;
const time = require("internal").time;
const toArgv = require("internal").toArgv;
const wait = require("internal").wait;
const platform = require("internal").platform;

const BLUE = require("internal").COLORS.COLOR_BLUE;
const CYAN = require("internal").COLORS.COLOR_CYAN;
const GREEN = require("internal").COLORS.COLOR_GREEN;
const RED = require("internal").COLORS.COLOR_RED;
const RESET = require("internal").COLORS.COLOR_RESET;
const YELLOW = require("internal").COLORS.COLOR_YELLOW;

let cleanupDirectories = [];
let serverCrashed = false;

const TOP_DIR = (function findTopDir() {
  const topDir = fs.normalize(fs.makeAbsolute("."));

  if (!fs.exists("3rdParty") && !fs.exists("arangod") &&
    !fs.exists("arangosh") && !fs.exists("UnitTests")) {
    throw "Must be in ArangoDB topdir to execute unit tests.";
  }

  return topDir;
}());

let BIN_DIR;
let CONFIG_DIR;
let ARANGOBENCH_BIN;
let ARANGODUMP_BIN;
let ARANGOD_BIN;
let ARANGOIMP_BIN;
let ARANGORESTORE_BIN;
let ARANGOSH_BIN;
let CONFIG_RELATIVE_DIR;
let JS_DIR;
let LOGS_DIR;
let PEM_FILE;
let UNITTESTS_DIR;

function makeResults(testname) {
  const startTime = time();

  return function(status, message) {
    let duration = time() - startTime;
    let results;

    if (status) {
      let result;

      try {
        result = JSON.parse(fs.read("testresult.json"));

        if ((typeof result[0] === 'object') &&
          result[0].hasOwnProperty('status')) {
          results = result[0];
        }

      } catch (x) {}
    }

    if (results === undefined) {
      results = {
        status: status,
        duration: duration,
        total: 1,
        failed: status ? 0 : 1,
        'testing.js': {
          status: status,
          duration: duration
        }
      };

      if (message) {
        results['testing.js'].message = message;
      }
    }

    let full = {};
    full[testname] = results;

    return full;
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief arguments for testing (server)
////////////////////////////////////////////////////////////////////////////////

function makeArgsArangod(options, appDir) {
  if (appDir === undefined) {
    appDir = fs.getTempPath();
  }

  fs.makeDirectoryRecursive(appDir, true);

  return {
    "configuration": "none",
    "database.force-sync-properties": "false",
    "database.maximal-journal-size": "1048576",
    "javascript.app-path": appDir,
    "javascript.startup-directory": JS_DIR,
    "javascript.v8-contexts": "5",
    "http.trusted-origin": "all",
    "log.level": "warn",
    "log.level=replication=warn": null,
    "server.allow-use-database": "true",
    "server.authentication": "false",
    "server.threads": "20",
    "ssl.keyfile": PEM_FILE
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief arguments for testing (client)
////////////////////////////////////////////////////////////////////////////////

function makeArgsArangosh(options) {
  return {
    "configuration": "none",
    "javascript.startup-directory": JS_DIR,
    "server.username": options.username,
    "server.password": options.password,
    "flatCommands": ["--console.colors", "false", "--quiet"]
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds authorization headers
////////////////////////////////////////////////////////////////////////////////

function makeAuthorizationHeaders(options) {
  return {
    "headers": {
      "Authorization": "Basic " + base64Encode(options.username + ":" +
        options.password)
    }
  };
}
////////////////////////////////////////////////////////////////////////////////
/// @brief converts endpoints to URL
////////////////////////////////////////////////////////////////////////////////

function endpointToURL(endpoint) {
  if (endpoint.substr(0, 6) === "ssl://") {
    return "https://" + endpoint.substr(6);
  }

  const pos = endpoint.indexOf("://");

  if (pos === -1) {
    return "http://" + endpoint;
  }

  return "http" + endpoint.substr(pos);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief scans the log files for important infos
////////////////////////////////////////////////////////////////////////////////

function readImportantLogLines(logPath) {
  const list = fs.list(logPath);
  let importantLines = {};

  for (let i = 0; i < list.length; i++) {
    let fnLines = [];

    if (list[i].slice(0, 3) === 'log') {
      const buf = fs.readBuffer(fs.join(logPath, list[i]));
      let lineStart = 0;
      let maxBuffer = buf.length;

      for (let j = 0; j < maxBuffer; j++) {
        if (buf[j] === 10) { // \n
          const line = buf.asciiSlice(lineStart, j);
          lineStart = j + 1;

          // filter out regular INFO lines, and test related messages
          let warn = line.search('WARNING about to execute:') !== -1;
          let info = line.search(' INFO ') !== -1;

          if (warn || info) {
            continue;
          }

          fnLines.push(line);
        }
      }
    }

    if (fnLines.length > 0) {
      importantLines[list[i]] = fnLines;
    }
  }

  return importantLines;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief analyzes a core dump using gdb (Unix)
///
/// We assume the system has core files in /var/tmp/, and we have a gdb.
/// you can do this at runtime doing:
///
/// echo 1 > /proc/sys/kernel/core_uses_pid
/// echo /var/tmp/core-%e-%p-%t > /proc/sys/kernel/core_pattern
///
/// or at system startup by altering /etc/sysctl.d/corepattern.conf : 
/// # We want core files to be located in a central location
/// # and know the PID plus the process name for later use.
/// kernel.core_uses_pid = 1
/// kernel.core_pattern =  /var/tmp/core-%e-%p-%t
///
/// If you set coreDirectory to empty, this behavior is changed: The core file
/// expected to be named simply "core" and should exist in the current
/// directory.
////////////////////////////////////////////////////////////////////////////////

function analyzeCoreDump(instanceInfo, options, storeArangodPath, pid) {
  let command;

  command = '(';
  command += "printf 'bt full\\n thread apply all bt\\n';";
  command += "sleep 10;";
  command += "echo quit;";
  command += "sleep 2";
  command += ") | gdb " + storeArangodPath + " ";

  if (options.coreDirectory === "") {
    command += "core";
  } else {
    command += options.coreDirectory + "/*core*" + pid + '*';
  }

  const args = ['-c', command];
  print(JSON.stringify(args));

  executeExternalAndWait("/bin/bash", args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief analyzes a core dump using cdb (Windows)
///  cdb is part of the WinDBG package.
////////////////////////////////////////////////////////////////////////////////

function analyzeCoreDumpWindows(instanceInfo) {
  const coreFN = instanceInfo.rootDir + "\\" + "core.dmp";

  if (!fs.exists(coreFN)) {
    print("core file " + coreFN + " not found?");
    return;
  }

  const dbgCmds = [
    "kp", // print curren threads backtrace with arguments
    "~*kb", // print all threads stack traces
    "dv", // analyze local variables (if)
    "!analyze -v", // print verbose analysis
    "q" //quit the debugger
  ];

  const args = [
    "-z",
    coreFN,
    "-c",
    dbgCmds.join("; ")
  ];

  print("running cdb " + JSON.stringify(args));
  executeExternalAndWait("cdb", args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the bad has happened, tell it the user and try to gather more
///        information about the incident.
////////////////////////////////////////////////////////////////////////////////
function analyzeServerCrash(arangod, options, checkStr)
{
  serverCrashed = true;
  const storeArangodPath = arangod.rootDir + "/arangod_" + arangod.pid;

  print(RED +
        "during: " + checkStr + ": Core dump written; copying arangod to " +
        storeArangodPath + " for later analysis.\n" +
        "Server shut down with :\n" +
        yaml.safeDump(arangod) +
        "marking build as crashy.");

  let corePath = (options.coreDirectory === "") ?
      "core" :
      options.coreDirectory + "/core*" + arangod.pid + "*'";

  arangod.exitStatus.gdbHint = "Run debugger with 'gdb " +
    storeArangodPath + " " + corePath;

  if (platform.substr(0, 3) === 'win') {
    // Windows: wait for procdump to do its job...
    statusExternal(arangod.monitor, true);
    analyzeCoreDumpWindows(arangod);
  } else {
    fs.copyFile(ARANGOD_BIN, storeArangodPath);
    analyzeCoreDump(arangod, options, storeArangodPath, arangod.pid);
  }

  print(RESET);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief periodic checks whether spawned arangod processes are still alive
////////////////////////////////////////////////////////////////////////////////
function checkArangoAlive(arangod, options) {
  if (arangod.hasOwnProperty('exitStatus')) {
    return false;
  }

  const res = statusExternal(arangod.pid, false);
  const ret = res.status === "RUNNING";

  if (!ret) {
    print("ArangoD with PID " + arangod.pid + " gone:");
    print(arangod);

    if (res.hasOwnProperty('signal') &&
        ((res.signal === 11) ||
         (res.signal === 6) ||
         // Windows sometimes has random numbers in signal...
         (platform.substr(0, 3) === 'win')
        )
       ) {
      arangod.exitStatus = res;
      analyzeServerCrash(arangod, options, "health Check");
    }
  }
  
  return ret;
}

function checkInstanceAlive(instanceInfo, options) {
  return instanceInfo.arangods.reduce((previous, arangod) => {
    return previous && checkArangoAlive(arangod, options);
  }, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for garbage collection using /_admin/execute
////////////////////////////////////////////////////////////////////////////////

function waitOnServerForGC(instanceInfo, options, waitTime) {
  try {
    print("waiting " + waitTime + " for server GC");
    const remoteCommand = 'require("internal").wait(' + waitTime + ', true);';

    const requestOptions = makeAuthorizationHeaders(options);
    requestOptions.method = "POST";
    requestOptions.timeout = waitTime * 10;
    requestOptions.returnBodyOnError = true;

    const reply = download(
      instanceInfo.url + "/_admin/execute?returnAsJSON=true",
      remoteCommand,
      requestOptions);

    print("waiting " + waitTime + " for server GC - done.");

    if (!reply.error && reply.code === 200) {
      return JSON.parse(reply.body);
    } else {
      return {
        status: false,
        message: yaml.safedump(reply.body)
      };
    }
  } catch (ex) {
    return {
      status: false,
      message: ex.message || String(ex),
      stack: ex.stack
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleans up the database direcory
////////////////////////////////////////////////////////////////////////////////

function cleanupDBDirectories(options) {
  if (options.cleanup) {
    while (cleanupDirectories.length) {
      const cleanupDirectory = cleanupDirectories.shift();

      // Avoid attempting to remove the same directory multiple times
      if (cleanupDirectories.indexOf(cleanupDirectory) === -1) {
        fs.removeDirectoryRecursive(cleanupDirectory, true);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a free port
////////////////////////////////////////////////////////////////////////////////

function findFreePort() {
  while (true) {
    const port = Math.floor(Math.random() * (65536 - 1024)) + 1024;
    const free = testPort("tcp://0.0.0.0:" + port);

    if (free) {
      return port;
    }
  }

  return 8529;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief build a unix path
////////////////////////////////////////////////////////////////////////////////

function makePathUnix(path) {
  return fs.join.apply(null, path.split("/"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief build a generic path
////////////////////////////////////////////////////////////////////////////////

function makePathGeneric(path) {
  return fs.join.apply(null, path.split(fs.pathSeparator));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs a remote unittest file using /_admin/execute
////////////////////////////////////////////////////////////////////////////////

function runThere(options, instanceInfo, file) {
  try {
    let testCode;

    if (file.indexOf("-spec") === -1) {
      testCode = 'const runTest = require("jsunity").runTest; ' +
        'return runTest(' + JSON.stringify(file) + ', true);';
    } else {
      testCode = 'const runTest = require("@arangodb/mocha-runner"); ' +
        'return runTest(' + JSON.stringify(file) + ', true);';
    }

    if (options.propagateInstanceInfo) {
      testCode = 'global.instanceInfo = ' + JSON.stringify(instanceInfo) + ';\n' + testCode;
    }

    let httpOptions = makeAuthorizationHeaders(options);
    httpOptions.method = "POST";
    httpOptions.timeout = 3600;

    if (options.valgrind) {
      httpOptions.timeout *= 2;
    }

    httpOptions.returnBodyOnError = true;

    const reply = download(instanceInfo.url + "/_admin/execute?returnAsJSON=true",
      testCode,
      httpOptions);

    if (!reply.error && reply.code === 200) {
      return JSON.parse(reply.body);
    } else {
      if (reply.hasOwnProperty('body')) {
        return {
          status: false,
          message: reply.body
        };
      } else {
        return {
          status: false,
          message: yaml.safeDump(reply)
        };
      }
    }
  } catch (ex) {
    return {
      status: false,
      message: ex.message || String(ex),
      stack: ex.stack
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs a list of tests
////////////////////////////////////////////////////////////////////////////////

function performTests(options, testList, testname) {
  let instanceInfo = startInstance("tcp", options, {}, testname);

  if (instanceInfo === false) {
    return {
      setup: {
        status: false,
        message: "failed to start server!"
      }
    };
  }

  if (testList.length === 0) {
    print("Testsuite is empty!");

    return {
      "EMPTY TESTSUITE": {
        status: false,
        message: "no testsuites found!"
      }
    };
  }

  let results = {};
  let continueTesting = true;

  for (let i = 0; i < testList.length; i++) {
    let te = testList[i];
    let filtered = {};

    if (filterTestcaseByOptions(te, options, filtered)) {
      let first = true;
      let loopCount = 0;

      while (first || options.loopEternal) {
        if (!continueTesting) {
          print('oops!');
          print("Skipping, " + te + " server is gone.");

          results[te] = {
            status: false,
            message: instanceInfo.exitStatus
          };

          instanceInfo.exitStatus = "server is gone.";

          break;
        }

        print("\n" + Date() + " arangod: Trying", te, "...");
        let reply = runThere(options, instanceInfo, te);

        if (reply.hasOwnProperty('status')) {
          results[te] = reply;

          if (results[te].status === false) {
            options.cleanup = false;
          }

          if (!reply.status && !options.force) {
            break;
          }
        } else {
          results[te] = {
            status: false,
            message: reply
          };

          if (!options.force) {
            break;
          }
        }

        continueTesting = checkInstanceAlive(instanceInfo, options);

        first = false;

        if (options.loopEternal) {
          if (loopCount % options.loopSleepWhen === 0) {
            print("sleeping...");
            sleep(options.loopSleepSec);
            print("continuing.");
          }

          ++loopCount;
        }
      }
    } else {
      if (options.extremeVerbosity) {
        print("Skipped " + te + " because of " + filtered.filter);
      }
    }
  }

  print("Shutting down...");
  shutdownInstance(instanceInfo, options);
  print("done.");

  return results;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs a stress test on arangod
////////////////////////////////////////////////////////////////////////////////

function runStressTest(options, command, testname) {
  const concurrency = options.concurrency;

  let extra = {
    "javascript.v8-contexts": concurrency + 2,
    "server.threads": concurrency + 2
  };

  let instanceInfo = startInstance("tcp", options, extra, testname);

  if (instanceInfo === false) {
    return {
      status: false,
      message: "failed to start server!"
    };
  }

  const requestOptions = makeAuthorizationHeaders(options);
  requestOptions.method = "POST";
  requestOptions.returnBodyOnError = true;
  requestOptions.headers = {
    "x-arango-async": "store"
  };

  const reply = download(
    instanceInfo.url + "/_admin/execute?returnAsJSON=true",
    command,
    requestOptions);

  if (reply.error || reply.code !== 202) {
    print("cannot execute command: (" +
      reply.code + ") " + reply.message);

    shutdownInstance(instanceInfo, options);

    return {
      status: false,
      message: reply.hasOwnProperty('body') ? reply.body : yaml.safeDump(reply)
    };
  }

  const id = reply.headers["x-arango-async-id"];

  const checkOpts = makeAuthorizationHeaders(options);
  checkOpts.method = "PUT";
  checkOpts.returnBodyOnError = true;

  while (true) {
    const check = download(
      instanceInfo.url + "/_api/job/" + id,
      "", checkOpts);

    if (!check.error) {
      if (check.code === 204) {
        print("still running (" + (new Date()) + ")");
        wait(60);
        continue;
      }

      if (check.code === 200) {
        print("stress test finished");
        break;
      }
    }

    print(yaml.safeDump(check));

    shutdownInstance(instanceInfo, options);

    return {
      status: false,
      message: check.hasOwnProperty('body') ? check.body : yaml.safeDump(check)
    };
  }

  print("Shutting down...");
  shutdownInstance(instanceInfo, options);
  print("done.");

  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a command, possible with valgrind
////////////////////////////////////////////////////////////////////////////////

function executeValgrind(cmd, args, options, valgrindTest) {
  if (valgrindTest && options.valgrind) {
    let valgrindOpts = {};

    if (options.valgrindArgs) {
      valgrindOpts = options.valgrindArgs;
    }

    let testfn = options.valgrindFileBase;

    if (testfn.length > 0) {
      testfn += '_';
    }

    testfn += valgrindTest;

    if (valgrindOpts.xml === "yes") {
      valgrindOpts["xml-file"] = testfn + '.%p.xml';
    }

    valgrindOpts["log-file"] = testfn + '.%p.valgrind.log';

    args = toArgv(valgrindOpts, true).concat([cmd]).concat(args);
    cmd = options.valgrind;
  } else if (options.rr) {
    args = [cmd].concat(args);
    cmd = "rr";
  }

  if (options.extremeVerbosity) {
    print("starting process " + cmd + " with arguments: " + JSON.stringify(args));
  }
  return executeExternal(cmd, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a command and wait for result
////////////////////////////////////////////////////////////////////////////////

function executeAndWait(cmd, args, options, valgrindTest) {
  if (valgrindTest && options.valgrind) {
    let valgrindOpts = {};

    if (options.valgrindArgs) {
      valgrindOpts = options.valgrindArgs;
    }

    let testfn = options.valgrindFileBase;

    if (testfn.length > 0) {
      testfn += '_';
    }

    testfn += valgrindTest;

    if (valgrindOpts.xml === "yes") {
      valgrindOpts["xml-file"] = testfn + '.%p.xml';
    }

    valgrindOpts["log-file"] = testfn + '.%p.valgrind.log';

    args = toArgv(valgrindOpts, true).concat([cmd]).concat(args);
    cmd = options.valgrind;
  }

  if (options.extremeVerbosity) {
    print("executeAndWait: cmd =", cmd, "args =", args);
  }

  const startTime = time();
  if ((typeof(cmd) !== "string") || (cmd === 'true') || (cmd === 'false')) {
    return {
      status: false,
      message: "true or false as binary name for test cmd =" + cmd + "args =" + args
    };
  }
  const res = executeExternalAndWait(cmd, args);
  const deltaTime = time() - startTime;

  let errorMessage = ' - ';

  if (res.status === "TERMINATED") {
    const color = (res.exit === 0 ? GREEN : RED);

    print(color + "Finished: " + res.status +
      " exit code: " + res.exit +
      " Time elapsed: " + deltaTime + RESET);

    if (res.exit === 0) {
      return {
        status: true,
        message: "",
        duration: deltaTime
      };
    } else {
      return {
        status: false,
        message: "exit code was " + res.exit,
        duration: deltaTime
      };
    }
  } else if (res.status === "ABORTED") {
    if (typeof(res.errorMessage) !== 'undefined') {
      errorMessage += res.errorMessage;
    }

    print("Finished: " + res.status +
      " Signal: " + res.signal +
      " Time elapsed: " + deltaTime + errorMessage);

    return {
      status: false,
      message: "irregular termination: " + res.status +
        " exit signal: " + res.signal + errorMessage,
      duration: deltaTime
    };
  } else {
    if (typeof(res.errorMessage) !== 'undefined') {
      errorMessage += res.errorMessage;
    }

    print("Finished: " + res.status +
      " exit code: " + res.signal +
      " Time elapsed: " + deltaTime + errorMessage);

    return {
      status: false,
      message: "irregular termination: " + res.status +
        " exit code: " + res.exit + errorMessage,
      duration: deltaTime
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs file in arangosh
////////////////////////////////////////////////////////////////////////////////

function runInArangosh(options, instanceInfo, file, addArgs) {
  let args = makeArgsArangosh(options);
  args["server.endpoint"] = instanceInfo.endpoint;
  args["javascript.unit-tests"] = fs.join(TOP_DIR, file);

  if (!options.verbose) {
    args["log.level"] = "warning";
  }

  if (addArgs !== undefined) {
    args = Object.assign(args, addArgs);
  }

  let rc = executeAndWait(ARANGOSH_BIN, toArgv(args), options);

  let result;

  try {
    result = JSON.parse(fs.read("testresult.json"));
  } catch (x) {
    return rc;
  }

  if ((typeof result[0] === 'object') &&
    result[0].hasOwnProperty('status')) {
    return result[0];
  } else {
    return rc;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs arangosh
////////////////////////////////////////////////////////////////////////////////

function runArangoshCmd(options, instanceInfo, addArgs, cmds) {
  let args = makeArgsArangosh(options);
  args["server.endpoint"] = instanceInfo.endpoint;

  if (addArgs !== undefined) {
    args = Object.assign(args, addArgs);
  }

  const argv = toArgv(args).concat(cmds);
  return executeAndWait(ARANGOSH_BIN, argv, options);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs arangoimp
////////////////////////////////////////////////////////////////////////////////

function runArangoImp(options, instanceInfo, what) {
  let args = {
    "server.username": options.username,
    "server.password": options.password,
    "server.endpoint": instanceInfo.endpoint,
    "file": fs.join(TOP_DIR, what.data),
    "collection": what.coll,
    "type": what.type
  };

  if (what.create !== undefined) {
    args["create-collection"] = what.create;
  }

  if (what.backslash !== undefined) {
    args["backslash-escape"] = what.backslash;
  }

  if (what.separator !== undefined) {
    args["separator"] = what.separator;
  }

  return executeAndWait(ARANGOIMP_BIN, toArgv(args), options);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs arangodump or arangorestore
////////////////////////////////////////////////////////////////////////////////

function runArangoDumpRestore(options, instanceInfo, which, database) {
  let args = {
    "configuration": "none",
    "server.username": options.username,
    "server.password": options.password,
    "server.endpoint": instanceInfo.endpoint,
    "server.database": database
  };

  let exe;

  if (which === "dump") {
    args["output-directory"] = fs.join(instanceInfo.rootDir, "dump");
    exe = ARANGODUMP_BIN;
  } else {
    args["create-database"] = "true";
    args["input-directory"] = fs.join(instanceInfo.rootDir, "dump");
    exe = ARANGORESTORE_BIN;
  }

  return executeAndWait(exe, toArgv(args), options);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs arangobench
////////////////////////////////////////////////////////////////////////////////

function runArangoBenchmark(options, instanceInfo, cmds) {
  let args = {
    "configuration": "none",
    "server.username": options.username,
    "server.password": options.password,
    "server.endpoint": instanceInfo.endpoint,
    // "server.request-timeout": 1200 // default now. 
    "server.connection-timeout": 10 // 5s default
  };

  args = Object.assign(args, cmds);

  if (!args.hasOwnProperty('verbose')) {
    args["log.level"] = "warning";
    args["flatCommands"] = ["--quiet"];
  }

  return executeAndWait(ARANGOBENCH_BIN, toArgv(args), options);
}

function shutdownArangod(arangod, options) {
  if (options.valgrind) {
    waitOnServerForGC(arangod, options, 60);
  }
  if (arangod.exitStatus === undefined ||
    arangod.exitStatus.status === "RUNNING") {
    const requestOptions = makeAuthorizationHeaders(options);
    requestOptions.method = "DELETE";

    print(arangod.url + "/_admin/shutdown");
    download(arangod.url + "/_admin/shutdown", "", requestOptions);
  } else {
    print("Server already dead, doing nothing.");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down an instance
////////////////////////////////////////////////////////////////////////////////

function shutdownInstance(instanceInfo, options) {
  if (!checkInstanceAlive(instanceInfo, options)) {
    print("Server already dead, doing nothing. This shouldn't happen?");
  }

  // Shut down all non-agency servers:
  const n = instanceInfo.arangods.length;

  let nonagencies = instanceInfo.arangods
    .filter(arangod => !arangod.isAgency);
  nonagencies.forEach(arangod =>
    shutdownArangod(arangod, options)
  );

  let agentsKilled = false;
  let nrAgents = n - nonagencies.length;

  let timeout = 60;
  if (options.valgrind) {
    timeout *= 10;
  }
  if (options.sanitizer) {
    timeout *= 2;
  }
  let count = 0;
  let bar = "[";

  let toShutdown = instanceInfo.arangods.slice();
  while (toShutdown.length > 0) {
    // Once all other servers are shut down, we take care of the agents,
    // we do this exactly once (agentsKilled flag) and only if there
    // are agents:
    if (!agentsKilled && nrAgents > 0 && toShutdown.length === nrAgents) {
      instanceInfo.arangods
        .filter(arangod => arangod.isAgency)
        .forEach(arangod => shutdownArangod(arangod, options));
      agentsKilled = true;
    }
    toShutdown = toShutdown.filter(arangod => {
      arangod.exitStatus = statusExternal(arangod.pid, false);

      if (arangod.exitStatus.status === "RUNNING") {
        ++count;

        if (count % 10 === 0) {
          bar = bar + "#";
        }

        if (count > timeout) {
          print("forcefully terminating " + yaml.safeDump(arangod.pid) +
            " after " + timeout + "s grace period; marking crashy.");
          serverCrashed = true;
          killExternal(arangod.pid);
          return false;
        } else {
          return true;
        }
      } else if (arangod.exitStatus.status !== "TERMINATED") {
        if (arangod.exitStatus.hasOwnProperty('signal')) {
          analyzeServerCrash(arangod, options, "instance Shutdown");
        }
      } else {
        print("Server shutdown: Success.");
        return false;
      }
    });
    if (toShutdown.length > 0) {
      print(toShutdown.length + " arangods are still running...");
      wait(1);
    }
  }

  if (!options.skipLogAnalysis) {
    instanceInfo.arangods.forEach(arangod => {
      let errorEntries = readImportantLogLines(arangod.rootDir);
      if (Object.keys(errorEntries).length > 0) {
        print("Found messages in the server logs: \n" +
          yaml.safeDump(errorEntries));
      }
    });
  }

  cleanupDirectories.push(instanceInfo.rootDir);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts an instance
///
/// protocol must be one of ["tcp", "ssl", "unix"]
////////////////////////////////////////////////////////////////////////////////

function startInstanceCluster(instanceInfo, protocol, options,
  addArgs, name, rootDir) {
  let makeArgs = function(name, args) {
    args = args || options.extraArgs;

    let subDir = fs.join(rootDir, name);
    fs.makeDirectoryRecursive(subDir);

    let subArgs = makeArgsArangod(options, fs.join(subDir, "apps"));
    subArgs = Object.assign(subArgs, args);

    return [subArgs, name, subDir];
  };

  options.agencySize = 1;
  options.agencyWaitForSync = false;
  startInstanceAgency(instanceInfo, protocol, options, ...makeArgs('agency', {}));

  let agencyEndpoint = instanceInfo.endpoint;
  let i;
  for (i = 0; i < options.clusterNodes; i++) {
    let endpoint = protocol + "://127.0.0.1:" + findFreePort();
    let primaryArgs = _.clone(options.extraArgs);
    primaryArgs['server.endpoint'] = endpoint;
    primaryArgs['cluster.my-address'] = endpoint;
    primaryArgs['cluster.my-local-info'] = endpoint;
    primaryArgs['cluster.my-role'] = 'PRIMARY';
    primaryArgs['cluster.agency-endpoint'] = agencyEndpoint;

    startInstanceSingleServer(instanceInfo, protocol, options, ...makeArgs('dbserver' + i, primaryArgs));
  }

  let endpoint = protocol + "://127.0.0.1:" + findFreePort();
  let coordinatorArgs = _.clone(options.extraArgs);
  coordinatorArgs['server.endpoint'] = endpoint;
  coordinatorArgs['cluster.my-address'] = endpoint;
  coordinatorArgs['cluster.my-local-info'] = endpoint;
  coordinatorArgs['cluster.my-role'] = 'COORDINATOR';
  coordinatorArgs['cluster.agency-endpoint'] = agencyEndpoint;

  startInstanceSingleServer(instanceInfo, protocol, options, ...makeArgs('coordinator', coordinatorArgs));

  //disabled because not in use (jslint)
  //let coordinatorUrl = instanceInfo.url;
  //let response;
  let httpOptions = makeAuthorizationHeaders(options);
  httpOptions.method = 'POST';
  httpOptions.returnBodyOnError = true;

  let count = 0;
  instanceInfo.arangods.forEach(arangod => {
    while (true) {
      const reply = download(arangod.url + "/_api/version", "", makeAuthorizationHeaders(options));

      if (!reply.error && reply.code === 200) {
        break;
      }

      ++count;

      if (count % 60 === 0) {
        if (!checkArangoAlive(arangod, options)) {
          throw new Error("startup failed! bailing out!");
        }
      }
      wait(0.5, false);
    }
  });

  arango.reconnect(endpoint, "_system", 'root', '');

  return true;
}

function startArango(protocol, options, addArgs, name, rootDir, isAgency) {
  const dataDir = fs.join(rootDir, "data");
  const appDir = fs.join(rootDir, "apps");

  fs.makeDirectoryRecursive(dataDir);
  fs.makeDirectoryRecursive(appDir);

  let args = makeArgsArangod(options, appDir);
  let endpoint;
  let port;

  if (!addArgs["server.endpoint"]) {
    port = findFreePort();
    endpoint = protocol + "://127.0.0.1:" + port;
  } else {
    endpoint = addArgs["server.endpoint"];
    port = endpoint.split(":").pop();
  }

  let instanceInfo = {
    isAgency, port, endpoint, rootDir
  };

  args["server.endpoint"] = endpoint;
  args["database.directory"] = dataDir;
  args["log.file"] = fs.join(rootDir, "log");

  if (options.verbose) {
    args["log.level"] = 'info';
  } else {
    args["log.level"] = 'error';
  }

  if (protocol === "ssl") {
    args["ssl.keyfile"] = fs.join("UnitTests", "server.pem");
  }

  args = Object.assign(args, options.extraArgs);

  if (addArgs !== undefined) {
    args = Object.assign(args, addArgs);
  }

  instanceInfo.url = endpointToURL(instanceInfo.endpoint);
  instanceInfo.pid = executeValgrind(ARANGOD_BIN, toArgv(args), options, name).pid;

  if (platform.substr(0, 3) === 'win') {
    const procdumpArgs = [
      '-accepteula',
      '-e',
      '-ma',
      instanceInfo.pid,
      fs.join(rootDir, 'core.dmp')
    ];

    try {
      instanceInfo.monitor = executeExternal('procdump', procdumpArgs);
    } catch (x) {
      print("failed to start procdump - is it installed?");
      throw x;
    }
  }
  return instanceInfo;
}

function startInstanceAgency(instanceInfo, protocol, options,
  addArgs, testname, rootDir) {

  const dataDir = fs.join(rootDir, "data");

  const N = options.agencySize;
  if (options.agencyWaitForSync === undefined) {
    options.agencyWaitForSync = false;
  }
  const wfs = options.agencyWaitForSync;

  for (let i = 0; i < N; i++) {
    let instanceArgs = _.clone(addArgs);
    instanceArgs["agency.id"] = String(i);
    instanceArgs["agency.size"] = String(N);
    instanceArgs["agency.wait-for-sync"] = String(wfs);
    instanceArgs["agency.supervision"] = "true";
    instanceArgs["database.directory"] = dataDir + String(i);

    if (i === N - 1) {
      const port = findFreePort();
      instanceArgs["server.endpoint"] = "tcp://127.0.0.1:" + port;
      let l = [];
      instanceInfo.arangods.forEach(arangod => {
        l.push("--agency.endpoint");
        l.push(arangod.endpoint);
      });
      l.push("--agency.endpoint");
      l.push("tcp://127.0.0.1:" + port);
      l.push("--agency.notify");
      l.push("true");

      instanceArgs["flatCommands"] = l;
    }
    let dir = fs.join(rootDir, 'agency-' + i);
    fs.makeDirectoryRecursive(dir);

    require("internal").print(instanceArgs);
    
    instanceInfo.arangods.push(startArango(protocol, options, instanceArgs, testname, rootDir, true));
  }

  instanceInfo.endpoint = instanceInfo.arangods[instanceInfo.arangods.length - 1].endpoint;
  instanceInfo.url = instanceInfo.arangods[instanceInfo.arangods.length - 1].url;
  print("Agency Endpoint: " + instanceInfo.endpoint);

  return instanceInfo;
}

function startInstanceSingleServer(instanceInfo, protocol, options,
  addArgs, testname, rootDir) {

  instanceInfo.arangods.push(startArango(protocol, options, addArgs, testname, rootDir, false));

  instanceInfo.endpoint = instanceInfo.arangods[instanceInfo.arangods.length - 1].endpoint;
  instanceInfo.url = instanceInfo.arangods[instanceInfo.arangods.length - 1].url;

  return instanceInfo;
}

function startInstance(protocol, options, addArgs, testname, tmpDir) {
  let rootDir = fs.join(tmpDir || fs.getTempFile(), testname);
  let instanceInfo = {
    rootDir, arangods: []
  };

  const startTime = time();
  try {
    if (options.cluster) {
      startInstanceCluster(instanceInfo, protocol, options,
        addArgs, testname, rootDir);
    } else if (options.agency) {
      startInstanceAgency(instanceInfo, protocol, options,
        addArgs, testname, rootDir);
    } else {
      startInstanceSingleServer(instanceInfo, protocol, options,
        addArgs, testname, rootDir);
    }

    if (!options.cluster) {
      let count = 0;
      instanceInfo.arangods.forEach(arangod => {
        while (true) {
          const reply = download(arangod.url + "/_api/version", "", makeAuthorizationHeaders(options));

          if (!reply.error && reply.code === 200) {
            break;
          }

          ++count;

          if (count % 60 === 0) {
            if (!checkArangoAlive(arangod, options)) {
              throw new Error("startup failed! bailing out!");
            }
          }
          wait(0.5, false);
        }
      });
    }
    print(CYAN + "up and running in " + (time() - startTime) + " seconds" + RESET);
  } catch (e) {
    print(e, e.stack);
    return false;
  }



  return instanceInfo;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs ruby tests using RSPEC
////////////////////////////////////////////////////////////////////////////////

function camelize(str) {
  return str.replace(/(?:^\w|[A-Z]|\b\w)/g, function(letter, index) {
    return index === 0 ? letter.toLowerCase() : letter.toUpperCase();
  }).replace(/\s+/g, '');
}

function rubyTests(options, ssl) {
  let instanceInfo;

  if (ssl) {
    instanceInfo = startInstance("ssl", options, {}, "ssl_server");
  } else {
    instanceInfo = startInstance("tcp", options, {}, "http_server");
  }

  if (instanceInfo === false) {
    return {
      status: false,
      message: "failed to start server!"
    };
  }

  const tmpname = fs.getTempFile() + ".rb";

  fs.write(tmpname, 'RSpec.configure do |c|\n' +
    '  c.add_setting :ARANGO_SERVER\n' +
    '  c.ARANGO_SERVER = "' +
    instanceInfo.endpoint.substr(6) + '"\n' +
    '  c.add_setting :ARANGO_SSL\n' +
    '  c.ARANGO_SSL = "' + (ssl ? '1' : '0') + '"\n' +
    '  c.add_setting :ARANGO_USER\n' +
    '  c.ARANGO_USER = "' + options.username + '"\n' +
    '  c.add_setting :ARANGO_PASSWORD\n' +
    '  c.ARANGO_PASSWORD = "' + options.password + '"\n' +
    'end\n');

  try {
    fs.makeDirectory(LOGS_DIR);
  } catch (err) {}

  const files = fs.list(fs.join("UnitTests", "HttpInterface"));

  let continueTesting = true;
  let filtered = {};
  let result = {};

  let args;
  let command;
  let rspec;

  if (options.ruby === "") {
    command = options.rspec;
    rspec = undefined;
  } else {
    command = options.ruby;
    rspec = options.rspec;
  }

  const parseRspecJson = function(testCase, res, totalDuration) {
    let tName = camelize(testCase.description);
    let status = (testCase.status === "passed");

    res[tName] = {
      status: status,
      message: testCase.full_description,
      duration: totalDuration // RSpec doesn't offer per testcase time...
    };

    res.total++;

    if (!status) {
      const msg = yaml.safeDump(testCase)
        .replace(/.*rspec\/core.*\n/gm, "")
        .replace(/.*rspec\\core.*\n/gm, "");
      print("RSpec test case falied: \n" + msg);
      res[tName].message += "\n" + msg;
    }
  };

  for (let i = 0; i < files.length; i++) {
    const te = files[i];

    if (te.substr(0, 4) === "api-" && te.substr(-3) === ".rb") {
      if (filterTestcaseByOptions(te, options, filtered)) {
        if (!continueTesting) {
          print("Skipping " + te + " server is gone.");

          result[te] = {
            status: false,
            message: instanceInfo.exitStatus
          };

          instanceInfo.exitStatus = "server is gone.";
          break;
        }

        args = ["--color",
          "-I", fs.join("UnitTests", "HttpInterface"),
          "--format", "d",
          "--format", "j",
          "--out", fs.join("out", "UnitTests", te + ".json"),
          "--require", tmpname,
          fs.join("UnitTests", "HttpInterface", te)
        ];

        if (rspec !== undefined) {
          args = [rspec].concat(args);
        }

        print("\n" + Date() + " rspec trying", te, "...");
        const res = executeAndWait(command, args, options);

        result[te] = {
          total: 0,
          status: res.status
        };

        const resultfn = fs.join("out", "UnitTests", te + ".json");

        try {
          const jsonResult = JSON.parse(fs.read(resultfn));

          if (options.extremeVerbosity) {
            print(yaml.safeDump(jsonResult));
          }

          for (let j = 0; j < jsonResult.examples.length; ++j) {
            parseRspecJson(jsonResult.examples[j], result[te],
              jsonResult.summary.duration);
          }

          result[te].duration = jsonResult.summary.duration;
        } catch (x) {
          print("Failed to parse rspec result: " + x);
          result[te]["complete_" + te] = res;

          if (res.status === false) {
            options.cleanup = false;

            if (!options.force) {
              break;
            }
          }
        }

        continueTesting = checkInstanceAlive(instanceInfo, options);
      } else {
        if (options.extremeVerbosity) {
          print("Skipped " + te + " because of " + filtered.filter);
        }
      }
    }
  }

  print("Shutting down...");

  fs.remove(tmpname);
  shutdownInstance(instanceInfo, options);
  print("done.");

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort test-cases according to pathname
////////////////////////////////////////////////////////////////////////////////

let testsCases = {
  setup: false
};

function findTests() {
  if (testsCases.setup) {
    return;
  }

  testsCases.common = _.filter(fs.list(makePathUnix("js/common/tests/shell")),
    function(p) {
      return p.substr(-3) === ".js";
    }).map(
    function(x) {
      return fs.join(makePathUnix("js/common/tests/shell"), x);
    }).sort();

  testsCases.server_only = _.filter(fs.list(makePathUnix("js/server/tests/shell")),
    function(p) {
      return p.substr(-3) === ".js";
    }).map(
    function(x) {
      return fs.join(makePathUnix("js/server/tests/shell"), x);
    }).sort();

  testsCases.client_only = _.filter(fs.list(makePathUnix("js/client/tests/shell")),
    function(p) {
      return p.substr(-3) === ".js";
    }).map(
    function(x) {
      return fs.join(makePathUnix("js/client/tests/shell"), x);
    }).sort();

  testsCases.server_aql = _.filter(fs.list(makePathUnix("js/server/tests/aql")),
    function(p) {
      return p.substr(-3) === ".js" && p.indexOf("ranges-combined") === -1;
    }).map(
    function(x) {
      return fs.join(makePathUnix("js/server/tests/aql"), x);
    }).sort();

  testsCases.server_aql_extended =
    _.filter(fs.list(makePathUnix("js/server/tests/aql")),
      function(p) {
        return p.substr(-3) === ".js" && p.indexOf("ranges-combined") !== -1;
      }).map(
      function(x) {
        return fs.join(makePathUnix("js/server/tests/aql"), x);
      }).sort();

  testsCases.server_aql_performance =
    _.filter(fs.list(makePathUnix("js/server/perftests")),
      function(p) {
        return p.substr(-3) === ".js";
      }).map(
      function(x) {
        return fs.join(makePathUnix("js/server/perftests"), x);
      }).sort();

  testsCases.server_http = _.filter(fs.list(makePathUnix("js/common/tests/http")),
    function(p) {
      return p.substr(-3) === ".js";
    }).map(
    function(x) {
      return fs.join(makePathUnix("js/common/tests/http"), x);
    }).sort();

  testsCases.replication = _.filter(fs.list(makePathUnix("js/common/tests/replication")),
    function(p) {
      return p.substr(-3) === ".js";
    }).map(
    function(x) {
      return fs.join(makePathUnix("js/common/tests/replication"), x);
    }).sort();

  testsCases.agency = _.filter(fs.list(makePathUnix("js/client/tests/agency")),
    function(p) {
      return p.substr(-3) === ".js";
    }).map(
    function(x) {
      return fs.join(makePathUnix("js/client/tests/agency"), x);
    }).sort();

  testsCases.resilience = _.filter(fs.list(makePathUnix("js/server/tests/resilience")),
    function(p) {
      return p.substr(-3) === ".js";
    }).map(
    function(x) {
      return fs.join(makePathUnix("js/server/tests/resilience"), x);
    }).sort();

  testsCases.server = testsCases.common.concat(testsCases.server_only);
  testsCases.client = testsCases.common.concat(testsCases.client_only);

  testsCases.setup = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief filter test-cases according to options
////////////////////////////////////////////////////////////////////////////////

function filterTestcaseByOptions(testname, options, whichFilter) {
  if (options.hasOwnProperty('test') && (typeof(options.test) !== 'undefined')) {
    whichFilter.filter = 'testcase';
    return testname === options.test;
  }

  if (options.replication) {
    whichFilter.filter = 'replication';
    return testname.indexOf('replication') !== -1;
  } else if (testname.indexOf('replication') !== -1) {
    whichFilter.filter = 'replication';
    return false;
  }

  if ((testname.indexOf('-cluster') !== -1) && !options.cluster) {
    whichFilter.filter = 'noncluster';
    return false;
  }

  if (testname.indexOf('-noncluster') !== -1 && options.cluster) {
    whichFilter.filter = 'cluster';
    return false;
  }

  if (testname.indexOf('-timecritical') !== -1 && options.skipTimeCritical) {
    whichFilter.filter = 'timecritical';
    return false;
  }

  if (testname.indexOf('-nightly') !== -1 && options.skipNightly && !options.onlyNightly) {
    whichFilter.filter = 'skip nightly';
    return false;
  }

  if (testname.indexOf('-geo') !== -1 && options.skipGeo) {
    whichFilter.filter = 'geo';
    return false;
  }

  if (testname.indexOf('-nondeterministic') !== -1 && options.skipNondeterministic) {
    whichFilter.filter = 'nondeterministic';
    return false;
  }

  if (testname.indexOf('-graph') !== -1 && options.skipGraph) {
    whichFilter.filter = 'graph';
    return false;
  }

  if (testname.indexOf('-disabled') !== -1) {
    whichFilter.filter = 'disabled';
    return false;
  }

  if ((testname.indexOf('-memoryintense') !== -1) && options.skipMemoryIntense) {
    whichFilter.filter = 'memoryintense';
    return false;
  }

  if (testname.indexOf('-nightly') === -1 && options.onlyNightly) {
    whichFilter.filter = 'only nightly';
    return false;
  }

  if ((testname.indexOf('-novalgrind') !== -1) && options.valgrind) {
    whichFilter.filter = 'skip in valgrind';
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief split into buckets
////////////////////////////////////////////////////////////////////////////////

function splitBuckets(options, cases) {
  if (!options.testBuckets || cases.length === 0) {
    return cases;
  }

  let m = cases.length;
  let n = options.testBuckets.split("/");
  let r = parseInt(n[0]);
  let s = parseInt(n[1]);

  if (r < 1) {
    r = 1;
  }

  if (r === 1) {
    return cases;
  }

  if (s < 0) {
    s = 0;
  }
  if (r <= s) {
    s = r - 1;
  }

  let result = [];

  for (let i = s % m; i < cases.length; i = i + r) {
    result.push(cases[i]);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test functions for all
////////////////////////////////////////////////////////////////////////////////

let allTests = [
  "agency",
  "arangosh",
  "authentication",
  "authentication_parameters",
  "boost",
  "config",
  "dump",
  "dump_authentication",
  "dfdb",
  "http_server",
  "importing",
  "server_http",
  "shell_client",
  "shell_server",
  "shell_server_aql",
  "ssl_server",
  "upgrade",
  "arangobench"
];

////////////////////////////////////////////////////////////////////////////////
/// @brief internal members of the results
////////////////////////////////////////////////////////////////////////////////

const internalMembers = [
  "code",
  "error",
  "status",
  "duration",
  "failed",
  "total",
  "crashed",
  "ok",
  "message",
  "suiteName"
];

function skipInternalMember(r, a) {
  return !r.hasOwnProperty(a) || internalMembers.indexOf(a) !== -1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: all
////////////////////////////////////////////////////////////////////////////////

let testFuncs = {
  'all': function() {}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: arangosh
////////////////////////////////////////////////////////////////////////////////

testFuncs.arangosh = function(options) {
  let ret = {
    "testArangoshExitCodeFail": {
      status: true,
      total: 0
    },
    "testArangoshExitCodeSuccess": {
      status: true,
      total: 0
    },
    "testArangoshShebang": {
      status: true,
      total: 0
    },
  };

  print("--------------------------------------------------------------------------------");
  print("Starting arangosh with exception throwing script:");
  print("--------------------------------------------------------------------------------");

  let args = makeArgsArangosh(options);
  args["javascript.execute-string"] = "throw('foo')";
  args["log.level"] = "error";

  const startTime = time();
  let rc = executeExternalAndWait(ARANGOSH_BIN, toArgv(args));
  const deltaTime = time() - startTime;
  const failSuccess = (rc.hasOwnProperty('exit') && rc.exit === 1);

  if (!failSuccess) {
    ret.testArangoshExitCodeFail['message'] =
      "didn't get expected return code (1): \n" +
      yaml.safeDump(rc);
  }

  ++ret.testArangoshExitCodeFail['total'];
  ret.testArangoshExitCodeFail['status'] = failSuccess;
  ret.testArangoshExitCodeFail['duration'] = deltaTime;
  print((failSuccess ? GREEN : RED) + "Status: " + (failSuccess ? "SUCCESS" : "FAIL") + RESET);

  print("\n--------------------------------------------------------------------------------");
  print("Starting arangosh with regular terminating script:");
  print("--------------------------------------------------------------------------------");

  args["javascript.execute-string"] = ";";
  args["log.level"] = "warning";

  const startTime2 = time();
  rc = executeExternalAndWait(ARANGOSH_BIN, toArgv(args));
  const deltaTime2 = time() - startTime2;

  const successSuccess = (rc.hasOwnProperty('exit') && rc.exit === 0);

  if (!successSuccess) {
    ret.testArangoshExitCodeFail['message'] =
      "didn't get expected return code (0): \n" +
      yaml.safeDump(rc);
  }

  ++ret.testArangoshExitCodeSuccess['total'];
  ret.testArangoshExitCodeSuccess['status'] = failSuccess;
  ret.testArangoshExitCodeSuccess['duration'] = deltaTime2;
  print((successSuccess ? GREEN : RED) + "Status: " + (successSuccess ? "SUCCESS" : "FAIL") + RESET);

  // test shebang execution with arangosh
  if (!options.skipShebang && platform.substr(0, 3) !== "win") {
    var shebangSuccess = true;
    var deltaTime3 = 0;
    var shebangFile = fs.getTempFile();

    print("\n--------------------------------------------------------------------------------");
    print("Starting arangosh via shebang script");
    print("--------------------------------------------------------------------------------");

    if (options.verbose) {
      print(CYAN + "shebang script: " + shebangFile + RESET);
    }

    fs.write(shebangFile,
      "#!" + fs.makeAbsolute(ARANGOSH_BIN) + " --javascript.execute \n" +
      "print('hello world');\n");

    executeExternalAndWait("sh", ["-c", "chmod a+x " + shebangFile]);

    const startTime3 = time();
    rc = executeExternalAndWait("sh", ["-c", shebangFile]);
    deltaTime3 = time() - startTime3;

    if (options.verbose) {
      print(CYAN + "execute returned: " + RESET, rc);
    }

    shebangSuccess = (rc.hasOwnProperty('exit') && rc.exit === 0);

    if (!shebangSuccess) {
      ret.testArangoshShebang['message'] =
        "didn't get expected return code (0): \n" +
        yaml.safeDump(rc);
    }

    fs.remove(shebangFile);

    ++ret.testArangoshShebang['total'];
    ret.testArangoshShebang['status'] = shebangSuccess;
    ret.testArangoshShebang['duration'] = deltaTime3;
    print((shebangSuccess ? GREEN : RED) + "Status: " + (shebangSuccess ? "SUCCESS" : "FAIL") + RESET);
  } else {
    ret.testArangoshShebang['skipped'] = true;
  }

  print();

  return ret;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: arangobench
////////////////////////////////////////////////////////////////////////////////

const benchTodos = [{
  "requests": "10000",
  "concurrency": "2",
  "test-case": "version",
  "keep-alive": "false"
}, {
  "requests": "10000",
  "concurrency": "2",
  "test-case": "version",
  "async": "true"
}, {
  "requests": "20000",
  "concurrency": "1",
  "test-case": "version",
  "async": "true"
}, {
  "requests": "100000",
  "concurrency": "2",
  "test-case": "shapes",
  "batch-size": "16",
  "complexity": "2"
}, {
  "requests": "100000",
  "concurrency": "2",
  "test-case": "shapes-append",
  "batch-size": "16",
  "complexity": "4"
}, {
  "requests": "100000",
  "concurrency": "2",
  "test-case": "random-shapes",
  "batch-size": "16",
  "complexity": "2"
}, {
  "requests": "1000",
  "concurrency": "2",
  "test-case": "version",
  "batch-size": "16"
}, {
  "requests": "100",
  "concurrency": "1",
  "test-case": "version",
  "batch-size": "0"
}, {
  "requests": "100",
  "concurrency": "2",
  "test-case": "document",
  "batch-size": "10",
  "complexity": "1"
}, {
  "requests": "2000",
  "concurrency": "2",
  "test-case": "crud",
  "complexity": "1"
}, {
  "requests": "4000",
  "concurrency": "2",
  "test-case": "crud-append",
  "complexity": "4"
}, {
  "requests": "4000",
  "concurrency": "2",
  "test-case": "edge",
  "complexity": "4"
}, {
  "requests": "5000",
  "concurrency": "2",
  "test-case": "hash",
  "complexity": "1"
}, {
  "requests": "5000",
  "concurrency": "2",
  "test-case": "skiplist",
  "complexity": "1"
}, {
  "requests": "500",
  "concurrency": "3",
  "test-case": "aqltrx",
  "complexity": "1",
  "transaction": true
}, {
  "requests": "100",
  "concurrency": "3",
  "test-case": "counttrx",
  "transaction": true
}, {
  "requests": "500",
  "concurrency": "3",
  "test-case": "multitrx",
  "transaction": true
}];

testFuncs.arangobench = function(options) {
  if (options.skipArangoBench === true) {
    print("skipping Benchmark tests!");
    return {
      arangobench: {
        status: true,
        skipped: true
      }
    };
  }

  print(CYAN + "arangobench tests..." + RESET);

  let instanceInfo = startInstance("tcp", options, {}, "arangobench");

  if (instanceInfo === false) {
    return {
      arangobench: {
        status: false,
        message: "failed to start server!"
      }
    };
  }

  let results = {};
  let continueTesting = true;

  for (let i = 0; i < benchTodos.length; i++) {
    const benchTodo = benchTodos[i];
    const name = "case" + i;

    if ((options.skipArangoBenchNonConnKeepAlive) &&
      benchTodo.hasOwnProperty('keep-alive') &&
      (benchTodo['keep-alive'] === "false")) {
      benchTodo['keep-alive'] = true;
    }

    // On the cluster we do not yet have working transaction functionality:
    if (!options.cluster || !benchTodo.transaction) {

      if (!continueTesting) {
        print(RED + "Skipping " + benchTodo + ", server is gone." + RESET);

        results[name] = {
          status: false,
          message: instanceInfo.exitStatus
        };

        instanceInfo.exitStatus = "server is gone.";

        break;
      }

      let args = _.clone(benchTodo);
      delete args.transaction;

      if (options.hasOwnProperty('benchargs')) {
        args = Object.assign(args, options.benchargs);
      }

      let oneResult = runArangoBenchmark(options, instanceInfo, args);
      print();

      results[name] = oneResult;
      results[name].total++;

      if (!results[name].status) {
        results.status = false;
      }

      continueTesting = checkInstanceAlive(instanceInfo, options);

      if (oneResult.status !== true && !options.force) {
        break;
      }
    }
  }

  print(CYAN + "Shutting down..." + RESET);
  shutdownInstance(instanceInfo, options);
  print(CYAN + "done." + RESET);

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: authentication
////////////////////////////////////////////////////////////////////////////////

testFuncs.authentication = function(options) {
  if (options.skipAuthentication === true) {
    print("skipping Authentication tests!");

    return {
      authentication: {
        status: true,
        skipped: true
      }
    };
  }

  if (options.cluster) {
    print("skipping Authentication tests on cluster!");
    return {
      authentication: {
        status: true,
        skipped: true
      }
    };
  }

  print(CYAN + "Authentication tests..." + RESET);

  let instanceInfo = startInstance("tcp", options, {
    "server.authentication": "true",
    "server.jwt-secret": "haxxmann",
  }, "authentication");

  if (instanceInfo === false) {
    return {
      authentication: {
        status: false,
        message: "failed to start server!"
      }
    };
  }

  let results = {};

  results.authentication = runInArangosh(options, instanceInfo,
    fs.join("js", "client", "tests", "auth.js"));

  print(CYAN + "Shutting down..." + RESET);
  shutdownInstance(instanceInfo, options);
  print(CYAN + "done." + RESET);

  print();

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: authentication parameters
////////////////////////////////////////////////////////////////////////////////

const authTestExpectRC = [
  [401, 401, 401, 401, 401, 401, 401],
  [401, 401, 401, 401, 401, 404, 404],
  [404, 404, 200, 301, 301, 404, 404]
];

const authTestUrls = [
  "/_api/",
  "/_api",
  "/_api/version",
  "/_admin/html",
  "/_admin/html/",
  "/test",
  "/the-big-fat-fox"
];

const authTestNames = [
  "Full",
  "SystemAuth",
  "None"
];

const authTestServerParams = [{
  "server.authentication": "true",
  "server.authentication-system-only": "false"
}, {
  "server.authentication": "true",
  "server.authentication-system-only": "true"
}, {
  "server.authentication": "false",
  "server.authentication-system-only": "true"
}];

function checkBodyForJsonToParse(request) {
  if (request.hasOwnProperty('body')) {
    request.body = JSON.parse(request.hasOwnProperty('body'));
  }
}

testFuncs.authentication_parameters = function(options) {
  if (options.skipAuthentication === true) {
    print(CYAN + "skipping Authentication with parameters tests!" + RESET);
    return {
      authentication_parameters: {
        status: true,
        skipped: true
      }
    };
  }
  
  if (options.cluster) {
    print("skipping Authentication with parameters tests on cluster!");
    return {
      authentication: {
        status: true,
        skipped: true
      }
    };
  }

  print(CYAN + "Authentication with parameters tests..." + RESET);

  let downloadOptions = {
    followRedirects: false,
    returnBodyOnError: true
  };

  if (options.valgrind) {
    downloadOptions.timeout = 300;
  }

  let continueTesting = true;
  let results = {};

  for (let test = 0; test < 3; test++) {
    let instanceInfo = startInstance("tcp", options,
      authTestServerParams[test],
      "authentication_parameters_" + authTestNames[test]);

    if (instanceInfo === false) {
      return {
        authentication_parameters: {
          status: false,
          total: 1,
          failed: 1,
          message: authTestNames[test] + ": failed to start server!"
        }
      };
    }

    print(CYAN + Date() + " Starting " + authTestNames[test] + " test" + RESET);

    const testName = 'auth_' + authTestNames[test];
    results[testName] = {
      failed: 0,
      total: 0
    };

    for (let i = 0; i < authTestUrls.length; i++) {
      const authTestUrl = authTestUrls[i];

      ++results[testName].total;

      print(CYAN + "  URL: " + instanceInfo.url + authTestUrl + RESET);

      if (!continueTesting) {
        print(RED + "Skipping " + authTestUrl + ", server is gone." + RESET);

        results[testName][authTestUrl] = {
          status: false,
          message: instanceInfo.exitStatus
        };

        results[testName].failed++;
        instanceInfo.exitStatus = "server is gone.";

        break;
      }

      let reply = download(instanceInfo.url + authTestUrl, "", downloadOptions);

      if (reply.code === authTestExpectRC[test][i]) {
        results[testName][authTestUrl] = {
          status: true
        };
      } else {
        checkBodyForJsonToParse(reply);

        ++results[testName].failed;

        results[testName][authTestUrl] = {
          status: false,
          message: "we expected " +
            authTestExpectRC[test][i] +
            " and we got " + reply.code +
            " Full Status: " + yaml.safeDump(reply)
        };
      }

      continueTesting = checkInstanceAlive(instanceInfo, options);
    }

    results[testName].status = results[testName].failed === 0;

    print(CYAN + "Shutting down " + authTestNames[test] + " test..." + RESET);
    shutdownInstance(instanceInfo, options);
    print(CYAN + "done with " + authTestNames[test] + " test." + RESET);
  }

  print();

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: boost
////////////////////////////////////////////////////////////////////////////////

function locateBoostTest(name) {
  var file = fs.join(UNITTESTS_DIR, name);
  if (platform.substr(0, 3) === 'win') {
    file += ".exe";
  }

  if (!fs.exists(file)) {
    return "";
  }
  return file;
}

testFuncs.boost = function(options) {
  const args = ["--show_progress"];

  let results = {};

  if (!options.skipBoost) {
    const run = locateBoostTest("basics_suite");

    if (run !== "") {
      results.basics = executeAndWait(run, args, options, "basics");
    } else {
      results.basics = {
        status: false,
        message: "binary 'basics_suite' not found"
      };
    }
  }

  if (!options.skipGeo) {
    const run = locateBoostTest("geo_suite");

    if (run !== "") {
      results.geo_suite = executeAndWait(run, args, options, "geo_suite");
    } else {
      results.geo_suite = {
        status: false,
        message: "binary 'geo_suite' not found"
      };
    }
  }

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: config
////////////////////////////////////////////////////////////////////////////////

testFuncs.config = function(options) {
  if (options.skipConfig) {
    return {
      config: {
        status: true,
        skipped: true
      }
    };
  }

  let results = {
    absolut: {
      status: true,
      total: 0,
      duration: 0
    },
    relative: {
      status: true,
      total: 0,
      duration: 0
    }
  };

  const ts = [
    "arangod",
    "arangobench",
    "arangodump",
    "arangoimp",
    "arangorestore",
    "arangosh",
    "arango-dfdb",
    "foxx-manager"
  ];

  print("--------------------------------------------------------------------------------");
  print("absolute config tests");
  print("--------------------------------------------------------------------------------");

  let startTime = time();

  for (let i = 0; i < ts.length; i++) {
    const test = ts[i];
    print(CYAN + "checking '" + test + "'" + RESET);

    const args = {
      "configuration": fs.join(CONFIG_DIR, test + ".conf"),
      "flatCommands": ["--check-configuration"]
    };

    const run = fs.join(BIN_DIR, test);

    results.absolut[test] = executeAndWait(run, toArgv(args), options, test);

    if (!results.absolut[test].status) {
      results.absolut.status = false;
    }

    results.absolut.total++;

    if (options.verbose) {
      print("Args for [" + test + "]:");
      print(yaml.safeDump(args));
      print("Result: " + results.absolut[test].status);
    }
  }

  results.absolut.duration = time() - startTime;

  print("\n--------------------------------------------------------------------------------");
  print("relative config tests");
  print("--------------------------------------------------------------------------------");

  startTime = time();

  for (let i = 0; i < ts.length; i++) {
    const test = ts[i];
    print(CYAN + "checking '" + test + "'" + RESET);

    const args = {
      "configuration": fs.join(CONFIG_RELATIVE_DIR, test + ".conf"),
      "flatCommands": ["--check-configuration"]
    };

    const run = fs.join(BIN_DIR, test);

    results.relative[test] = executeAndWait(run, toArgv(args), options, test);

    if (!results.relative[test].status) {
      results.relative.status = false;
    }

    results.relative.total++;

    if (options.verbose) {
      print("Args for (relative) [" + test + "]:");
      print(yaml.safeDump(args));
      print("Result: " + results.relative[test].status);
    }
  }

  results.relative.duration = time() - startTime;

  print();

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: dfdb
////////////////////////////////////////////////////////////////////////////////

testFuncs.dfdb = function(options) {
  const dataDir = fs.getTempFile();
  const args = ["-c", "etc/relative/arango-dfdb.conf", dataDir];

  let results = {};

  results.dfdb = executeAndWait(ARANGOD_BIN, args, options, "dfdb");

  print();

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: dump
////////////////////////////////////////////////////////////////////////////////

testFuncs.dump = function(options) {
  let cluster;

  if (options.cluster) {
    cluster = "-cluster";
  } else {
    cluster = "";
  }

  print(CYAN + "dump tests..." + RESET);

  let instanceInfo = startInstance("tcp", options, {}, "dump");

  if (instanceInfo === false) {
    return {
      dump: {
        status: false,
        message: "failed to start server!"
      }
    };
  }

  print(CYAN + Date() + ": Setting up" + RESET);

  let results = {};
  results.setup = runInArangosh(options, instanceInfo,
    makePathUnix("js/server/tests/dump/dump-setup" + cluster + ".js"));

  if (checkInstanceAlive(instanceInfo, options) &&
    (results.setup.status === true)) {
    print(CYAN + Date() + ": Dump and Restore - dump" + RESET);

    results.dump = runArangoDumpRestore(options, instanceInfo, "dump",
      "UnitTestsDumpSrc");

    if (checkInstanceAlive(instanceInfo, options) &&
      (results.dump.status === true)) {
      print(CYAN + Date() + ": Dump and Restore - restore" + RESET);

      results.restore = runArangoDumpRestore(options, instanceInfo, "restore",
        "UnitTestsDumpDst");

      if (checkInstanceAlive(instanceInfo, options) &&
        (results.restore.status === true)) {
        print(CYAN + Date() + ": Dump and Restore - dump after restore" + RESET);

        results.test = runInArangosh(options, instanceInfo,
          makePathUnix("js/server/tests/dump/dump" + cluster + ".js"), {
            "server.database": "UnitTestsDumpDst"
          });

        if (checkInstanceAlive(instanceInfo, options) &&
          (results.test.status === true)) {
          print(CYAN + Date() + ": Dump and Restore - teardown" + RESET);

          results.tearDown = runInArangosh(options, instanceInfo,
            makePathUnix("js/server/tests/dump/dump-teardown" + cluster + ".js"));
        }
      }
    }
  }

  print(CYAN + "Shutting down..." + RESET);
  shutdownInstance(instanceInfo, options);
  print(CYAN + "done." + RESET);

  print();

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: dump_authentication
////////////////////////////////////////////////////////////////////////////////

testFuncs.dump_authentication = function(options) {
  if (options.cluster) {
    if (options.extremeVerbosity) {
      print(CYAN + "Skipped because of cluster." + RESET);
    }

    return {
      "dump_authentication": {
        "status": true,
        "message": "skipped because of cluster",
        "skipped": true
      }
    };
  }

  print(CYAN + "dump_authentication tests..." + RESET);

  const auth1 = {
    "server.authentication": "true"
  };

  const auth2 = {
    "server.authentication": "true"
  };

  let instanceInfo = startInstance("tcp", options, auth1, "dump_authentication");

  if (instanceInfo === false) {
    return {
      "dump_authentication": {
        status: false,
        message: "failed to start server!"
      }
    };
  }

  print(CYAN + Date() + ": Setting up" + RESET);

  let results = {};
  results.setup = runInArangosh(options, instanceInfo,
    makePathUnix("js/server/tests/dump/dump-authentication-setup.js"),
    auth2);

  if (checkInstanceAlive(instanceInfo, options) &&
    (results.setup.status === true)) {
    print(CYAN + Date() + ": Dump and Restore - dump" + RESET);

    let authOpts = {
      username: "foobaruser",
      password: "foobarpasswd"
    };

    _.defaults(authOpts, options);

    results.dump = runArangoDumpRestore(authOpts, instanceInfo, "dump",
      "UnitTestsDumpSrc");

    if (checkInstanceAlive(instanceInfo, options) &&
      (results.dump.status === true)) {
      print(CYAN + Date() + ": Dump and Restore - restore" + RESET);

      results.restore = runArangoDumpRestore(authOpts, instanceInfo, "restore",
        "UnitTestsDumpDst");

      if (checkInstanceAlive(instanceInfo, options) &&
        (results.restore.status === true)) {
        print(CYAN + Date() + ": Dump and Restore - dump after restore" + RESET);

        results.test = runInArangosh(authOpts, instanceInfo,
          makePathUnix("js/server/tests/dump/dump-authentication.js"), {
            "server.database": "UnitTestsDumpDst"
          });

        if (checkInstanceAlive(instanceInfo, options) &&
          (results.test.status === true)) {
          print(CYAN + Date() + ": Dump and Restore - teardown" + RESET);

          results.tearDown = runInArangosh(options, instanceInfo,
            makePathUnix("js/server/tests/dump/dump-teardown.js"), auth2);
        }
      }
    }
  }

  print(CYAN + "Shutting down..." + RESET);
  shutdownInstance(instanceInfo, options);
  print(CYAN + "done." + RESET);

  print();

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: foxx manager
////////////////////////////////////////////////////////////////////////////////

testFuncs.foxx_manager = function(options) {
  print("foxx_manager tests...");

  let instanceInfo = startInstance("tcp", options, {}, "foxx_manager");

  if (instanceInfo === false) {
    return {
      foxx_manager: {
        status: false,
        message: "failed to start server!"
      }
    };
  }

  let results = {};

  results.update = runArangoshCmd(options, instanceInfo, {
    "configuration": "etc/relative/foxx-manager.conf"
  }, ["update"]);

  if (results.update.status === true || options.force) {
    results.search = runArangoshCmd(options, instanceInfo, {
      "configuration": "etc/relative/foxx-manager.conf"
    }, ["search", "itzpapalotl"]);
  }

  print("Shutting down...");
  shutdownInstance(instanceInfo, options);
  print("done.");

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: http_replication
////////////////////////////////////////////////////////////////////////////////

testFuncs.http_replication = function(options) {
  var opts = {
    "replication": true
  };
  _.defaults(opts, options);

  return rubyTests(opts, false);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: http_server
////////////////////////////////////////////////////////////////////////////////

testFuncs.http_server = function(options) {
  return rubyTests(options, false);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: importing
////////////////////////////////////////////////////////////////////////////////

const impTodos = [{
  id: "json1",
  data: makePathUnix("js/common/test-data/import/import-1.json"),
  coll: "UnitTestsImportJson1",
  type: "json",
  create: undefined
}, {
  id: "json2",
  data: makePathUnix("js/common/test-data/import/import-2.json"),
  coll: "UnitTestsImportJson2",
  type: "json",
  create: undefined
}, {
  id: "json3",
  data: makePathUnix("js/common/test-data/import/import-3.json"),
  coll: "UnitTestsImportJson3",
  type: "json",
  create: undefined
}, {
  id: "json4",
  data: makePathUnix("js/common/test-data/import/import-4.json"),
  coll: "UnitTestsImportJson4",
  type: "json",
  create: undefined
}, {
  id: "json5",
  data: makePathUnix("js/common/test-data/import/import-5.json"),
  coll: "UnitTestsImportJson5",
  type: "json",
  create: undefined
}, {
  id: "csv1",
  data: makePathUnix("js/common/test-data/import/import-1.csv"),
  coll: "UnitTestsImportCsv1",
  type: "csv",
  create: "true"
}, {
  id: "csv2",
  data: makePathUnix("js/common/test-data/import/import-2.csv"),
  coll: "UnitTestsImportCsv2",
  type: "csv",
  create: "true"
}, {
  id: "csv3",
  data: makePathUnix("js/common/test-data/import/import-3.csv"),
  coll: "UnitTestsImportCsv3",
  type: "csv",
  create: "true"
}, {
  id: "csv4",
  data: makePathUnix("js/common/test-data/import/import-4.csv"),
  coll: "UnitTestsImportCsv4",
  type: "csv",
  create: "true",
  separator: ";",
  backslash: true
}, {
  id: "csv5",
  data: makePathUnix("js/common/test-data/import/import-5.csv"),
  coll: "UnitTestsImportCsv5",
  type: "csv",
  create: "true",
  separator: ";",
  backslash: true
}, {
  id: "tsv1",
  data: makePathUnix("js/common/test-data/import/import-1.tsv"),
  coll: "UnitTestsImportTsv1",
  type: "tsv",
  create: "true"
}, {
  id: "tsv2",
  data: makePathUnix("js/common/test-data/import/import-2.tsv"),
  coll: "UnitTestsImportTsv2",
  type: "tsv",
  create: "true"
}, {
  id: "edge",
  data: makePathUnix("js/common/test-data/import/import-edges.json"),
  coll: "UnitTestsImportEdge",
  type: "json",
  create: "false"
}];

testFuncs.importing = function(options) {
  if (options.cluster) {
    if (options.extremeVerbosity) {
      print("Skipped because of cluster.");
    }

    return {
      "importing": {
        "status": true,
        "message": "skipped because of cluster",
        "skipped": true
      }
    };
  }

  let instanceInfo = startInstance("tcp", options, {}, "importing");

  if (instanceInfo === false) {
    return {
      "importing": {
        status: false,
        message: "failed to start server!"
      }
    };
  }

  let result = {};

  try {
    result.setup = runInArangosh(options, instanceInfo,
      makePathUnix("js/server/tests/import/import-setup.js"));

    if (result.setup.status !== true) {
      throw new Error("cannot start import setup");
    }

    for (let i = 0; i < impTodos.length; i++) {
      const impTodo = impTodos[i];

      result[impTodo.id] = runArangoImp(options, instanceInfo, impTodo);

      if (result[impTodo.id].status !== true && !options.force) {
        throw new Error("cannot run import");
      }
    }

    result.check = runInArangosh(options, instanceInfo,
      makePathUnix("js/server/tests/import/import.js"));

    result.teardown = runInArangosh(options, instanceInfo,
      makePathUnix("js/server/tests/import/import-teardown.js"));

  } catch (banana) {
    print("An exceptions of the following form was caught:",
      yaml.safeDump(banana));
  }

  print("Shutting down...");
  shutdownInstance(instanceInfo, options);
  print("done.");

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: recovery
////////////////////////////////////////////////////////////////////////////////

function runArangodRecovery(instanceInfo, options, script, setup) {
  if (!instanceInfo.tmpDataDir) {
    let td = fs.join(fs.getTempFile(), "data");
    fs.makeDirectoryRecursive(td);

    instanceInfo.tmpDataDir = td;
  }

  if (!instanceInfo.recoveryArgs) {
    let args = makeArgsArangod(options);
    args["server.threads"] = 1;
    args["wal.reserve-logfiles"] = 1;
    args["database.directory"] = instanceInfo.tmpDataDir;

    instanceInfo.recoveryArgv = toArgv(args).concat(["--server.rest-server", "false"]);
  }

  let argv = instanceInfo.recoveryArgv;

  if (setup) {
    argv = argv.concat([
      "--log.level", "fatal",
      "--javascript.script-parameter", "setup"
    ]);
  } else {
    argv = argv.concat([
      "--log.level", "info",
      "--wal.ignore-logfile-errors", "true",
      "--javascript.script-parameter", "recovery"
    ]);
  }

  argv = argv.concat([
    "--javascript.script",
    fs.join(".", "js", "server", "tests", "recovery", script + ".js")
  ]);

  instanceInfo.pid = executeAndWait(ARANGOD_BIN, argv, options);
}

const recoveryTests = [
  "disk-full-logfile",
  "disk-full-logfile-data",
  "disk-full-datafile",
  "collection-drop-recreate",
  "create-with-temp",
  "create-with-temp-old",
  "create-collection-fail",
  "create-database-existing",
  "create-database-fail",
  "empty-datafiles",
  "flush-drop-database-and-fail",
  "drop-database-flush-and-fail",
  "create-databases",
  "recreate-databases",
  "drop-databases",
  "create-and-drop-databases",
  "drop-database-and-fail",
  "flush-drop-database-and-fail",
  "collection-rename-recreate",
  "collection-rename-recreate-flush",
  "collection-unload",
  "resume-recovery-multi-flush",
  "resume-recovery-simple",
  "resume-recovery-all",
  "resume-recovery-other",
  "resume-recovery",
  "foxx-directories",
  "collection-rename",
  "collection-properties",
  "empty-logfiles",
  "many-logs",
  "multiple-logs",
  "collection-recreate",
  "drop-indexes",
  "create-indexes",
  "create-collections",
  "recreate-collection",
  "drop-single-collection",
  "drop-collections",
  "collections-reuse",
  "collections-different-attributes",
  "indexes-hash",
  "indexes-rocksdb",
  "indexes-rocksdb-nosync",
  "indexes-sparse-hash",
  "indexes-skiplist",
  "indexes-sparse-skiplist",
  "indexes-geo",
  "edges",
  "indexes",
  "many-inserts",
  "many-updates",
  "wait-for-sync",
  "attributes",
  "no-journal",
  "write-throttling",
  "collector-oom",
  "transaction-no-abort",
  "transaction-no-commit",
  "transaction-just-committed",
  "multi-database-durability",
  "disk-full-no-collection-journal",
  "no-shutdown-info-with-flush",
  "no-shutdown-info-no-flush",
  "no-shutdown-info-multiple-logs",
  "insert-update-remove",
  "insert-update-remove-distance",
  "big-transaction-durability",
  "transaction-durability",
  "transaction-durability-multiple",
  "corrupt-wal-marker-multiple",
  "corrupt-wal-marker-single"
];

testFuncs.recovery = function(options) {
  let results = {};
  let status = true;

  for (let i = 0; i < recoveryTests.length; ++i) {
    let test = recoveryTests[i];

    if (options.test === undefined || options.test === test) {
      let instanceInfo = {};

      runArangodRecovery(instanceInfo, options, test, true);

      runArangodRecovery(instanceInfo, options, test, false);

      if (instanceInfo.tmpDataDir) {
        fs.removeDirectoryRecursive(instanceInfo.tmpDataDir, true);
      }

      results[test] = instanceInfo.pid;

      if (!results[test].status) {
        status = false;
      }
    } else {
      results[test] = {
        status: true,
        skipped: true
      };
    }
  }

  results.status = status;

  return {
    recovery: results
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: replication_ongoing
////////////////////////////////////////////////////////////////////////////////

testFuncs.replication_ongoing = function(options) {
  const mr = makeResults('replication');

  let master = startInstance("tcp", options, {}, "master_ongoing");

  if (master === false) {
    return mr(false, "failed to start master!");
  }

  let slave = startInstance("tcp", options, {}, "slave_ongoing");

  if (slave === false) {
    shutdownInstance(master, options);
    return mr(false, "failed to start slave!");
  }

  let res = runArangoshCmd(options, master, {}, [
    "--javascript.unit-tests",
    "./js/server/tests/replication/replication-ongoing.js",
    slave.endpoint
  ]);

  let results;

  if (!res.status) {
    results = mr(false, "replication-ongoing.js failed");
  } else {
    results = mr(true);
  }

  print("Shutting down...");
  shutdownInstance(slave, options);
  shutdownInstance(master, options);
  print("done.");

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: replication_static
////////////////////////////////////////////////////////////////////////////////

testFuncs.replication_static = function(options) {
  const mr = makeResults('replication');

  let master = startInstance("tcp", options, {
    "server.authentication": "true"
  }, "master_static");

  if (master === false) {
    return mr(false, "failed to start master!");
  }

  let slave = startInstance("tcp", options, {}, "slave_static");

  if (slave === false) {
    shutdownInstance(master, options);
    return mr(false, "failed to start slave!");
  }

  let res = runArangoshCmd(options, master, {}, [
    "--javascript.execute-string",
    "var users = require('@arangodb/users'); " +
    "users.save('replicator-user', 'replicator-password', true); " +
    "users.grantDatabase('replicator-user', '_system'); " +
    "users.reload();"
  ]);

  let results;

  if (res.status) {
    res = runArangoshCmd(options, master, {}, [
      "--javascript.unit-tests",
      "./js/server/tests/replication/replication-static.js",
      slave.endpoint
    ]);

    if (res.status) {
      results = mr(true);
    } else {
      results = mr(false, "replication-static.js failed");
    }
  } else {
    results = mr(false, "cannot create users");
  }

  print("Shutting down...");
  shutdownInstance(slave, options);
  shutdownInstance(master, options);
  print("done.");

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: replication_sync
////////////////////////////////////////////////////////////////////////////////

testFuncs.replication_sync = function(options) {
  const mr = makeResults('replication');
  let master = startInstance("tcp", options, {}, "master_sync");

  if (master === false) {
    return mr(false, "failed to start master!");
  }

  let slave = startInstance("tcp", options, {}, "slave_sync");

  if (slave === false) {
    shutdownInstance(master, options);
    return mr(false, "failed to start slave!");
  }

  let res = runArangoshCmd(options, master, {}, [
    "--javascript.execute-string",
    "var users = require('@arangodb/users'); " +
    "users.save('replicator-user', 'replicator-password', true); " +
    "users.reload();"
  ]);

  let results;

  if (res.status) {
    res = runArangoshCmd(options, master, {}, [
      "--javascript.unit-tests",
      "./js/server/tests/replication/replication-sync.js",
      slave.endpoint
    ]);

    if (res.status) {
      results = mr(true);
    } else {
      results = mr(false, "replication-sync.js failed");
    }
  } else {
    results = mr(false, "cannot create users");
  }

  print("Shutting down...");
  shutdownInstance(slave, options);
  shutdownInstance(master, options);
  print("done.");

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: resilience
////////////////////////////////////////////////////////////////////////////////

testFuncs.resilience = function(options) {
  findTests();
  options.propagateInstanceInfo = true;
  options.cluster = true;
  if (options.clusterNodes < 5) {
    options.clusterNodes = 5;
  }
  return performTests(options, testsCases.resilience, 'resilience');
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: shell_replication
////////////////////////////////////////////////////////////////////////////////

testFuncs.shell_replication = function(options) {
  findTests();

  var opts = {
    "replication": true
  };
  _.defaults(opts, options);

  return performTests(opts, testsCases.replication, 'shell_replication');
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: shell_client
////////////////////////////////////////////////////////////////////////////////

testFuncs.shell_client = function(options) {
  findTests();

  let instanceInfo = startInstance("tcp", options, {}, "shell_client");

  if (instanceInfo === false) {
    return {
      shell_client: {
        status: false,
        message: "failed to start server!"
      }
    };
  }

  let results = {};
  let filtered = {};
  let continueTesting = true;

  for (let i = 0; i < testsCases.client.length; i++) {
    const te = testsCases.client[i];

    if (filterTestcaseByOptions(te, options, filtered)) {
      if (!continueTesting) {
        print("Skipping, " + te + " server is gone.");

        results[te] = {
          status: false,
          message: instanceInfo.exitStatus
        };

        instanceInfo.exitStatus = "server is gone.";

        break;
      }

      print("\narangosh: Trying", te, "...");

      const reply = runInArangosh(options, instanceInfo, te);
      results[te] = reply;

      if (reply.status !== true) {
        options.cleanup = false;

        if (!options.force) {
          break;
        }
      }

      continueTesting = checkInstanceAlive(instanceInfo, options);
    } else {
      if (options.extremeVerbosity) {
        print("Skipped " + te + " because of " + filtered.filter);
      }
    }
  }

  print("Shutting down...");
  shutdownInstance(instanceInfo, options);
  print("done.");

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: shell_http
////////////////////////////////////////////////////////////////////////////////

testFuncs.server_http = function(options) {
  findTests();

  return performTests(options, testsCases.server_http, 'server_http');
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: shell_server
////////////////////////////////////////////////////////////////////////////////

testFuncs.shell_server = function(options) {
  findTests();
  options.propagateInstanceInfo = true;

  return performTests(options, testsCases.server, 'shell_server');
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: shell_server_aql
////////////////////////////////////////////////////////////////////////////////

testFuncs.shell_server_aql = function(options) {
  findTests();

  let cases;
  let name;

  if (!options.skipAql) {
    if (options.skipRanges) {
      cases = testsCases.server_aql;
      name = 'shell_server_aql_skipranges';
    } else {
      cases = testsCases.server_aql.concat(testsCases.server_aql_extended);
      name = 'shell_server_aql';
    }

    cases = splitBuckets(options, cases);

    return performTests(options, cases, name);
  }

  return {
    shell_server_aql: {
      status: true,
      skipped: true
    }
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: shell_server_only
////////////////////////////////////////////////////////////////////////////////

testFuncs.shell_server_only = function(options) {
  findTests();

  return performTests(options, testsCases.server_only, 'shell_server_only');
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: shell_server_perf
////////////////////////////////////////////////////////////////////////////////

testFuncs.shell_server_perf = function(options) {
  findTests();

  return performTests(options, testsCases.server_aql_performance,
    'shell_server_perf');
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: single_client
////////////////////////////////////////////////////////////////////////////////

function single_usage(testsuite, list) {
  print("single_" + testsuite + ": No test specified!\n Available tests:");
  let filelist = "";

  for (let fileNo in list) {
    if (/\.js$/.test(list[fileNo])) {
      filelist += " " + list[fileNo];
    }
  }

  print(filelist);
  print("usage: single_" + testsuite + " '{\"test\":\"<testfilename>\"}'");
  print(" where <testfilename> is one from the list above.");

  return {
    usage: {
      status: false,
      message: "No test specified!"
    }
  };
}

testFuncs.single_client = function(options) {
  options.writeXmlReport = false;

  if (options.test !== undefined) {
    let instanceInfo = startInstance("tcp", options, {}, "single_client");

    if (instanceInfo === false) {
      return {
        single_client: {
          status: false,
          message: "failed to start server!"
        }
      };
    }

    const te = options.test;

    print("\n" + Date() + " arangosh: Trying ", te, "...");

    let result = {};
    result[te] = runInArangosh(options, instanceInfo, te);

    print("Shutting down...");

    if (result[te].status === false) {
      options.cleanup = false;
    }

    shutdownInstance(instanceInfo, options);

    print("done.");

    return result;
  } else {
    findTests();
    return single_usage("client", testsCases.client);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: single_server
////////////////////////////////////////////////////////////////////////////////

testFuncs.single_server = function(options) {
  options.writeXmlReport = false;

  if (options.test === undefined) {
    findTests();
    return single_usage("server", testsCases.server);
  }

  let instanceInfo = startInstance("tcp", options, {}, "single_server");

  if (instanceInfo === false) {
    return {
      single_server: {
        status: false,
        message: "failed to start server!"
      }
    };
  }

  const te = options.test;

  print("\n" + Date() + " arangod: Trying", te, "...");

  let result = {};
  let reply = runThere(options, instanceInfo, makePathGeneric(te));

  if (reply.hasOwnProperty('status')) {
    result[te] = reply;

    if (result[te].status === false) {
      options.cleanup = false;
    }
  }

  print("Shutting down...");

  if (result[te].status === false) {
    options.cleanup = false;
  }

  shutdownInstance(instanceInfo, options);

  print("done.");

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: ssl_server
////////////////////////////////////////////////////////////////////////////////

testFuncs.ssl_server = function(options) {
  if (options.skipSsl) {
    return {
      ssl_server: {
        status: true,
        skipped: true
      }
    };
  }

  return rubyTests(options, true);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: upgrade
////////////////////////////////////////////////////////////////////////////////

testFuncs.upgrade = function(options) {
  if (options.cluster) {
    return {
      "upgrade": {
        "status": true,
        "message": "skipped because of cluster",
        "skipped": true
      }
    };
  }

  let result = {
    upgrade: {
      status: true,
      total: 1
    }
  };

  const tmpDataDir = fs.getTempFile();
  fs.makeDirectoryRecursive(tmpDataDir);

  const appDir = fs.join(tmpDataDir, "app");
  const port = findFreePort();

  let args = makeArgsArangod(options, appDir);
  args["server.endpoint"] = "tcp://127.0.0.1:" + port;
  args["database.directory"] = fs.join(tmpDataDir, "data");
  args["database.auto-upgrade"] = true;

  fs.makeDirectoryRecursive(fs.join(tmpDataDir, "data"));

  const argv = toArgv(args);

  result.upgrade.first = executeAndWait(ARANGOD_BIN, argv, options, "upgrade");

  if (result.upgrade.first !== 0 && !options.force) {
    print("not removing " + tmpDataDir);
    return result;
  }

  ++result.upgrade.total;

  result.upgrade.second = executeAndWait(ARANGOD_BIN, argv, options, "upgrade");

  cleanupDirectories.push(tmpDataDir);

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief STRESS TEST: stress_crud
////////////////////////////////////////////////////////////////////////////////

testFuncs.stress_crud = function(options) {
  const duration = options.duration;
  const concurrency = options.concurrency;

  const command = `
    const stressCrud = require("./js/server/tests/stress/crud");

    stressCrud.createDeleteUpdateParallel({
      concurrency: ${concurrency},
      duration: ${duration},
      gnuplot: true,
      pauseFor: 60
    });
`;

  return runStressTest(options, command, "stress_crud");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief STRESS TEST: stress_killing
////////////////////////////////////////////////////////////////////////////////

testFuncs.stress_killing = function(options) {
  const duration = options.duration;

  let opts = {
    concurrency: 4
  };

  _.defaults(opts, options);

  const command = `
    const stressCrud = require("./js/server/tests/stress/killingQueries");

    stressCrud.killingParallel({
      duration: ${duration},
      gnuplot: true
    });
`;

  return runStressTest(opts, command, "stress_killing");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief STRESS TEST: stress_locks
////////////////////////////////////////////////////////////////////////////////


testFuncs.stress_locks = function(options) {
  const duration = options.duration;
  const concurrency = options.concurrency;

  const command = `
    const deadlock = require("./js/server/tests/stress/deadlock");

    deadlock.lockCycleParallel({
      concurrency: ${concurrency},
      duration: ${duration},
      gnuplot: true
    });
`;

  return runStressTest(options, command, "stress_lock");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief agency tests
////////////////////////////////////////////////////////////////////////////////

testFuncs.agency = function(options) {
  findTests();

  var saveAgency = options.agency;
  var saveCluster = options.cluster;

  options.agency = true;
  options.cluster = false;

  let instanceInfo = startInstance("tcp", options, {}, "agency");

  if (instanceInfo === false) {
    return {
      shell_client: {
        status: false,
        message: "failed to start agency!"
      }
    };
  }

  let results = {};
  let filtered = {};
  let continueTesting = true;

  for (let i = 0; i < testsCases.agency.length; i++) {
    let te = testsCases.agency[i];

    if (filterTestcaseByOptions(te, options, filtered)) {
      if (!continueTesting) {
        print("Skipping, " + te + " server is gone.");

        results[te] = {
          status: false,
          message: instanceInfo.exitStatus
        };

        instanceInfo.exitStatus = "server is gone.";

        break;
      }

      let agencyServers = instanceInfo.arangods.map(arangod => {
        return arangod.url;
      });


      print("\narangosh: Trying", te, "...");

      const reply = runInArangosh(options, instanceInfo, te, {
        "flatCommands": agencyServers.join(' ')
      });
      results[te] = reply;

      if (reply.status !== true) {
        options.cleanup = false;

        if (!options.force) {
          break;
        }
      }

      continueTesting = checkInstanceAlive(instanceInfo, options);
    } else {
      if (options.extremeVerbosity) {
        print("Skipped " + te + " because of " + filtered.filter);
      }
    }
  }

  print("Shutting down...");
  shutdownInstance(instanceInfo, options);
  print("done.");

  options.agency = saveAgency;
  options.cluster = saveCluster;

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief pretty prints the result
////////////////////////////////////////////////////////////////////////////////

function testCaseMessage(test) {
  if (typeof test.message === "object" && test.message.hasOwnProperty('body')) {
    return test.message.body;
  } else {
    return test.message;
  }
}

function unitTestPrettyPrintResults(r) {
  print(BLUE + "================================================================================");
  print("TEST RESULTS");
  print("================================================================================\n" + RESET);

  let failedSuite = 0;
  let failedTests = 0;

  try {
    /*jshint forin: false */
    for (let testrunName in r) {
      if (skipInternalMember(r, testrunName)) {
        continue;
      }

      let testrun = r[testrunName];

      print("* Test '" + testrunName + "'");

      let successCases = {};
      let failedCases = {};
      let isSuccess = true;

      for (let testName in testrun) {
        if (skipInternalMember(testrun, testName)) {
          continue;
        }

        let test = testrun[testName];

        if (test.status) {
          successCases[testName] = test;
        } else {
          isSuccess = false;
          ++failedSuite;

          if (test.hasOwnProperty('message')) {
            ++failedTests;
            failedCases[testName] = {
              test: testCaseMessage(test)
            };
          } else {
            let fails = failedCases[testName] = {};

            for (let oneName in test) {
              if (skipInternalMember(test, oneName)) {
                continue;
              }

              let oneTest = test[oneName];

              if (!oneTest.status) {
                ++failedTests;
                fails[oneName] = testCaseMessage(oneTest);
              }
            }
          }
        }
      }

      for (let name in successCases) {
        if (!successCases.hasOwnProperty(name)) {
          continue;
        }

        let details = successCases[name];

        if (details.skipped) {
          print(YELLOW + "    [SKIPPED] " + name + RESET);
        } else {
          print(GREEN + "    [SUCCESS] " + name + RESET);
        }
      }

      for (let name in failedCases) {
        if (!failedCases.hasOwnProperty(name)) {
          continue;
        }

        print(RED + "    [FAILED]  " + name + RESET);

        let details = failedCases[name];

        for (let one in details) {
          if (!details.hasOwnProperty(one)) {
            continue;
          }

          print(RED + "      '" + one + "' failed: " + details[one] + RESET);
        }
      }
    }
    /*jshint forin: true */

    let color = (!r.crashed && r.status === true) ? GREEN : RED;
    let crashText = "";
    if (r.crashed === true) {
      crashText = RED + " BUT! - We had at least one unclean shutdown or crash during the testrun." + RESET;
    }
    print("\n" + color + "* Overall state: " + ((r.status === true) ? "Success" : "Fail") + RESET + crashText);

    if (r.status !== true) {
      print(color + "   Suites failed: " + failedSuite + " Tests Failed: " + failedTests + RESET);
    }

  } catch (x) {
    print("exception caught while pretty printing result: ");
    print(x.message);
    print(JSON.stringify(r));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print usage information
////////////////////////////////////////////////////////////////////////////////

function printUsage() {
  print();
  print("Usage: UnitTest([which, ...], options)");
  print();
  print('       where "which" is one of:\n');

  for (let i in testFuncs) {
    if (testFuncs.hasOwnProperty(i)) {
      let oneFunctionDocumentation;

      if (functionsDocumentation.hasOwnProperty(i)) {
        oneFunctionDocumentation = ' - ' + functionsDocumentation[i];
      } else {
        oneFunctionDocumentation = '';
      }

      let checkAll;

      if (allTests.indexOf(i) !== -1) {
        checkAll = '[x]';
      } else {
        checkAll = '   ';
      }

      print('    ' + checkAll + ' ' + i + ' ' + oneFunctionDocumentation);
    }
  }

  for (let i in optionsDocumentation) {
    if (optionsDocumentation.hasOwnProperty(i)) {
      print(optionsDocumentation[i]);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief framework to perform unittests
///
/// This function gets one or two arguments, the first describes which tests
/// to perform and the second is an options object. For `which` the following
/// values are allowed:
///  Empty will give you a complete list.
////////////////////////////////////////////////////////////////////////////////

function unitTest(cases, options) {
  if (typeof options !== "object") {
    options = {};
  }

  _.defaults(options, optionsDefaults);

  if (cases === undefined || cases.length === 0) {
    printUsage();

    print('FATAL: "which" is undefined\n');

    return {
      status: false
    };
  }

  let builddir = options.build;

  if (builddir === "") {
    if (fs.exists("build") && fs.exists(fs.join("build", "bin"))) {
      builddir = "build";
    } else if (fs.exists("bin")) {
      builddir = ".";
    } else {
      print('FATAL: cannot find binaries, use "--build"\n');

      return {
        status: false
      };
    }
  }

  BIN_DIR = fs.join(TOP_DIR, builddir, "bin");
  UNITTESTS_DIR = fs.join(TOP_DIR, fs.join(builddir, "tests"));

  if (options.buildType !== "") {
    BIN_DIR = fs.join(BIN_DIR, options.buildType);
    UNITTESTS_DIR = fs.join(UNITTESTS_DIR, options.buildType);
  }

  CONFIG_DIR = fs.join(TOP_DIR, builddir, "etc", "arangodb3");
  ARANGOBENCH_BIN = fs.join(BIN_DIR, "arangobench");
  ARANGODUMP_BIN = fs.join(BIN_DIR, "arangodump");
  ARANGOD_BIN = fs.join(BIN_DIR, "arangod");
  ARANGOIMP_BIN = fs.join(BIN_DIR, "arangoimp");
  ARANGORESTORE_BIN = fs.join(BIN_DIR, "arangorestore");
  ARANGOSH_BIN = fs.join(BIN_DIR, "arangosh");
  CONFIG_RELATIVE_DIR = fs.join(TOP_DIR, "etc", "relative");
  JS_DIR = fs.join(TOP_DIR, "js");
  LOGS_DIR = fs.join(TOP_DIR, "logs");
  PEM_FILE = fs.join(TOP_DIR, "UnitTests", "server.pem");

  let checkFiles = [
    ARANGOBENCH_BIN,
    ARANGODUMP_BIN,
    ARANGOD_BIN,
    ARANGOIMP_BIN,
    ARANGORESTORE_BIN,
    ARANGOSH_BIN];
  for (let b = 0; b < checkFiles.length; ++b) {
    if (! fs.isFile(checkFiles[b]) && ! fs.isFile(checkFiles[b]+ ".exe" )) {
      throw "unable to locate " + checkFiles[b];
    }
  }
  
  const jsonReply = options.jsonReply;
  delete options.jsonReply;

  // tests to run
  let caselist = [];

  for (let n = 0; n < cases.length; ++n) {
    let splitted = cases[n].split(/[,;\.|]/);

    for (let m = 0; m < splitted.length; ++m) {
      let which = splitted[m];

      if (which === "all") {
        caselist = caselist.concat(allTests);
      } else if (testFuncs.hasOwnProperty(which)) {
        caselist.push(which);
      } else {
        let line = "Unknown test '" + which + "'\nKnown tests are: ";
        let sep = "";

        Object.keys(testFuncs).map(function(key) {
          line += sep + key;
          sep = ", ";
        });

        print(line);

        return {
          status: false
        };
      }
    }
  }

  let globalStatus = true;
  let results = {};

  // running all tests
  for (let n = 0; n < caselist.length; ++n) {
    const currentTest = caselist[n];

    print(BLUE + "================================================================================");
    print("Executing test", currentTest);
    print("================================================================================\n" + RESET);

    if (options.verbose) {
      print(CYAN + "with options:", options, RESET);
    }

    let result = testFuncs[currentTest](options);
    results[currentTest] = result;

    let status = true;

    for (let i in result) {
      if (result.hasOwnProperty(i)) {
        if (result[i].status !== true) {
          status = false;
        }
      }
    }

    result.status = status;

    if (!status) {
      globalStatus = false;
    }
  }

  results.status = globalStatus;
  results.crashed = serverCrashed;

  if (globalStatus && !serverCrashed) {
    cleanupDBDirectories(options);
  } else {
    print("not cleaning up as some tests weren't successful:\n" +
      yaml.safeDump(cleanupDirectories));
  }

  try {
    yaml.safeDump(JSON.parse(JSON.stringify(results)));
  } catch (err) {
    print(RED + "cannot dump results: " + String(err) + RESET);
    print(RED + require("internal").inspect(results) + RESET);
  }

  if (jsonReply === true) {
    return results;
  } else {
    return globalStatus;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief exports
////////////////////////////////////////////////////////////////////////////////

exports.unitTest = unitTest;

exports.internalMembers = internalMembers;
exports.testFuncs = testFuncs;
exports.unitTestPrettyPrintResults = unitTestPrettyPrintResults;

// TODO write test for 2.6-style queues
// testFuncs.queue_legacy = function (options) {
//   if (options.skipFoxxQueues) {
//     print("skipping test of legacy queue job types");
//     return {};
//   }
//   var startTime;
//   var queueAppMountPath = '/test-queue-legacy';
//   print("Testing legacy queue job types");
//   var instanceInfo = startInstance("tcp", options, [], "queue_legacy");
//   if (instanceInfo === false) {
//     return {status: false, message: "failed to start server!"};
//   }
//   var data = {
//     naive: {_key: 'potato', hello: 'world'},
//     forced: {_key: 'tomato', hello: 'world'},
//     plain: {_key: 'banana', hello: 'world'}
//   };
//   var results = {};
//   results.install = runArangoshCmd(options, instanceInfo, {
//     "configuration": "etc/relative/foxx-manager.conf"
//   }, [
//     "install",
//     "js/common/test-data/apps/queue-legacy-test",
//     queueAppMountPath
//   ]);

//   print("Restarting without foxx-queues-warmup-exports...");
//   shutdownInstance(instanceInfo, options);
//   instanceInfo = startInstance("tcp", options, {
//     "server.foxx-queues-warmup-exports": "false"
//   }, "queue_legacy", instanceInfo.flatTmpDataDir);
//   if (instanceInfo === false) {
//     return {status: false, message: "failed to restart server!"};
//   }
//   print("done.");

//   var res, body;
//   startTime = time();
//   try {
//     res = download(
//       instanceInfo.url + queueAppMountPath + '/',
//       JSON.stringify(data.naive),
//       {method: 'POST'}
//     );
//     body = JSON.parse(res.body);
//     results.naive = {status: body.success === false, message: JSON.stringify({body: res.body, code: res.code})};
//   } catch (e) {
//     results.naive = {status: true, message: JSON.stringify({body: res.body, code: res.code})};
//   }
//   results.naive.duration = time() - startTime;

//   startTime = time();
//   try {
//     res = download(
//       instanceInfo.url + queueAppMountPath + '/?allowUnknown=true',
//       JSON.stringify(data.forced),
//       {method: 'POST'}
//     );
//     body = JSON.parse(res.body);
//     results.forced = (
//       body.success
//       ? {status: true}
//       : {status: false, message: body.error, stacktrace: body.stacktrace}
//      );
//   } catch (e) {
//     results.forced = {status: false, message: JSON.stringify({body: res.body, code: res.code})};
//   }
//   results.forced.duration = time() - startTime;

//   print("Restarting with foxx-queues-warmup-exports...");
//   shutdownInstance(instanceInfo, options);
//   instanceInfo = startInstance("tcp", options, {
//     "server.foxx-queues-warmup-exports": "true"
//   }, "queue_legacy", instanceInfo.flatTmpDataDir);
//   if (instanceInfo === false) {
//     return {status: false, message: "failed to restart server!"};
//   }
//   print("done.");

//   startTime = time();
//   try {
//     res = download(instanceInfo.url + queueAppMountPath + '/', JSON.stringify(data.plain), {method: 'POST'});
//     body = JSON.parse(res.body);
//     results.plain = (
//       body.success
//       ? {status: true}
//       : {status: false, message: JSON.stringify({body: res.body, code: res.code})}
//     );
//   } catch (e) {
//     results.plain = {status: false, message: JSON.stringify({body: res.body, code: res.code})};
//   }
//   results.plain.duration = time() - startTime;

//   startTime = time();
//   try {
//     for (var i = 0; i < 60; i++) {
//       wait(1);
//       res = download(instanceInfo.url + queueAppMountPath + '/');
//       body = JSON.parse(res.body);
//       if (body.length === 2) {
//         break;
//       }
//     }
//     results.final = (
//       body.length === 2 && body[0]._key === data.forced._key && body[1]._key === data.plain._key
//       ? {status: true}
//       : {status: false, message: JSON.stringify({body: res.body, code: res.code})}
//     );
//   } catch (e) {
//     results.final = {status: false, message: JSON.stringify({body: res.body, code: res.code})};
//   }
//   results.final.duration = time() - startTime;

//   results.uninstall = runArangoshCmd(options, instanceInfo, {
//     "configuration": "etc/relative/foxx-manager.conf"
//   }, [
//     "uninstall",
//     queueAppMountPath
//   ]);
//   print("Shutting down...");
//   shutdownInstance(instanceInfo, options);
//   print("done.");
//   if ((!options.skipLogAnalysis) &&
//       instanceInfo.hasOwnProperty('importantLogLines') &&
//       Object.keys(instanceInfo.importantLogLines).length > 0) {
//     print("Found messages in the server logs: \n" + yaml.safeDump(instanceInfo.importantLogLines));
//   }
//   return results;
// };
