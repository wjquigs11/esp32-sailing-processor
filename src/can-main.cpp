/*
Simplified(?) Garmin N2K mast rotation correction

Using ESPberry and PICAN-M HAT, but future incarnations could use Arduino-format ESP32 devkit (that can take 12VDC power)
with possibly Arduino CAN HAT or simply MCP2515 SPI CAN interface (as long as it's a module that supports 3.3v; most do not)

This version reads raw CAN frames from the PICAN-M MCP interface. It only parses PGN 130306 (wind).
It creates a tN2KMsg with the corrected wind data.
All other frames on the "wind bus" are used to create a tN2KMsg from the source frame.
In both cases, messages are transmitted on the Main bus (which currently is the native ESP32 CAN interface)

*/
#include "include-general.h"
#include "windparse.h"
#include "BoatData.h"

const unsigned long TransmitMessages[] PROGMEM={130306L,0};
bool displayOnToggle=true, honeywellOnToggle=false, windToggle=true;
unsigned int num_n2k_messages, num_wind_messages, num_wind_other, num_wind_xmit, num_wind_fail, num_xmit, num_fail_xmit;

#ifdef TACK
bool tackDetectionEnabled = true;
#endif

#ifdef HONEY
movingAvg honeywellSensor(5);
Adafruit_ADS1015 ads;
int adsInit;
#endif

#ifdef N2K
tNMEA2000 *n2kMain;
bool n2kMainOpen = false;
bool debugN2K = false;
bool n2kWindOpen = false;
bool debugWind = false;
tN2kWindReference wRef;
void HandleMainN2kMessage(const tN2kMsg &N2kMsg);
#endif

tBoatData BoatData;
tEnvStats ENVdata;
tBoatData *pBD=&BoatData;
tEnvStats *pENV=&ENVdata;
#ifdef RTK_TIMO
tRTKstats RTKdata;
tRTKstats *pRTK=&RTKdata;
tSatelliteStats SATdata;
tSatelliteStats *pSAT=&SATdata;
#endif

tNMEA2000_mcp *n2kWind;

// Initialize static variables for RotationSensor Class
int RotationSensor::newValue{0};
int RotationSensor::oldValue{0};

double WindSensor::windSpeedKnots{0.0};
double WindSensor::windAngleDegrees{0.0};
double WindSensor::windSpeedMeters{0.0};
double WindSensor::windAngleRadians{0.0};

int rotateout=0;
int portRange=50, stbdRange=50; // NB BOTH are positive
int mastAngle;
int PotValue, PotLo=999, PotHi;
bool logPot = false;

static int i=0;

void OnOpenMain() {
  log::toAll("MAIN open");
  n2kMainOpen = true;
  // Start schedulers now.
}

void OnOpenWind() {
  n2kWindOpen = true;
  log::toAll("WIND open");
}

void setup_can() {
  //preferences.putString("hostname", "espwind");
  Wire.begin();
#ifdef DISPLAYON
  setup_display();
#endif
#ifdef HONEY
  if (!(adsInit = ads.begin())) {  // start ADS1015 ADC default 0x4A ??? same as bno
    log::toAll("ads sensor not found");
    honeywellOnToggle = false;
    //i2cScan(Wire);
  } else {
    honeywellOnToggle = true;
    honeywellSensor.begin();    // Instantiates the moving average object
  }
#endif // HONEY
// re-transmitting AIS sentences on UDP and sending the weird Seatalk wind sentence to the autopilot
// there's no reason to specify RX port for Seatalk, and no reason for TX port for AIS
// SEATALK uses RS232->UART module and softwareSerial
#ifdef SEATALK
  AutopilotSerial.begin(STBAUD, SERIAL_8N1, -1, STTX);
  if (!AutopilotSerial)
    log::toAll("failed to open SeaTalk serial port");
  else
    log::toAll("opened Seatalk serial port TX:" + String(STTX) + " baud:" + String(STBAUD));
#endif
#ifdef AIS_FORWARD
  AISserial.begin(AISBAUD, SERIAL_8N1, AISRX, -1);
  if (!AISserial)
    log::toAll("failed to open AISserial serial port");
  else
    log::toAll("opened AISserial serial port RX:" + String(AISRX) + " baud:" + String(AISBAUD));
#endif
#if 0
  // handle the case where we're using both on same port
  AISserial.begin(AISBAUD, SERIAL_8N1, AISRX, STTX);
  if (!AISserial)
    log::toAll("failed to open AISserial/Seatalk serial port");
  else
    log::toAll("opened AISserial/Seatalk serial port RX:" + String(AISRX) + " baud:" + String(AISBAUD));
#endif
  pBD->magOrientation = preferences.getInt("magOrientation",0);
  pBD->sensOrientation = preferences.getInt("sensOrientation",0);
  pBD->rtkOrientation = preferences.getInt("rtkOrientation",0);
  windToggle = preferences.getBool("WindToggle",true);
#ifdef N2K
  n2kMain = new tNMEA2000_esp32(CAN_TX_PIN, CAN_RX_PIN);
  // Reserve enough buffer for sending all messages.
  n2kMain->SetN2kCANSendFrameBufSize(10);
  n2kMain->SetN2kCANReceiveFrameBufSize(250);
  // Set Product information
  n2kMain->SetProductInformation(
      "20240608",  // Manufacturer's Model serial code (max 32 chars)
      103,         // Manufacturer's product code
      "SH-ESP32 Wind Correction",  // Manufacturer's Model ID (max 33 chars)
      "0.1.0.0 (2021-03-31)",  // Manufacturer's Software version code (max 40
                               // chars)
      "0.0.3.1 (2021-03-07)"   // Manufacturer's Model version (max 24 chars)
  );
  // Set device information
  n2kMain->SetDeviceInformation(
      20240608, // serial
      132, // analog gateway
      25, // inter/intranetwork device 
      2046
  );
  n2kMain->SetMode(tNMEA2000::N2km_ListenAndSend);
  n2kMain->SetForwardType(tNMEA2000::fwdt_Text); // Show bus data in clear
  n2kMain->EnableForward(false);
  n2kMain->SetForwardOwnMessages(false);
  n2kMain->SetMsgHandler(HandleMainN2kMessage); 
  n2kMain->ExtendTransmitMessages(TransmitMessages);
  n2kMain->SetOnOpen(OnOpenMain);
  log::toAll("opening n2kMain");
  n2kMain->Open();
#endif
  n2kWind = new tNMEA2000_mcp(SPI_CS_PIN,MCP_16MHz);
  n2kWind->SetN2kCANMsgBufSize(8);
  n2kWind->SetN2kCANReceiveFrameBufSize(100);
  n2kWind->SetMode(tNMEA2000::N2km_ListenOnly);
  n2kWind->SetForwardStream(&Serial);
  n2kWind->SetForwardType(tNMEA2000::fwdt_Text);
  n2kWind->EnableForward(false);
  n2kWind->SetForwardOwnMessages(false);
  n2kWind->SetOnOpen(OnOpenWind);
  n2kWind->SetMsgHandler(HandleWindN2KMessage);
  log::toAll("opening n2kWind");
  n2kWind->Open();
#if defined(RTK) || defined(RTK_TIMO)
  setupRTK();
#endif
// move to platformio
#ifdef BNO08X
  log::toAll("BNO08X");
  compass.IMUready = compass.begin(BNOADDR);
  if (compass.IMUready) {
    compass.OnToggle = true;  // in case I want to turn it off and use only RTK
    // setReports() will be called automatically in getHeading() when wasReset() returns true
  } else {
    log::toAll("BNO08x not found");
    //i2cScan(Wire);
  }
#endif
  // BME280 environmental sensor
  if (bmeFound = startBME()) {
    log::toAll("BME280 found");
  } else {
    log::toAll("Could not find a valid BME280 sensor");
  }
  initWindFrequencyMeasurement();
  
#ifdef TACK
  // Initialize tack detection system
  if (tackDetectionEnabled) {
    initTackDetection();
    log::toAll("Tack detection enabled");
  }
#endif
#ifdef MOB_PING
  setupMOB();
#endif
}

void loop_can() {
  static unsigned long lastEventTime;
  // MOB check - runs once per second
#ifdef MOB_PING
  static unsigned long lastMOBCheck = 0;
  if (now - lastMOBCheck >= 1000 || lastMOBCheck == 0) {
    lastMOBCheck = now;
    checkMOB();
  }
#endif
  n2kMain->ParseMessages();
  n2kWind->ParseMessages();
#if defined(RTK) || defined(RTK_TIMO)
  loopRTK();
#endif
//#if defined(UDP_FORWARD) || defined(TCP_FORWARD)
  // handling incoming AIS data differently; not one char at a time
  // might be more efficient but also might take longer per sentence, hence yield() after
  //handle_serial_event();
  //yield();
//#endif
#ifdef SEATALK
  sendSeaTalktoAP();
#endif
  // this part of the loop will only execute every timerDelay msecs
  if (now - lastEventTime > timerDelay || lastEventTime == 0) {
    lastEventTime = now;
#ifdef BNO08X
  if (compass.IMUready) {
    float heading;
    int err;
    if ((err = compass.getHeading(pBD->magOrientation)) < 0) {
      if (compass.headingErrCount % 500 == 0)
        log::toAll("heading total reports: " + String(compass.totalReports) + " error count: " + String(compass.headingErrCount) + " ret val: " + String(err));
    } else {
      pBD->magHeading = compass.boatHeading;
      pBD->BNOheading = compass.boatHeading;  // Store in BNOheading as well
      if (teleplot) {
        //Serial.printf(">mag: %.1f\n", pBD->magHeading);
      }
      if (debugCompass)
        log::toAll("heading: " + String(pBD->magHeading) + " accuracy: " + String(compass.boatAccuracy)
        + " calstatus: " + String(compass.boatCalStatus*RADTODEG,0)
        + " total reports: " + String(compass.totalReports) + " error count: " + String(compass.headingErrCount) + " ret val: " + String(err));
      }
#ifdef N2K
      // if BNO is enabled/defined, and xmitBNOheading == true, send on N2K bus
      if (n2kMain!=nullptr && xmitBNOheading) {
        SetN2kPGN127250(n2kMsg, 255, pBD->BNOheading*DEGTORAD, 0, pBD->Variation*DEGTORAD, N2khr_magnetic);
        if (!n2kMain->SendMsg(n2kMsg)) num_fail_xmit++; else num_xmit++;
      }
#endif
    }
#endif  // BNO08X
#ifdef DISPLAYON
      // this is where we really need reactESP because display update should not be same time period as web update
      if (displayOnToggle) loop_display();
#endif
      if (bmeFound) readBME();
      
#ifdef TACK
      // Update tack detection system
      if (tackDetectionEnabled) {
        updateTackDetection();
      }
#endif
  } // end of timer block
}

PGNStats pgnTracker[MAX_TRACKED_PGNS];
int trackedPGNCount = 0;

// Helper function to find or create PGN entry in sparse array
int findOrCreatePGNEntry(unsigned long pgn) {
  // First, try to find existing entry
  for (int i = 0; i < trackedPGNCount; i++) {
    if (pgnTracker[i].pgn == pgn) {
      return i;
    }
  }
  // If not found and we have space, create new entry
  if (trackedPGNCount < MAX_TRACKED_PGNS) {
    int newIndex = trackedPGNCount++;
    pgnTracker[newIndex].pgn = pgn;
    pgnTracker[newIndex].count = 0;
    pgnTracker[newIndex].age = 0;
    return newIndex;
  }
  
  // Array is full, return -1 to indicate failure
  return -1;
}

int lowSet=lowset;
int highSet=highset;

#ifdef HONEY
// returns degrees, and corresponds to the current value of the Honeywell sensor
float readAnalogRotationValue() {
  if (adsInit)
    PotValue = ads.readADC_SingleEnded(0);
  //int AltValue = adc1_get_raw(ADC1_CHANNEL_5);
  if (!PotValue)
    return 0;
  // determine range of A2D values; this might be different on your boat
  // after calibration, a value lower than PotLo or higher than PotHi should cause an error
  if (PotValue < PotLo && PotValue > 0) { 
    PotLo = PotValue;
    readings["PotLo"] = String(PotLo); // for calibration
    readings["lowSet"] = String(lowset);
    if (PotLo < lowset)
      log::toAll("WARNING! PotValue lower than lowset " + String(PotLo));
  } else if (PotValue > PotHi) {
    PotHi = PotValue;
    readings["PotHi"] = String(PotHi); // for calibration
    readings["highset"] = String(highset);
    if (PotHi > highset) 
      log::toAll("WARNING! PotValue greater than highset value:" + String(PotHi) + " highset:" + String(highset));
  }
  readings["PotValue"] = String(PotValue);
  if (logPot) {
    sprintf(prbuf, " pot low:%d (lowset:%d)/current: %d/high: %d (highset:%d)", PotLo, lowset, PotValue, PotHi, highset);
    log::toAll(prbuf);
  }
  // the moving average variable is only initialized if ADC is present
  int newValue = honeywellSensor.reading(PotValue);    // calculate the moving average
  int oldValue = RotationSensor::oldValue;
  
  if (newValue < highset) {  // writes value to oldsensor if below highset threshold
    oldValue = newValue;
  }
  // Update values for new and old values (for the next loop iteration)
  RotationSensor::newValue = newValue;
  RotationSensor::oldValue = oldValue;

  // map 10 bit number to degrees of rotation
  mastAngle = map(oldValue, lowSet, highSet, -portRange, stbdRange)-pBD->sensOrientation;
  //sprintf(prbuf,"map: %d %d %d %d %d = %ld\n", oldValue, lowset, highset, -portRange, stbdRange, mastAngle[0]);
  //log::toAll(prbuf);
  return mastAngle; 
}
#endif // HONEY
