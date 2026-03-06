#ifdef MOB_PING
#include "include-general.h"
#include "windparse.h"
#include "BoatData.h"

// Web server endpoint for MOB ping
// accept GET requests to /mobping with 'username' parameter
// respond with 'ok username'
// if we miss 5 consecutive messages OR no message in timeout period, sound buzzer
// if buzzer sounds for 30 seconds and nobody presses button, send command to autopilot
// command is either steer to 0 AWA or put tiller hard over to circle boat

// MOB tracking variables
MOBEntry mobList[MAX_MOB];
int mobCount = 0;

// Find MOB entry by username, returns index or -1 if not found
int findMOBEntry(const char* username) {
  for (int i = 0; i < mobCount; i++) {
    if (mobList[i].username && strcmp(mobList[i].username, username) == 0) {
      return i;
    }
  }
  return -1;
}

// Add new MOB entry to the list
void addMOBEntry(const char* username, const char* client, int interval) {
  if (mobCount >= MAX_MOB) {
    log::toAll("MOB list full, cannot add new entry for: " + String(username));
    return;
  }
  
  // Allocate memory for strings
  mobList[mobCount].username = (char*)malloc(strlen(username) + 1);
  mobList[mobCount].client = (char*)malloc(strlen(client) + 1);
  
  if (mobList[mobCount].username && mobList[mobCount].client) {
    strcpy(mobList[mobCount].username, username);
    strcpy(mobList[mobCount].client, client);
    mobList[mobCount].interval = interval;
    mobList[mobCount].num_missed = 0;
    mobList[mobCount].timeout = interval;
    
    log::toAll("Added MOB entry for: " + String(username) + " from " + String(client) + " (interval: " + String(interval) + "s)");
    mobCount++;
  } else {
    log::toAll("Failed to allocate memory for MOB entry: " + String(username));
    if (mobList[mobCount].username) free(mobList[mobCount].username);
    if (mobList[mobCount].client) free(mobList[mobCount].client);
  }
}

// Reset MOB timer for existing entry
void resetMOBTimer(int index) {
  if (index >= 0 && index < mobCount) {
    mobList[index].timeout = mobList[index].interval;
    mobList[index].num_missed = 0;
    log::toAll("Reset timer for MOB: " + String(mobList[index].username));
  }
}

// Check MOB timers - called once per second from main loop
void checkMOB() {
  for (int i = 0; i < mobCount; i++) {
    // Skip users with disabled timers (interval = 0)
    if (mobList[i].interval == 0) {
      continue;
    }
    
    if (mobList[i].timeout > 0) {
      mobList[i].timeout--;
    } else {
      // Timer expired, increment missed count
      mobList[i].num_missed++;
      mobList[i].timeout = mobList[i].interval; // Reset timer for next check
      
      log::toAll("MOB timeout for: " + String(mobList[i].username) + " (missed: " + String(mobList[i].num_missed) + "/" + String(NUM_MISSED) + ")");
      
      // Check if we've reached the alarm threshold
      if (mobList[i].num_missed >= NUM_MISSED) {
        log::toAll("MOB ALARM: " + String(mobList[i].username) + " has missed " + String(NUM_MISSED) + " consecutive pings!");
        // TODO: Add buzzer/alarm functionality here
        // TODO: Add autopilot command functionality here
        //SetN2kMOBNotification(tN2kMsg &N2kMsg, unsigned char SID, uint32_t MobEmitterId, tN2kMOBStatus MOBStatus, double ActivationTime, tN2kMOBPositionSource PositionSource, uint16_t PositionDate, double PositionTime, double Latitude, double Longitude, tN2kHeadingReference COGReference, double COG, double SOG, uint32_t MMSI, tN2kMOBEmitterBatteryStatus MOBEmitterBatteryStatus)      }
    }
  }
}

// Setup MOB ping web endpoint
void setupMOB() {
  // Initialize MOB list
  mobCount = 0;
  for (int i = 0; i < MAX_MOB; i++) {
    mobList[i].username = nullptr;
    mobList[i].client = nullptr;
    mobList[i].interval = 0;
    mobList[i].num_missed = 0;
    mobList[i].timeout = 0;
  }
  
  // Add the /mobping endpoint to the web server
  server.on("/mobping", HTTP_PUT, [](AsyncWebServerRequest *request) {
    String username = "";
    
    // Check if username parameter is provided
    if (request->hasParam("username")) {
      username = request->getParam("username")->value();
      String clientIP = request->client()->remoteIP().toString();
      
      // Get optional interval parameter (default to DEF_TIMEOUT)
      int interval = DEF_TIMEOUT;
      bool disableTimer = false;
      if (request->hasParam("interval")) {
        interval = request->getParam("interval")->value().toInt();
        if (interval == -1) {
          disableTimer = true;
          interval = 0; // Set to 0 to disable timer
        } else if (interval <= 0) {
          interval = DEF_TIMEOUT;
        }
      }
      
      // Check if username is already in the list
      int existingIndex = findMOBEntry(username.c_str());
      if (existingIndex >= 0) {
        // User exists
        if (disableTimer) {
          // Disable timer for this user
          mobList[existingIndex].interval = 0;
          mobList[existingIndex].timeout = 0;
          mobList[existingIndex].num_missed = 0;
          log::toAll("Disabled timer for MOB: " + String(mobList[existingIndex].username));
        } else {
          // Reset their timer normally
          resetMOBTimer(existingIndex);
        }
      } else {
        // New user, add to list
        addMOBEntry(username.c_str(), clientIP.c_str(), interval);
        if (disableTimer) {
          log::toAll("Added MOB entry with disabled timer for: " + username);
        }
      }
      
      // Log the received ping
      log::toAll("MOB ping received from: " + username + " (IP: " + clientIP + ")");
      
      // Respond with "ok username"
      String response = "ok " + username;
      request->send(200, "text/plain", response);
      
      log::toAll("Sent MOB ping response: " + response);
    } else {
      // No username parameter provided
      log::toAll("MOB ping request missing username parameter from IP: " + request->client()->remoteIP().toString());
      request->send(400, "text/plain", "Missing username parameter");
    }
  });
  
  log::toAll("MOB ping web endpoint configured at /mobping");
}

#endif