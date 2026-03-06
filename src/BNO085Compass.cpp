#ifdef BNO08X
/*
NOTE: i2c address is set by #ifdef or -D in platformio.ini
*/
#include <Adafruit_BNO08x.h>
#include "include-general.h"
#include "windparse.h"
#include "compass.h"
#include "BNO085Compass.h"

bool debugCompass = false;
bool xmitBNOheading = true;
const char* calStatus[] = {"uncalibrated", "poor", "good", "excellent"};

BNO085Compass compass;

BNO085Compass::BNO085Compass(int8_t reset_pin) :
    bno08x(reset_pin),
    reportType(0x09),
    frequency(10),
    heading(0.0),
    boatHeading(0.0),
    boatAccuracy(0.0),
    boatIMU(0.0),
    boatCalStatus(0),
    IMUready(false),
    OnToggle(false),
    teleplot(false),
    totalReports(0),
    headingErrCount(0)
{
    // Initialize numReports array
    for(int i = 0; i < SH2_MAX_SENSOR_ID; i++) {
        numReports[i] = 0;
    }
}

bool BNO085Compass::begin(int i2c_addr) {
    if (!bno08x.begin_I2C(i2c_addr)) {
        log::toAll("BNO08x not found");
        //i2cScan(Wire);
        return false;
    }
    log::toAll("BNO08x Found\n");
    for (int n = 0; n < bno08x.prodIds.numEntries; n++) {
        String logString = "Part " + String(bno08x.prodIds.entry[n].swPartNumber) + ": Version :" + String(bno08x.prodIds.entry[n].swVersionMajor) + "." + String(bno08x.prodIds.entry[n].swVersionMinor) + "." + String(bno08x.prodIds.entry[n].swVersionPatch) + " Build " + String(bno08x.prodIds.entry[n].swBuildNumber);
        log::toAll(logString);
    }
    return true;
}

// we ALWAYS enable SH2_ARVR_STABILIZED_GRV, as well as another report set by the user, 
// usually SH2_GEOMAGNETIC_ROTATION_VECTOR (0x09) to get a compass heading
// note that GRV is requested 10x the rate of the other (compass) report
bool BNO085Compass::setReports() {
    log::toAll("Setting compass report to: 0x" + String(this->reportType,HEX) + " at frequency " + String(this->frequency));
    if (!bno08x.enableReport(this->reportType, this->frequency*10000)) {
        log::toAll("could not set report type: " + String(this->reportType,HEX));
        return false;
    }
    #if 0
    if (!bno08x.enableReport(SH2_ARVR_STABILIZED_GRV, this->frequency*1000)) {
        log::toAll("could not set report type (2): " + String(SH2_ARVR_STABILIZED_GRV,HEX));
        return false;
    } else log::toAll("enabled " + String(SH2_ARVR_STABILIZED_GRV,HEX));
    #endif
    return true;
}
/*        if (!bno08x.enableReport((sh2_SensorId_t)reportType)) {
        return false;
    }
    else return true;
*/

void BNO085Compass::logPart() {
    log::toAll("test");
}

int BNO085Compass::getHeading(int correction) {
    if (bno08x.wasReset()) {
        setReports();
    }

    if (!bno08x.getSensorEvent(&sensorValue)) {
        this->headingErrCount++;
        Serial.println("no compass event available");
        return -1;
    }

    numReports[sensorValue.sensorId]++;
    totalReports++;
    switch (sensorValue.sensorId) {
        case SH2_GAME_ROTATION_VECTOR:
        case SH2_GEOMAGNETIC_ROTATION_VECTOR:
            this->boatAccuracy = sensorValue.un.rotationVector.accuracy;
            this->boatCalStatus = sensorValue.status;
            this->boatHeading = calculateHeading(sensorValue.un.rotationVector.real,
                                    sensorValue.un.rotationVector.i,
                                    sensorValue.un.rotationVector.j,
                                    sensorValue.un.rotationVector.k,
                                    correction);
            return 1;
        case SH2_ARVR_STABILIZED_GRV:
            this->boatIMU = calculateHeading(sensorValue.un.arvrStabilizedGRV.real,
                                        sensorValue.un.arvrStabilizedGRV.i,
                                        sensorValue.un.arvrStabilizedGRV.j,
                                        sensorValue.un.arvrStabilizedGRV.k,
                                        correction);
            return 1;
        default:
            this->headingErrCount++;
            Serial.println("no sensorvalue handler");
            return -2;
    }
}

float BNO085Compass::getBoatAccuracy() const { return this->boatAccuracy; }
int BNO085Compass::getBoatCalStatus() const { return this->boatCalStatus; }

float BNO085Compass::calculateHeading(float r, float i, float j, float k, int correction) {
    float r11 = 1 - 2 * (j * j + k * k);
    float r21 = 2 * (i * j + r * k);
    float r31 = 2 * (i * k - r * j);
    float r32 = 2 * (j * k + r * i);
    float r33 = 1 - 2 * (i * i + j * j);

    float theta = -asin(r31);
    float phi = atan2(r32, r33);
    float psi = atan2(-r21, r11);
    float heading = (psi * RADTODEG) + (float)correction;

    if ((int)heading > 359) {
        heading -= 360.0;
    }
    if ((int)heading < 0) {
        heading += 360.0;
    }

    return heading;
}
#endif
