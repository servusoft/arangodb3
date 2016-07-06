/*jshint browser: true */
/*jshint unused: false */
/*global frontendConfig, arangoHelper, Backbone, templateEngine, $, window*/
(function () {
  "use strict";

  window.UserBarView = Backbone.View.extend({

    events: {
      "change #userBarSelect"         : "navigateBySelect",
      "click .tab"                    : "navigateByTab",
      "mouseenter .dropdown"          : "showDropdown",
      "mouseleave .dropdown"          : "hideDropdown",
      "click #userLogoutIcon"         : "userLogout",
      "click #userLogout"             : "userLogout"
    },

    initialize: function (options) {
      this.userCollection = options.userCollection;
      this.userCollection.fetch({
        cache: false,
        async: true
      });
      this.userCollection.bind("change:extra", this.render.bind(this));
    },

    template: templateEngine.createTemplate("userBarView.ejs"),

    navigateBySelect: function () {
      var navigateTo = $("#arangoCollectionSelect").find("option:selected").val();
      window.App.navigate(navigateTo, {trigger: true});
    },

    navigateByTab: function (e) {
      var tab = e.target || e.srcElement;
      tab = $(tab).closest("a");
      var navigateTo = tab.attr("id");
      if (navigateTo === "user") {
        $("#user_dropdown").slideToggle(200);
        e.preventDefault();
        return;
      }
      window.App.navigate(navigateTo, {trigger: true});
      e.preventDefault();
    },

    toggleUserMenu: function() {
      $('#userBar .subBarDropdown').toggle();
    },

    showDropdown: function () {
      $("#user_dropdown").fadeIn(1);
    },

    hideDropdown: function () {
      $("#user_dropdown").fadeOut(1);
    },

    render: function () {

      if (frontendConfig.authenticationEnabled === false) {
        return;
      }

      var self = this;

      var callback = function(error, username) {
        if (error) {
          arangoHelper.arangoErro("User", "Could not fetch user.");
        }
        else {
          var img = null,
            name = null,
            active = false,
            currentUser = null;
          if (username !== false) {
            currentUser = this.userCollection.findWhere({user: username});
            currentUser.set({loggedIn : true});
            name = currentUser.get("extra").name;
            img = currentUser.get("extra").img;
            active = currentUser.get("active");
            if (! img) {
              img = "img/default_user.png";
            } 
            else {
              img = "https://s.gravatar.com/avatar/" + img + "?s=80";
            }
            if (! name) {
              name = "";
            }

            this.$el = $("#userBar");
            this.$el.html(this.template.render({
              img : img,
              name : name,
              username : username,
              active : active
            }));

            this.delegateEvents();
            return this.$el;
          }
        }
      }.bind(this);

      $('#userBar').on('click', function() {
        self.toggleUserMenu();
      });

      this.userCollection.whoAmI(callback);
    },

    userLogout : function() {

      var userCallback = function(error) {
        if (error) {
          arangoHelper.arangoError("User", "Logout error");
        }
        else {
          this.userCollection.logout();
        } 
      }.bind(this);
      this.userCollection.whoAmI(userCallback);
    }
  });
}());
