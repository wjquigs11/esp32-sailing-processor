#ifndef _BoatData_H_
#define _BoatData_H_

#define MTOKTS 1.9438444924
#define KTSTOM 0.5144444444

#define VARIATION -15.2

struct tBoatData {
  unsigned long DaysSince1970;   // Days since 1970-01-01
  
  // degrees not radians
  // remember to convert if you transmit in n2k
  double trueHeading,SOG,COG,Variation, // stored in DEGREES!!!
         magHeading,
         BNOheading,  // heading from BNO085 compass
         RTKheading,  // heading from RTK GPS
         STW, // meters/sec
         AWS, AWA,
         TWS, TWA, // meters/sec and DEGREES NOT RADIANS
         TWD, // relative to north
         VMG, // meters/sec (velocity made good to wind)
         maxTWS,
         GPSTime,// Secs since midnight,
         Latitude, Longitude, Altitude, HDOP, GeoidalSeparation, DGPSAge;
  int magOrientation; // is compass pointing towards bow?
  int sensOrientation;  // is Honeywell sensor at zero on centerline?
  int rtkOrientation; // how the RTK module/antennas are oriented relative to bow of boat
  int GPSQuality, SatelliteCount, DGPSReferenceStationID;
  bool MOBActivated;
  // Navigation data for RMB/APB sentences
  double XTE;
  double DistanceToWaypoint;
  double BearingToWaypoint;
  uint32_t WaypointNumber;
  bool PerpendicularCrossed;
  bool ArrivalCircleEntered;
  double WaypointLatitude;
  double WaypointLongitude;

public:
  tBoatData() {
    trueHeading=-1.0; // init to -1 in case we have no sources for heading
    magHeading=-1.0; // from IMU, if installed
    BNOheading=-1.0; // from BNO085 compass
    RTKheading=-1.0; // from RTK GPS
    SOG=0.0;
    COG=0.0; 
    Variation=VARIATION;
    STW=0.0;
    AWS=0.0; AWA=0.0;
    TWS=0.0; TWA=0.0; // TWA = relative to bow
    TWD=0.0; // TWD = compass direction of wind
    VMG=0.0;
    maxTWS=0.0;
    GPSTime=0;
    Altitude=0;
    HDOP=100000;
    DGPSAge=100000;
    DaysSince1970=0; 
    MOBActivated=false; 
    SatelliteCount=0; 
    DGPSReferenceStationID=0;
    magOrientation=0;
    sensOrientation=0;
    rtkOrientation=0;
    //minAWA=0;
    //maxAWA=0;    
    // Initialize navigation data
    XTE=0.0;
    DistanceToWaypoint=0.0;
    BearingToWaypoint=0.0;
    WaypointNumber=0;
    PerpendicularCrossed=false;
    ArrivalCircleEntered=false;
    WaypointLatitude=0.0;
    WaypointLongitude=0.0;
  };
};

struct tRTKstats {
  int antennaAstat, antennaBstat, baseLen;
  double GPStime, pitch, roll, heading, pAcc, rAcc, hAcc, usedSV;
  int RTKqual;
  // Variables from HandleRTKSTATUS()
  unsigned int GPSSource, BDSSource1, BDSSource2, GLOSource, GALSource1, GALSource2, QZSSSource;
  int PositionType, IonDetected, DualRtkFlag;

public:
  tRTKstats() {
    antennaAstat=0;
    antennaBstat=0;
    baseLen=0;
    GPStime=0;
    RTKqual=0;
    pitch=0;
    roll=0;
    heading=0;
    pAcc=0;
    rAcc=0;
    hAcc=0;
    usedSV=0;
    // Initialize new variables from HandleRTKSTATUS()
    GPSSource=0;
    BDSSource1=0;
    BDSSource2=0;
    GLOSource=0;
    GALSource1=0;
    GALSource2=0;
    QZSSSource=0;
    PositionType=0;
    IonDetected=0;
    DualRtkFlag=0;
  };
};

struct tEnvStats {
  float temp, pressure, humidity;
public:
  tEnvStats() {
    temp=0;
    pressure=0;
    humidity=0;
  };
};
extern bool bmeFound;
bool startBME();
void readBME();

extern tBoatData *pBD;
extern tBoatData BoatData;
extern tRTKstats *pRTK;
extern tRTKstats RTKdata;
extern tEnvStats *pENV;
extern tEnvStats ENVdata;

// don't track satellites if we're forwarding RTK instead of parsing
// NOTE this also disables RTK Orientation, so heading will be invalid unless I extract that parameter...hmm...
#ifdef RTK_TIMO
// Individual satellite data structure - compact version
struct tSatelliteData {
  uint8_t SVID;
  int8_t Elevation;    // -90 to +90 degrees
  uint16_t Azimuth;    // 0-359 degrees
  uint8_t SNR;         // 0-99 dB-Hz
  uint8_t constellation; // 0=GP, 1=GL, 2=GA, 3=GB, 4=GQ
  uint32_t lastSeen;   // millis() timestamp
  
  tSatelliteData() {
    SVID = 0;
    Elevation = 0;
    Azimuth = 0;
    SNR = 0;
    constellation = 0;
    lastSeen = 0;
  }
};

struct tSatelliteStats {
  // Bit fields for satellites in view (each bit represents one satellite ID)
  uint32_t GPS_sats_low;      // GPS satellites 1-32
  uint32_t GPS_sats_high;     // GPS satellites 33-64 (if needed)
  uint32_t GLONASS_sats;      // GLONASS satellites 65-96 (mapped to bits 0-31)
  uint32_t Galileo_sats;      // Galileo satellites 1-32 (mapped to bits 0-31)
  uint32_t BeiDou_sats_low;   // BeiDou satellites 1-32
  uint32_t BeiDou_sats_high;  // BeiDou satellites 33-64
  uint32_t QZSS_sats;         // QZSS satellites 193-202 (mapped to bits 0-9)
  
  // Detailed satellite data (reduced to 24 satellites max to save memory)
  tSatelliteData satellites[24];
  uint8_t satelliteCount;
  
  // Counts derived from bit fields
  uint8_t GPS_count;
  uint8_t GLONASS_count;
  uint8_t Galileo_count;
  uint8_t BeiDou_count;
  uint8_t QZSS_count;
  uint8_t total_count;
  uint32_t last_update;

public:
  tSatelliteStats() {
    GPS_sats_low = 0;
    GPS_sats_high = 0;
    GLONASS_sats = 0;
    Galileo_sats = 0;
    BeiDou_sats_low = 0;
    BeiDou_sats_high = 0;
    QZSS_sats = 0;
    satelliteCount = 0;
    GPS_count = 0;
    GLONASS_count = 0;
    Galileo_count = 0;
    BeiDou_count = 0;
    QZSS_count = 0;
    total_count = 0;
    last_update = 0;
  };
  
  // Helper functions to set/clear satellite bits
  void setSatellite(String talker, int satID);
  void setSatelliteData(String talker, int satID, int elevation, int azimuth, int snr);
  void clearConstellation(String talker);
  void updateCounts();
  int countBits(uint32_t value);
  tSatelliteData* findSatellite(int svid);
  const char* getConstellationName(uint8_t constId);
};

extern tSatelliteStats *pSAT;
extern tSatelliteStats SATdata;

extern char fixQuality[][24];

// Define an enumeration for GPS quality indicators
typedef enum {
  GPS_QUALITY_INVALID = 0, // Fix not available or invalid
  GPS_QUALITY_SINGLE_POINT = 1, // Single point positioning
  GPS_QUALITY_DIFFERENTIAL = 2, // Differential positioning
  GPS_QUALITY_PPS_MODE = 3, // GPS PPS mode
  GPS_QUALITY_RTK_INT = 4, // RTK Int
  GPS_QUALITY_RTK_FLOAT = 5, // RTK Float
  GPS_QUALITY_DEAD_RECKONING = 6, // Dead reckoning mode
  GPS_QUALITY_MANUAL_INPUT = 7, // Manual input mode
  GPS_QUALITY_SIMULATOR_MODE = 8 // Simulator mode
} GPSQuality;
#endif

// redefine value in NMEA0183.h
// UM982 sends NMEA0183 messages longer than 81 chars
//#define MAX_NMEA0183_MSG_BUF_LEN 101

#endif // _BoatData_H_

