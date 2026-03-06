#ifdef DISPLAYON
#include "include-general.h"
#include "windparse.h"
#include "BoatData.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET 13
//Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SSD1306 *display;

void setup_display() {
  pinMode(OLED_RESET, OUTPUT); 
  digitalWrite(OLED_RESET, LOW); delay (100); digitalWrite(OLED_RESET, HIGH); 
  display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
  if (display->begin(SSD1306_SWITCHCAPVCC, 0x3C))
    Serial.println("SSD1306 allocation OK");
  else
    Serial.println(F("SSD1306 allocation failed"));
  display->setRotation(0);
  display->clearDisplay();
  display->setTextSize(1);
  display->setTextColor(SSD1306_WHITE);
  display->setCursor(0,0);             // Start at top-left corner
  display->println(F("ESP32 Mast Rotation"));
  display->display();
}

void turnoff() {
  log::toAll("turning display off");
  display->clearDisplay();
  display->display();
}

void loop_display() {
  double windSpeedKnots = WindSensor::windSpeedKnots;
  double windAngleDegrees = WindSensor::windAngleDegrees;
  display->clearDisplay();
  display->setCursor(0, 5);
  //display->printf("CAN: %s ", can_state.c_str());
  unsigned long uptime = now / 1000;
  display->printf("Up:%*lu ", 3, uptime % 10000); // just print last 3 digits
  if (WiFi.status() == WL_CONNECTED) {
    if (uptime % 9 == 0)
      display->printf("%s\n", WiFi.SSID().substring(0,12));
    else display->printf("%s\n", host.substring(0,12));
  } else display->printf("----\n");
  if (uptime % 300 == 0)  
    log::toAll("uptime: " + String(uptime) + " heap: " + String(ESP.getFreeHeap()));
  display->printf("N2K:%d ", num_n2k_messages % 10000);
  display->printf("Wind:%d\n", num_wind_messages % 10000);
  display->printf("S/A/R:%2.1f/%2.0f/%d\n", windSpeedKnots, windAngleDegrees,rotateout);
  //display->printf("Rot:%d\n", mastRotate);
#ifdef HONEY
  if (honeywellOnToggle) {
    display->printf("S:%d/%d/%d M:%d\n", PotLo, PotValue, PotHi, mastAngle);
  }
#endif
#ifdef BNO08X
  if (compass.OnToggle) {
    //display->printf("M:%.1f° B:%.1f T:%0.1f\n", mastCompassDeg, compass.boatIMU, BoatData.TrueHeading);
    display->printf("M H:%.0f A:%.0f C:%d\n", compass.boatHeading,compass.boatAccuracy*RADTODEG,compass.boatCalStatus);
    //display->printf("Delta:%2d\n", mastAngle);
  }
#endif
#ifdef RTK
  display->printf("Lat:%2.2f Lon:%2.2f\nH:%.0f COG:%0.f\n", pBD->Latitude, pBD->Longitude, pBD->RTKheading, pBD->COG);
#endif
  display->display();
}
#endif
