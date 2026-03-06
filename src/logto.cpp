#include "include-general.h"
#include "logto.h"

extern bool serverStarted;
File consLog; // TBD move conslog here in main branch and make it part of the class

bool log::logToSerial = true;  // Definition

void log::toAll(String s) {
  if (logToSerial) {
    if (s.endsWith("\n")) s.remove(s.length() - 1);
    Serial.println(s);
    consLog.println(s);
#ifdef WEBSERIAL
    if (serverStarted) {
      WebSerial.print(s);
    }
#endif
    s = String();
  }
}

bool log::initConsole(String conslogName) {
  consLog = SPIFFS.open(conslogName, "a", true);
  if (!consLog) {
    //log::toAll("failed to open console log");
    return false;
  }
  if (consLog.println("ESP console log.")) {
    //log::toAll("console log written");
    return true;
  } else {
    //log::toAll("console log write failed");
    return false;
  }
}

