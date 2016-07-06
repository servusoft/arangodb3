/*jshint unused: false */
/*global $, window, navigator, _, arangoHelper*/
(function() {
  "use strict";

  var disableVersionCheck = function() {
    $.ajax({
      type: "POST",
      url: arangoHelper.databaseUrl("/_admin/aardvark/disableVersionCheck")
    });
  };

  
  var isVersionCheckEnabled = function(cb) {
    $.ajax({
      type: "GET",
      url: arangoHelper.databaseUrl("/_admin/aardvark/shouldCheckVersion"),
      success: function(data) {
        if (data === true) {
          cb();
        }
      }
    });
  };

  var showInterface = function(currentVersion, json) {
    var buttons = [];
    /*buttons.push(window.modalView.createNotificationButton("Don't ask again", function() {
      disableVersionCheck();
      window.modalView.hide();
    }));*/
    buttons.push(window.modalView.createSuccessButton("Download Page", function() {
      window.open('https://www.arangodb.com/download','_blank');
      window.modalView.hide();
    }));
    var infos = [];
    var cEntry = window.modalView.createReadOnlyEntry.bind(window.modalView);
    infos.push(cEntry("current", "Current", currentVersion.toString()));
    if (json.major) {
      infos.push(cEntry("major", "Major", json.major.version));
    }
    if (json.minor) {
      infos.push(cEntry("minor", "Minor", json.minor.version));
    }
    if (json.bugfix) {
      infos.push(cEntry("bugfix", "Bugfix", json.bugfix.version));
    }
    window.modalView.show(
      "modalTable.ejs", "New Version Available", buttons, infos
    );
  };

  var getInformation = function() {
    var nVer = navigator.appVersion;
    var nAgt = navigator.userAgent;
    var browserName  = navigator.appName;
    var fullVersion  = '' + parseFloat(navigator.appVersion); 
    var majorVersion = parseInt(navigator.appVersion,10);
    var nameOffset,verOffset,ix;

    if ((verOffset=nAgt.indexOf("Opera")) !== -1) {
      browserName = "Opera";
      fullVersion = nAgt.substring(verOffset + 6);
      if ((verOffset=nAgt.indexOf("Version")) !== -1) { 
        fullVersion = nAgt.substring(verOffset + 8);
      }
    }
    else if ((verOffset=nAgt.indexOf("MSIE")) !== -1) {
      browserName = "Microsoft Internet Explorer";
      fullVersion = nAgt.substring(verOffset + 5);
    }
    else if ((verOffset=nAgt.indexOf("Chrome")) !== -1) {
      browserName = "Chrome";
      fullVersion = nAgt.substring(verOffset + 7);
    }
    else if ((verOffset=nAgt.indexOf("Safari")) !== -1) {
      browserName = "Safari";
      fullVersion = nAgt.substring(verOffset + 7);
      if ((verOffset=nAgt.indexOf("Version")) !== -1) {
        fullVersion = nAgt.substring(verOffset + 8);
      }
    }
    else if ((verOffset=nAgt.indexOf("Firefox")) !== -1) {
      browserName = "Firefox";
      fullVersion = nAgt.substring(verOffset + 8);
    }
    else if ((nameOffset=nAgt.lastIndexOf(' ') + 1) < (verOffset=nAgt.lastIndexOf('/'))) {
      browserName = nAgt.substring(nameOffset, verOffset);
      fullVersion = nAgt.substring(verOffset + 1);
      if (browserName.toLowerCase() === browserName.toUpperCase()) {
        browserName = navigator.appName;
      }
    }
    if ((ix=fullVersion.indexOf(";")) !== -1) {
      fullVersion=fullVersion.substring(0, ix);
    }
    if ((ix=fullVersion.indexOf(" ")) !== -1) {
      fullVersion=fullVersion.substring(0, ix);
    }

    majorVersion = parseInt('' + fullVersion, 10);

    if (isNaN(majorVersion)) {
      fullVersion  = '' + parseFloat(navigator.appVersion); 
      majorVersion = parseInt(navigator.appVersion, 10);
    }

    return {
      browserName: browserName,
      fullVersion: fullVersion,
      majorVersion: majorVersion,
      appName: navigator.appName,
      userAgent: navigator.userAgent
    };
  };

  var getOS = function() {
    var OSName = "Unknown OS";
    if (navigator.appVersion.indexOf("Win") !== -1) {
      OSName="Windows";
    }
    if (navigator.appVersion.indexOf("Mac") !== -1) {
      OSName="MacOS";
    }
    if (navigator.appVersion.indexOf("X11") !== -1) {
      OSName="UNIX";
    }
    if (navigator.appVersion.indexOf("Linux") !== -1) {
      OSName="Linux";
    }

    return OSName;
  };

  window.checkVersion = function() {
    // this checks for version updates
    $.ajax({
      type: "GET",
      cache: false,
      url: arangoHelper.databaseUrl("/_api/version"),
      contentType: "application/json",
      processData: false,
      async: true,
      success: function(data) {
        var currentVersion =
        window.versionHelper.fromString(data.version);
        $('.navbar #currentVersion').text(" " + data.version.substr(0,3));

        window.parseVersions = function (json) {
          if (_.isEmpty(json)) {
            $('#currentVersion').addClass('up-to-date');
            return; // no new version.
          }
          $('#currentVersion').addClass('out-of-date');
          $('#currentVersion').click(function() {
            showInterface(currentVersion, json);
          });
          //isVersionCheckEnabled(showInterface.bind(window, currentVersion, json));
        };

        //TODO: append to url below
        /*
        var browserInfo = getInformation();
        console.log(browserInfo);
        console.log(encodeURIComponent(JSON.stringify(browserInfo)));

        var osInfo = getOS();
        console.log(osInfo);
        */

        $.ajax({
          type: "GET",
          async: true,
          crossDomain: true,
          timeout: 3000,
          dataType: "jsonp",
          url: "https://www.arangodb.com/repositories/versions.php" +
          "?jsonp=parseVersions&version=" + encodeURIComponent(currentVersion.toString())
        });
      }
    });
  };
}());
