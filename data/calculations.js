/**
 * True Wind Calculations for Marine Navigation
 * Converted from C++ implementation in calculations.cpp
 * 
 * Inputs received via SSE:
 * - aws: Apparent Wind Speed (m/s)
 * - awa: Apparent Wind Angle (degrees)
 * - stw: Speed Through Water (m/s)
 * 
 * Optional inputs:
 * - trueHeading: Boat heading in degrees (for TWD calculation)
 */

// Constants
const DEGTORAD = Math.PI / 180.0;
const RADTODEG = 180.0 / Math.PI;

// Global variables to track maximum values
let maxTWS = 0.0;

/**
 * Calculate True Wind Angle and related wind parameters
 * @param {number} aws - Apparent Wind Speed in m/s
 * @param {number} awa - Apparent Wind Angle in degrees
 * @param {number} stw - Speed Through Water in m/s
 * @param {number} trueHeading - Optional: Boat heading in degrees (for TWD calculation)
 * @returns {object} Object containing calculated wind parameters
 */
function calcTrueWindAngle(aws, awa, stw, trueHeading = null) {
  // Initialize result object
  const result = {
    TWS: 0.0,    // True Wind Speed (m/s)
    TWA: 0.0,    // True Wind Angle (degrees)
    VMG: 0.0,    // Velocity Made Good (m/s)
    TWD: null,   // True Wind Direction (degrees, only if trueHeading provided)
    maxTWS: maxTWS
  };
  
  // Handle edge cases
  if (aws < 0.01) {  // Very low wind speed
    // With no apparent wind, true wind is opposite to boat motion
    result.TWS = stw;
    result.TWA = 180.0;
    if (result.TWS > maxTWS) {
      maxTWS = result.TWS;
      result.maxTWS = maxTWS;
    }
    // Calculate VMG - in this case, it's negative STW (sailing directly away from wind)
    result.VMG = -stw;
    
    // Calculate TWD if heading is provided
    if (trueHeading !== null) {
      result.TWD = result.TWA + trueHeading;
      // Normalize TWD to 0-360 degrees range
      if (result.TWD < 0) {
        result.TWD += 360;
      } else if (result.TWD >= 360) {
        result.TWD -= 360;
      }
    }
    
    return result;
  }
  
  if (stw < 0.01) {  // Very low boat speed
    // With no boat speed, apparent wind equals true wind
    result.TWS = aws;
    result.TWA = awa;
    if (result.TWS > maxTWS) {
      maxTWS = result.TWS;
      result.maxTWS = maxTWS;
    }
    // Normalize the angle
    if (result.TWA < 0) {
      result.TWA += 360;
    } else if (result.TWA >= 360) {
      result.TWA -= 360;
    }
    // With no boat speed, VMG is zero
    result.VMG = 0.0;
    
    // Calculate TWD if heading is provided
    if (trueHeading !== null) {
      result.TWD = result.TWA + trueHeading;
      // Normalize TWD to 0-360 degrees range
      if (result.TWD < 0) {
        result.TWD += 360;
      } else if (result.TWD >= 360) {
        result.TWD -= 360;
      }
    }
    
    return result;
  }
  
  // Convert AWA from degrees to radians for trigonometric calculations
  const AWA_rad = awa * DEGTORAD;
  
  // Calculate apparent wind components in boat-relative frame
  const aw_x = aws * Math.sin(AWA_rad);  // positive to starboard
  const aw_y = aws * Math.cos(AWA_rad);  // positive forward
  
  // Calculate true wind components by adding boat velocity
  const tw_x = aw_x;                     // no change in x-component
  const tw_y = aw_y - stw;               // subtract boat speed from y-component
  
  // Calculate true wind speed
  result.TWS = Math.sqrt(tw_x * tw_x + tw_y * tw_y);
  if (result.TWS > maxTWS || (maxTWS < 0.001 && result.TWS > 0.001)) {
    maxTWS = result.TWS;
    result.maxTWS = maxTWS;
  }
  
  // Calculate true wind angle
  const wind_angle_rad = Math.atan2(tw_x, tw_y);
  
  // Convert TWA from radians to degrees
  result.TWA = wind_angle_rad * RADTODEG;
  
  // Normalize TWA to 0-360 degrees range
  if (result.TWA < 0) {
    result.TWA += 360;
  } else if (result.TWA >= 360) {
    result.TWA -= 360;
  }

  // Calculate VMG (Velocity Made Good) to wind
  // VMG should be positive when sailing upwind (AWA < 90°) and negative when sailing downwind (AWA > 90°)
  // Use AWA directly for VMG calculation, not TWA
  let vmg_angle = awa;
  
  // Normalize AWA to -180 to +180 range for proper VMG calculation
  if (vmg_angle > 180.0) {
    vmg_angle -= 360.0;
  }
  
  // VMG = STW * cos(AWA)
  // Positive VMG means making progress toward the wind (upwind sailing)
  // Negative VMG means moving away from the wind (downwind sailing)
  result.VMG = stw * Math.cos(vmg_angle * DEGTORAD);
  
  // Calculate TWD if we have compass heading (true)
  if (trueHeading !== null) {
    result.TWD = result.TWA + trueHeading;
    
    // Normalize TWD to 0-360 degrees range
    if (result.TWD < 0) {
      result.TWD += 360;
    } else if (result.TWD >= 360) {
      result.TWD -= 360;
    }
  }
  
  return result;
}

/**
 * Reset the maximum TWS tracker
 */
function resetMaxTWS() {
  maxTWS = 0.0;
}

/**
 * Get the current maximum TWS
 * @returns {number} Maximum True Wind Speed recorded
 */
function getMaxTWS() {
  return maxTWS;
}

/**
 * Convert meters per second to knots
 * @param {number} mps - Speed in meters per second
 * @returns {number} Speed in knots
 */
function mpsToKnots(mps) {
  return mps * 1.9438444924;
}

/**
 * Convert knots to meters per second
 * @param {number} knots - Speed in knots
 * @returns {number} Speed in meters per second
 */
function knotsToMps(knots) {
  return knots * 0.5144444444;
}

/**
 * Enhanced Wind Calculator Class for comprehensive wind data processing
 * Handles SSE data streams, history tracking, and ESP32 communication
 */
class WindCalculator {
  constructor(options = {}) {
    this.history = [];
    this.maxHistoryLength = options.maxHistoryLength || 1000;
    this.maxValues = { TWS: 0, VMG: 0, AWS: 0 };
    this.esp32PostEnabled = options.esp32PostEnabled || false;
    this.esp32PostInterval = options.esp32PostInterval || 5000; // 5 seconds
    this.lastESP32Post = 0;
    
    // Performance tracking
    this.stats = {
      calculationsPerSecond: 0,
      lastSecondCount: 0,
      lastSecondTime: Date.now()
    };
  }
  
  /**
   * Process incoming SSE data and calculate wind parameters
   * @param {object} rawData - Raw sensor data from ESP32
   * @returns {object} Calculated wind data with additional metadata
   */
  processSSEData(rawData) {
    const startTime = performance.now();
    
    // Extract raw sensor values
    const { aws, awa, stw, trueHeading, timestamp } = rawData;
    
    // Calculate true wind parameters using existing function
    const calculated = calcTrueWindAngle(aws, awa, stw, trueHeading);
    
    // Add metadata
    calculated.timestamp = timestamp || Date.now();
    calculated.processingTime = performance.now() - startTime;
    calculated.aws = aws; // Keep original apparent wind data
    calculated.awa = awa;
    calculated.stw = stw;
    calculated.trueHeading = trueHeading;
    
    // Update performance stats
    this.updatePerformanceStats();
    
    // Update max values tracking
    this.updateMaxValues(calculated);
    
    // Store in history
    this.addToHistory(calculated);
    
    // Convert to knots for display
    const displayData = this.addKnotsConversion(calculated);
    
    // Send to ESP32 if enabled and interval elapsed
    if (this.esp32PostEnabled && this.shouldPostToESP32()) {
      this.sendToESP32(calculated);
    }
    
    return displayData;
  }
  
  /**
   * Add knots conversion for display purposes
   */
  addKnotsConversion(data) {
    return {
      ...data,
      TWS_knots: mpsToKnots(data.TWS),
      VMG_knots: mpsToKnots(data.VMG),
      AWS_knots: mpsToKnots(data.aws),
      STW_knots: mpsToKnots(data.stw),
      maxTWS_knots: mpsToKnots(data.maxTWS)
    };
  }
  
  /**
   * Update performance statistics
   */
  updatePerformanceStats() {
    const now = Date.now();
    this.stats.lastSecondCount++;
    
    if (now - this.stats.lastSecondTime >= 1000) {
      this.stats.calculationsPerSecond = this.stats.lastSecondCount;
      this.stats.lastSecondCount = 0;
      this.stats.lastSecondTime = now;
    }
  }
  
  /**
   * Update maximum values tracking
   */
  updateMaxValues(data) {
    if (data.TWS > this.maxValues.TWS) {
      this.maxValues.TWS = data.TWS;
    }
    if (Math.abs(data.VMG) > Math.abs(this.maxValues.VMG)) {
      this.maxValues.VMG = data.VMG;
    }
    if (data.aws > this.maxValues.AWS) {
      this.maxValues.AWS = data.aws;
    }
  }
  
  /**
   * Add data to history with size management
   */
  addToHistory(data) {
    this.history.push(data);
    
    // Trim history if it exceeds max length
    if (this.history.length > this.maxHistoryLength) {
      this.history = this.history.slice(-this.maxHistoryLength);
    }
  }
  
  /**
   * Check if we should post data to ESP32
   */
  shouldPostToESP32() {
    const now = Date.now();
    if (now - this.lastESP32Post >= this.esp32PostInterval) {
      this.lastESP32Post = now;
      return true;
    }
    return false;
  }
  
  /**
   * Send calculated data back to ESP32 for logging/storage
   */
  async sendToESP32(data) {
    try {
      const response = await fetch('/api/wind-data', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Accept': 'application/json'
        },
        body: JSON.stringify({
          TWS: data.TWS,
          TWA: data.TWA,
          VMG: data.VMG,
          TWD: data.TWD,
          timestamp: data.timestamp,
          maxTWS: data.maxTWS
        })
      });
      
      if (!response.ok) {
        console.warn('Failed to send data to ESP32:', response.status);
      }
    } catch (error) {
      console.warn('Error sending data to ESP32:', error);
    }
  }
  
  /**
   * Get recent history for analysis
   */
  getRecentHistory(seconds = 60) {
    const cutoff = Date.now() - (seconds * 1000);
    return this.history.filter(item => item.timestamp >= cutoff);
  }
  
  /**
   * Get performance statistics
   */
  getPerformanceStats() {
    return {
      ...this.stats,
      historyLength: this.history.length,
      maxValues: { ...this.maxValues }
    };
  }
  
  /**
   * Reset all tracking data
   */
  reset() {
    this.history = [];
    this.maxValues = { TWS: 0, VMG: 0, AWS: 0 };
    resetMaxTWS();
    this.stats.calculationsPerSecond = 0;
    this.stats.lastSecondCount = 0;
  }
  
  /**
   * Enable/disable ESP32 posting
   */
  setESP32Posting(enabled, interval = 5000) {
    this.esp32PostEnabled = enabled;
    this.esp32PostInterval = interval;
  }
}

// Export functions for use in other modules
if (typeof module !== 'undefined' && module.exports) {
  // Node.js environment
  module.exports = {
    calcTrueWindAngle,
    resetMaxTWS,
    getMaxTWS,
    mpsToKnots,
    knotsToMps,
    WindCalculator
  };
}

// Example usage for SSE integration:
/*
// Initialize wind calculator
const windCalc = new WindCalculator({
  maxHistoryLength: 500,
  esp32PostEnabled: true,
  esp32PostInterval: 10000 // Post every 10 seconds
});

// Set up SSE connection
const eventSource = new EventSource('/api/sensor-stream');
eventSource.onmessage = function(event) {
  const rawData = JSON.parse(event.data);
  const windData = windCalc.processSSEData(rawData);
  
  // Update UI with calculated data
  updateWindGauges(windData);
  updatePerformanceDisplay(windCalc.getPerformanceStats());
};

// Example wind gauge update function
function updateWindGauges(data) {
  // Update TWS gauge
  document.getElementById('tws-value').textContent = data.TWS_knots.toFixed(1);
  
  // Update TWA gauge
  document.getElementById('twa-value').textContent = data.TWA.toFixed(0);
  
  // Update VMG display
  document.getElementById('vmg-value').textContent = data.VMG_knots.toFixed(1);
  document.getElementById('vmg-value').className = data.VMG >= 0 ? 'positive' : 'negative';
  
  // Update TWD compass
  document.getElementById('twd-arrow').style.transform = `rotate(${data.TWD}deg)`;
}
*/