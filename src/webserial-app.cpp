#ifdef WEBSERIAL
#include "include-general.h"
#include "windparse.h"
#include "BoatData.h"
#include "logto.h"

/* app-specific webserial handlers
    This allows me to avoid changes in webserial.cpp that would cause conflicts when merging from main into a project-specific branch
    All changes should be made in this file.
    add #define for APPHANDLER in platformio.ini if you are going to use app-specific handlers
    TBD: change argument to an array of Strings
*/
#ifndef APPHANDLER
Handler appHandler = nullptr;
Handler togHandler = nullptr;
const char* appCommandList[] = {};
const char* appToggleList[] = {};
#else
const char* appCommandList[] = {"rtkstat", "gps", "gsv", "sat", "boat", "compass", "orient [compass|honey|rtk]", "build", "display", "seatalk", ""};
const char* appToggleList[] = {"wind", "display", "rtkdebug", "directRTK", "compassdebug", "n2kdebug", "nmea0183debug", "stdebug", "heading (rtk|bno)", ""};

// Example application-specific command handler
void myAppHandler(String* words, int totalWords) {
  int i;
  String command = words[i];
if (command.startsWith("disp")) {
  log::toAll("Up:uptime SSID");
  log::toAll("N2K:messages received Wind:message received");
  log::toAll("Wind Speed/Wind Angle/corrected angle (Rotateout)");
  log::toAll("Sensor: low/current/high rotation sensor Mast: mast angle");
  log::toAll("M: magnetic compass heading A: Accuracy (degrees) C: calibration status (0-4)");
  log::toAll("Latitude Longitude (from RTK)");
  log::toAll("H: true heading (from RTK) COG: course over ground (from RTK)");
  return;
}
// kinda sorta considering ripping all this crap out and just relying on uploading a new json file to spiffs to change these params
#ifdef RTK
  if (command.startsWith("rtkco")) {
    // Send arbitrary RTK command with optional period
    if (totalWords > 1 && !words[++i].isEmpty()) {
      String rtkCommand = words[i];
      rtkCommand.toUpperCase();  // Convert command to uppercase
      String fullCommand = rtkCommand;
      float interval = -1;
      if (totalWords > 2 && !words[++i].isEmpty()) {
        String period = words[i];
        interval = atof(period.c_str());
        fullCommand = rtkCommand + " " + period;
      }
      log::toAll("Sending RTK command: " + fullCommand);
      sendCommand(fullCommand);
      updateRTKCommands(rtkCommand, interval);
    } else {
      // Display current RTK commands from JSON file
      File file = SPIFFS.open("/rtkcommands.json", "r");
      if (!file) {
        log::toAll("Failed to open rtkcommands.json");
        return;
      }
      String jsonString = file.readString();
      file.close();
      log::toAll("=== Current RTK Commands Configuration ===");
      int startIndex = 0;
      int endIndex = jsonString.indexOf('\n');
      while (endIndex != -1) {
        String line = jsonString.substring(startIndex, endIndex);
        log::toAll(line);
        startIndex = endIndex + 1;
        endIndex = jsonString.indexOf('\n', startIndex);
      }
      if (startIndex < jsonString.length()) {
        String lastLine = jsonString.substring(startIndex);
        log::toAll(lastLine);
      }
      log::toAll("Usage: rtkconfig <command> [period]");
    }
    return;
  }
#endif
  if (command.startsWith("rtkstat")) {
#ifdef RTK_TIMO
    if (pRTK) {
      // Print RTK status information
      log::toAll("=== RTK Status ===");
      log::toAll("Orientation: " + String(pBD->rtkOrientation));
      log::toAll("Antenna A Status: " + String(pRTK->antennaAstat));
      log::toAll("Antenna B Status: " + String(pRTK->antennaBstat));
      log::toAll("Baseline Length: " + String(pRTK->baseLen));
      log::toAll("GPS Time: " + String(pRTK->GPStime));
      log::toAll("Pitch: " + String(pRTK->pitch) + "°");
      log::toAll("Roll: " + String(pRTK->roll) + "°");
      log::toAll("Heading: " + String(pRTK->heading) + "°");
      log::toAll("Position Accuracy: " + String(pRTK->pAcc));
      log::toAll("Roll Accuracy: " + String(pRTK->rAcc));
      log::toAll("Heading Accuracy: " + String(pRTK->hAcc));
      log::toAll("Used Satellites: " + String(pRTK->usedSV));
      log::toAll("RTK Quality: " + String(pRTK->RTKqual));
      
      // Print HandleRTKSTATUS() variables
      log::toAll("=== RTK Status Details ===");
      log::toAll("GPS Source: " + String(pRTK->GPSSource, BIN));
      log::toAll("BDS Source 1: " + String(pRTK->BDSSource1, BIN));
      log::toAll("BDS Source 2: " + String(pRTK->BDSSource2, BIN));
      log::toAll("GLONASS Source: " + String(pRTK->GLOSource, BIN));
      log::toAll("Galileo Source 1: " + String(pRTK->GALSource1, BIN));
      log::toAll("Galileo Source 2: " + String(pRTK->GALSource2, BIN));
      log::toAll("QZSS Source: " + String(pRTK->QZSSSource, BIN));
      log::toAll("Position Type: " + String(pRTK->PositionType));
      log::toAll("Ion Detected: " + String(pRTK->IonDetected));
      log::toAll("Dual RTK Flag: 0x" + String(pRTK->DualRtkFlag, HEX));
    }
    return;
#else   // not RTK_TIMO
    log::toAll("not processing RTK with Timo; try 'gps'");
#endif
  } // rtkstat
  if (command.equals("boat")) {
    log::toAll("=== Boat Data ===");
    log::toAll("    BNO Heading: " + String(pBD->BNOheading) + "°");
    log::toAll("    RTK Heading: " + String(pBD->RTKheading) + "°");
    log::toAll("             SOG: " + String(pBD->SOG*MTOKTS) + " kts");
    log::toAll("             COG: " + String(pBD->COG) + "°");
    log::toAll("       Variation: " + String(pBD->Variation) + "°");
    log::toAll("Magnetic Heading: " + String(pBD->magHeading) + "°");
    log::toAll("             STW: " + String(pBD->STW*MTOKTS) + " kts");
    log::toAll("             AWA: " + String(pBD->AWA));
    log::toAll("             AWS: " + String(pBD->AWS));
    log::toAll("       rotateout: " + String(rotateout));
    log::toAll("             TWS: " + String(pBD->TWS*MTOKTS) + " kts");
    log::toAll("             TWA: " + String(pBD->TWA) + "°");
    log::toAll("             TWD: " + String(pBD->TWD) + "°");
    log::toAll("   VMG (to Wind): " + String(pBD->VMG*MTOKTS) + " kts");
    log::toAll("         Max TWS: " + String(pBD->maxTWS*MTOKTS) + " kts");
    log::toAll("             XTE: " + String(pBD->XTE) + "m");
    log::toAll("Distance to WPT: " + String(pBD->DistanceToWaypoint) + "m");
    log::toAll(" Bearing to WPT: " + String(pBD->BearingToWaypoint) + "°");
    log::toAll("   Waypoint Num: " + String(pBD->WaypointNumber));
    log::toAll("    Perp Crossed: " + String(pBD->PerpendicularCrossed ? "true" : "false"));
    log::toAll("Arrival Circle: " + String(pBD->ArrivalCircleEntered ? "true" : "false"));
    log::toAll("  Waypoint Lat: " + String(pBD->WaypointLatitude, 6));
    log::toAll("  Waypoint Lon: " + String(pBD->WaypointLongitude, 6));
    if (bmeFound) {
      log::toAll("   Temperature: " + String(pENV->temp) + "°");
      log::toAll("      Pressure: " + String(pENV->pressure) + " hPa, " + String(pENV->humidity*0.02953) + " in Hg");
      log::toAll("      Humidity: " + String(pENV->humidity) + "%");
    }
    return;
  }
  if (command.startsWith("gps")) {
#ifdef RTK
    log::toAll("GPS Time: " + String(pBD->GPSTime) + " secs since midnight");
    log::toAll("Latitude: " + String(pBD->Latitude, 6));
    log::toAll("Longitude: " + String(pBD->Longitude, 6));
    log::toAll("Altitude: " + String(pBD->Altitude) + " m");
    log::toAll("HDOP: " + String(pBD->HDOP));
    log::toAll("Geoidal Separation: " + String(pBD->GeoidalSeparation) + " m");
    log::toAll("DGPS Age: " + String(pBD->DGPSAge) + " s");
    log::toAll("GPS Quality: " + String(fixQuality[pBD->GPSQuality]));
    log::toAll("Satellite Count: " + String(pBD->SatelliteCount));
#ifdef RTK_TIMO
    log::toAll("pSAT Sat count: " + String(pSAT->satelliteCount) + " " + String(pSAT->total_count));
#endif
    log::toAll("DGPS Reference Station ID: " + String(pBD->DGPSReferenceStationID));
    log::toAll("MOB Activated: " + String(pBD->MOBActivated ? "true" : "false"));
#endif
    return;
  }
#ifdef RTK_TIMO
  if (command.startsWith("gsv")) {
    if (pSAT == nullptr) {
      log::toAll("Satellite data not available");
      return;
    }
    
    log::toAll("=== Satellite Status ===");
    log::toAll("Last Update: " + String((now - pSAT->last_update) / 1000) + " seconds ago");
    log::toAll("Total Satellites: " + String(pSAT->total_count));
    
    // Display constellation counts
    log::toAll("");
    log::toAll("=== Constellation Counts ===");
    log::toAll("GPS (GP):      " + String(pSAT->GPS_count) + " satellites");
    log::toAll("GLONASS (GL):  " + String(pSAT->GLONASS_count) + " satellites");
    log::toAll("Galileo (GA):  " + String(pSAT->Galileo_count) + " satellites");
    log::toAll("BeiDou (GB):   " + String(pSAT->BeiDou_count) + " satellites");
    log::toAll("QZSS (GQ):     " + String(pSAT->QZSS_count) + " satellites");
    
    // Display detailed satellite lists
    log::toAll("");
    log::toAll("=== Satellites in View ===");
    
    // GPS satellites
    if (pSAT->GPS_count > 0) {
      String gpsList = "GPS: ";
      for (int i = 1; i <= 32; i++) {
        if (pSAT->GPS_sats_low & (1UL << (i - 1))) {
          gpsList += String(i) + " ";
        }
      }
      for (int i = 33; i <= 64; i++) {
        if (pSAT->GPS_sats_high & (1UL << (i - 33))) {
          gpsList += String(i) + " ";
        }
      }
      log::toAll(gpsList);
    }
    
    // GLONASS satellites
    if (pSAT->GLONASS_count > 0) {
      String gloList = "GLONASS: ";
      for (int i = 65; i <= 96; i++) {
        if (pSAT->GLONASS_sats & (1UL << (i - 65))) {
          gloList += String(i) + " ";
        }
      }
      log::toAll(gloList);
    }
    
    // Galileo satellites
    if (pSAT->Galileo_count > 0) {
      String galList = "Galileo: ";
      for (int i = 1; i <= 32; i++) {
        if (pSAT->Galileo_sats & (1UL << (i - 1))) {
          galList += String(i) + " ";
        }
      }
      log::toAll(galList);
    }
    
    // BeiDou satellites
    if (pSAT->BeiDou_count > 0) {
      String bdsList = "BeiDou: ";
      for (int i = 1; i <= 32; i++) {
        if (pSAT->BeiDou_sats_low & (1UL << (i - 1))) {
          bdsList += String(i) + " ";
        }
      }
      for (int i = 33; i <= 63; i++) {
        if (pSAT->BeiDou_sats_high & (1UL << (i - 33))) {
          bdsList += String(i) + " ";
        }
      }
      log::toAll(bdsList);
    }
    
    // QZSS satellites
    if (pSAT->QZSS_count > 0) {
      String qzssList = "QZSS: ";
      for (int i = 193; i <= 202; i++) {
        if (pSAT->QZSS_sats & (1UL << (i - 193))) {
          qzssList += String(i) + " ";
        }
      }
      log::toAll(qzssList);
    }
    
    // Display bit field values for debugging
    log::toAll("");
    log::toAll("=== Raw Bit Fields (Hex) ===");
    log::toAll("GPS Low:       0x" + String(pSAT->GPS_sats_low, HEX));
    log::toAll("GPS High:      0x" + String(pSAT->GPS_sats_high, HEX));
    log::toAll("GLONASS:       0x" + String(pSAT->GLONASS_sats, HEX));
    log::toAll("Galileo:       0x" + String(pSAT->Galileo_sats, HEX));
    log::toAll("BeiDou Low:    0x" + String(pSAT->BeiDou_sats_low, HEX));
    log::toAll("BeiDou High:   0x" + String(pSAT->BeiDou_sats_high, HEX));
    log::toAll("QZSS:          0x" + String(pSAT->QZSS_sats, HEX));
    
    return;
  }
  if (command.startsWith("sat")) {
    if (pSAT == nullptr) {
      log::toAll("Satellite data not available");
      return;
    }
    
    log::toAll("=== Satellite Details ===");
    log::toAll("Last Update: " + String((now - pSAT->last_update) / 1000) + " seconds ago");
    log::toAll("Active Satellites: " + String(pSAT->satelliteCount));
    log::toAll("");
    
    if (pSAT->satelliteCount == 0) {
      log::toAll("No satellites currently tracked");
      return;
    }
    
    // Display header
    log::toAll("SVID  Const   Elev  Azim   SNR  Age(s)");
    log::toAll("----  -----   ----  ----   ---  ------");
    
    // Sort satellites by constellation for better display
    for (uint8_t constellation = 0; constellation < 5; constellation++) {
      
      for (int i = 0; i < pSAT->satelliteCount; i++) {
        if (pSAT->satellites[i].constellation == constellation) {
          unsigned long age = (now - pSAT->satellites[i].lastSeen) / 1000;
          
          // Format: SVID(4) Const(5) Elev(4) Azim(4) SNR(3) Age(6)
          String line = "";
          line += String(pSAT->satellites[i].SVID);
          while (line.length() < 4) line += " ";
          
          line += "  " + String(pSAT->getConstellationName(pSAT->satellites[i].constellation));
          while (line.length() < 12) line += " ";
          
          line += String(pSAT->satellites[i].Elevation) + "°";
          while (line.length() < 18) line += " ";
          
          line += String(pSAT->satellites[i].Azimuth) + "°";
          while (line.length() < 25) line += " ";
          
          line += String(pSAT->satellites[i].SNR);
          // 40 is min for RTK fix
          if (pSAT->satellites[i].SNR > 39) line += "*";
          while (line.length() < 30) line += " ";
          
          line += String(age) + "s";
          
          log::toAll(line);
        }
      }
      WebSerial.flush();
    }
    
    log::toAll("");
    log::toAll("Constellation Summary:");
    log::toAll("GPS: " + String(pSAT->GPS_count) + ", GLONASS: " + String(pSAT->GLONASS_count) +
               ", Galileo: " + String(pSAT->Galileo_count) + ", BeiDou: " + String(pSAT->BeiDou_count) +
               ", QZSS: " + String(pSAT->QZSS_count));
    log::toAll("Sat count: " + String(pSAT->satelliteCount) + " " + String(pSAT->total_count));
    
    return;
  }
#endif
#ifdef BNO08X
  if (command.startsWith("comp")) {
          log::toAll("heading: " + String(pBD->magHeading) + "° accuracy: " + String(compass.boatAccuracy*RADTODEG,0) + "°"
        + " calstatus: " + String(compass.boatCalStatus) + "/" + calStatus[compass.boatCalStatus]
        + " total reports: " + String(compass.totalReports) + " error count: " + String(compass.headingErrCount));
    return;
  }
#endif
  if (command.startsWith("n2k")) {
    log::toAll("main bus messages: " + String(num_n2k_messages));
    log::toAll("    wind messages: " + String(num_wind_messages));
    log::toAll("other on wind bus: " + String(num_wind_other));
    log::toAll("        transmits: " + String(num_xmit));
    log::toAll("            fails: " + String(num_fail_xmit));
    log::toAll(" PGN  :  count :   age");
    for (int i=0; i<trackedPGNCount; i++) {
      sprintf(prbuf,"%6lu:%8lu:%8lu", pgnTracker[i].pgn, pgnTracker[i].count, pgnTracker[i].age);
      log::toAll(prbuf);
    }
    return;
  }
  if (command.startsWith("sens")) {
    if (totalWords > 1 && !words[++i].isEmpty()) {
      if (words[i].startsWith("lo"))
        lowSet = atoi(words[++i].c_str());
      else if (words[i].startsWith("hi"))
        highSet = atoi(words[++i].c_str());
    }
    log::toAll("mast sensor low: " + String(lowSet) + " high: " + String(highSet));
    return;
  }
  if (command.startsWith("orie")) {
    if (totalWords > 1 && !words[++i].isEmpty()) {
      // magnetic compass orientation
      // adjust to match RTK compass, or motor in a straight line with no current and adjust to match COG
      if (words[i].startsWith("comp")) {  
        int magOrientation = atoi(words[++i].c_str());
        preferences.putInt("magOrientation",magOrientation);
        pBD->magOrientation = magOrientation;
        log::toAll("App handler setting magOrientation to: " + words[i]);
      } else 
      // honeywell (or other) mast rotation sensor orientation
      // adjustment if readAnalogRotationValue() does not return 0 when mast is centered
      if (words[i].startsWith("honey")) { 
        int sensOrientation = atoi(words[++i].c_str());
        preferences.putInt("sensOrientation",sensOrientation);
        pBD->sensOrientation = sensOrientation;
        log::toAll("App handler setting sensOrientation to: " + words[i]);
      } else
      // RTK antenna plane orientation
      if (words[i].startsWith("rtk")) {
        int rtkOrientation = atoi(words[++i].c_str());
        preferences.putInt("rtkOrientation",rtkOrientation);
        pBD->rtkOrientation = rtkOrientation;
        log::toAll("App handler setting rtkOrientation to: " + words[i]);
      }
    ; // null to handle else clause with #ifdefs
    } else {
      log::toAll("compass orientation: " + String(pBD->magOrientation));
      log::toAll("mast sensor orientation: " + String(pBD->sensOrientation));
      log::toAll("RTK antenna orientation: " + String(pBD->rtkOrientation));
      //log::toAll("Usage: orient [compass|honey|rtk] number (degrees)");
    }
    return;
  }
  if (command.equals("build")) {
    log::toAll("=== Build Information ===");
    log::toAll("Build Timestamp: " + String(BUILD_TIMESTAMP));
    log::toAll("Build Date: " + String(BUILD_DATE));
    log::toAll("Git Hash: " + String(GIT_HASH));
    log::toAll("Boot Count: " + String(bootCount));
    log::toAll("Uptime: " + String(now/1000) + " seconds");
    return;
  }
#ifdef SEATALK
  if (command.startsWith("sea")) { // seatalk debug
    log::toAll("STdebug: " + String(STdebug ? "enabled" : "disabled"));
    log::toAll("sendSeaTalk: " + String(sendSeaTalk ? "enabled" : "disabled"));
    return;
  }
#endif
  // Add more application-specific commands here
  log::toAll("Unknown app command: " + command);
}

// toggle switches
void myToggleHandler(String* words, int totalWords) {
  String toggle = words[0];
  if (toggle.startsWith("corr")) {  // toggle correction
    windToggle = !windToggle;
    log::toAll("windToggle " + String(windToggle ? "on" : "off"));
    preferences.putBool("WindToggle",windToggle);
    return;
  }
  if (toggle.startsWith("wind")) {  // toggle wind debug
    debugWind = !debugWind;
    log::toAll("debugWind " + String(debugWind ? "on" : "off"));
    preferences.putBool("debugWind",debugWind);
    return;
  }
  if (toggle.startsWith("disp")) {
    displayOnToggle = !displayOnToggle;
    turnoff();
    log::toAll("display " + String(displayOnToggle ? "on" : "off"));
    return;
  }
#ifdef RTK
  if (toggle.startsWith("rtk")) {
    debugRTK = !debugRTK;
    log::toAll("debugRTK: " + String(debugRTK ? "enabled" : "disabled"));
    return;
  }
  if (toggle.startsWith("direc")) {
    directRTK = !directRTK;
    log::toAll("directRTK: " + String(directRTK ? "enabled" : "disabled"));
    if (directRTK) 
      log::toAll("Sending RTK messages to serial not NMEA0183Handlers");    return;
  }
#endif
#ifdef BNO08X
  if (toggle.startsWith("comp")) {
    debugCompass = !debugCompass;
    log::toAll("debugCompass: " + String(debugCompass ? "enabled" : "disabled"));
    return;
  }
#endif
// not sure which ifdef this should be in, so will probably break when I change configuration
// RTK heading should probably be false because I am sending it on a TCP port
// BNO heading should probably be true if I want to track it in SK/database
  if (toggle.startsWith("head")) {
    if (totalWords > 1 && !words[1].isEmpty()) {
      if (words[1].startsWith("bno"))
        xmitBNOheading = !xmitBNOheading;
      if (words[1].startsWith("rtk"))
        xmitRTKheading = !xmitRTKheading;
    }
    log::toAll("xmit BNO heading (" + String(pBD->BNOheading) + "): " + String(xmitBNOheading));
    log::toAll("xmit RTK heading (" + String(pBD->RTKheading) + "): " + String(xmitRTKheading));
    return;
  }
#ifdef N2K
  if (toggle.startsWith("n2k")) {
    debugN2K = !debugN2K;
    log::toAll("debugN2K: " + String(debugN2K ? "enabled" : "disabled"));
    return;
  }
#endif
#ifdef NMEA0183
  if (toggle.startsWith("nmea")) {
    debugNMEA = !debugNMEA;
    DebugNMEA0183Handlers(&Serial);
    log::toAll("debugNMEA: " + String(debugNMEA ? "enabled" : "disabled"));
    return;
  }
#endif
#ifdef SEATALK
  if (toggle.startsWith("sea")) { // toggle seatalk debug
    STdebug = !STdebug;
    log::toAll("STdebug: " + String(STdebug ? "enabled" : "disabled"));
    return;
  }
  if (toggle.startsWith("tiller")) { // toggle sending Seatalk to tiller pilot
    sendSeaTalk = !sendSeaTalk;
    log::toAll("sendSeaTalk: " + String(sendSeaTalk ? "enabled" : "disabled"));
    return;
  }
#endif
#ifdef TCP_FORWARD
  if (toggle.startsWith("tcp")) {
    TCPdebug = !TCPdebug;
    log::toAll("TCPdebug: " + String(TCPdebug ? "enabled" : "disabled"));
    return;
  }
#endif
  // Add more application-specific toggles here
  log::toAll("Unknown toggle: " + toggle);
}

// Define the handler function pointers that webserial.cpp expects
Handler appHandler = myAppHandler;
Handler togHandler = myToggleHandler;

#endif // APPHANDLER
#endif