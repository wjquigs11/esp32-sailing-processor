/*
 * Tack and Jibe Detection System
 * 
 * Implements a robust multi-state machine approach to detect sailing maneuvers
 * using AWA, heading, and boat speed data from tBoatData structure.
 * 
 * Key features:
 * - Multi-state machine with hysteresis to avoid false positives
 * - Uses both AWA sign changes and heading changes for confirmation
 * - Adaptive thresholds for upwind vs downwind sailing
 * - Rate-of-change analysis to distinguish real maneuvers from wind shifts
 * - Separate logic for tacks (upwind) and jibes (downwind)
 */
#ifdef TACK
#include "include-general.h"
#include "BoatData.h"
#include "windparse.h"
#include <movingAvg.h>

// Tack/Jibe Detection States
enum TackState {
  NORMAL_PORT,        // Sailing normally on port tack
  NORMAL_STARBOARD,   // Sailing normally on starboard tack
  TACK_CANDIDATE,     // Potential tack detected, awaiting confirmation
  TACK_CONFIRMED,     // Tack confirmed and completed
  JIBE_CANDIDATE,     // Potential jibe detected, awaiting confirmation
  JIBE_CONFIRMED,     // Jibe confirmed and completed
  UNKNOWN             // Initial state or invalid data
};

// Sailing Mode Detection
enum SailingMode {
  UPWIND,    // Close-hauled, AWA typically 25-45°
  REACHING,  // Beam to broad reach, AWA 45-120°
  DOWNWIND   // Running, AWA > 120°
};

// Maneuver Types
enum ManeuverType {
  NO_MANEUVER,
  TACK_PORT_TO_STARBOARD,
  TACK_STARBOARD_TO_PORT,
  JIBE_PORT_TO_STARBOARD,
  JIBE_STARBOARD_TO_PORT
};

// Configuration Constants
const float UPWIND_MIN_AWA = 25.0;      // Minimum AWA for upwind sailing (degrees)
const float REACHING_MIN_AWA = 45.0;    // Transition to reaching mode
const float DOWNWIND_MIN_AWA = 120.0;   // Transition to downwind mode
const float HYSTERESIS_MARGIN = 5.0;    // Degrees of hysteresis to prevent chattering
const float MIN_HEADING_CHANGE = 40.0;  // Minimum heading change for tack/jibe (degrees)
const float MAX_HEADING_CHANGE = 150.0; // Maximum reasonable heading change
const float MIN_AWA_RATE = 5.0;         // Minimum AWA rate of change (deg/sec)
const float MIN_HEADING_RATE = 10.0;    // Minimum heading rate of change (deg/sec)
const unsigned long CANDIDATE_TIMEOUT = 15000;  // Max time in candidate state (ms)
const unsigned long CONFIRMED_TIMEOUT = 5000;   // Time to stay in confirmed state (ms)
const unsigned long MIN_MANEUVER_TIME = 2000;   // Minimum time between maneuvers (ms)

// Moving averages for filtering
movingAvg awaFilter(10);        // AWA filter (1-2 seconds at 5-10Hz)
movingAvg headingFilter(5);     // Heading filter (shorter for responsiveness)
movingAvg speedFilter(10);      // Speed filter for context

// State variables
static TackState currentState = UNKNOWN;
static SailingMode sailingMode = REACHING;
static ManeuverType lastManeuver = NO_MANEUVER;
static unsigned long stateChangeTime = 0;
static unsigned long lastManeuverTime = 0;

// Baseline tracking
static float portBaselineAWA = 0.0;
static float starboardBaselineAWA = 0.0;
static float candidateStartHeading = 0.0;
static float candidateStartAWA = 0.0;
static unsigned long candidateStartTime = 0;

// Rate tracking
static float lastAWA = 0.0;
static float lastHeading = 0.0;
static unsigned long lastUpdateTime = 0;

// Statistics
static unsigned long tackCount = 0;
static unsigned long jibeCount = 0;
static unsigned long falsePositiveCount = 0;

// Function declarations
void initTackDetection();
void updateTackDetection();
SailingMode determineSailingMode(float awa, float aws);
TackState determineInitialState(float awa);
float getEffectiveHeading();
float normalizeAngle(float angle);
float angleDifference(float angle1, float angle2);
bool isValidManeuverData();
void updateBaselines(float awa, TackState state);
void processNormalState(float awa, float heading, float awaRate, float headingRate);
void processCandidateState(float awa, float heading);
void processConfirmedState();
void logManeuver(ManeuverType maneuver, float startHeading, float endHeading, float startAWA, float endAWA);

/*
 * Initialize the tack detection system
 * Call this once during setup
 */
void initTackDetection() {
  awaFilter.begin();
  headingFilter.begin();
  speedFilter.begin();
  
  currentState = UNKNOWN;
  sailingMode = REACHING;
  lastManeuver = NO_MANEUVER;
  stateChangeTime = now;
  lastManeuverTime = 0;
  
  portBaselineAWA = 0.0;
  starboardBaselineAWA = 0.0;
  
  tackCount = 0;
  jibeCount = 0;
  falsePositiveCount = 0;
  
  log::toAll("Tack detection system initialized");
}

/*
 * Main update function - call this regularly (5-10Hz) with fresh boat data
 * Processes current AWA, heading, and speed to detect maneuvers
 */
void updateTackDetection() {
  if (!pBD || !isValidManeuverData()) {
    return;
  }
  
  // now is already available as global variable
  
  // Get current values and apply filtering
  float rawAWA = pBD->AWA;
  float rawHeading = getEffectiveHeading();
  float rawSpeed = pBD->STW * MTOKTS; // Convert to knots
  
  // Apply moving average filters
  float filteredAWA = awaFilter.reading((int)(rawAWA * 10)) / 10.0; // Convert to/from int for movingAvg
  float filteredHeading = headingFilter.reading((int)(rawHeading * 10)) / 10.0;
  float filteredSpeed = speedFilter.reading((int)(rawSpeed * 100)) / 100.0;
  
  // Calculate rates of change
  float awaRate = 0.0;
  float headingRate = 0.0;
  
  if (lastUpdateTime > 0 && now > lastUpdateTime) {
    float deltaTime = (now - lastUpdateTime) / 1000.0; // Convert to seconds
    if (deltaTime > 0.1 && deltaTime < 5.0) { // Reasonable time delta
      awaRate = angleDifference(filteredAWA, lastAWA) / deltaTime;
      headingRate = angleDifference(filteredHeading, lastHeading) / deltaTime;
    }
  }
  
  // Update sailing mode
  sailingMode = determineSailingMode(abs(filteredAWA), pBD->AWS * MTOKTS);
  
  // Initialize state if unknown
  if (currentState == UNKNOWN) {
    currentState = determineInitialState(filteredAWA);
    stateChangeTime = now;
    updateBaselines(filteredAWA, currentState);
  }
  
  // State machine processing
  switch (currentState) {
    case NORMAL_PORT:
    case NORMAL_STARBOARD:
      processNormalState(filteredAWA, filteredHeading, awaRate, headingRate);
      break;
      
    case TACK_CANDIDATE:
    case JIBE_CANDIDATE:
      processCandidateState(filteredAWA, filteredHeading);
      break;
      
    case TACK_CONFIRMED:
    case JIBE_CONFIRMED:
      processConfirmedState();
      break;
  }
  
  // Update history
  lastAWA = filteredAWA;
  lastHeading = filteredHeading;
  lastUpdateTime = now;
  
  // Update baselines for current state
  if (currentState == NORMAL_PORT || currentState == NORMAL_STARBOARD) {
    updateBaselines(filteredAWA, currentState);
  }
}

/*
 * Determine sailing mode based on AWA and AWS
 */
SailingMode determineSailingMode(float awa, float aws) {
  float absAWA = abs(awa);
  
  if (absAWA < REACHING_MIN_AWA) {
    return UPWIND;
  } else if (absAWA < DOWNWIND_MIN_AWA) {
    return REACHING;
  } else {
    return DOWNWIND;
  }
}

/*
 * Determine initial tack state based on AWA sign
 */
TackState determineInitialState(float awa) {
  return (awa >= 0) ? NORMAL_STARBOARD : NORMAL_PORT;
}

/*
 * Get effective heading - prefer RTK or BNO heading if available, otherwise use COG
 note that if a sensor comes online or goes offline, this might cause errors
 */
float getEffectiveHeading() {
  if (pBD->RTKheading > -1.0) {
    return pBD->RTKheading;
  } else if (pBD->BNOheading > -1.0) {
    return pBD->BNOheading;
  } else if (pBD->magHeading > -1.0) {
    return pBD->magHeading;
  } else {
    return pBD->COG;
  }
}

/*
 * Normalize angle to 0-360 degrees
 */
float normalizeAngle(float angle) {
  while (angle < 0) angle += 360.0;
  while (angle >= 360.0) angle -= 360.0;
  return angle;
}

/*
 * Calculate the smallest angle difference between two headings
 */
float angleDifference(float angle1, float angle2) {
  float diff = angle1 - angle2;
  while (diff > 180.0) diff -= 360.0;
  while (diff < -180.0) diff += 360.0;
  return diff;
}

/*
 * Check if we have valid data for maneuver detection
 */
bool isValidManeuverData() {
  return (pBD->AWS > 0.1 &&  // Minimum wind speed
          abs(pBD->AWA) < 180.0 &&  // Valid AWA range
          (pBD->RTKheading > -1.0 || pBD->BNOheading > -1.0 || pBD->magHeading > -1.0 || pBD->COG >= 0.0)); // Valid heading source
}

/*
 * Update baseline AWA values for each tack
 */
void updateBaselines(float awa, TackState state) {
  const float BASELINE_ALPHA = 0.95; // Exponential filter coefficient
  
  if (state == NORMAL_PORT && awa < 0) {
    if (portBaselineAWA == 0.0) {
      portBaselineAWA = awa;
    } else {
      portBaselineAWA = BASELINE_ALPHA * portBaselineAWA + (1.0 - BASELINE_ALPHA) * awa;
    }
  } else if (state == NORMAL_STARBOARD && awa > 0) {
    if (starboardBaselineAWA == 0.0) {
      starboardBaselineAWA = awa;
    } else {
      starboardBaselineAWA = BASELINE_ALPHA * starboardBaselineAWA + (1.0 - BASELINE_ALPHA) * awa;
    }
  }
}

/*
 * Process normal sailing state - look for maneuver candidates
 */
void processNormalState(float awa, float heading, float awaRate, float headingRate) {
  // now is already available as global variable
  
  // Prevent rapid maneuver detection
  if (now - lastManeuverTime < MIN_MANEUVER_TIME) {
    return;
  }
  
  float absAWA = abs(awa);
  float effectiveMinAWA = UPWIND_MIN_AWA;
  
  // Adjust thresholds based on sailing mode
  switch (sailingMode) {
    case UPWIND:
      effectiveMinAWA = UPWIND_MIN_AWA;
      break;
    case REACHING:
      effectiveMinAWA = REACHING_MIN_AWA;
      break;
    case DOWNWIND:
      effectiveMinAWA = DOWNWIND_MIN_AWA;
      break;
  }
  
  bool enteringTackZone = false;
  bool significantRates = (abs(awaRate) > MIN_AWA_RATE || abs(headingRate) > MIN_HEADING_RATE);
  
  // Check for entry into tack/jibe zone
  if (sailingMode == UPWIND || sailingMode == REACHING) {
    // Tack detection for upwind/reaching
    enteringTackZone = (absAWA < effectiveMinAWA) && significantRates;
  } else {
    // Jibe detection for downwind - look for AWA approaching 180°
    enteringTackZone = (absAWA > (180.0 - effectiveMinAWA)) && significantRates;
  }
  
  if (enteringTackZone) {
    // Enter candidate state
    if (sailingMode == DOWNWIND) {
      currentState = JIBE_CANDIDATE;
    } else {
      currentState = TACK_CANDIDATE;
    }
    
    candidateStartTime = now;
    candidateStartHeading = heading;
    candidateStartAWA = awa;
    stateChangeTime = now;
    
    log::toAll("Maneuver candidate detected: " + 
               String(sailingMode == DOWNWIND ? "JIBE" : "TACK") + 
               " AWA=" + String(awa, 1) + "° Heading=" + String(heading, 1) + "°");
  }
}

/*
 * Process candidate state - confirm or reject maneuver
 */
void processCandidateState(float awa, float heading) {
  // now is already available as global variable
  
  // Check for timeout
  if (now - candidateStartTime > CANDIDATE_TIMEOUT) {
    // Timeout - return to normal state
    currentState = determineInitialState(awa);
    stateChangeTime = now;
    falsePositiveCount++;
    log::toAll("Maneuver candidate timed out, returning to normal");
    return;
  }
  
  float headingChange = abs(angleDifference(heading, candidateStartHeading));
  bool awaSignChanged = ((candidateStartAWA >= 0) != (awa >= 0));
  float absAWA = abs(awa);
  
  // Determine confirmation criteria based on sailing mode
  bool confirmed = false;
  ManeuverType maneuverType = NO_MANEUVER;
  
  if (currentState == TACK_CANDIDATE) {
    // Tack confirmation criteria
    confirmed = awaSignChanged && 
                (headingChange > MIN_HEADING_CHANGE) && 
                (headingChange < MAX_HEADING_CHANGE) &&
                (absAWA > UPWIND_MIN_AWA + HYSTERESIS_MARGIN);
    
    if (confirmed) {
      maneuverType = (candidateStartAWA < 0) ? TACK_PORT_TO_STARBOARD : TACK_STARBOARD_TO_PORT;
    }
  } else if (currentState == JIBE_CANDIDATE) {
    // Jibe confirmation criteria
    confirmed = awaSignChanged && 
                (headingChange > MIN_HEADING_CHANGE) && 
                (headingChange < MAX_HEADING_CHANGE) &&
                (absAWA > DOWNWIND_MIN_AWA - HYSTERESIS_MARGIN);
    
    if (confirmed) {
      maneuverType = (candidateStartAWA < 0) ? JIBE_PORT_TO_STARBOARD : JIBE_STARBOARD_TO_PORT;
    }
  }
  
  if (confirmed) {
    // Maneuver confirmed
    currentState = (currentState == TACK_CANDIDATE) ? TACK_CONFIRMED : JIBE_CONFIRMED;
    stateChangeTime = now;
    lastManeuver = maneuverType;
    lastManeuverTime = now;
    
    if (currentState == TACK_CONFIRMED) {
      tackCount++;
    } else {
      jibeCount++;
    }
    
    logManeuver(maneuverType, candidateStartHeading, heading, candidateStartAWA, awa);
  }
}

/*
 * Process confirmed state - brief hold before returning to normal
 */
void processConfirmedState() {
  // now is already available as global variable
  
  if (now - stateChangeTime > CONFIRMED_TIMEOUT) {
    // Return to normal state
    float currentAWA = awaFilter.reading(0) / 10.0; // Get current filtered AWA
    currentState = determineInitialState(currentAWA);
    stateChangeTime = now;
  }
}

/*
 * Log completed maneuver with details
 */
void logManeuver(ManeuverType maneuver, float startHeading, float endHeading, float startAWA, float endAWA) {
  String maneuverName;
  switch (maneuver) {
    case TACK_PORT_TO_STARBOARD:
      maneuverName = "TACK Port→Starboard";
      break;
    case TACK_STARBOARD_TO_PORT:
      maneuverName = "TACK Starboard→Port";
      break;
    case JIBE_PORT_TO_STARBOARD:
      maneuverName = "JIBE Port→Starboard";
      break;
    case JIBE_STARBOARD_TO_PORT:
      maneuverName = "JIBE Starboard→Port";
      break;
    default:
      maneuverName = "UNKNOWN";
      break;
  }
  
  float headingChange = angleDifference(endHeading, startHeading);
  
  log::toAll("MANEUVER CONFIRMED: " + maneuverName + 
             " | Heading: " + String(startHeading, 1) + "°→" + String(endHeading, 1) + 
             "° (Δ" + String(headingChange, 1) + "°)" +
             " | AWA: " + String(startAWA, 1) + "°→" + String(endAWA, 1) + "°" +
             " | Mode: " + String(sailingMode == UPWIND ? "UPWIND" : 
                                 sailingMode == REACHING ? "REACHING" : "DOWNWIND"));
}

/*
 * Get current tack detection statistics
 */
String getTackStats() {
  String stats = "Tacks: " + String(tackCount) + 
                 ", Jibes: " + String(jibeCount) + 
                 ", False+: " + String(falsePositiveCount) +
                 ", State: ";
  
  switch (currentState) {
    case NORMAL_PORT: stats += "Normal-Port"; break;
    case NORMAL_STARBOARD: stats += "Normal-Starboard"; break;
    case TACK_CANDIDATE: stats += "Tack-Candidate"; break;
    case TACK_CONFIRMED: stats += "Tack-Confirmed"; break;
    case JIBE_CANDIDATE: stats += "Jibe-Candidate"; break;
    case JIBE_CONFIRMED: stats += "Jibe-Confirmed"; break;
    case UNKNOWN: stats += "Unknown"; break;
  }
  
  stats += ", Mode: ";
  switch (sailingMode) {
    case UPWIND: stats += "Upwind"; break;
    case REACHING: stats += "Reaching"; break;
    case DOWNWIND: stats += "Downwind"; break;
  }
  
  return stats;
}

/*
 * Reset tack detection statistics
 */
void resetTackStats() {
  tackCount = 0;
  jibeCount = 0;
  falsePositiveCount = 0;
  log::toAll("Tack detection statistics reset");
}

/*
 * Get last detected maneuver
 */
ManeuverType getLastManeuver() {
  return lastManeuver;
}

/*
 * Check if currently in a maneuver
 */
bool isInManeuver() {
  return (currentState == TACK_CANDIDATE || currentState == TACK_CONFIRMED ||
          currentState == JIBE_CANDIDATE || currentState == JIBE_CONFIRMED);
}
#endif