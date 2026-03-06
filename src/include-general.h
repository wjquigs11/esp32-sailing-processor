#include <Arduino.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <time.h>
#include <Wire.h>

#ifdef DEEPSLEEP
// ESP32 deep sleep includes
#include "esp_system.h"
#include "esp_sleep.h"
#endif

#ifdef WIFI
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
extern AsyncWebServer server;
extern AsyncEventSource events;
extern AsyncWebSocket ws;
extern bool serverStarted;
extern bool wifiConnected;
extern String host;
extern unsigned long wifiStartTime;
extern TimerHandle_t connectTimer;
extern JsonDocument readings;
extern int timerDelay;
#define HTTP_PORT 80
void checkSPIFFS();
bool readWiFiCredentials();
extern bool gotWifiCreds;
// for captive portal
#include <DNSServer.h>
extern DNSServer dnsServer;
#define DNS_INTERVAL 300
bool setupWifi();
void resetWifi();
void startWebServer();
void startPortal();
void onWiFiConnected();
void onWiFiDisconnected();
void handleWifiConnect();
String getSensorReadings();
String processor();
String settings_processor(const String& var);
#endif
#define DRD_TIMEOUT 10

// Build information variables (defined by build flags)
#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP "unknown"
#endif
#ifndef BUILD_DATE
#define BUILD_DATE "unknown"
#endif
#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif

// External variables
extern int bootCount;

#ifdef WEBSERIAL
#include <WebSerialPro.h>
void WebSerialonMessage(uint8_t *data, size_t len);
extern const char* appCommandList[];
extern const char* appToggleList[];
using Handler = void(*)(String*, int);
extern Handler appHandler;
extern Handler togHandler;
#endif
#ifdef ELEGANTOTA
#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1
#include <ElegantOTA.h>
#endif

#include "logto.h"

extern Preferences preferences;
extern File consLog;

// Timer variables
#include <time.h>
#define DEFDELAY 1000
extern unsigned long lastTime;
// timerDelay defines how long we wait to send an update to connected web clients
extern int timerDelay;
// loopDelay defines how long we delay at the end of each iteration of loop();
// if we're not doing captive portal (for dns) it can be 0, although spinning on CPU for most of my projects seems a bit silly
extern int loopDelay;
extern int minReadRate;
// store last update based on clock time from client browser
extern time_t lastUpdate, updateTime;
extern struct tm *ptm;
#define PRBUF 128
extern char prbuf[];

// global time to avoid calling millis() constantly
extern unsigned long now;

#ifdef NTP
// Time synchronization functions
void setupTime();
bool waitForTimeSync(int timeoutSeconds = 10);
String getFormattedTime();
unsigned long getEpochTime();
bool isNtpSyncSuccessful();
void resyncNTP();
#endif

// Custom panic handler setup
void setup_custom_panic_handler();

#ifdef DEEPSLEEP
// Deep sleep variables
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60       /* Time ESP32 will go to sleep (in seconds) */
extern int awakeTimer;          /* Time to stay awake before going to sleep (in seconds) */
extern unsigned long startTime; /* Time when the device started */

// Function to print the wakeup reason
void print_wakeup_reason();
#endif
