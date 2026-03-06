#include "include-general.h"
#include "windparse.h"
#include "BoatData.h"

tN2kMsg correctN2kMsg;
float mastRotate;

void HandleWindN2KMessage(const tN2kMsg &N2kMsg) {
  if (debugWind) log::toAll("WIND bus PGN " + String(N2kMsg.PGN));
    unsigned char SID;  
    switch (N2kMsg.PGN) {
    case 130306L: {
      num_wind_messages++;
      //calcWindFrequency();
      // wind PGN on wind bus indicates wind sensor is here
      if (windOnMain) {
        if (debugN2K) log::toAll("WIND: wind on wind bus, toggling");
        windOnMain = false;
      }
      double windSpeedMeters;
      double windAngleDegrees, windAngleRads;
      tN2kWindReference wRef;
      if (ParseN2kPGN130306(N2kMsg, SID, WindSensor::windSpeedMeters, windAngleRads, wRef)) {
        if (wRef != N2kWind_Apparent) { // N2kWind_Apparent
          if (debugN2K) log::toAll("got WIND wind PGN not apparent: " + String(wRef));
          // most likely wind true relative to boat from SK Derived Data
          return;
        }
        windAngleDegrees = windAngleRads*RADTODEG;
      }
      if (windToggle) {
        // send corrected wind on main bus
        // note we are sending the original speed reading in m/s
        // and the AWA converted from rads to degrees, corrected, and converted back to rads
        WindSensor::windSpeedKnots = WindSensor::windSpeedMeters * 1.943844; // convert m/s to kts
        pBD->AWS = WindSensor::windSpeedMeters;
        rotateout = windCorrect(windAngleDegrees);  // Get correction amount (mast rotation)
        WindSensor::windAngleDegrees = windAngleDegrees + rotateout;  // Apply correction to get corrected angle
        
        // Normalize corrected angle to 0-359 degrees
        if (WindSensor::windAngleDegrees < 0) {
          WindSensor::windAngleDegrees += 360;
        } else if (WindSensor::windAngleDegrees >= 360) {
          WindSensor::windAngleDegrees -= 360;
        }
        
        pBD->AWA = WindSensor::windAngleDegrees;
        //log::toAll("corrected wind, original angle " + String(windAngleDegrees) + " corrected " + String(WindSensor::windAngleDegrees));
        SetN2kPGN130306(correctN2kMsg, 0xFF, WindSensor::windSpeedMeters, WindSensor::windAngleDegrees*DEGTORAD, N2kWind_Apparent);
        readings["awa"] = String(WindSensor::windAngleDegrees);
        readings["aws"] = String(WindSensor::windSpeedKnots);
        if (n2kMain->SendMsg(correctN2kMsg)) {
          num_wind_xmit++;
          if (debugWind) log::toAll("sent corrected wind on main bus, original angle: " + String(windAngleDegrees) + " mast rot: " + String(mastRotate,0) + " corrected: " + String(WindSensor::windAngleDegrees));
        } else {
          num_wind_fail++;
          if (debugWind) log::toAll("failed to forward corrected wind on main bus");
        }
#ifdef SEATALK
        //sendSTwind();
        sendSeaTalk = true;
#endif
      } else {
        // windToggle false; forward uncorrected wind
        if (n2kMain->SendMsg(N2kMsg)) {
          num_wind_xmit++;
          if (debugWind && 0) log::toAll("sent uncorrected wind on main bus");
        } else {
          num_wind_fail++;
          if (debugWind) log::toAll("failed to forward uncorrected wind on main bus");
        }
      break;
      }
    }
    case 128259L: {
      // Speed through water - just in case transducer is on wind bus
      // we need to parse to get stw for vmg calculations
      parseSTW(N2kMsg);
      if (n2kMain->SendMsg(N2kMsg)) {
        num_wind_xmit++;
        if (debugWind && 0) log::toAll("sent STW on main bus");
      } else {
        num_wind_fail++;
        if (debugWind) log::toAll("failed to forward STW on main bus");
      }
      break;
    }
    // Add other PGNs here as needed
    default: {
      // other PGN - forward to main bus
      if (n2kMain->SendMsg(N2kMsg)) {
        num_wind_xmit++;
        if (debugWind && 0) log::toAll("sent other on main bus PGN:" + String(N2kMsg.PGN));
      } else {
        num_wind_fail++;
        if (debugWind) log::toAll("failed to forward other on main bus PGN:" + String(N2kMsg.PGN));
      }
      break;
    }
  }
}

// correct wind angle base on mast rotation
// angle in DEGREES
// Returns the correction amount (mastRotate), not the corrected angle
int windCorrect(double angle) {
  //Serial.println("windCorrect");
  // read rotation value and correct
  mastRotate = 0.0;
#ifdef HONEY
  if (honeywellOnToggle) {
    mastRotate = readAnalogRotationValue();
  }
#endif
  // Return just the correction amount (mast rotation)
  return (int)mastRotate;
}


unsigned long lastWindPacketTime = 0;
bool windFreqInitialized = false;
movingAvg arrivalRate(100);
float currentWindFreq = 0.0;  // Current frequency in Hz

void initWindFrequencyMeasurement() {
  arrivalRate.begin();
  windFreqInitialized = true;
  lastWindPacketTime = now;
}

void calcWindFrequency() {
  // measure average arrival frequency
  // now is already available as global variable
  if (windFreqInitialized && lastWindPacketTime > 0) {
    unsigned long interval = now - lastWindPacketTime;
    if (interval > 0 && interval < 10000) {  // Ignore intervals > 10 seconds (likely startup/reset)
      // Calculate frequency: 1000ms / interval = Hz
      float instantFreq = 1000.0 / interval;
      // Add to moving average (convert to centi-Hz to avoid floating point in movingAvg)
      int freqCentiHz = (int)(instantFreq * 100);
      int avgFreqCentiHz = arrivalRate.reading(freqCentiHz);
      // Convert back to Hz
      currentWindFreq = avgFreqCentiHz / 100.0;
      #ifdef DEBUG
      if (num_wind_messages % 20 == 0) {  // Log every 20th packet to avoid spam
        log::toAll("Wind freq: " + String(instantFreq, 2) + " Hz (avg: " + 
                  String(currentWindFreq, 2) + " Hz, interval: " + String(interval) + " ms)");
      }
      #endif
    }
  }
  lastWindPacketTime = now; 
}

