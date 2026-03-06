#ifdef N2K // using Timo libraries to process N2K as opposed to raw CAN bus
#include "include-general.h"
#include "windparse.h"
#include "BoatData.h"

tN2kMsg n2kMsg;

// if we see wind PGNs on the main bus, this indicates that either
// a) there is only one bus or 
// b) wind sensor is on main bus, so we should use that data instead
// however, wind bus should have priority
bool windOnMain = false;

bool mainXmitWind(double windSpeed, int windAngle) {
  if (debugN2K)
    log::toAll("sending aws " + String(windSpeed, 1) + " awa " + String(windAngle));
  pBD->AWS = windSpeed;
  pBD->AWA = windAngle;
  readings["awa"] = String(windAngle);
  readings["aws"] = String(windSpeed*MTOKTS);
  SetN2kPGN130306(n2kMsg, 0xFF, WindSensor::windSpeedMeters, windAngle*DEGTORAD, N2kWind_Apparent); 
  return n2kMain->SendMsg(n2kMsg);
}

bool mainXmitOther(int PGN, uint8_t SRC, unsigned char SID, byte cdata[], uint8_t len, unsigned long ID) {
  if (debugN2K)
    log::toAll("sending wind bus frame on main, PGN " + String(PGN));
  uint8_t priority = (ID >> 26) & 0x07;
  uint8_t destination = (ID >> 8) & 0xFF;
  n2kMsg.Init(priority, PGN, SRC, destination);
  n2kMsg.MsgTime = now;
  // Copy raw frame data directly into N2K message
  n2kMsg.AddBuf(cdata, len);
  return n2kMain->SendMsg(n2kMsg);
}

void parseSTW(const tN2kMsg &N2kMsg) {
  unsigned char SID;  
  if (debugN2K) log::toAll("speed through water PGN 128259");
  double SpeedWaterMeters;
  double SpeedGroundMeters;
  tN2kSpeedWaterReferenceType SWRT;
  if (ParseN2kPGN128259(N2kMsg, SID, SpeedWaterMeters, SpeedGroundMeters, SWRT)) {
    pBD->STW = SpeedWaterMeters; // KEEP METERS PER SECOND even though it's annoyingly inconsistent
    readings["stw"] = String(SpeedWaterMeters * MTOKTS);
    if (debugN2K) log::toAll("STW updated: " + String(SpeedWaterMeters * MTOKTS, 1) + " kts");
  }
}

void ProcessN2kMessage(const tN2kMsg &N2kMsg) {
    unsigned char SID;  
    switch (N2kMsg.PGN) {
      case 128259L: {
        parseSTW(N2kMsg);
        break;
      }
      case 130306L: {
        num_wind_messages++;
        // wind PGN on main bus indicates wind sensor is here
        // NOTE I am NOT correcting wind for rotation if it's on the main bus!!!
        if (!windOnMain) {
          if (debugN2K) log::toAll("MAIN: Wind on main bus, toggling");
          windOnMain = true;
        }
        double windSpeedMeters;
        double windAngleRads;
        tN2kWindReference wRef;
        if (ParseN2kPGN130306(N2kMsg, SID, WindSensor::windSpeedMeters, WindSensor::windAngleRadians, wRef)) {
          if (wRef != N2kWind_Apparent) { // N2kWind_Apparent
            //Serial.printf("got MAIN wind PGN not apparent: %d\n",wRef);
            return;
          }
          WindSensor::windSpeedKnots = WindSensor::windSpeedMeters * 1.943844; // convert m/s to kts
          WindSensor::windAngleDegrees = WindSensor::windAngleRadians * RADTODEG;
          pBD->AWS = WindSensor::windSpeedMeters;
          pBD->AWA = WindSensor::windAngleDegrees;
        }
        calcWindFrequency();
        // no need to forward n2k
        // but send to autopilot
#ifdef SEATALK
        // NOTE: right now I'm sending at a rate of 20Hz (ST_RATE) but I could switch back to direct send
        //sendSTwind();
        sendSeaTalk = true;
#endif        
        break;
      }
#ifdef SEATALK
      case 129283L: { // XTE - for autopilot
        tN2kXTEMode XTEMode;
        bool NavTerm;
        if (ParseN2kPGN129283(N2kMsg, SID, XTEMode, NavTerm, pBD->XTE)) {
          sendSeaTalk = true;
          if (debugN2K) log::toAll("XTE: " + String(pBD->XTE));
        }
        break;
      }
      case 129284L: { // Nav Data - for autopilot
        tN2kHeadingReference BearingReference;
        tN2kDistanceCalculationType CalculationType;
        double ETATime;
        int16_t ETADate;
        double BearingOriginToDestinationWaypoint;
        uint32_t OriginWaypointNumber;
        double WaypointClosingVelocity;
        if (ParseN2kPGN129284(N2kMsg, SID, pBD->DistanceToWaypoint, BearingReference, pBD->PerpendicularCrossed, pBD->ArrivalCircleEntered, CalculationType, ETATime, ETADate, BearingOriginToDestinationWaypoint, pBD->BearingToWaypoint, OriginWaypointNumber, pBD->WaypointNumber, pBD->WaypointLatitude, pBD->WaypointLongitude, WaypointClosingVelocity)) {
          sendSeaTalk = true;
          if (debugN2K) log::toAll("Nav data destination lat: " + String(pBD->WaypointLatitude) + " lon: " + String(pBD->WaypointLongitude));
        }
        break;
      }
      // Add other PGNs here as needed
      default:
        // Unknown PGN - no local processing needed
        break;
#endif
      }
#ifdef SEATALKXXX // temporarily moving these functions to tcp_sender
    if (sendSeaTalk) {
      sendRMB();
      sendAPB();
    }
#endif
}

void HandleMainN2kMessage(const tN2kMsg &N2kMsg) {
  num_n2k_messages++;
  if (debugN2K) log::toAll("MAIN bus PGN " + String(N2kMsg.PGN));
  // Update PGN tracking
  int index = findOrCreatePGNEntry(N2kMsg.PGN);
  if (index >= 0) {
    pgnTracker[index].count++;
    pgnTracker[index].age = now;
  }
  // Use unified processing function
  ProcessN2kMessage(N2kMsg);
}
#endif
