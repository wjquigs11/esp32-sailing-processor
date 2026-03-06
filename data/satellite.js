const size = 340;
const radius = size / 2 - 20;

// color scale by SNR (dB)
function snrColor(snr) {
  if (snr >= 40) return "green";
  if (snr >= 30) return "#003f8c";  // dark blue
  if (snr > 0)  return "#b00000";   // red
  return "black";
}

// convert azimuth/elevation to x,y
function project(azimuthDeg, elevationDeg) {
  const r = radius * (1 - elevationDeg / 90);      // 0 at horizon, center at 90°
  const rad = (azimuthDeg - 90) * Math.PI / 180;   // 0° at N, clockwise
  return {
    x: Math.cos(rad) * r,
    y: Math.sin(rad) * r
  };
}

// render satellites array: [{id, az, el, snr}, ...]
function renderSatellites(sats) {
  // Ensure chart is initialized before rendering
  if (typeof window.satLayer === 'undefined' || !window.satLayer) {
    console.log('Chart not initialized yet, initializing...');
    initSatelliteChart();
    if (typeof window.satLayer === 'undefined' || !window.satLayer) {
      console.error('Failed to initialize satellite chart');
      return;
    }
  }

  // Filter out satellites with invalid positions
  const validSats = sats.filter(d =>
    d.az >= 0 && d.az <= 360 &&
    d.el >= 0 && d.el <= 90 &&
    d.id > 0
  );

  const nodes = window.satLayer.selectAll("g.sat")
    .data(validSats, d => d.id);

  const enter = nodes.enter()
    .append("g")
    .attr("class", "sat");

  enter.append("circle")
    .attr("r", 8)
    .attr("stroke", "#333")
    .attr("stroke-width", 1);

  enter.append("text")
    .attr("class", "sat-label")
    .attr("dy", "0.35em")
    .text(d => d.id);

  const all = enter.merge(nodes);

  all.each(function (d) {
    const p = project(d.az, d.el);
    d3.select(this)
      .attr("transform", `translate(${p.x},${p.y})`);
  });

  all.select("circle")
    .attr("fill", d => snrColor(d.snr))
    .attr("r", d => d.snr > 0 ? 8 : 6); // Smaller circles for no signal

  all.select("text")
    .text(d => d.id)
    .attr("fill", d => d.snr > 0 ? "white" : "#666");

  nodes.exit().remove();
}

// Initialize the chart when called
function initSatelliteChart() {
  // Clear any existing content
  d3.select("#satellite-chart").selectAll("*").remove();
  
  // Recreate the chart
  const svg = d3.select("#satellite-chart")
    .append("svg")
    .attr("viewBox", `0 0 ${size} ${size}`);

  const g = svg.append("g")
    .attr("transform", `translate(${size / 2},${size / 2})`);

  // circles for elevation (e.g. 30°, 60°, 90°)
  [30, 60, 90].forEach(elev => {
    g.append("circle")
      .attr("r", radius * (1 - elev / 90))
      .attr("fill", "none")
      .attr("stroke", "#ddd")
      .attr("stroke-width", 1);
  });

  // N/E/S/W labels
  const dirs = [
    {label: "N", ang: 0},
    {label: "E", ang: 90},
    {label: "S", ang: 180},
    {label: "W", ang: 270}
  ];
  dirs.forEach(d => {
    const rad = (d.ang - 90) * Math.PI / 180;
    const x = Math.cos(rad) * (radius + 10);
    const y = Math.sin(rad) * (radius + 10);
    svg.append("text")
      .attr("x", size / 2 + x)
      .attr("y", size / 2 + y + 4)
      .attr("text-anchor", "middle")
      .attr("font-size", "14px")
      .attr("font-weight", "bold")
      .attr("fill", "#333")
      .text(d.label);
  });

  // group where satellites will be drawn
  window.satLayer = g.append("g").attr("class", "sat-layer");
}

// Call initialization
if (typeof window !== 'undefined') {
  // Initialize when DOM is ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initSatelliteChart);
  } else {
    initSatelliteChart();
  }
}
