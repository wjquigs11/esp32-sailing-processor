#ifdef SEATALK
/*
We are extracting route data from the N2K network in n2k.cpp
The route PGNs are typically sent by SignalK/Freeboard
We convert the route PGNs to BoatData fields (destination lat/lon, XTE, etc)
Then we convert BoatData fields to NMEA0183 sentences and send on serial to the autopilot

It would be simpler to use the N2K->0183 plugin on the Pi
I thought I didn't want to create a dependency on the Pi, but that's silly because the Pi is the only way I can set a route currently.
*/
#include "include-general.h"
#include "windparse.h"
#include "BoatData.h"

//EspSoftwareSerial::UART AutopilotSerial;
bool STdebug = false;
bool sendSeaTalk = false;
char n0183buf[MAX_NMEA0183_MSG_LEN], n0183cksumbuf[MAX_NMEA0183_MSG_LEN];
unsigned long lastSeatalkTX = 0;

void sendSeaTalktoAP() {
    if (sendSeaTalk && (now - lastSeatalkTX > ST_RATE)) {
        lastSeatalkTX = now;
        sendSTwind();
        // this is a pretty crappy way to check if we have an active waypoint, TBD
        if (pBD->DistanceToWaypoint > 0.00000001) {
            sendRMB();
            sendAPB();
        }
        sendSeaTalk = false;
    }
}

// bit of a misnomer since it's sending NMEA0183 wind to the Seatalk instrument (ST2000+)
void sendSTwind() {
    float bowAngle;
    char direction;
    if (rotateout > 180) {
        bowAngle = 360 - rotateout;
        direction = 'L'; // Left
    } else {
        bowAngle = rotateout;
        direction = 'R'; // Right
    }
    sprintf(n0183buf,"$IIVWR,%2d,%c,%2.1f,N,%2.1f,M,%2.1f,K*",(int)rotateout, direction, WindSensor::windSpeedKnots, WindSensor::windSpeedMeters, WindSensor::windSpeedMeters/3.6);
    int cksum = calculateNMEAChecksum(n0183buf);
    sprintf(n0183cksumbuf,"%s%02X", n0183buf, cksum);
    if (STdebug) log::toAll("STwind:" + String(n0183cksumbuf));
    AutopilotSerial.println(n0183cksumbuf);
}

/* sentences for autopilot, generated from NMEA2000 messages from SK on RPI from 'navigate to waypoint'
In practice, a typical GPS/plotter outputting RMB + APB (or APB + BWC/BWR) at 4800 baud satisfies what the ST2000+ needs for Track mode and waypoint advance, as those sentences include XTE, bearing, distance, and waypoint ID.
*/
void sendRMB() {
    if (pBD == nullptr) return;
    // RMB - Recommended Minimum Navigation Information
    // $--RMB,A,x.x,a,c--c,c--c,llll.ll,a,yyyyy.yy,a,x.x,x.x,x.x,A*hh
    // Field 1: Data status (A=OK, V=warning)
    // Field 2: Cross-track error magnitude in nautical miles (0-9.99 nm typical)
    // Field 3: Direction to steer (L=steer left, R=steer right to correct XTE)
    // Field 4: Origin waypoint ID (name/number of starting waypoint for this leg)
    // Field 5: Destination waypoint ID (active target waypoint)
    // Field 6: Destination latitude (ddmm.mm format)
    // Field 7: Destination latitude hemisphere (N or S)
    // Field 8: Destination longitude (dddmm.mm format)
    // Field 9: Destination longitude hemisphere (E or W)
    // Field 10: Range to destination in nautical miles
    // Field 11: True bearing to destination in degrees true
    // Field 12: Velocity toward destination (closing speed, knots)
    // Field 13: Arrival status (A=arrived, V=not arrived)
    
    char xteDirection = (pBD->XTE < 0) ? 'L' : 'R';
    double xteNM = fabs(pBD->XTE) * 0.000539957; // Convert meters to nautical miles
    double distanceNM = pBD->DistanceToWaypoint * 0.000539957; // Convert meters to nautical miles
    
    // Extract latitude/longitude components for ddmm.mm format
    int latDeg = (int)fabs(pBD->WaypointLatitude);
    double latMin = (fabs(pBD->WaypointLatitude) - latDeg) * 60.0;
    char latNS = (pBD->WaypointLatitude >= 0) ? 'N' : 'S';
    
    int lonDeg = (int)fabs(pBD->WaypointLongitude);
    double lonMin = (fabs(pBD->WaypointLongitude) - lonDeg) * 60.0;
    char lonEW = (pBD->WaypointLongitude >= 0) ? 'E' : 'W';
    
    // Calculate velocity toward destination (VMG to waypoint)
    // This would typically require SOG and COG data to calculate closing speed
    double velocityToward = 0.0; // Default to 0 if we can't calculate it
    if (pBD->SOG > 0 && pBD->COG >= 0) {
        // Calculate velocity component toward waypoint
        double bearingDiff = fabs(pBD->COG - pBD->BearingToWaypoint);
        if (bearingDiff > 180) bearingDiff = 360 - bearingDiff;
        velocityToward = pBD->SOG * cos(bearingDiff * M_PI / 180.0) * MTOKTS; // Convert m/s to knots
        if (velocityToward < 0) velocityToward = 0.0; // Only positive closing speeds
    }
    
    // Determine arrival status - using ArrivalCircleEntered from BoatData
    char arrivalStatus = pBD->ArrivalCircleEntered ? 'A' : 'V';
    
    sprintf(n0183buf,
        "$GPRMB,A,%.2f,%c,WPT%03lu,WPT%03lu,%02d%05.2f,%c,%03d%05.2f,%c,%.1f,%.1f,%.1f,%c*",
        xteNM, xteDirection,
        (pBD->WaypointNumber > 0) ? pBD->WaypointNumber - 1 : 0,
        pBD->WaypointNumber,
        latDeg, latMin, latNS,
        lonDeg, lonMin, lonEW,
        distanceNM, pBD->BearingToWaypoint, velocityToward, arrivalStatus);
    
    int cksum = calculateNMEAChecksum(n0183buf);
    sprintf(n0183cksumbuf, "%s%02X", n0183buf, cksum);
    if (STdebug) log::toAll(n0183cksumbuf);
    AutopilotSerial.println(n0183cksumbuf);
}

void sendAPB() {
    if (pBD == nullptr) return;    
    // APB - Autopilot Sentence "B"
    // $--APB,A,A,x.x,a,N,A,A,x.x,a,c--c,x.x,a,x.x,a*hh
    // A = General warning flag (A=OK, V=warning)
    // x.x = Cross-track error magnitude
    // a = Direction to steer (L/R)
    // N = Cross-track units (N=nautical miles)
    // A = Status (A=arrival circle entered, V=not entered)
    // A = Status (A=perpendicular passed at waypoint, V=not passed)
    // x.x = Bearing origin to destination
    // a = M = Magnetic, T = True
    // c--c = Destination waypoint ID
    // x.x = Bearing, present position to destination
    // a = M = Magnetic, T = True
    // x.x = Heading to steer to destination waypoint
    // a = M = Magnetic, T = True
    char xteDirection = (pBD->XTE < 0) ? 'L' : 'R';
    double xteNM = fabs(pBD->XTE) * 0.000539957; // Convert meters to nautical miles
    char arrivalFlag = pBD->ArrivalCircleEntered ? 'A' : 'V';
    char perpFlag = pBD->PerpendicularCrossed ? 'A' : 'V';
    
    // Calculate heading to steer (bearing to waypoint)
    double headingToSteer = pBD->BearingToWaypoint;
    
    sprintf(n0183buf, "$GPAPB,A,%.3f,%c,N,%c,%c,%.1f,T,WPT%03lu,%.1f,T,%.1f,T*",
            xteNM, xteDirection, arrivalFlag, perpFlag,
            pBD->BearingToWaypoint, pBD->WaypointNumber,
            pBD->BearingToWaypoint, headingToSteer);
    
    int cksum = calculateNMEAChecksum(n0183buf);
    sprintf(n0183cksumbuf, "%s%02X", n0183buf, cksum);
    if (STdebug) log::toAll(n0183cksumbuf);
    AutopilotSerial.println(n0183cksumbuf);
}
#endif