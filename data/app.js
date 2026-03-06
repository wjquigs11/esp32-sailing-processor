/**
 * Unified Application JavaScript
 * Handles shared SSE connection, menu functionality, and data distribution
 * Eliminates redundancy across multiple page-specific scripts
 */

// Global application state
window.AppState = {
  eventSource: null,
  isConnected: false,
  lastData: {},
  subscribers: new Map(),
  menuInitialized: false
};

/**
 * Initialize the unified application
 */
function initApp() {
  console.log('Initializing unified app...');
  
  // Initialize SSE connection
  initEventSource();
  
  // Initialize menu functionality if hamburger menu exists
  initMenuFunctionality();
  
  // Initial data fetch
  getInitialReadings();
}

/**
 * Initialize Server-Sent Events connection
 */
function initEventSource() {
  if (!window.EventSource) {
    console.error('EventSource not supported');
    return;
  }

  // Close existing connection if any
  if (window.AppState.eventSource) {
    window.AppState.eventSource.close();
  }

  window.AppState.eventSource = new EventSource('/events');
  
  window.AppState.eventSource.addEventListener('open', function(e) {
    console.log('SSE Connected');
    window.AppState.isConnected = true;
    notifySubscribers('connection', { connected: true });
  }, false);

  window.AppState.eventSource.addEventListener('error', function(e) {
    if (e.target.readyState != EventSource.OPEN) {
      console.log('SSE Disconnected');
      window.AppState.isConnected = false;
      notifySubscribers('connection', { connected: false });
    }
  }, false);
  
  window.AppState.eventSource.addEventListener('message', function(e) {
    console.log('SSE message:', e.data);
  }, false);
  
  window.AppState.eventSource.addEventListener('new_readings', function(e) {
    console.log('SSE new_readings:', e.data);
    try {
      const data = JSON.parse(e.data);
      
      // Store latest data directly (field names are now standardized at source)
      window.AppState.lastData = data;
      
      // Notify all subscribers
      notifySubscribers('data', data);
      
    } catch (error) {
      console.error('Error parsing SSE data:', error);
    }
  }, false);
}

// Field names are now standardized at the source, no mapping needed

/**
 * Subscribe to data updates
 */
function subscribe(id, callback) {
  if (!window.AppState.subscribers.has(id)) {
    window.AppState.subscribers.set(id, []);
  }
  window.AppState.subscribers.get(id).push(callback);
  
  // Send current data if available
  if (Object.keys(window.AppState.lastData).length > 0) {
    callback('data', window.AppState.lastData);
  }
}

/**
 * Unsubscribe from data updates
 */
function unsubscribe(id) {
  window.AppState.subscribers.delete(id);
}

/**
 * Notify all subscribers of data updates
 */
function notifySubscribers(type, data) {
  window.AppState.subscribers.forEach((callbacks, id) => {
    callbacks.forEach(callback => {
      try {
        callback(type, data);
      } catch (error) {
        console.error(`Error in subscriber ${id}:`, error);
      }
    });
  });
}

/**
 * Initialize menu functionality for hamburger menus
 */
function initMenuFunctionality() {
  // Handle both old-style and new-style hamburger menus
  
  // Old-style menu (main-script.js style)
  window.menuFun = function() {
    const links = document.getElementById("myLinks");
    if (links) {
      if (links.style.display === "block") {
        links.style.display = "none";
      } else {
        links.style.display = "block";
      }
    }
  };
  
  // New-style menu (index.html style)
  window.toggleMenu = function() {
    const dropdown = document.getElementById("menuDropdown");
    if (dropdown) {
      dropdown.classList.toggle("show");
    }
  };
  
  // Close dropdown when clicking outside (for new-style menu)
  window.onclick = function(event) {
    if (!event.target.matches('.hamburger-icon') && !event.target.matches('.fa-bars')) {
      const dropdowns = document.getElementsByClassName("menu-dropdown");
      for (let i = 0; i < dropdowns.length; i++) {
        const openDropdown = dropdowns[i];
        if (openDropdown.classList.contains('show')) {
          openDropdown.classList.remove('show');
        }
      }
    }
  };
  
  window.AppState.menuInitialized = true;
  console.log('Menu functionality initialized');
}

/**
 * Get initial readings via XHR
 */
function getInitialReadings() {
  const xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      try {
        const data = JSON.parse(this.responseText);
        window.AppState.lastData = data;
        notifySubscribers('initial', data);
      } catch (error) {
        console.error('Error parsing initial readings:', error);
      }
    }
  };
  xhr.open("GET", "/readings", true);
  xhr.send();
}

/**
 * Utility function for checkbox toggles (used in settings)
 */
function toggleCheckbox(element) {
  const xhr = new XMLHttpRequest();
  if (element.checked) {
    xhr.open("GET", "/params?output=" + element.id + "&state=on", true);
  } else {
    xhr.open("GET", "/params?output=" + element.id + "&state=off", true);
  }
  xhr.send();
}

/**
 * Wind correction toggle functionality (for index.html)
 */
function toggleWindCorrection() {
  const checkbox = document.getElementById('windCorrectionToggle');
  const status = document.getElementById('correctionStatus');
  
  if (!checkbox || !status) return;
  
  // Send POST request to toggle wind correction
  fetch('/api/wind-correction', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({
      enabled: checkbox.checked
    })
  })
  .then(response => response.json())
  .then(data => {
    status.textContent = checkbox.checked ? 'Wind correction enabled' : 'Wind correction disabled';
    status.style.color = checkbox.checked ? '#059e8a' : '#666';
  })
  .catch(error => {
    console.error('Error toggling wind correction:', error);
    status.textContent = 'Error updating correction setting';
    status.style.color = '#d32f2f';
  });
}

/**
 * Get initial wind correction state (for index.html)
 */
function getWindCorrectionState() {
  fetch('/api/wind-correction')
  .then(response => response.json())
  .then(data => {
    const checkbox = document.getElementById('windCorrectionToggle');
    const status = document.getElementById('correctionStatus');
    if (checkbox && status) {
      checkbox.checked = data.enabled;
      status.textContent = data.enabled ? 'Wind correction enabled' : 'Wind correction disabled';
      status.style.color = data.enabled ? '#059e8a' : '#666';
    }
  })
  .catch(error => {
    console.error('Error getting wind correction state:', error);
  });
}

// Export functions for global access
window.initApp = initApp;
window.subscribe = subscribe;
window.unsubscribe = unsubscribe;
window.toggleCheckbox = toggleCheckbox;
window.toggleWindCorrection = toggleWindCorrection;
window.getWindCorrectionState = getWindCorrectionState;

// Auto-initialize when DOM is loaded
document.addEventListener('DOMContentLoaded', function() {
  initApp();
  
  // Initialize wind correction toggle if present
  if (document.getElementById('windCorrectionToggle')) {
    getWindCorrectionState();
  }
});

// Initialize immediately if DOM is already loaded
if (document.readyState === 'loading') {
  // DOM is still loading, event listener will handle it
} else {
  // DOM is already loaded
  initApp();
  if (document.getElementById('windCorrectionToggle')) {
    getWindCorrectionState();
  }
}