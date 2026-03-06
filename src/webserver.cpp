#ifdef WIFI
#include "include-general.h"
#include "windparse.h"
#include "BoatData.h"
#include <Preferences.h>

extern Preferences preferences;

// Forward declaration for satelliteEvents
extern AsyncEventSource satelliteEvents;

bool serverStarted;
JsonDocument readings;

String getSensorReadings() {
  //readings["sensor"] = "0";
  
  // Ensure both magnetic and true heading are always available
  if (pBD != nullptr) {
    readings["heading"] = String(pBD->magHeading);
    readings["BNOheading"] = String(pBD->BNOheading);
    readings["RTKheading"] = String(pBD->RTKheading);
  }
  
  String jsonString;
  serializeJson(readings,jsonString);
  return jsonString;
}

String getWindData() {
  JsonDocument windDoc;
  // Send only raw sensor data - calculations moved to client-side JavaScript
  windDoc["aws"] = String(pBD->AWS, 2);  // Apparent Wind Speed in m/s
  windDoc["awa"] = String(pBD->AWA, 2);  // Apparent Wind Angle in degrees
  windDoc["stw"] = String(pBD->STW, 2);  // Speed Through Water in m/s
  windDoc["windFreq"] = String(currentWindFreq,2);  // frequency of wind PGNs
  windDoc["windPackets"] = String(num_wind_messages);
  windDoc["BNOheading"] = String(pBD->BNOheading, 2);  // BNO heading in degrees
  windDoc["RTKheading"] = String(pBD->RTKheading, 2);  // RTK heading in degrees
  
#ifdef TACK
  if (tackDetectionEnabled) {
    windDoc["tackStats"] = getTackStats();
    windDoc["lastManeuver"] = String((int)getLastManeuver());
    windDoc["inManeuver"] = String(isInManeuver() ? "true" : "false");
  }
#endif
  windDoc["timestamp"] = String(now);  // Timestamp for client processing
  
  String jsonString;
  serializeJson(windDoc, jsonString);
  return jsonString;
}

String getWeatherData() {
  JsonDocument weatherDoc;
  weatherDoc["temperature"] = String(pENV->temp, 2);
  weatherDoc["humidity"] = String(pENV->humidity, 2);
  weatherDoc["pressure"] = String(pENV->pressure, 2);
  
  String jsonString;
  serializeJson(weatherDoc, jsonString);
  return jsonString;
}

#ifdef RTK_TIMO
String getSatelliteData() {
  JsonDocument satDoc;
  JsonArray satellites = satDoc["satellites"].to<JsonArray>();
  
  if (pSAT != nullptr) {
    // Add detailed satellite data from the satellites array
    int validSatCount = 0;
    for (int i = 0; i < pSAT->satelliteCount && i < 24; i++) {
      const tSatelliteData& sat = pSAT->satellites[i];
      if (sat.SVID > 0) {  // Valid satellite
        JsonObject satObj = satellites.add<JsonObject>();
        satObj["id"] = sat.SVID;
        satObj["az"] = sat.Azimuth;
        satObj["el"] = sat.Elevation;
        satObj["snr"] = sat.SNR;
        satObj["constellation"] = pSAT->getConstellationName(sat.constellation);
        validSatCount++;
      }
    }
    // Add debug info to help troubleshoot
    //if (validSatCount == 0) {
    //  log::toAll("getSatelliteData: No valid satellites found. pSAT->satelliteCount=" + String(pSAT->satelliteCount));
    //  log::toAll("check if GSV is enabled on GPS");
    //}
  } else {
    log::toAll("getSatelliteData: pSAT is nullptr");
  }
  
  String jsonString;
  serializeJson(satDoc, jsonString);
  return jsonString;
}
#endif

/*
  There's a placeholder in the html file %BUTTONPLACEHOLDER%
  When the page renders the "processor" function I define below will get called to replace the placeholder(s)
  with html generated and placed in the string(s) below
  The CSS in the HTML file changes the appearance of the slider based on whether the checkbox shows as "checked" or not
*/
String settings_processor(const String& var) {
  log::toAll("settings processor var: " + var);
  if (var == "BUTTONPLACEHOLDER") {
    String result = "";
    result.reserve(800); // Reserve space to prevent reallocations
    
    result += "<div class=\"toggle-section\"><label><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"display\" ";
    if (displayOnToggle) result += "checked";
    result += ">Display On/Off</label></div>";
    
#ifdef HONEY
    result += "<div class=\"toggle-section\"><label><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"honeywell\" ";
    if (honeywellOnToggle) result += "checked";
    result += ">Honeywell On/Off</label></div>";
#endif

    return result;
  }
  
  // Return simple values directly without using global string
  if (var == "webtimerdelay") return String(timerDelay);
  if (var == "orientation") return String(pBD->magOrientation);
  if (var == "sensorient") return String(pBD->sensOrientation);
  if (var == "boatorient") return String(pBD->magOrientation);
  if (var == "RTKorient") return String(pBD->rtkOrientation);
#ifdef BNO08X
  if (var == "frequency") return String(compass.frequency);
#endif
  if (var == "controlMAC") return WiFi.macAddress();
  if (var == "variation") return String(pBD->Variation);
  
#ifdef HONEY
  // Pot values for index.html
  if (var == "lowSet") return String(lowSet);
  if (var == "PotLo") return String(PotLo);
  if (var == "PotValue") return String(PotValue);
  if (var == "highSet") return String(highSet);
  if (var == "PotHi") return String(PotHi);
#endif
  
  return String("settings processor: placeholder not found " + var);
}

void startAppWebServer() {

  // Request the latest sensor readings
  /* As the rest of the code runs, receiving updates like wind speed and boat heading, 
     it updates a JSON array called "readings".
     The index of each array element represents a variable passed to javascript on the web page
     Any page that uses script.js and has an element whose "span id" is the same as one of the readings elements
     will have that element's value replaced by the latest data, if that reading value is in the array
     For example, windparse.cpp does this: readings["windSpeed"] = String(windSpeedKnots);
     So any page that needs windSpeed can create an element with that label and get the value 
  */

  // Add readings endpoint for initial data fetch
  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", getSensorReadings());
  });

  // Consolidated weather data endpoint returning JSON
  server.on("/weather-data", HTTP_GET, [](AsyncWebServerRequest *request) {
    //Serial.println("/weather-data");
    JsonDocument weatherDoc;
    
    // Temperature is already in Fahrenheit from readBME()
    weatherDoc["temperature"] = String(pENV->temp, 2);
    weatherDoc["humidity"] = String(pENV->humidity, 2);
    weatherDoc["pressure"] = String(pENV->pressure, 2);
    
    String jsonString;
    serializeJson(weatherDoc, jsonString);
    request->send(200, "application/json", jsonString);
  });
  
  // Raw wind data endpoint (GET) - returns only sensor data
  server.on("/wind-data", HTTP_GET, [](AsyncWebServerRequest *request) {
    //Serial.println("/wind-data GET");
    JsonDocument windDoc;
    
    // Send only raw sensor data
    windDoc["aws"] = String(pBD->AWS, 4);  // m/s
    windDoc["awa"] = String(pBD->AWA, 2);  // degrees
    windDoc["stw"] = String(pBD->STW, 4);  // m/s
    windDoc["BNOheading"] = String(pBD->BNOheading, 2);  // degrees
    windDoc["RTKheading"] = String(pBD->RTKheading, 2);  // degrees
    windDoc["timestamp"] = String(now);
    
    String jsonString;
    serializeJson(windDoc, jsonString);
    request->send(200, "application/json", jsonString);
  });
  
  // Wind data endpoint (POST) - receives calculated data from JavaScript
  server.on("/api/wind-data", HTTP_POST, [](AsyncWebServerRequest *request){
    // Add CORS headers for POST response
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"received\"}");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      // Process calculated wind data from JavaScript
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data);
      
      if (!error) {
        // Store calculated values back into BoatData for logging/N2K if needed
        if (doc.containsKey("TWS")) pBD->TWS = doc["TWS"].as<double>();
        if (doc.containsKey("TWA")) pBD->TWA = doc["TWA"].as<double>();
        if (doc.containsKey("VMG")) pBD->VMG = doc["VMG"].as<double>();
        if (doc.containsKey("TWD")) pBD->TWD = doc["TWD"].as<double>();
        if (doc.containsKey("maxTWS")) pBD->maxTWS = doc["maxTWS"].as<double>();
        
        // Optional: Log the received calculated data
        Serial.printf("Received calculated wind data: TWS=%.2f, TWA=%.1f, VMG=%.2f, TWD=%.1f\n",
                     pBD->TWS, pBD->TWA, pBD->VMG, pBD->TWD);
      } else {
        Serial.println("Error parsing wind data JSON");
      }
    }
  );
  
  // Wind correction toggle endpoints
  server.on("/api/wind-correction", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["enabled"] = windToggle;
    
    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
  });
  
  server.on("/api/wind-correction", HTTP_POST, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"ok\"}");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data);
      
      if (!error && doc.containsKey("enabled")) {
        windToggle = doc["enabled"].as<bool>();
        preferences.putBool("WindToggle", windToggle);
        log::toAll("Wind correction " + String(windToggle ? "enabled" : "disabled") + " via web interface");
      }
    }
  );
  
  server.on("/weather", HTTP_GET, [](AsyncWebServerRequest *request) {
    log::toAll("weather.html");
    request->send(SPIFFS, "/weather.html", "text/html");
  });
  
  server.on("/wind", HTTP_GET, [](AsyncWebServerRequest *request) {
    log::toAll("wind.html");
    request->send(SPIFFS, "/wind.html", "text/html");
  });

  server.on("/compass", HTTP_GET, [](AsyncWebServerRequest *request) {
    log::toAll("compass.html");
    request->send(SPIFFS, "/compass.html", "text/html");
  });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    log::toAll("settings.html");
    request->send(SPIFFS, "/settings.html", "text/html", false, settings_processor);
  });

  // Handle settings form submission
  server.on("/params", HTTP_POST, [](AsyncWebServerRequest *request) {
    log::toAll("POST /params");
    
    // Process form parameters
    if (request->hasParam("webtimerdelay", true)) {
      timerDelay = request->getParam("webtimerdelay", true)->value().toInt();
      if (timerDelay < 0) timerDelay = DEFDELAY;
      if (timerDelay > 10000) timerDelay = 10000;
      preferences.putInt("timerdelay", timerDelay);
      log::toAll("Updated webtimerdelay: " + String(timerDelay));
    }
    
    if (request->hasParam("sensorient", true)) {
      pBD->sensOrientation = request->getParam("sensorient", true)->value().toInt();
      preferences.putInt("sensOrient", pBD->sensOrientation);
      log::toAll("Updated sensorient: " + String(pBD->sensOrientation));
    }
    
    if (request->hasParam("frequency", true)) {
#ifdef BNO08X
      compass.frequency = request->getParam("frequency", true)->value().toInt();
      preferences.putInt("frequency", compass.frequency);
      log::toAll("Updated frequency: " + String(compass.frequency));
#endif
    }
    
    if (request->hasParam("variation", true)) {
      pBD->Variation = request->getParam("variation", true)->value().toFloat();
      preferences.putFloat("Variation", pBD->Variation);
      log::toAll("Updated variation: " + String(pBD->Variation));
    }
    
    if (request->hasParam("boatorient", true)) {
      pBD->magOrientation = request->getParam("boatorient", true)->value().toInt();
      preferences.putInt("magOrient", pBD->magOrientation);
      log::toAll("Updated boatorient: " + String(pBD->magOrientation));
    }
    
    if (request->hasParam("RTKorient", true)) {
      pBD->rtkOrientation = request->getParam("RTKorient", true)->value().toInt();
      preferences.putInt("rtkOrient", pBD->rtkOrientation);
      log::toAll("Updated RTKorient: " + String(pBD->rtkOrientation));
    }
    
#ifdef HONEY
    // Handle pot calibration settings from index.html
    if (request->hasParam("lowSet", true)) {
      lowSet = request->getParam("lowSet", true)->value().toInt();
      preferences.putInt("lowSet", lowSet);
      log::toAll("Updated lowSet: " + String(lowSet));
    }
    
    if (request->hasParam("highSet", true)) {
      highSet = request->getParam("highSet", true)->value().toInt();
      preferences.putInt("highSet", highSet);
      log::toAll("Updated highSet: " + String(highSet));
    }
#endif
    
    // Determine which page to redirect to based on referer
    String referer = request->header("Referer");
    if (referer.indexOf("/settings") >= 0) {
      request->redirect("/settings");
    } else {
      request->redirect("/");
    }
  });

  // Handle toggle checkbox state changes
  server.on("/params", HTTP_GET, [](AsyncWebServerRequest *request) {
    log::toAll("GET /params");
    
    if (request->hasParam("output") && request->hasParam("state")) {
      String output = request->getParam("output")->value();
      String state = request->getParam("state")->value();
      bool isOn = (state == "on");
      
      if (output == "display") {
        displayOnToggle = isOn;
        preferences.putBool("DisplayToggle", displayOnToggle);
        log::toAll("Display toggle: " + String(isOn ? "ON" : "OFF"));
      }
#ifdef HONEY
      else if (output == "honeywell") {
        honeywellOnToggle = isOn;
        preferences.putBool("HoneywellToggle", honeywellOnToggle);
        log::toAll("Honeywell toggle: " + String(isOn ? "ON" : "OFF"));
      }
#endif
      
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing parameters");
    }
  });
#ifdef RTK_TIMO
  // Add route for satellite page
  server.on("/sat.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    log::toAll("sat.html requested");
    request->send(SPIFFS, "/sat.html", "text/html");
  });
  
  // Add satellite event source to server
  log::toAll("Adding satellite event source handler...");
  server.addHandler(&satelliteEvents);
  
  // Add CORS support for /satellites OPTIONS
  server.on("/satellites", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    log::toAll("Satellites OPTIONS request received");
    AsyncWebServerResponse *response = request->beginResponse(204);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, Cache-Control");
    response->addHeader("Access-Control-Max-Age", "3600");
    request->send(response);
  });
  
  // Add connection handler for satellite events
  satelliteEvents.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Satellite client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    log::toAll("Satellite SSE client connected");
    // Send initial satellite data immediately
    String satData = getSatelliteData();
    log::toAll("Sending initial satellite data: " + satData);
    client->send(satData.c_str(), "satellite_data", now);
  });
#endif  
  log::toAll("App web server routes configured - satellite endpoints ready");

}
#endif // WIFI
