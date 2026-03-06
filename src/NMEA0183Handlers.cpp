#ifdef NMEA0183

#include "include-general.h"
#include "NMEA0183Handlers.h"
#include "windparse.h"
#include "BoatData.h"
#include "logto.h"
#include <string.h>

bool teleplot=true;
// transmit RTK heading on CAN/N2K bus?
// currently false since we are sending it on TCP port to SK/UM982 plugin
bool xmitRTKheading=false;

unsigned int num_0183_messages;

tNMEA0183Msg NMEA0183Msg;

// WITmotion RTK output:
// $GPGSV,3,1,10,22,76,043,20,17,75,121,22,14,55,074,25,19,53,194,24,1*60 (multi-part)
// $GNGGA,010709.000,4740.262183,N,12219.447475,W,1,17,1.12,91.5,M,-17.3,M,,*7D
// $GNRMC,010709.000,A,4740.262183,N,12219.44
// $GLGSV,1,1,03,79,70,027,14,81,50,131,17,78,29,094,25,1*45 // Glonass
// $GAGSV,2,1,07,25,81,345,22,03,38,235,15,24,36,076,20,08,32,301,17,7*7E // Galileo
// $GBGSV,2,1,07,24,80,092,21,44,57,123,16,12,44,160,15,25,42,297,15,1*78 // Beidou
// $GQGSV,1,1,00,1*65 // QZSS
// $GNGSA,A,3,22,17,14,19,30,02,21,,,,,,1.39,1.12,0.82,1*0B
// $GNGLL,4740.262183,N,12219.447475,W,010709.000,A,A*59
// $GNVTG,242.46,T,,M,0.078,N,0.144,K,A*2B
// $PQTMANTENNASTATUS,2,2,2,2,2*4D
// $PQTMTAR,1,010709.000,0,,0.000,1.985139,-0.924438,,0.003247,0.003242,,00*47

// IC-M330 output:
// RMC $GPRMC	Time, date, position, course, speed data.
// GSA $GPGSA	GPS receiver operating mode, satellites used in the position solution, DOP values.
// GSV $GPGSV	Number of satellites in view, satellite ID numbers, elevation, azimuth, SNR values.
//
// old bridge sentences:
// 129026: COGSOGRapid 
// 129029: GNSS Position Data 
// 129540: SatelliteInfo 
// 127258: Magnetic Variation 
// 129025: SetLatLonRapid 

// configuration of UM-982:
// unlog: stop all logging on current port
// config: show configuration
// saveconfig
// uniloglist
// freset: factory reset
// gpgga com1 1
// mode heading2 lowdynamic
// gphpr com1 1: Heading Pitch Roll on com1 every second
/*
#VERSION,94,GPS,FINE,2397,455389900,0,0,18,94;UM982,R4.10Build11826,HRPT00-S10C-P,2310415000012-LR23A1224521419,ff3ba998aa74755b,2023/11/24*c032a6e7

$CONFIG,ANTENNA,CONFIG ANTENNA POWERON*7A
$CONFIG,NMEAVERSION,CONFIG NMEAVERSION V410*47
$CONFIG,RTK,CONFIG RTK TIMEOUT 600*69
$CONFIG,RTK,CONFIG RTK RELIABILITY 3 1*76
$CONFIG,PPP,CONFIG PPP TIMEOUT 120*6C
$CONFIG,HEADING,CONFIG HEADING RELIABILITY 3*67
$CONFIG,HEADING,CONFIG HEADING FIXLENGTH*6F
$CONFIG,HEADING,CONFIG HEADING LENGTH 0.00 0.00*38
$CONFIG,DGPS,CONFIG DGPS TIMEOUT 600*69
$CONFIG,RTCMB1CB2A,CONFIG RTCMB1CB2A ENABLE*25
$CONFIG,ANTENNADELTAHEN,CONFIG ANTENNADELTAHEN 0.0000 0.0000 0.0000*3A
$CONFIG,PPS,CONFIG PPS ENABLE GPS POSITIVE 500000 1000 0 0*6E
$CONFIG,SIGNALGROUP,CONFIG SIGNALGROUP 3 6*01
$CONFIG,ANTIJAM,CONFIG ANTIJAM AUTO*2B
$CONFIG,AGNSS,CONFIG AGNSS DISABLE*70
$CONFIG,BASEOBSFILTER,CONFIG BASEOBSFILTER DISABLE*70
$CONFIG,COM1,CONFIG COM1 115200*23
*** currently connected to COM2 ***
$CONFIG,COM2,CONFIG COM2 115200*23
$CONFIG,COM3,CONFIG COM3 115200*23
*/
// Predefinition for functions to make it possible for constant definition for NMEA0183Handlers
void HandleRMC(const tNMEA0183Msg &NMEA0183Msg);
void HandleGGA(const tNMEA0183Msg &NMEA0183Msg);
void HandleGSV(const tNMEA0183Msg &NMEA0183Msg);
void HandleGSA(const tNMEA0183Msg &NMEA0183Msg);
void HandleVTG(const tNMEA0183Msg &NMEA0183Msg);
void HandleGLL(const tNMEA0183Msg &NMEA0183Msg);
#ifdef RTK_HYBRID
void HandleHPR(const tNMEA0183Msg &NMEA0183Msg);
#endif
#ifdef RTK_TIMO
void HandleVTG(const tNMEA0183Msg &NMEA0183Msg);
void HandleHEADINGSTATUS(const tNMEA0183Msg &NMEA0183Msg);
void HandleRTKSTATUS(const tNMEA0183Msg &NMEA0183Msg);
#endif

// Internal variables
Stream* NMEA0183HandlersDebugStream=nullptr;
struct tGSV SatInfo[5];
struct tGSV GSVseen[MAXSAT];  
//struct tSatelliteInfo[MaxSatelliteInfoCount];
//struct tSatelliteInfo[8];
bool debugNMEA = false;
bool debugRTK = false;

// RTK quality change tracking variables
static int previousRTKqual = -1;  // Initialize to invalid value
static unsigned long lastRTKqualChangeTime = 0;  // Time of last quality change

const char* fixQuality[] = {"Invalid", "Single Point", "Differential", "GPS PPS Mode", "RTK Int/Fix (best)", "RTK Float (good)", "DR Mode", "Manual", "Extra wide", "SBAS", "unknown"};

#ifdef RTK_HYBRID
tNMEA0183Handler NMEA0183Handlers[]={
  {"HPR",&HandleHPR, 0}, // Heading, pitch, roll
  {"GGA",&HandleGGA, 0}, // UTC Time, Latitude, Direction of Latitude, Longitude, Direction of Longitude, GPS Quality Indicator, Number of Satellites in Use, HDOP, Altitude, Unit of Altitude, Geoidal Separation, Unit of Geoidal Separation, Age of Differential GPS Data, Differential Reference Station ID
  {"RMC",&HandleRMC, 0}, // Position, Velocity, and Time
  {0,0,0}
};
#else
tNMEA0183Handler NMEA0183Handlers[]={
  {"GSV",&HandleGSV, 0}, // Number of SVs in view, PRN, elevation, azimuth, and SNR
  {"GSA",&HandleGSA, 0}, // GPS DOP and active satellites
  {"VTG",&HandleVTG, 0}, // track made good (course over ground) and the speed over ground
  {"GLL",&HandleGLL, 0}, // GPS lat lon
  {"VTG",&HandleVTG, 0}, // track made good (course over ground) and the speed over ground
  {0,0,0}
};
#endif

#ifdef RTK_TIMO
// special Handlers do not process checksums at this time
tNMEA0183Handler specialHandlers[]={
  {"HEADINGSTATUSA",&HandleHEADINGSTATUS, 0}, // Heading status
  {"RTKSTATUSA",&HandleRTKSTATUS, 0}, // RTK solution status
  {0,0,0}
};
#endif

void DebugNMEA0183Handlers(Stream* _stream) {
  NMEA0183HandlersDebugStream=_stream;
}

#ifdef N2K
tN2kGNSSmethod GNSMethofNMEA0183ToN2k(int Method) {
  switch (Method) {
    case 0: return N2kGNSSm_noGNSS;
    case 1: return N2kGNSSm_GNSSfix;
    case 2: return N2kGNSSm_DGNSS;
    default: return N2kGNSSm_noGNSS;  
  }
}
#endif

void HandleNMEA0183Msg(const tNMEA0183Msg &NMEA0183Msg) {
  num_0183_messages++;
  if (debugNMEA) {
    snprintf(prbuf, PRBUF, "%u %s%s", num_0183_messages, NMEA0183Msg.Sender(), NMEA0183Msg.MessageCode());
    log::toAll(String(prbuf));
  }
  int iHandler;
  // Find handler
  for (iHandler=0; NMEA0183Handlers[iHandler].Code!=0 && !NMEA0183Msg.IsMessageCode(NMEA0183Handlers[iHandler].Code); iHandler++);
  if (NMEA0183Handlers[iHandler].Code!=0) {
    NMEA0183Handlers[iHandler].numMessages++;
    //log::toAll(String(NMEA0183Handlers[iHandler].Code) + " " + String(NMEA0183Handlers[iHandler].numMessages));
    NMEA0183Handlers[iHandler].Handler(NMEA0183Msg); 
    // do not return; drop through to forward on TCP
    //return;
  }
#ifdef TCP_FORWARD
  // forward to TCP
  NMEA0183Msg.GetMessage(nmeaSentence, MAX_NMEA0183_MSG_LEN);
  transmit_outgoing_over_tcp(nmeaSentence, strlen(nmeaSentence));
#endif
#if defined(RTK_TIMO_SPECIAL)
  // No standard handler found, check special handlers
    char idMess[32];
    for (iHandler=0; specialHandlers[iHandler].Code!=0; iHandler++) {
      // need to concat sender and code to get message identifier
      snprintf(idMess, sizeof(idMess), "%s%s", NMEA0183Msg.Sender(), NMEA0183Msg.MessageCode());
      size_t codeLen = strlen(specialHandlers[iHandler].Code);
      if (strncmp(idMess, specialHandlers[iHandler].Code, codeLen) == 0) {
        // handles messages with suffix like "RTKSTATUSA"
        log::toAll("Special handler: " + String(idMess));
        specialHandlers[iHandler].Handler(NMEA0183Msg);
        return;
      }
    }
#endif
#ifdef UDP_FORWARD_TIMO
  // we only get here if no handler matched the message
  // Build complete NMEA sentence in prbuf
  // send as udp broadcast
  int offset = 0;
  offset += snprintf(prbuf + offset, PRBUF - offset, "%c%s%s",
                      NMEA0183Msg.GetPrefix(),
                      NMEA0183Msg.Sender(),
                      NMEA0183Msg.MessageCode());
  
  for (int i = 0; i < NMEA0183Msg.FieldCount() && offset < PRBUF - 10; i++) {
    offset += snprintf(prbuf + offset, PRBUF - offset, ",%s", NMEA0183Msg.Field(i));
  }
  if (offset < PRBUF - 10) {
    offset += snprintf(prbuf + offset, PRBUF - offset, "*%02X", NMEA0183Msg.GetCheckSum());
    offset += snprintf(prbuf + offset, PRBUF - offset, "\r\n");
  }
  //Serial.print(prbuf);
  IPAddress broadcastIP(255, 255, 255, 255);
  //int result = udp_server.beginPacket(INADDR_NONE, UDP_FORWARD_PORT);
  int result = udp_server.beginPacket(broadcastIP, UDP_FORWARD_PORT);
  udp_server.write((const uint8_t*)prbuf, offset);
  udp_server.endPacket();
#endif
}

// NMEA0183 message Handler functions

void HandleRMC(const tNMEA0183Msg &NMEA0183Msg) {
  //Serial.println("RMC");
  if (pBD==nullptr) return;
  double variation;
  if (NMEA0183ParseRMC_nc(NMEA0183Msg,pBD->GPSTime,pBD->Latitude,pBD->Longitude,pBD->COG,pBD->SOG,pBD->DaysSince1970,pBD->Variation)) {
  } else if (debugNMEA && NMEA0183HandlersDebugStream != nullptr) { NMEA0183HandlersDebugStream->println("Failed to parse RMC"); }
  pBD->COG *= RADTODEG;
#if 0
  // no heading from RTK sensor so use GPS/COG as heading
  if (pBD->trueHeading < 0) //&& pBD->SOG > 0.1)
    pBD->trueHeading = pBD->COG;
#endif
// not using Timo libraries makes it "interesting" to translate 0183 to n2k
#ifdef N2K_000
    if (n2kMain!=nullptr) {
      // COGSOGRapid
      SetN2kPGN129026(n2kMsg, 255, N2khr_true, pBD->COG*DEGTORAD, pBD->SOG);
      if (!n2kMain->SendMsg(n2kMsg)) num_fail_xmit++; else num_xmit++;
      // GNSSPosition
      SetN2kPGN129029(n2kMsg,255,pBD->DaysSince1970,pBD->GPSTime,
                  pBD->Latitude,pBD->Longitude,pBD->Altitude,
                  N2kGNSSt_GPS,N2kGNSSm_GNSSfix,
                  pBD->SatelliteCount,pBD->HDOP,0,0,
                  0,N2kGNSSt_GPS,0,0);
      if (!n2kMain->SendMsg(n2kMsg)) num_fail_xmit++; else num_xmit++;
      // LatLonRapid
      //SetN2kPGN129025(n2kMsg, pBD->Latitude, pBD->Longitude);
      //if (!n2kMain->SendMsg(n2kMsg)) num_fail_xmit++; else num_xmit++;
      // Variation - taking out since it doesn't seem to be parsing correctly
      //SetN2kPGN127258(n2kMsg, 255, N2kmagvar_Calc, pBD->DaysSince1970, pBD->Variation);
      //n2kMain->SendMsg(n2kMsg);
    }
#endif
  if (debugNMEA && NMEA0183HandlersDebugStream != nullptr) {
      NMEA0183HandlersDebugStream->print("RMC Time="); NMEA0183HandlersDebugStream->println(pBD->GPSTime);
      NMEA0183HandlersDebugStream->print("Latitude="); NMEA0183HandlersDebugStream->println(pBD->Latitude,5);
      NMEA0183HandlersDebugStream->print("Longitude="); NMEA0183HandlersDebugStream->println(pBD->Longitude,5);
      NMEA0183HandlersDebugStream->print("COG="); NMEA0183HandlersDebugStream->println(pBD->COG);
      NMEA0183HandlersDebugStream->print("SOG="); NMEA0183HandlersDebugStream->println(pBD->SOG);
      NMEA0183HandlersDebugStream->print("Variation="); NMEA0183HandlersDebugStream->println(pBD->Variation);
  }
  if (debugRTK) {
    log::toAll("RMC lat: " + String(pBD->Latitude,5) + " lon: " + String(pBD->Longitude,5) + " COG: " + String(pBD->COG));
    // if we are tuning, output relevant data to compare e.g. paddle log with GPS speed
    sprintf(prbuf, "STW: %2.2f SOG: %2.2f RTK: %2.2f COG: %2.2f", pBD->STW*MTOKTS, pBD->SOG*MTOKTS, pBD->RTKheading, pBD->COG);
    log::toAll(prbuf);
  }
}

#ifdef RTK_TIMO
void HandleGSV(const tNMEA0183Msg &NMEA0183Msg) {
  int thisMSG;
  int totalMSG;
  int SatelliteCount;
  if (pBD==nullptr || pSAT==nullptr) return;
  
  if (NMEA0183ParseGSV_nc(NMEA0183Msg, totalMSG, thisMSG, SatelliteCount, SatInfo[0], SatInfo[1], SatInfo[2], SatInfo[3])) {    
    // Determine constellation from talker ID
    String talker = NMEA0183Msg.Sender();    
    // Clear constellation bit field on first message of sequence
    if (thisMSG == 1) {
      pSAT->clearConstellation(talker);
      if (debugNMEA) {
        snprintf(prbuf, PRBUF, "GSV %s seq %d/%d total sats: %d",
                 talker.c_str(), thisMSG, totalMSG, SatelliteCount);
        log::toAll(String(prbuf));
      }
    }
    // Process each satellite in this message (up to 4)
    for (int i = 0; i < 4; i++) {
      if (SatInfo[i].SVID > 0) {  // Valid satellite ID
        pSAT->setSatellite(talker, SatInfo[i].SVID);
        pSAT->setSatelliteData(talker, SatInfo[i].SVID, SatInfo[i].Elevation, SatInfo[i].Azimuth, SatInfo[i].SNR);
        if (debugNMEA) {
          snprintf(prbuf, PRBUF, "  Sat %d elev:%d az:%d snr:%d",
                   SatInfo[i].SVID, SatInfo[i].Elevation, SatInfo[i].Azimuth, SatInfo[i].SNR);
          log::toAll(String(prbuf));
        }
      }
    }
    // Update counts when sequence completes
    if (thisMSG == totalMSG) {
      pSAT->updateCounts();
      pSAT->last_update = now;
      // Only update pBD if significantly different
      if (abs(pSAT->total_count - pBD->SatelliteCount) > 1) {
        if (debugNMEA) {
          snprintf(prbuf, PRBUF, "Satellite count updated: GPS=%d GLO=%d GAL=%d BDS=%d QZSS=%d Total=%d",
                   pSAT->GPS_count, pSAT->GLONASS_count, pSAT->Galileo_count,
                   pSAT->BeiDou_count, pSAT->QZSS_count, pSAT->total_count);
          log::toAll(String(prbuf));
        }
        pBD->SatelliteCount = pSAT->total_count;
      }
    }
  } else if (NMEA0183HandlersDebugStream!=nullptr) {
    NMEA0183HandlersDebugStream->println("Failed to parse GSV");
    for (int i=0; i<MAX_NMEA0183_MSG_LEN; i++) {
      Serial.print(NMEA0183Msg.Data[i]);
      if (!NMEA0183Msg.Data[i]) break;
    }
    Serial.println();
  }
}

// Function to print the GPS quality indicator as a string
const char* getGPSQualityString(GPSQuality quality) {
    switch (quality) {
        case GPS_QUALITY_INVALID: return "Fix not available or invalid";
        case GPS_QUALITY_SINGLE_POINT: return "Single point positioning";
        case GPS_QUALITY_DIFFERENTIAL: return "Differential positioning";
        case GPS_QUALITY_PPS_MODE: return "GPS PPS mode";
        case GPS_QUALITY_RTK_INT: return "RTK Int";
        case GPS_QUALITY_RTK_FLOAT: return "RTK Float";
        case GPS_QUALITY_DEAD_RECKONING: return "Dead reckoning mode";
        case GPS_QUALITY_MANUAL_INPUT: return "Manual input mode";
        case GPS_QUALITY_SIMULATOR_MODE: return "Simulator mode";
        default: return "Unknown quality indicator";
    }
}
#else
// TBD: fix up above not here
void HandleGSV(const tNMEA0183Msg &NMEA0183Msg) {
}
#endif

void HandleGSA(const tNMEA0183Msg &NMEA0183Msg) {
  //Serial.println("GSA");
  return;
}

void HandleGGA(const tNMEA0183Msg &NMEA0183Msg) {
  // heading data with full fix information
  int SatelliteCount;
  if (pBD==nullptr) return;
  //Serial.println("parsing GGA");
  if (NMEA0183ParseGGA_nc(NMEA0183Msg,pBD->GPSTime,pBD->Latitude,pBD->Longitude,
                   pBD->GPSQuality,SatelliteCount,pBD->HDOP,pBD->Altitude,pBD->GeoidalSeparation,
                   pBD->DGPSAge,pBD->DGPSReferenceStationID)) {
    } else if (debugNMEA && NMEA0183HandlersDebugStream != nullptr) { NMEA0183HandlersDebugStream->println("Failed to parse GGA"); }
  if (abs(SatelliteCount-pBD->SatelliteCount) > 1) {
    //log::toAll("GGA sat count changed old: " + String(pBD->SatelliteCount) + " new: " + String(SatelliteCount));
    pBD->SatelliteCount = SatelliteCount;
  }
  // disabling for now because I don't feel like debugging the panic
#ifdef N2K000
  if (n2kMain!=nullptr) {
    Serial.print("SetN2kGNSS params - SID: "); Serial.println(1);
    Serial.print("DaysSince1970: "); Serial.println(pBD->DaysSince1970);
    Serial.print("GPSTime: "); Serial.println(pBD->GPSTime);
    Serial.print("Latitude: "); Serial.println(pBD->Latitude, 6);
    Serial.print("Longitude: "); Serial.println(pBD->Longitude, 6);
    Serial.print("Altitude: "); Serial.println(pBD->Altitude);
    Serial.print("GNSStype: "); Serial.println(N2kGNSSt_GPS);
    Serial.print("GNSSmethod: "); Serial.println(GNSMethofNMEA0183ToN2k(pBD->GPSQuality));
    Serial.print("SatelliteCount: "); Serial.println(pBD->SatelliteCount);
    Serial.print("HDOP: "); Serial.println(pBD->HDOP);
    Serial.print("PDOP: "); Serial.println(0);
    Serial.print("GeoidalSeparation: "); Serial.println(pBD->GeoidalSeparation);
    Serial.print("nReferenceStations: "); Serial.println(1);
    Serial.print("ReferenceStationType: "); Serial.println(N2kGNSSt_GPS);
    Serial.print("ReferenceSationID: "); Serial.println(pBD->DGPSReferenceStationID);
    Serial.print("AgeOfCorrection: "); Serial.println(pBD->DGPSAge);
    SetN2kGNSS(n2kMsg,1,pBD->DaysSince1970,pBD->GPSTime,pBD->Latitude,pBD->Longitude,pBD->Altitude,
              N2kGNSSt_GPS,GNSMethofNMEA0183ToN2k(pBD->GPSQuality),pBD->SatelliteCount,pBD->HDOP,0,
              pBD->GeoidalSeparation,1,N2kGNSSt_GPS,pBD->DGPSReferenceStationID,pBD->DGPSAge
              );
    //Serial.println("sending message");
    n2kMain->SendMsg(n2kMsg);
  }
#endif
  if (debugNMEA && NMEA0183HandlersDebugStream != nullptr) {
    NMEA0183HandlersDebugStream->print("Time="); NMEA0183HandlersDebugStream->println(pBD->GPSTime);
    NMEA0183HandlersDebugStream->print("Latitude="); NMEA0183HandlersDebugStream->println(pBD->Latitude,5);
    NMEA0183HandlersDebugStream->print("Longitude="); NMEA0183HandlersDebugStream->println(pBD->Longitude,5);
    NMEA0183HandlersDebugStream->print("Altitude="); NMEA0183HandlersDebugStream->println(pBD->Altitude,1);
    NMEA0183HandlersDebugStream->print("GPSQuality="); NMEA0183HandlersDebugStream->println(pBD->GPSQuality);
    NMEA0183HandlersDebugStream->print("SatelliteCount="); NMEA0183HandlersDebugStream->println(pBD->SatelliteCount);
    NMEA0183HandlersDebugStream->print("HDOP="); NMEA0183HandlersDebugStream->println(pBD->HDOP);
    NMEA0183HandlersDebugStream->print("GeoidalSeparation="); NMEA0183HandlersDebugStream->println(pBD->GeoidalSeparation);
    NMEA0183HandlersDebugStream->print("DGPSAge="); NMEA0183HandlersDebugStream->println(pBD->DGPSAge);
    NMEA0183HandlersDebugStream->print("DGPSReferenceStationID="); NMEA0183HandlersDebugStream->println(pBD->DGPSReferenceStationID);
  }
  if (debugRTK && (num_0183_messages % 10 == 0)) {
    // Use direct array access since fixQuality is now char[][24]
    const char* qualStr = (pBD->GPSQuality >= 0 && pBD->GPSQuality < 11) ? fixQuality[pBD->GPSQuality] : "unknown";
    snprintf(prbuf, PRBUF, "GGA lat: %.5f lon: %.5f qual: %d(%s) sat: %d",
             pBD->Latitude, pBD->Longitude, pBD->GPSQuality, qualStr, pBD->SatelliteCount);
    log::toAll(String(prbuf));
  }
}

void HandleVTG(const tNMEA0183Msg &NMEA0183Msg) {
  double MagneticCOG;
  if (pBD==nullptr) return;
  if (NMEA0183ParseVTG_nc(NMEA0183Msg,pBD->COG,MagneticCOG,pBD->SOG)) {
    //pBD->Variation=pBD->COG-MagneticCOG; // Save variation for Magnetic heading
    pBD->COG *= RADTODEG;
  } else if (debugNMEA && NMEA0183HandlersDebugStream != nullptr) { NMEA0183HandlersDebugStream->println("Failed to parse VTG"); }
#ifdef N2K
  if (n2kMain!=nullptr) {
    SetN2kCOGSOGRapid(n2kMsg,1,N2khr_true,pBD->COG,pBD->SOG);
    if (!n2kMain->SendMsg(n2kMsg)) num_fail_xmit++; else num_xmit++;
//      SetN2kBoatSpeed(n2kMsg,1,SOG);
//      n2kMain.SendMsg(n2kMsg);
  }
#endif
  if (debugNMEA && NMEA0183HandlersDebugStream != nullptr) {
    NMEA0183HandlersDebugStream->print("True heading="); NMEA0183HandlersDebugStream->println(pBD->trueHeading);
  }
}

void HandleGLL(const tNMEA0183Msg &NMEA0183Msg) {
  // Basic fix data. Nobody sends this without sending GGA so I use that instead.
  return;
}

#ifdef RTK_HYBRID
double NMEA0183GetDouble(const char *data);

// these messages are from the UM982
//*****************************************************************************
// $GNHPR,074615.00,320.9610,-66.1712,000.0000,4,47,0.00,0999*45
//        UTC      ,HEADING ,PITCH   ,ROLL    ,QUAL,Sat#,Age,stnID,CKSUM
bool NMEA0183ParseHPR(const tNMEA0183Msg &NMEA0183Msg, double &UTC, double &Heading, double &Pitch, double &Roll, int &QF, int &SatNo, double &Age, int &Station) {
  bool result=( NMEA0183Msg.FieldCount()>=7 );
#if 0
  for (int i=0; i < NMEA0183Msg.FieldCount(); i++) {
    Serial.printf("%d: %s ",i, NMEA0183Msg.Field(i));
    //if (i<NMEA0183Msg.FieldCount()-1 ) Serial.print(" ");
  }
  Serial.println(); 
#endif
  if (result) { 
    UTC=NMEA0183GetDouble(NMEA0183Msg.Field(0));
    Heading=NMEA0183GetDouble(NMEA0183Msg.Field(1));
    Pitch=NMEA0183GetDouble(NMEA0183Msg.Field(2));
    Roll=NMEA0183GetDouble(NMEA0183Msg.Field(3));
    QF=atoi(NMEA0183Msg.Field(4));
    SatNo=atoi(NMEA0183Msg.Field(5));
    Age=NMEA0183GetDouble(NMEA0183Msg.Field(6));
    Station=atoi(NMEA0183Msg.Field(7));
  }
  return result;
}

/*
Quality:
0 = Fix invalid
1 = Single point positioning 
2 = Differential GPS
3 = GPS PPS mode (only for GGA message)
4 = RTK fix
5 = RTK float
6 = Dead reckoning mode
7 = Manual input mode (fixed value) 
8 = Extra wide-lane
9 = SBAS
*/
void HandleHPR(const tNMEA0183Msg &NMEA0183Msg) {
  double UTC, Heading, Pitch, Roll, Age;
  int QF, SatNo, Station;
  if (pBD==nullptr) return;
  // parse returns RADIANS
  //Serial.println("parsing HPR");
  if (NMEA0183ParseHPR(NMEA0183Msg, UTC, Heading, Pitch, Roll, QF, SatNo, Age, Station)) {
    //if (Heading > 0.00001 && Pitch > 0.00001 && Roll > 0.00001) {
      // sensor is currently sending 0 for HPR so only set heading if all are >0
      // note this has a rare error condition if heading due north and not moving but then do we care?
      // also doesn't work if the boat is sitting on the trailer
      pBD->RTKheading=fmod(Heading+pBD->rtkOrientation, 359.9);
      if (debugRTK) {
        const char* qualityStr = (QF >= 0 && QF < 10) ? fixQuality[QF] : "Unknown";
        snprintf(prbuf, PRBUF, "RTK: %.1f HPR: Heading=%.1f° Pitch=%.1f° Roll=%.1f° Quality=%d (%s) Sats=%d Age=%.2f",
                 pBD->RTKheading, Heading, Pitch, Roll, QF, qualityStr, SatNo, Age);
        log::toAll(String(prbuf));
      }
      if (teleplot) {
        //Serial.printf("> RTK: %.1f\n",pBD->RTKheading);
      }
    // RTK data tracking - different behavior for RTK_TIMO vs RTK_HYBRID
#ifdef RTK_TIMO
    if (pRTK != nullptr) {
      pRTK->GPStime = UTC;
      // Check if RTK quality has changed
      if (previousRTKqual != -1 && QF != previousRTKqual) {
        unsigned long currentTime = now;
        unsigned long timeSinceLastChange = currentTime - lastRTKqualChangeTime;
        
        // Log the RTK quality change
        snprintf(prbuf, PRBUF, "RTK Quality Change - Time since reboot: %lums, Time since last change: %lums, Quality changed from %d to %d %d satellite count",
                 currentTime, timeSinceLastChange, previousRTKqual, QF, pBD->SatelliteCount);
        log::toAll(String(prbuf));
        // Log satellite status when RTK quality changes
        if (pSAT != nullptr) {
          snprintf(prbuf, PRBUF, "Satellites: GPS=%d GLO=%d GAL=%d BDS=%d QZSS=%d Total=%d",
                   pSAT->GPS_count, pSAT->GLONASS_count, pSAT->Galileo_count,
                   pSAT->BeiDou_count, pSAT->QZSS_count, pSAT->total_count);
          log::toAll(String(prbuf));
        }
        lastRTKqualChangeTime = currentTime;
      } else if (previousRTKqual == -1) {
        // First time initialization
        lastRTKqualChangeTime = now;
      }
      
      // Update stored values
      previousRTKqual = QF;
      pRTK->RTKqual = QF; // quality of fix
      pRTK->pitch = Pitch;
      pRTK->roll = Roll;
      pRTK->usedSV = SatNo;
      if (debugRTK && (num_0183_messages % 10 == 0)) {
        const char* qualityStr = (QF >= 0 && QF < 10) ? fixQuality[QF] : "Unknown";
        snprintf(prbuf, PRBUF, "HPR: Heading=%.1f° Pitch=%.1f° Roll=%.1f° Quality=%d (%s) Sats=%d Age=%.2f",
                 Heading, Pitch, Roll, QF, qualityStr, SatNo, Age);
        log::toAll(String(prbuf));
      }
    }
#else
    //Serial.println("checking quality");
    // RTK_HYBRID mode - limited RTK tracking without pRTK/pSAT
    // Check if RTK quality has changed
    if (previousRTKqual != -1 && QF != previousRTKqual) {
      unsigned long currentTime = now;
      unsigned long timeSinceLastChange = currentTime - lastRTKqualChangeTime;
      
      // Log the RTK quality change (without satellite details in HYBRID mode)
      snprintf(prbuf, PRBUF, "RTK Quality Change - Time since reboot: %lums, Time since last change: %lums, Quality changed from %d to %d Sats=%d",
               currentTime, timeSinceLastChange, previousRTKqual, QF, SatNo);
      log::toAll(String(prbuf));
      lastRTKqualChangeTime = currentTime;
    } else if (previousRTKqual == -1) {
      // First time initialization
      lastRTKqualChangeTime = now;
    }
    
    // Update stored values
    previousRTKqual = QF;
    if (debugRTK && (num_0183_messages % 10 == 0)) {
      const char* qualityStr = (QF >= 0 && QF < 10) ? fixQuality[QF] : "Unknown";
      snprintf(prbuf, PRBUF, "HPR: Heading=%.1f° Pitch=%.1f° Roll=%.1f° Quality=%d (%s) Sats=%d Age=%.2f",
               Heading, Pitch, Roll, QF, qualityStr, SatNo, Age);
      log::toAll(String(prbuf));
    }
#endif
  } else if (debugNMEA && NMEA0183HandlersDebugStream != nullptr) {
    NMEA0183HandlersDebugStream->println("Failed to parse HPR");
  }
#ifdef N2K
  //Serial.println("sending 127150");
  if (n2kMain!=nullptr && xmitRTKheading) {
    // Vessel Heading (deviation should always be 0 since it's not a magnetic compass)
    // heading arrives in DEGREES convert to radians for n2k
    SetN2kPGN127250(n2kMsg, 255, pBD->RTKheading*DEGTORAD, 0, 0, N2khr_true);
    if (!n2kMain->SendMsg(n2kMsg)) num_fail_xmit++; else num_xmit++;
    //log::toAll("sending 127250");
    // Attitude
    //setN2kPGN127257();
  }
#endif
  if (debugNMEA && NMEA0183HandlersDebugStream != nullptr) {
    NMEA0183HandlersDebugStream->print("RTK heading="); NMEA0183HandlersDebugStream->println(pBD->RTKheading);
    NMEA0183HandlersDebugStream->print("Pitch="); NMEA0183HandlersDebugStream->println(Pitch);
    NMEA0183HandlersDebugStream->print("Roll="); NMEA0183HandlersDebugStream->println(Roll);
    NMEA0183HandlersDebugStream->print("Quality Fix="); NMEA0183HandlersDebugStream->println(QF);
    NMEA0183HandlersDebugStream->print("SatNo="); NMEA0183HandlersDebugStream->println(SatNo);
    NMEA0183HandlersDebugStream->print("Age="); NMEA0183HandlersDebugStream->println(Age);
    NMEA0183HandlersDebugStream->print("Station="); NMEA0183HandlersDebugStream->println(Station);
  }
}
#endif

#ifdef RTK_TIMO
//*****************************************************************************
// RTKSTATUS - RTK Solution Status
// Based on Table 7-121 in Unicore Reference Manual
// Example: #RTKSTATUSA,97,GPS,FINE,2190,365354000,0,0,18,1;0,0,0,0,0,0,0,0,0,0,0,NONE,0,0,0,0,0*
// Fields after standard Unicore header (97,GPS,FINE,2190,365354000):
// Field 5: gpsSource (source data decoding status for GPS satellites 1-32)
// Field 6: Reserved
// Field 7: bdsSource1 (source data decoding status for BDS satellites 1-32)
// Field 8: bdsSource2 (source data decoding status for BDS satellites 33-63)
// Field 9: Reserved
// Field 10: gloSource (source data decoding status for GLONASS satellites 1-23)
// Field 11: Reserved
// Field 12: galSource1 (source data decoding status for Galileo satellites 1-32)
// Field 13: galSource2 (source data decoding status for Galileo satellites 33-36)
// Field 14: qzssSource (source data decoding status for QZSS satellites 193-202)
// Field 15: Reserved
// Field 16: PositionType (refer to Table 0-4 Position or Velocity Type)
// Field 17: CalculateStatus (RTK/RTD solution status)
// Field 18: IonDetected (ionospheric scintillation detected)
// Field 19: DualRtkFlag (dual-antenna baseline status, UM982 Build9669+)
// Field 20: ADRNumber (number of valid carrier phase observations)
bool NMEA0183ParseRTKSTATUS(const tNMEA0183Msg &NMEA0183Msg,
    unsigned int &GPSSource, unsigned int &BDSSource1, unsigned int &BDSSource2,
    unsigned int &GLOSource, unsigned int &GALSource1, unsigned int &GALSource2,
    unsigned int &QZSSSource, int &PositionType, int &CalculateStatus,
    int &IonDetected, int &DualRtkFlag, int &ADRNumber) {
    
  // Need at least 21 fields for complete parsing (header + 16 data fields minimum)
  //bool result = (NMEA0183Msg.FieldCount() >= 21);
  // except proprietary message parsing does not count fields
  bool result = true;
  if (result) {
    // Parse source data decoding status fields (hexadecimal format)
    GPSSource = strtoul(NMEA0183Msg.Field(5), NULL, 16);      // Field 5: gpsSource
    // Field 6 is reserved
    BDSSource1 = strtoul(NMEA0183Msg.Field(7), NULL, 16);     // Field 7: bdsSource1
    BDSSource2 = strtoul(NMEA0183Msg.Field(8), NULL, 16);     // Field 8: bdsSource2
    // Field 9 is reserved
    GLOSource = strtoul(NMEA0183Msg.Field(10), NULL, 16);     // Field 10: gloSource
    // Field 11 is reserved
    GALSource1 = strtoul(NMEA0183Msg.Field(12), NULL, 16);    // Field 12: galSource1
    GALSource2 = strtoul(NMEA0183Msg.Field(13), NULL, 16);    // Field 13: galSource2
    QZSSSource = strtoul(NMEA0183Msg.Field(14), NULL, 16);    // Field 14: qzssSource
    // Field 15 is reserved
    PositionType = atoi(NMEA0183Msg.Field(16));               // Field 16: Position type
    CalculateStatus = atoi(NMEA0183Msg.Field(17));            // Field 17: Calculate status
    IonDetected = atoi(NMEA0183Msg.Field(18));                // Field 18: Ion detected
    DualRtkFlag = atoi(NMEA0183Msg.Field(19));                // Field 19: Dual rtk flag
    ADRNumber = atoi(NMEA0183Msg.Field(20));                  // Field 20: ADR Number
  } else {
    log::toAll("NMEA0183ParseRTKSTATUS, field count: " + String(NMEA0183Msg.FieldCount()));
    log::toAll("Prefix: " + String(NMEA0183Msg.GetPrefix()));
    char c;
    for (int i=0; i<MAX_NMEA0183_MSG_LEN; i++) {
      c = NMEA0183Msg.Data[i];
      if (c) Serial.print(c);
        else break;
    }
    Serial.println();
  }
  return result;
}

String RTKcalStatus[] = {"No differential data", "Insufficient observation", "High latency", "Active ionosphere", "Insufficient observation (rover)", "RTK solution"};

const PositionType positionTypes[] PROGMEM = {
  {0, "NONE", "No solution"},
  {1, "FIXEDPOS", "Position fixed by the FIX POSITION command"},
  {2, "FIXEDHEIGHT", "Not supported currently"},
  {8, "DOPPLER_VELOCITY", "Velocity computed using instantaneous Doppler"},
  {16, "SINGLE", "Single point positioning"},
  {17, "PSRDIFF", "Pseudorange differential solution"},
  {18, "SBAS", "SBAS corrected"},
  {32, "L1_FLOAT", "L1 float ambiguity solution"},
  {33, "IONOFREE_FLOAT", "Ionosphere-free float solution"},
  {34, "NARROW_FLOAT", "Narrow-lane float solution"},
  {48, "L1_INT", "L1 fixed ambiguity solution"},
  {49, "WIDE_INT", "Wide-lane fixed solution"},
  {50, "NARROW_INT", "Narrow-lane fixed solution"},
  {52, "INS", "Inertial navigation solution"},
  {55, "INS_RTKFLOAT", "INS with RTK float"},
  {56, "INS_RTKFIXED", "INS with RTK fixed"},
  {68, "PPP_CONVERGING", "PPP converging"},
  {69, "PPP", "Precise Point Positioning"},
  {-1, nullptr, nullptr}  // Sentinel value
};

// Lookup function
const char* getPositionTypeName(int code) {
  for (int i = 0; positionTypes[i].code != -1; i++) {
    if (positionTypes[i].code == code) {
      return positionTypes[i].name;
    }
  }
  return "UNKNOWN";
}

const char* getPositionTypeDescription(int code) {
  for (int i = 0; positionTypes[i].code != -1; i++) {
    if (positionTypes[i].code == code) {
      return positionTypes[i].description;
    }
  }
  return "Unknown position type";
}

// status from UM-982
void HandleRTKSTATUS(const tNMEA0183Msg &NMEA0183Msg) {
  //log::toAll("HandleRTKSTATUS"); 
  unsigned int GPSSource, BDSSource1, BDSSource2, GLOSource, GALSource1, GALSource2, QZSSSource;
  int PositionType, CalculateStatus, IonDetected, DualRtkFlag, ADRNumber;
  
  if (pRTK == nullptr) return;
  
  if (NMEA0183ParseRTKSTATUS(NMEA0183Msg, GPSSource, BDSSource1, BDSSource2,
                              GLOSource, GALSource1, GALSource2, QZSSSource,
                              PositionType, CalculateStatus, IonDetected,
                              DualRtkFlag, ADRNumber)) {
      // Store in RTK stats structure
      pRTK->RTKqual = CalculateStatus;  // Use calculate status as the main RTK quality indicator
      pRTK->usedSV = ADRNumber;         // Use ADR number as satellite count
      
      // Store all parsed variables in the RTK structure
      pRTK->GPSSource = GPSSource;
      pRTK->BDSSource1 = BDSSource1;
      pRTK->BDSSource2 = BDSSource2;
      pRTK->GLOSource = GLOSource;
      pRTK->GALSource1 = GALSource1;
      pRTK->GALSource2 = GALSource2;
      pRTK->QZSSSource = QZSSSource;
      pRTK->PositionType = PositionType;
      pRTK->IonDetected = IonDetected;
      pRTK->DualRtkFlag = DualRtkFlag;
      
      if (debugRTK) {
        String calcStatusString = (CalculateStatus >= 0 && CalculateStatus < 6) ? RTKcalStatus[CalculateStatus] : "Unknown";
        snprintf(prbuf, PRBUF, "RTKSTATUS: calc_status='%s' pos_type=%d (%s)",
                 calcStatusString.c_str(), PositionType, getPositionTypeName(PositionType));
        log::toAll(String(prbuf));
        snprintf(prbuf, PRBUF, "ion_detected=%d adr_sats=%d dual_rtk=%X",
                 IonDetected, ADRNumber, DualRtkFlag);
        log::toAll(String(prbuf));
        // Note: BIN format not supported in snprintf, using hex instead
        snprintf(prbuf, PRBUF, "GPSsrc=%X BDSsrc1=%X BDSsrc2=%X GLONASSsrc=%X Galileosrc1=%X Galileosrc2=%X QZSSsrc=%X",
                 GPSSource, BDSSource1, BDSSource2, GLOSource, GALSource1, GALSource2, QZSSSource);
        log::toAll(String(prbuf));
      }
  } else if (debugNMEA && NMEA0183HandlersDebugStream != nullptr) {
      NMEA0183HandlersDebugStream->println("Failed to parse RTKSTATUS");
  }
}
// Helper functions for tSatelliteStats
int tSatelliteStats::countBits(uint32_t value) {
  int count = 0;
  while (value) {
    count += value & 1;
    value >>= 1;
  }
  return count;
}

void tSatelliteStats::setSatellite(String talker, int satID) {
  if (talker == "GP") {
    // GPS satellites 1-32
    if (satID >= 1 && satID <= 32) {
      GPS_sats_low |= (1UL << (satID - 1));
    } else if (satID >= 33 && satID <= 64) {
      GPS_sats_high |= (1UL << (satID - 33));
    }
  } else if (talker == "GL") {
    // GLONASS satellites 65-96 (map to bits 0-31)
    if (satID >= 65 && satID <= 96) {
      GLONASS_sats |= (1UL << (satID - 65));
    }
  } else if (talker == "GA") {
    // Galileo satellites 1-36 (map to bits 0-35, but we only have 32 bits)
    if (satID >= 1 && satID <= 32) {
      Galileo_sats |= (1UL << (satID - 1));
    }
  } else if (talker == "GB") {
    // BeiDou satellites 1-63
    if (satID >= 1 && satID <= 32) {
      BeiDou_sats_low |= (1UL << (satID - 1));
    } else if (satID >= 33 && satID <= 63) {
      BeiDou_sats_high |= (1UL << (satID - 33));
    }
  } else if (talker == "GQ") {
    // QZSS satellites 193-202 (map to bits 0-9)
    if (satID >= 193 && satID <= 202) {
      QZSS_sats |= (1UL << (satID - 193));
    }
  }
}

void tSatelliteStats::clearConstellation(String talker) {
  if (talker == "GP") {
    GPS_sats_low = 0;
    GPS_sats_high = 0;
  } else if (talker == "GL") {
    GLONASS_sats = 0;
  } else if (talker == "GA") {
    Galileo_sats = 0;
  } else if (talker == "GB") {
    BeiDou_sats_low = 0;
    BeiDou_sats_high = 0;
  } else if (talker == "GQ") {
    QZSS_sats = 0;
  }
}

uint8_t getConstellationId(String talker) {
  if (talker == "GP") return 0;
  else if (talker == "GL") return 1;
  else if (talker == "GA") return 2;
  else if (talker == "GB") return 3;
  else if (talker == "GQ") return 4;
  return 0;
}

const char* tSatelliteStats::getConstellationName(uint8_t constId) {
  switch (constId) {
    case 0: return "GP";
    case 1: return "GL";
    case 2: return "GA";
    case 3: return "GB";
    case 4: return "GQ";
    default: return "??";
  }
}

void tSatelliteStats::setSatelliteData(String talker, int satID, int elevation, int azimuth, int snr) {
  // Find existing satellite or add new one
  tSatelliteData* sat = findSatellite(satID);
  if (sat == nullptr && satelliteCount < 24) {  // Reduced from 64 to 24
    // Add new satellite
    sat = &satellites[satelliteCount++];
  }
  
  if (sat != nullptr) {
    sat->SVID = (uint8_t)satID;
    sat->Elevation = (int8_t)elevation;
    sat->Azimuth = (uint16_t)azimuth;
    sat->SNR = (uint8_t)snr;
    sat->constellation = getConstellationId(talker);
    sat->lastSeen = now;
  }
}

tSatelliteData* tSatelliteStats::findSatellite(int svid) {
  for (int i = 0; i < satelliteCount; i++) {
    if (satellites[i].SVID == svid) {
      return &satellites[i];
    }
  }
  return nullptr;
}

void tSatelliteStats::updateCounts() {
  GPS_count = countBits(GPS_sats_low) + countBits(GPS_sats_high);
  GLONASS_count = countBits(GLONASS_sats);
  Galileo_count = countBits(Galileo_sats);
  BeiDou_count = countBits(BeiDou_sats_low) + countBits(BeiDou_sats_high);
  QZSS_count = countBits(QZSS_sats);
  total_count = GPS_count + GLONASS_count + Galileo_count + BeiDou_count + QZSS_count;
  
  // Clean up old satellites (remove satellites not seen in last 30 seconds)
  unsigned long currentTime = now;
  for (int i = 0; i < satelliteCount; i++) {
    if (currentTime - satellites[i].lastSeen > 30000) {
      // Remove this satellite by shifting others down
      for (int j = i; j < satelliteCount - 1; j++) {
        satellites[j] = satellites[j + 1];
      }
      satelliteCount--;
      i--; // Check this position again
    }
  }
}
#endif

byte calculateNMEAChecksum(const char* sentence) {
  byte checksum = 0;
  // Start after the '$' and continue until '*' or end of string
  for (int i = 1; sentence[i] != '\0' && sentence[i] != '*'; i++) {
    checksum ^= sentence[i];
  }
  return checksum;
}
#endif
