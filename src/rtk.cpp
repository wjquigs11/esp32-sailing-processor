#if defined(RTK) || defined(RTK_TIMO)
#include "include-general.h"
#include "windparse.h"
#include "BoatData.h"

#define RTKserial Serial2 
#define RTKRX 33  // espberry pin 33  
#define RTKTX 14  // espberry pin 31
#define RTKBAUD 115200
tNMEA0183 RTKport;

/* We push these commands at setup()
 Unfortunately some like "GPGSVH" are not "sticky" and don't persist even after SAVECONFIG
 so might as well push all the reporting commands
 Commands are now loaded from /data/rtkcommands.json
 
 Original hardcoded commands for reference:
const char* commandsInUse[] = {
  "GPGSV 30",
  "GPGSVH 30",
  "UNIHEADINGA 10",
  "GNGGA 30",
  "GPHPR 10",
  // increase frequency of GPHPR for local parsing, GPTHS for Signalk
  "GPTHS 10",
  "CONFIG",
  "MODE"
  //"SAVECONFIG"
};
const int commandsInUseLength = sizeof(commandsInUse) / sizeof(commandsInUse[0]);
*/


bool headingValid = false;
unsigned long lastHeadingUpdate = 0;

#if defined(RTK_TIMO) || defined (RTK_HYBRID)
bool directRTK = false;
#else
bool directRTK = true;
#endif

void sendCommand(String command) {
  RTKserial.println(command);
  
  // Loop and wait until RTK serial has data available
  unsigned long startTime = millis();
  unsigned long timeout = 5000; // 5 second timeout
  
  while (!RTKserial.available() && (millis() - startTime < timeout)) {
    delay(10); // Small delay to prevent busy waiting
  }
  
  if (RTKserial.available()) {
    String response = RTKserial.readStringUntil('\n');
    if (response.length() > 0) {
      log::toAll("RTK response: " + response);
    }
  } else {
    log::toAll("RTK response timeout for command: " + command);
  }
}

void updateRTKCommands(String command, float interval) {
  // Read current JSON file
  File file = SPIFFS.open("/rtkcommands.json", "r");
  if (!file) {
    log::toAll("Failed to open rtkcommands.json for reading");
    return;
  }
  String jsonString = file.readString();
  file.close();
  // Parse JSON
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, jsonString);
  if (error) {
    log::toAll("Failed to parse rtkcommands.json: " + String(error.c_str()));
    return;
  }
  JsonArray commandsInUse = doc["commandsInUse"];
  bool found = false;
  // Check if command already exists
  for (JsonVariant item : commandsInUse) {
    if (item["command"].as<String>() == command) {
      found = true;
      // Update interval if provided and different
      if (interval >= 0) {
        if (item.containsKey("interval")) {
          item["interval"] = interval;
        } else {
          item["interval"] = interval;
        }
        log::toAll("Updated " + command + " interval to " + String(interval));
      }
      break;
    }
  }
  // Add new command if not found
  if (!found) {
    JsonObject newCommand = commandsInUse.createNestedObject();
    newCommand["command"] = command;
    if (interval >= 0) {
      newCommand["interval"] = interval;
    }
    log::toAll("Added new RTK command: " + command + (interval >= 0 ? " " + String(interval) : ""));
  }
  // Write back to file
  file = SPIFFS.open("/rtkcommands.json", "w");
  if (!file) {
    log::toAll("Failed to open rtkcommands.json for writing");
    return;
  }
  serializeJsonPretty(doc, file);
  file.close();
  sendCommand("SAVECONFIG");
  log::toAll("RTK commands configuration updated on SPIFFS and SAVECONFIG");
}

void setupRTK() {
  pBD->rtkOrientation = preferences.getInt("rtkOrientation",0);
  log::toAll("configuring RTK, offset=" + rtkOrientation);
  RTKserial.begin(RTKBAUD,SERIAL_8N1,RTKRX,RTKTX);
  if (!RTKserial)
    log::toAll("failed to open RTK serial port");
  else
    log::toAll("opened RTK serial port");
  // Load and send commands from JSON file
  File file = SPIFFS.open("/rtkcommands.json", "r");
  if (!file) {
    log::toAll("Failed to open rtkcommands.json");
    return;
  }
  String jsonString = file.readString();
  file.close();
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, jsonString);
  if (error) {
    log::toAll("Failed to parse rtkcommands.json: " + String(error.c_str()));
    return;
  }
  JsonArray commandsInUse = doc["commandsInUse"];
  for (JsonVariant item : commandsInUse) {
    String command = item["command"];
    String fullCommand = command;
    // Check if interval is specified and not null
    if (item.containsKey("interval") && !item["interval"].isNull()) {
      float interval = item["interval"];
      fullCommand = command + " " + String(interval);
    }
    sendCommand(fullCommand);
    log::toAll("RTK command: " + fullCommand);
  }
#if defined(RTK_TIMO) || defined (RTK_HYBRID)
  RTKport.SetMsgHandler(HandleNMEA0183Msg);
  RTKport.SetMessageStream(&RTKserial);
  RTKport.Open();
#endif
}

char nmeaSentence[MAX_NMEA0183_MSG_LEN];

void loopRTK() {
  if (!directRTK) {
    // use Timo handlers (also for hybrid mode)
    RTKport.ParseMessages();
  } else {
    if (RTKserial.available()) {
      int bytesRead = RTKserial.readBytesUntil('\n', nmeaSentence, sizeof(nmeaSentence) - 3);
      // Add proper NMEA 0183 line termination
      // now done in tcp_sender
      //nmeaSentence[bytesRead] = '\r';
      //nmeaSentence[bytesRead + 1] = '\n';
      //nmeaSentence[bytesRead + 2] = '\0';
      if (bytesRead > 0 && (nmeaSentence[0] == '$' || nmeaSentence[0] == '#')) {
        bytesRead+=2;
        if (strncmp(nmeaSentence,"$GNTHS",6) == 0) {
          //Serial.println(nmeaSentence);
          double heading;
          unsigned int cksum;
          if (sscanf(nmeaSentence, "%*[^,],%lf%*[^*]*%x", &heading, &cksum) == 2) {
            byte cksumck = calculateNMEAChecksum(nmeaSentence);
            if (cksumck == cksum) {
              Serial.printf("value = %f, checksum = 0x%02X, cksumck = 0x%02X\n", heading, cksum, cksumck);
              pBD->RTKheading = heading; // modify to include offset
            }
          } else {
            log::toAll("RTK serial parse error");
          }
        } else {
          if (debugRTK) {
            log::toAll(nmeaSentence);
          }
        }
#ifdef UDP_FORWARD
        transmit_outgoing_over_udp(nmeaSentence, bytesRead);
#endif
#ifdef TCP_FORWARD
        transmit_outgoing_over_tcp(nmeaSentence, bytesRead);
#endif
      }
    }
    // Handle direct serial communication for debugging
    if (Serial.available()) {
      String line = Serial.readStringUntil('\n');
      if (line.length() > 0) {
        if (!line.endsWith("\n")) {
          line += "\n";
        }
        Serial.printf("Debug->UM: %s", line.c_str());
        RTKserial.print(line);
      }
    }
  }
}
#endif