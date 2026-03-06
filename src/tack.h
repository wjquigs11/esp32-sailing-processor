#ifndef _TACK_H_
#define _TACK_H_

#include <Arduino.h>

// Maneuver Types
enum ManeuverType {
  NO_MANEUVER,
  TACK_PORT_TO_STARBOARD,
  TACK_STARBOARD_TO_PORT,
  JIBE_PORT_TO_STARBOARD,
  JIBE_STARBOARD_TO_PORT
};

// Function declarations for tack detection system
void initTackDetection();
void updateTackDetection();
String getTackStats();
void resetTackStats();
ManeuverType getLastManeuver();
bool isInManeuver();

#endif // _TACK_H_