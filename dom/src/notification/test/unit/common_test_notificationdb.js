"use strict";

const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

let systemNotification = {
  origin: "app://system.gaiamobile.org/manifest.webapp",
  id: "{2bc883bf-2809-4432-b0f4-f54e10372764}",
  title: "SystemNotification:" + Date.now(),
  dir: "auto",
  lang: "",
  body: "System notification body",
  tag: "",
  icon: "icon.png"
};

let calendarNotification = {
  origin: "app://calendar.gaiamobile.org/manifest.webapp",
  id: "{d8d11299-a58e-429b-9a9a-57c562982fbf}",
  title: "CalendarNotification:" + Date.now(),
  dir: "auto",
  lang: "",
  body: "Calendar notification body",
  tag: "",
  icon: "icon.png"
};

// Helper to start the NotificationDB
function startNotificationDB() {
  Cu.import("resource://gre/modules/NotificationDB.jsm");
}

// Helper function to add a listener, send message and treat the reply
function addAndSend(msg, reply, callback, payload, runNext = true) {
  let handler = {
    receiveMessage: function(message) {
      if (message.name === reply) {
        cpmm.removeMessageListener(reply, handler);
        callback(message);
        if (runNext) {
          run_next_test();
        }
      }
    }
  };
  cpmm.addMessageListener(reply, handler);
  cpmm.sendAsyncMessage(msg, payload);
}

// helper fonction, comparing two notifications
function compareNotification(notif1, notif2) {
  // retrieved notification should be the second one sent
  for (let prop in notif1) {
    // compare each property
    do_check_eq(notif1[prop], notif2[prop]);
  }
}
