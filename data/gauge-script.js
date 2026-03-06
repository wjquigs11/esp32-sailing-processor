// Using Canvas Gauges library with proper configuration
// Documentation: https://canvas-gauges.com/documentation/user-guide/configuration

var lastTime;
var mastGauge;
var windCompassGauge;

function isSmallScreen() {
  // adjust breakpoint to taste (e.g. 768, 900, etc.)
  var isSmall = window.matchMedia('(max-width: 768px)').matches;
  console.log('Screen check - Width:', window.innerWidth, 'Height:', window.innerHeight, 'IsSmallScreen:', isSmall);
  return isSmall;
}

function getGaugeSize() {
  var screenWidth = window.innerWidth;
  if (isSmallScreen()) {
    // Make gauges smaller on small screens - use 80% of screen width but cap at reasonable size
    var maxSize = Math.min(screenWidth * 0.8, 250);
    var finalSize = Math.max(maxSize, 200); // Minimum size of 200px
    console.log('Small screen detected - Screen width:', screenWidth, 'Calculated size:', maxSize, 'Final gauge size:', finalSize);
    return finalSize;
  }
  console.log('Large screen detected - Screen width:', screenWidth, 'Using default gauge size: 300');
  return 300; // Default size for larger screens
}

// Initialize gauges when the page loads
window.addEventListener('load', function() {
  // Initialize gauges
  initializeMastAngleGauge();
  initializeWindCompassGauge();
  
  // Note: Data will come via Server-Sent Events only
});

// optional: update on resize/orientation change
window.addEventListener('resize', () => {
  console.log('Window resized - reinitializing gauges');
  // Re-initialize gauges with new size
  initializeMastAngleGauge();
  initializeWindCompassGauge();
});

// Function to initialize the mast angle gauge (100-degree sweep)
function initializeMastAngleGauge() {
  var gaugeSize = getGaugeSize();
  var opts = {
    renderTo: 'mast-angle-gauge',
    width: gaugeSize,
    height: gaugeSize,
    fontTitleWeight: "bold",
    title: 'Mast Angle',
    minValue: -50,
    startAngle: 270,
    ticksAngle: 180,
    valueBox: false,
    maxValue: 50,
    majorTicks: [
      "50", "40", "30", "20", "10", "0", "-10", "-20", "-30", "-40", "-50" ],
    highlights: [
      { from: -50, to: 50, color: 'rgba(216, 211, 211, 0.53)' },
    ],
    minorTicks: 2,
    strokeTicks: true,
    colorPlate: "#fff",
    borderShadowWidth: 0,
    borders: false,
    needleType: "arrow",
    needleWidth: 2,
    needleCircleSize: 7,
    needleCircleOuter: true,
    needleCircleInner: false,
    animationDuration: 100,
    animationRule: "linear"
  };

  try {
    mastGauge = new RadialGauge(opts);
    mastGauge.draw();
    console.log('Mast angle gauge initialized successfully');
  } catch (error) {
    console.error('Failed to initialize mast angle gauge:', error);
    mastGauge = null;
  }
}

// Function to initialize the wind compass gauge (360-degree full circle)
function initializeWindCompassGauge() {
  var gaugeSize = getGaugeSize();
  var opts = {
    renderTo: 'wind-compass-gauge',
    width: gaugeSize,
    height: gaugeSize,
    fontTitleWeight: "bold",
    title: 'AWA',
    minValue: 0,
    maxValue: 360,
    majorTicks: ['0', '45', '90', '135', '180', '225', '270', '285', ''],
    minorTicks: 22,
    ticksAngle: 360,
    startAngle: 180,
    strokeTicks: false,
    highlights: false,
    colorPlate: '#fff',
    colorMajorTicks: '#333',
    colorMinorTicks: '#666',
    colorNumbers: '#333',
    borderShadowWidth: 0,
    borders: false,
    needleType: 'arrow',
    needleWidth: 2,
    needleCircleSize: 7,
    needleCircleOuter: true,
    needleCircleInner: false,
    animationDuration: 100,
    animationRule: 'linear',
    valueBox: false
  };

  try {
    windCompassGauge = new RadialGauge(opts);
    windCompassGauge.draw();
    console.log('Wind compass gauge initialized successfully');
  } catch (error) {
    console.error('Failed to initialize wind compass gauge:', error);
    windCompassGauge = null;
  }
}

// Function to convert compass AWA to bow-relative AWA
function convertToBowRelative(compassAwa) {
  // Convert compass bearing to degrees from bow
  // 285° compass = 45° from bow, 270° compass = 90° from bow, etc.
  let bowRelative = Math.abs(((compassAwa + 540) % 360) - 180);
  return bowRelative;
}

// Function to update wind compass gauge
function updateWindCompassGauge(direction, speed, stw = 0) {
  if (windCompassGauge) {
    windCompassGauge.options.highlights = [
      { from: 0, to: direction, color: 'rgba(37, 42, 47, 0.3)' }
    ];
    windCompassGauge.value = direction;
    windCompassGauge.update();
  }
  
  // Calculate bow-relative angle
  const bowRelative = convertToBowRelative(direction);
  
  document.getElementById('wind-info').innerHTML =
    'AWA: ' + direction.toFixed(0) + '°/' + bowRelative.toFixed(0) + '°<br>AWS: ' + speed.toFixed(1);
  
  document.getElementById('stw-info').innerHTML = 'STW: ' + stw.toFixed(1);

  // Calculate VMG using calculations.js
  if (typeof calcTrueWindAngle === 'function') {
    // Convert AWS from knots to m/s for calculation
    const awsMs = knotsToMps(speed);
    const stwMs = knotsToMps(stw);
    
    // Calculate true wind parameters including VMG
    const windData = calcTrueWindAngle(awsMs, direction, stwMs);
    
    // Convert VMG back to knots for display
    const vmgKnots = mpsToKnots(windData.VMG);
    
    // Update VMG display
    document.getElementById('vmg-info').innerHTML =
      'VMG: ' + vmgKnots.toFixed(1) + ' kts';
  } else {
    // Fallback if calculations.js is not loaded
    document.getElementById('vmg-info').innerHTML = 'VMG: --';
  }
}
// Function to update mast angle gauge
function updateMastGauge(angle) {
  if (mastGauge) {
    mastGauge.value = -angle;
  }
  document.getElementById('mast-angle-value').innerHTML = angle.toFixed(1) + '&deg;';
}


// Subscribe to unified app data updates
if (typeof subscribe === 'function') {
  subscribe('gauge-script', function(type, data) {
    if (type === 'data' || type === 'initial') {
      console.log('Gauge script received data:', data);
      
      var timeDelta = data.timestamp - lastTime;
      lastTime = data.timestamp;
      console.log('Time delta:', timeDelta/1000);
      
      // Handle mast angle (using standardized field name)
      if (data.mastAngle !== undefined) {
        var angle = parseFloat(data.mastAngle);
        updateMastGauge(angle);
      }
      
      // Handle wind data (using standardized field names)
      var windDir = data.awa !== undefined ? parseFloat(data.awa) : 0;
      var windSpeed = data.aws !== undefined ? parseFloat(data.aws) : 0;
      var boatSpeed = data.stw !== undefined ? parseFloat(data.stw) : 0;
      updateWindCompassGauge(windDir, windSpeed, boatSpeed);
    }
  });
} else {
  console.error('Unified app not loaded - gauge script requires app.js');
}
