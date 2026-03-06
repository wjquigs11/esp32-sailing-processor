#include "include-general.h"
#include "windparse.h"
#include "BoatData.h"

// switching now to global so we can use it everywhere; delays *shouldn't* matter
unsigned long now = millis();

void setup_can();
void loop_can();

Preferences preferences;
bool doubleReset = false;

#ifdef WIFI
bool wifiEnabled = true;
bool wifiConnected = false;
unsigned long wifiStartTime = 0;
String host;
#else
bool wifiEnabled = false;
bool wifiConnected = false;
#endif

int timerDelay = 1000; // this sets loop time after housekeeping tasks are done
int loopDelay = 10;
time_t lastUpdate, updateTime;
unsigned long lastTime = 0;
unsigned long loopCount, maxLoopCount, webLoopCount;
struct tm *ptm;
char prbuf[PRBUF]; // PRBUF needs to be defined in include.h

#ifdef DEEPSLEEP
// Deep sleep variables
int awakeTimer = 300;  // stay awake for X seconds each time you wake up
// RTC memory variables (persist across deep sleep)
RTC_DATA_ATTR int bootCount = 0;

// Function to print the reason by which ESP32 has been awaken from sleep
void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0: log::toAll("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1: log::toAll("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER: log::toAll("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: log::toAll("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP: log::toAll("Wakeup caused by ULP program"); break;
    default: log::toAll("Wakeup was not caused by deep sleep: " + String((int)wakeup_reason)); break;
  }
}
#else
int bootCount = 0;
#endif
unsigned long startTime; // Time when the device started

void finishSetup();

void setup() {
  Serial.begin(115200); delay(300);
  startTime = now;
  // Setup custom panic handler
  setup_custom_panic_handler();
  // Enable WiFi debugging
  //Serial.println("Enabling WiFi debugging...");
  //esp_log_level_set("wifi", ESP_LOG_VERBOSE);
#ifdef WIFI
  if (SPIFFS.begin()) {
    Serial.println("opened SPIFFS");
    checkSPIFFS();
    gotWifiCreds = readWiFiCredentials();
  } else {
    Serial.println("failed to open SPIFFS");
  }
#endif
  if (!log::initConsole()) {
    log::toAll("failed to open console log");
  } else {
    log::toAll("console log open");
  }
  // Increment boot number and print it every reboot
  ++bootCount;
  log::toAll("Boot number: " + String(bootCount));
#ifdef DEEPSLEEP
  // Print the wakeup reason for ESP32
  print_wakeup_reason();

  // Configure the wake up source - set ESP32 to wake up every TIME_TO_SLEEP seconds
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  log::toAll("Setup ESP32 to sleep for " + String(TIME_TO_SLEEP) + " Seconds every " + String(awakeTimer) + " seconds");
#endif
  preferences.begin("ESPprefs", false);
  timerDelay = preferences.getInt("timerdelay", 10000);
  if (timerDelay<100) {
    timerDelay = 100;
    preferences.putInt("timerdelay", 100);
  }
  log::toAll("timerDelay " + String(timerDelay));
  wifiEnabled = preferences.getBool("wifi", true);
  // here is usually a good place to add app-specfic setup code
  
#ifdef WIFI
  host = preferences.getString("hostname", "ESPmcu");
  log::toAll("hostname: " + host);
  doubleReset = preferences.getBool("DRD", false);
  preferences.putBool("DRD", true);
  if (wifiEnabled) {
    setupWifi(); // Async - will connect in background via event handlers
    if (doubleReset) {
      log::toAll("double reset detected");
      preferences.putBool("DRD", false);
      preferences.end();
    } else {
      // set up DRD if another reboot happens in 10 seconds
      preferences.putBool("DRD", true);
      finishSetup();
    }
#ifdef ELEGANTOTA
    // always start elegantOTA
    ElegantOTA.begin(&server);
#endif
    // mDNS will be initialized by WiFi event handler when connected
  }
#endif // WIFI

  // app-specific setup can go here
  consLog.flush();
}

void finishSetup() {
  // Start the web server regardless of WiFi connection status
  // In fallback mode, it will serve from AP mode
  startWebServer();
  serverStarted = true;
#ifdef WEBSERIAL
  WebSerial.begin(&server);
  WebSerial.onMessage(WebSerialonMessage);
#endif
  setup_can();
}

static int loopcount = 0;

void loop() {
  now = millis();
  static unsigned long lastEventTime, lastTimeTime, startTime;
  loopCount++;
  if (doubleReset) {
    static bool drdCleared = false;
    // Clear DRD flag after DRD_TIMEOUT seconds for double reset detection
    // unless reset occurs within the timeout period
    if (!drdCleared && (now > (DRD_TIMEOUT * 1000))) {
      preferences.putBool("DRD", false);
      drdCleared = true;
      doubleReset = false;
      log::toAll("DRD timeout - cleared double reset flag");
      finishSetup();
    } else {
      static unsigned long lastPrintTime = 0;
      if (now - lastPrintTime >= 1000) {
        // Calculate remaining seconds until DRD_TIMEOUT
        unsigned long remainingMs = (DRD_TIMEOUT * 1000) - now;
        unsigned long remainingSeconds = remainingMs / 1000;
        Serial.println("DRD timeout in: " + String(remainingSeconds) + " seconds");
        lastPrintTime = now;
      }
    }
  } else {
    // no doubleReset
    loopcount++;
    if (loopCount > maxLoopCount) maxLoopCount = loopCount;
    loop_can();
    webLoopCount++;
#ifdef WEBSERIAL
    WebSerial.loop();
#endif
#ifdef WIFI
    // Handle WiFi connection attempts (moved from timer context)
    handleWifiConnect();
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
      dnsServer.processNextRequest(); // for captive portal
#endif
    
    // this part of the loop will only execute every timerDelay msecs
    if ((now - lastEventTime > timerDelay || lastEventTime == 0) && wifiConnected) {
      lastEventTime = now;
#if defined(WIFI)
#if defined(NTP)
      // Get current time from NTP-synchronized system clock if available
      if (isNtpSyncSuccessful()) {
        // Use the NTP-synchronized time
        lastUpdate = getEpochTime();
      } else if (updateTime > 0) {
        // Fallback: Use the browser's timestamp as base and add elapsed milliseconds
        lastUpdate = updateTime + (now - lastEventTime);
      } else {
        // If neither NTP nor browser time is available, use millis() as last resort
        // This will be incorrect since ESP32 millis() is not a Unix timestamp
        lastUpdate = now;
      }
      
      // kind of annoying that it resyncs right after startup sync but I guess I can live with it for now
      // Periodically resync NTP time (once per day)
      static unsigned long lastNTPSync = 0;
      if (isNtpSyncSuccessful() && (now - lastNTPSync > 86400000 || lastNTPSync == 0)) {
        resyncNTP();
        lastNTPSync = now;
      }
#endif // NTP
      readings["mastAngle"] = String(mastRotate);
      //log::toAll("mast angle=" + readings["mastangle"];
      // Send the timestamp in milliseconds since epoch (Unix timestamp)
      readings["lastUpdate"] = String(lastUpdate);
      events.send(getSensorReadings().c_str(),"new_readings" ,now);
      
      // Send wind data via SSE (every loop iteration)
      extern String getWindData();
      events.send(getWindData().c_str(), "wind_data", now);
      
      // Send weather data via SSE (less frequently - every 60 seconds)
      static unsigned long lastWeatherTime = 0;
      if (now - lastWeatherTime > 60000 || lastWeatherTime == 0) {
        lastWeatherTime = now;
        extern String getWeatherData();
        events.send(getWeatherData().c_str(), "weather_data", now);
      }
#ifdef RTK_TIMO
      // Send satellite data via SSE (every 5 seconds)
      static unsigned long lastSatelliteTime = 0;
      if (now - lastSatelliteTime > 5000 || lastSatelliteTime == 0) {
        lastSatelliteTime = now;
        extern String getSatelliteData();
        extern AsyncEventSource satelliteEvents;
        satelliteEvents.send(getSatelliteData().c_str(), "satellite_data", now);
      }
#endif
#endif // WIFI
#if 0
      // Use system time directly instead of lastUpdate
      time_t now_time;
      time(&now_time);
      ptm = localtime(&now_time);
      sprintf(prbuf,"%d [%02d/%02d %02d:%02d:%02d] ",loopcount,ptm->tm_mon+1,ptm->tm_mday,ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
      log::toAll(String(prbuf));
      consLog.flush();
#endif
    } // end of timer block

#ifdef ECHO
    if (Serial.available() > 0) {
      String input = "";
      while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
          // Ignore newline and carriage return characters
          break;
        }
        input += c;
      }
      if (input.length() > 0) {
        Serial.print("Received: ");
        Serial.println(input);
        if (input == "value") {
          // do something with serial input
          Serial.println("Value command received");
        }
      }
    }
#endif
#ifdef DEEPSLEEP
    // Check if it's time to go to sleep
    if ((now - startTime) > (awakeTimer * 1000)) {
      log::toAll("Going to sleep in 5 seconds...");
      
      // Flush any pending data
      consLog.flush();
  #ifdef WEBSERIAL
      WebSerial.flush();
  #endif
      // Give time for final communications
      delay(5000);
      // Enter deep sleep
      log::toAll("Entering deep sleep for " + String(TIME_TO_SLEEP) + " seconds");
      esp_deep_sleep_start();
      // Code after this point will not be executed
    }
#endif
  } // else no doubleReset
  // the only thing we're going to do in doubleReset "mode" is elegantOTA, until timeout
#ifdef ELEGANTOTA
  ElegantOTA.loop();
#endif
  //delay(loopDelay);
}
