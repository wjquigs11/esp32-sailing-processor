#if defined(WIFI)
#include "include-general.h"

// Include AsyncWebServer headers directly for compilation
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
AsyncWebServer server(HTTP_PORT);
AsyncEventSource events("/events");
AsyncEventSource satelliteEvents("/satellites");

void startAppWebServer();

void startWebServer() {
  log::toAll("starting web server");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    log::toAll("index.html");
    request->send(SPIFFS, "/index.html", "text/html", false, settings_processor);
  });

  // start serving from SPIFFS
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  server.serveStatic("/", SPIFFS, "/");


  server.on("/host", HTTP_GET, [](AsyncWebServerRequest *request) {
    String buf = "hostname: " + host;
    buf += ", ESP local MAC addr: " + String(WiFi.macAddress());
    log::toAll(buf);
    request->send(200, "text/plain", buf.c_str());
    buf = String();
  });

#ifdef TEMPLATE
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    log::toAll("config:");
    String response = "none";
    if (request->hasParam("hostname")) {
      Serial.printf("hostname %s", request->getParam("hostname")->value().c_str());
      host = request->getParam("hostname")->value();
      response = "change hostname to " + host;
      log::toAll(response);
      preferences.putString("hostname",host);
      log::toAll("preferences " + preferences.getString("hostname", "unknown"));
    } else if (request->hasParam("webtimer")) {
      timerDelay = atoi(request->getParam("webtimer")->value().c_str());
      if (timerDelay < 0) timerDelay = DEFDELAY;
      if (timerDelay > 10000) timerDelay = 10000;
      response = "change web timer to " + String(timerDelay);
      log::toAll(response);
      preferences.putInt("timerdelay",timerDelay);
    } else if (request->hasParam("value")) {
      // do something with "value" parameter
      response = "value successful";
      log::toAll(response);
    }
    request->send(200, "text/plain", response.c_str());
    response = String();
  });
#endif

  // Always register the /clienttime endpoint to avoid 404 errors
  server.on("/clienttime", HTTP_POST, [](AsyncWebServerRequest *request){
    // Add CORS headers for POST response
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Time received");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
#ifdef NTP
      // Check if NTP sync was successful
      if (!isNtpSyncSuccessful()) {
        // Only use client time if NTP sync failed
        log::toAll("Using client time as NTP sync failed");
        JsonDocument doc;
        ::deserializeJson(doc, data);
        String clientTime = doc["datetime"];
        Serial.print("Received client time: ");
        Serial.println(clientTime);
        
        int year, month, day, hour, minute, second;
        sscanf(clientTime.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second);
        struct tm timeinfo;
        timeinfo.tm_year = year - 1900;   // tm_year: years since 1900
        timeinfo.tm_mon  = month - 1;     // tm_mon: months since January [0,11]
        timeinfo.tm_mday = day;           // tm_mday: day of the month [1,31]
        timeinfo.tm_hour = hour;
        timeinfo.tm_min  = minute;
        timeinfo.tm_sec  = second;
        time_t t = mktime(&timeinfo);  // Convert to time_t (seconds since epoch)
        struct timeval now = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&now, NULL);      // Set the ESP32 system clock
        request->send(200, "text/plain", "Time received and set");
      } else
#else
        {
        // If NTP sync was successful, acknowledge but don't use the client time
        log::toAll("Received client time.");
        request->send(200, "text/plain", "Time received.");
      }
#endif      
      // Clean up
      String clientTime = String();
    }
  );

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("hello!", NULL, now, 1000);
  });
  
  // Add CORS headers to AsyncEventSource
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Cache-Control");
  
  server.addHandler(&events);
  
  // Add CORS support for /events OPTIONS
  server.on("/events", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(204);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, Cache-Control");
    response->addHeader("Access-Control-Max-Age", "3600");
    request->send(response);
  });
  
  // Add CORS support for /clienttime endpoint
  server.on("/clienttime", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(204);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    response->addHeader("Access-Control-Max-Age", "3600");
    request->send(response);
  });
  
  // Add CORS support for /api/wind-data endpoint
  server.on("/api/wind-data", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(204);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    response->addHeader("Access-Control-Max-Age", "3600");
    request->send(response);
  });
  
  //server.addHandler(&ws);

  // Handle OPTIONS preflight requests for CORS
  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      AsyncWebServerResponse *response = request->beginResponse(204);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      response->addHeader("Access-Control-Max-Age", "3600");
      request->send(response);
      return;
    }
    // Handle not found for other requests
    request->send(404);
  });

  startAppWebServer();
}
#endif