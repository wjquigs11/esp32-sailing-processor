/**
 * Unified Hamburger Menu System
 * Dynamically creates and manages the hamburger menu for all pages
 */

class HamburgerMenu {
  constructor() {
    this.menuItems = [
      { href: '/', text: 'Home' },
      { href: '/settings', text: 'Settings' },
      { href: '/wifimanager.html', text: 'Wifi Settings' },
      { href: '/compass', text: 'Compass' },
      { href: '/update.html', text: 'Update' },
      { href: '/host', text: 'Host info' },
      { href: '/weather.html', text: 'Weather' },
      { href: '/wind.html', text: 'Wind' }
    ];
    
    this.init();
  }

  init() {
    this.injectCSS();
    this.createMenuHTML();
    this.attachEventListeners();
  }

  injectCSS() {
    const style = document.createElement('style');
    style.textContent = `
      /* Hamburger Menu Styles */
      .hamburger-menu {
        position: fixed;
        top: 20px;
        right: 20px;
        z-index: 1000;
      }
      
      .hamburger-icon {
        background: #333;
        border: none;
        padding: 8px;
        border-radius: 5px;
        cursor: pointer;
        width: 45px;
        height: 45px;
        display: flex;
        align-items: center;
        justify-content: center;
      }
      
      .hamburger-icon:hover {
        background: #555;
      }
      
      .hamburger-icon img {
        width: 24px;
        height: 24px;
        filter: invert(1); /* Makes the image white */
      }
      
      .menu-dropdown {
        display: none;
        position: absolute;
        top: 50px;
        right: 0;
        background: white;
        border: 1px solid #ccc;
        border-radius: 5px;
        box-shadow: 0 2px 10px rgba(0,0,0,0.3);
        min-width: 200px;
        z-index: 1001;
        max-height: 400px;
        overflow-y: auto;
      }
      
      .menu-dropdown.show {
        display: block !important;
        visibility: visible !important;
        opacity: 1 !important;
      }
      
      .menu-dropdown a {
        display: block;
        padding: 12px 16px;
        text-decoration: none;
        color: #333;
        border-bottom: 1px solid #eee;
      }
      
      .menu-dropdown a:last-child {
        border-bottom: none;
      }
      
      .menu-dropdown a:hover {
        background-color: #f5f5f5;
      }
    `;
    document.head.appendChild(style);
  }

  createMenuHTML() {
    // Create the hamburger menu container
    const menuContainer = document.createElement('div');
    menuContainer.className = 'hamburger-menu';
    
    // Create the hamburger button
    const button = document.createElement('button');
    button.className = 'hamburger-icon';
    button.onclick = () => this.toggleMenu();
    
    // Create the hamburger image
    const img = document.createElement('img');
    img.src = 'hamburger.png';
    img.alt = 'Menu';
    button.appendChild(img);
    
    // Create the dropdown menu
    const dropdown = document.createElement('div');
    dropdown.className = 'menu-dropdown';
    dropdown.id = 'menuDropdown';
    
    // Add menu items
    this.menuItems.forEach(item => {
      const link = document.createElement('a');
      link.href = item.href;
      link.textContent = item.text;
      dropdown.appendChild(link);
    });
    
    // Assemble the menu
    menuContainer.appendChild(button);
    menuContainer.appendChild(dropdown);
    
    // Add to page
    document.body.appendChild(menuContainer);
  }

  toggleMenu() {
    console.log('toggleMenu called');
    const dropdown = document.getElementById('menuDropdown');
    if (dropdown) {
      console.log('Dropdown found, toggling show class');
      dropdown.classList.toggle('show');
      console.log('Dropdown classes:', dropdown.className);
      
      // Additional debugging
      const computedStyle = window.getComputedStyle(dropdown);
      console.log('Dropdown computed styles:', {
        display: computedStyle.display,
        visibility: computedStyle.visibility,
        opacity: computedStyle.opacity,
        zIndex: computedStyle.zIndex,
        position: computedStyle.position,
        top: computedStyle.top,
        right: computedStyle.right
      });
      
      // Force visibility for testing
      if (dropdown.classList.contains('show')) {
        dropdown.style.display = 'block';
        dropdown.style.visibility = 'visible';
        dropdown.style.opacity = '1';
        dropdown.style.zIndex = '1001';
        console.log('Forced dropdown to be visible');
      }
    } else {
      console.error('Dropdown not found!');
    }
  }

  attachEventListeners() {
    // Close menu when clicking outside
    document.addEventListener('click', (event) => {
      const menu = document.querySelector('.hamburger-menu');
      const dropdown = document.getElementById('menuDropdown');
      
      if (menu && dropdown && !menu.contains(event.target)) {
        dropdown.classList.remove('show');
      }
    });

    // Close menu on escape key
    document.addEventListener('keydown', (event) => {
      if (event.key === 'Escape') {
        const dropdown = document.getElementById('menuDropdown');
        if (dropdown) {
          dropdown.classList.remove('show');
        }
      }
    });
  }
}

// Initialize the hamburger menu
function initializeMenu() {
  console.log('Initializing hamburger menu...');
  try {
    new HamburgerMenu();
    console.log('Hamburger menu initialized successfully');
  } catch (error) {
    console.error('Error initializing hamburger menu:', error);
  }
}

// Initialize when DOM is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', initializeMenu);
} else {
  // DOM is already ready, initialize immediately
  initializeMenu();
}