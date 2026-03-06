const compassCircle = document.querySelector(".compass-circle");
const startBtn = document.querySelector(".start-btn");

window.addEventListener('load', getReadings);

function init() {
  window.addEventListener("deviceorientationabsolute", handler, true);
}

function handler(e) {
  compass = e.webkitCompassHeading || Math.abs(e.alpha - 360);
  updateCompass(compass);
}

// Function to update compass display
function updateCompass(bearing) {
  if (compassCircle) {
    // Rotate the compass card in the opposite direction of the bearing
    compassCircle.style.transform = `translate(-50%, -50%) rotate(${-bearing}deg)`;
  }
}

// Function to toggle between magnetic and true heading
function toggleHeadingType(checkbox) {
  // Store preference in localStorage
  localStorage.setItem('usetrueHeading', checkbox.checked);
  
  // Update compass with current data if available
  if (window.AppState && window.AppState.lastData) {
    updateCompassFromData(window.AppState.lastData);
  }
}

// Function to update compass based on selected heading type
function updateCompassFromData(data) {
  const useTrue = document.getElementById('headingToggle').checked;
  let selectedHeading;
  
  if (useTrue) {
    // Prefer RTK heading, fallback to BNO heading
    if (data.RTKheading !== undefined && data.RTKheading >= 0) {
      selectedHeading = parseFloat(data.RTKheading);
    } else if (data.BNOheading !== undefined && data.BNOheading >= 0) {
      selectedHeading = parseFloat(data.BNOheading);
    } else {
      selectedHeading = 0;
    }
  } else if (data.heading !== undefined) {
    selectedHeading = parseFloat(data.heading);
  } else {
    selectedHeading = 0;
  }
  
  updateCompass(selectedHeading);
}

// Initialize heading type preference on page load
function initHeadingPreference() {
  const headingToggle = document.getElementById('headingToggle');
  if (headingToggle) {
    const savedPreference = localStorage.getItem('usetrueHeading');
    headingToggle.checked = savedPreference === 'true';
  }
}

// Function to update calibration status
function updateCalibrationStatus(calstatus) {
  const imageElement = document.getElementById("calibration");
  if (imageElement) {
    switch (calstatus) {
      case 0:
        imageElement.src = "red.png";
        break;
      case 1:
        imageElement.src = "orange.png";
        break;
      case 2:
        imageElement.src = "yellow.png";
        break;
      case 3:
        imageElement.src = "green.png";
        break;
      default:
        imageElement.src = "favicon.ico";
        break;
    }
  } else {
    console.log("no calibration image");
  }
}

// Function to get current readings (kept for compatibility)
function getReadings() {
  // This is now handled by the unified app, but kept for backward compatibility
  console.log("getReadings called - now handled by unified app");
}

// Subscribe to unified app data updates
if (typeof subscribe === 'function') {
  subscribe('compass-script', function(type, data) {
    if (type === 'data' || type === 'initial') {
      console.log('Compass script received data:', data);
      
      // Update individual heading displays
      const magneticElement = document.getElementById('bearing');
      if (magneticElement && data.heading !== undefined) {
        magneticElement.innerHTML = parseFloat(data.heading).toFixed(1);
      }
      
      const bnoElement = document.getElementById('bnoHeading');
      if (bnoElement && data.BNOheading !== undefined) {
        bnoElement.innerHTML = data.BNOheading >= 0 ? parseFloat(data.BNOheading).toFixed(1) : '--';
      }
      
      const rtkElement = document.getElementById('rtkHeading');
      if (rtkElement && data.RTKheading !== undefined) {
        rtkElement.innerHTML = data.RTKheading >= 0 ? parseFloat(data.RTKheading).toFixed(1) : '--';
      }
      
      // Update compass display based on selected heading type
      updateCompassFromData(data);
      
      // Handle calibration status
      if (data.calstatus !== undefined) {
        updateCalibrationStatus(data.calstatus);
      }
    }
  });
  
  // Initialize heading preference when page loads
  document.addEventListener('DOMContentLoaded', initHeadingPreference);
  if (document.readyState !== 'loading') {
    initHeadingPreference();
  }
} else {
  console.error('Unified app not loaded - compass script requires app.js');
}
