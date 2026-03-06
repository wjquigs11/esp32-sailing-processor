#include <Wire.h>
#include <movingAvg.h>

#define DEGTORAD 0.01745329252
#define RADTODEG 57.2957795131

extern bool windToggle;
extern int timerDelay;
extern bool windOnMain;
extern movingAvg arrivalRate;
extern float currentWindFreq;
void initWindFrequencyMeasurement();
void calcWindFrequency();

#ifdef N2K
#include <N2kMessages.h>
#include <N2kMsg.h>
#include <NMEA2000.h>
#include <NMEA2000_esp32.h>
#include <SPI.h>
extern tNMEA2000 *n2kMain;
extern tN2kMsg n2kMsg;
extern bool debugN2K;
extern bool debugWind;
#endif // N2K

bool mainXmitWind(double windSpeed, int rotateout);
bool mainXmitOther(int PGN, uint8_t SRC, unsigned char SID, byte cdata[], uint8_t len, unsigned long ID);

// ESPberry ESP32 CAN interface
#define CAN_TX_PIN GPIO_NUM_27 // 27 = IO27, not GPIO27, RPI header pin 18
#define CAN_RX_PIN GPIO_NUM_35 // RPI header pin 15

#define SPI_CS_PIN 5  // ESPberry CE0 = GPIO8 = IO5
#define MAX_DATA_SIZE 12
#define N2K_INT_PIN 0xff
#include <NMEA2000_mcp.h>
extern tNMEA2000_mcp *n2kWind;

void parseMainCAN();
void parseWindCAN();
int windCorrect(double);
void HandleWindN2KMessage(const tN2kMsg &N2kMsg);
void parseSTW(const tN2kMsg &N2kMsg);

extern unsigned int num_n2k_messages, num_wind_messages, num_wind_other, num_wind_xmit, num_wind_fail, num_xmit, num_fail_xmit;

// Wind frequency measurement variables
extern float currentWindFreq;
void initWindFrequencyMeasurement();
// Structure for tracking N2K PGN statistics
struct PGNStats {
  unsigned long pgn;
  unsigned long count;
  unsigned long age;
};
// Sparse array for PGN tracking - adjust MAX_TRACKED_PGNS as needed
#define MAX_TRACKED_PGNS 50
extern PGNStats pgnTracker[];
extern int trackedPGNCount;
int findOrCreatePGNEntry(unsigned long pgn);

#ifdef HONEY
#include <Adafruit_ADS1X15.h>
// Honeywell sensor
extern movingAvg honeywellSensor;                // define the moving average object
extern int PotValue, PotLo, PotHi;
extern int portRange, stbdRange;
//extern int sensOrientation; // Honeywell orientation relative to centerline (moved to boatdata)
extern Adafruit_ADS1015 ads;
extern int adsInit;
// Honeywell observed range
// TBD: make this dynamic based on min/max over a long runtime
#define lowset 10
#define highset 270
extern int lowSet, highSet;
extern bool honeywellOnToggle;
float readAnalogRotationValue();
#endif // HONEY
extern float mastRotate;
extern int rotateout;
extern int mastAngle;

class RotationSensor {
  public:
    static int newValue;
    static int oldValue;
};

class WindSensor {
  public:
    static double windSpeedKnots;
    static double windSpeedMeters;
    static double windAngleDegrees;
    static double windAngleRadians;
};

#ifdef RTK
void setupRTK();
void loopRTK();
void sendCommand(String command);
void updateRTKCommands(String command, float interval = -1);
extern char nmeaSentence[];

extern const char* fixQuality[];
extern const char* calStatus[];
// RTK
struct PositionType {
  int code;
  const char* name;
  const char* description;
};
extern const PositionType positionTypes[];
const char* getPositionTypeName(int code);
const char* getPositionTypeDescription(int code);

extern bool debugRTK;   // debug RTK by printing extra info in NMEA0183Handlers
extern bool directRTK;  // debug RTK by sending output to Serial instead of to NMEA0183Handlers
#endif

#ifdef NMEA0183
#include <NMEA0183.h>
#include <NMEA0183Msg.h>
#include <NMEA0183Messages.h>
extern tNMEA0183 NMEA0183_3;
extern bool debugNMEA;
// no idea how many satellites there are, but not tracking them RN anyway
#define MAXSAT 140  
extern struct tGSV GSVseen[]; 
extern int maxSat;
extern bool GSVtoggle;
void HandleNMEA0183Msg(const tNMEA0183Msg &NMEA0183Msg);
void DebugNMEA0183Handlers(Stream* _stream);
byte calculateNMEAChecksum(const char* sentence);
#endif

#ifdef UDP_FORWARD
#include <WiFiUdp.h>
extern WiFiUDP udp_server;
extern IPAddress broadcastIP;
#define UDP_FORWARD_PORT 2000
void setupUDP();
void handle_serial_event();
void handle_outgoing_sentence(const char *sentence, size_t length);
void transmit_outgoing_over_udp(const char *sentence, size_t length);
//#define NMEA0183RX 16 // ESPberry RX0 = IO16
//#define NMEA0183TX 15 
// SK Pang says it's mapped to /dev/ttyS0 which is 14(tx) and 15(rx) on RPI
// but that's RPI GPIOs, and RPI GPIO 15 is mapped to IO16 on ESPBerry
#endif

#ifdef TCP_FORWARD
#include <WiFiClient.h>
extern WiFiServer tcp_server;
extern WiFiClient tcp_client;
#define TCP_PORT 4444
extern bool TCPdebug;
void setupTCP();
void handle_tcp_connections();
void handle_serial_event();
void handle_outgoing_sentence(const char *sentence, size_t length);
void transmit_outgoing_over_tcp(const char *sentence, size_t length);
#endif

#ifdef SEATALK
#define AutopilotSerial Serial1
#endif
extern bool STdebug;
extern bool sendSeaTalk;  // flag will be set whenever we have ST data to send to autopilot
//#define STTX 25
#define STTX 15 // ESPBerry TX0 = IO15 PICAN-M
#define STBAUD 4800
#define ST_RATE 50 // 50 msec = 20 Hz transmit rate
void sendSeaTalktoAP();
void sendSTwind();
void sendRMB();
void sendAPB();

#ifdef AIS_FORWARD
#define AISserial Serial1
#endif
extern bool AISdebug;
#define AISRX 16 // ESPberry RX0 = IO16 PICAN-M
#define AISBAUD 38400

#ifdef BNO08X
#include "BNO085Compass.h"
extern BNO085Compass compass;
//extern int boatOrientation; // moved to boatdata
extern bool debugCompass;
extern const char* calStatus[];
extern bool xmitBNOheading;
extern bool xmitRTKheading;
#endif

#ifdef DISPLAYON
void setup_display();
void loop_display();
void turnoff();
extern bool displayOnToggle;
#endif

void i2cScan(TwoWire& wire);

#ifdef TACK
#include "tack.h"
extern bool tackDetectionEnabled;
#endif

#ifdef MOB_PING
#include <WiFiUdp.h>

// MOB constants
#define MAX_MOB 12
#define MOB_PORT 3559
#define DEF_TIMEOUT 30
#define NUM_MISSED 5
#define WAIT 30

// MOB tracking data structure
struct MOBEntry {
  char* username;           // Username of the MOB device
  char* client;            // IP address or hostname
  int interval;            // Ping interval in seconds
  int num_missed;          // Number of consecutive missed pings
  unsigned long timeout;   // Countdown timer in seconds
};

// MOB tracking variables
extern MOBEntry mobList[MAX_MOB];
extern int mobCount;

// MOB functions
void setupMOB();
void checkMOB();
int findMOBEntry(const char* username);
void addMOBEntry(const char* username, const char* client, int interval);
void resetMOBTimer(int index);

#endif

extern bool teleplot;