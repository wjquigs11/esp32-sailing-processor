#include "include-general.h"
#include "windparse.h"
#include "BoatData.h"

// this idiocy is necessary because the BME library and MCP_CAN library both define these constants
#undef MODE_SLEEP
#undef MODE_NORMAL

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
Adafruit_BME280 bme;
bool bmeFound;
#define SEALEVELPRESSURE_HPA (1013.25)
#define BME_ADDR 0x76

bool startBME() {
  return bme.begin(BME_ADDR);
}

void readBME() {
  pENV->temp = 1.8 * bme.readTemperature() + 32.0;
  pENV->pressure = bme.readPressure()/100.0;
  pENV->humidity = bme.readHumidity();
  //bme.readAltitude(SEALEVELPRESSURE_HPA)*3.28084);
  //log::toAll("Temperature = " + String(1.8 * bme.readTemperature() + 32));
  //log::toAll("Pressure = " + String(press / 100.0F) + " hPa " + String(press * 0.0002953) + " inHg");
  //log::toAll("Altitude = " + String(bme.readAltitude(SEALEVELPRESSURE_HPA)*3.28084));
  //log::toAll("Humidity = " + String(bme.readHumidity()) + " %");
}

#if 0
String readBME280Temperature() {
  // Read temperature as Celsius (the default)
  float t = bme.readTemperature();
  // Convert temperature to Fahrenheit
  t = 1.8 * t + 32;
  if (isnan(t)) {    
    Serial.println("Failed to read from BME280 sensor!");
    return "";
  }
  else {
    //Serial.printf("temp %0.0f\n", t);
    return String(t,0);
  }
}

String readBME280Humidity() {
  float h = bme.readHumidity();
  if (isnan(h)) {
    Serial.println("Failed to read from BME280 sensor!");
    return "";
  }
  else {
    //Serial.printf("humidity %0.0f\n", h);
    return String(h,0);
  }
}

String readBME280Pressure() {
  float p = bme.readPressure() / 100.0F;
  if (isnan(p)) {
    Serial.println("Failed to read from BME280 sensor!");
    return "";
  }
  else {
    //Serial.printf("pressure %0.0f\n", p);
    return String(p,0);
  }
}
#endif

