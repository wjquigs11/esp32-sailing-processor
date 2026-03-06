/*
Start wifi. If there are credentials saved in a file, use them.
If not, start a captive portal (using DNS).
*/
#include "include-general.h"
#include "windparse.h"

bool gotWifiCreds = false;
const char* wifiPath = "/wifi.txt";

// WiFi credentials structure
struct WiFiCredentials {
  String ssid;
  String password;
  String ip;
  String gateway;
};

#define MAX_WIFI_NETWORKS 5
WiFiCredentials wifi[MAX_WIFI_NETWORKS];
int wifiCount = 0;

TimerHandle_t connectTimer = NULL;
static int connectIdx = 0;
static int connectCount = 0;
#define MAX_RETRY (wifiCount*6)

// Global flag for WiFi connection handling
volatile bool needWifiConnect = false;

bool spiffsDebug = false;

IPAddress broadcastIP;

// check SPIFFS even if wifi is not enabled
void checkSPIFFS() {
  size_t totalBytes = SPIFFS.totalBytes();
  size_t usedBytes = SPIFFS.usedBytes();
  Serial.print("SPIFFS Total space: ");
  Serial.print(totalBytes);
  Serial.println(" bytes");
  Serial.print("SPIFFS Used space: ");
  Serial.print(usedBytes);
  Serial.println(" bytes");
  Serial.print("SPIFFS Free space: ");
  Serial.print(totalBytes - usedBytes);
  Serial.println(" bytes");
  float usedPercentage = ((float)usedBytes / totalBytes) * 100;
  Serial.print("SPIFFS Usage: ");
  Serial.print(usedPercentage);
  Serial.println("%");
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  Serial.println("Files on SPIFFS:");
  while (file) {
    Serial.print("  ");
    Serial.print(file.name());
    Serial.print(" - ");
    Serial.print(file.size());
    Serial.println(" bytes");
    file = root.openNextFile();
  }
}

// Read WiFi credentials from wifi.txt file
bool readWiFiCredentials() {
  File file = SPIFFS.open(wifiPath, "r");
  //if(!file || file.isDirectory()) {
  //  log::toAll("Failed to open wifi.txt for reading, open returned " + String(File));
  //  return false;
  if (!file) {
      Serial.println("Failed to open file");
      
      // Check if file exists
      if (!SPIFFS.exists("/file.txt")) {
          Serial.println("Error: File does not exist");
      }
      // Check if it's a directory
      else if (file.isDirectory()) {
          Serial.println("Error: Path is a directory, not a file");
      }
      // Check SPIFFS status
      else if (!SPIFFS.begin()) {
          Serial.println("Error: SPIFFS not mounted");
      }
      // Check available space
      else if (SPIFFS.totalBytes() == 0) {
          Serial.println("Error: SPIFFS has no space");
      }
      else {
          Serial.println("Error: Unknown file open error (corruption/permissions?)");
      }
      return false;
  }

  wifiCount = 0;
  
  // Read each line from the file
  while(file.available() && wifiCount < MAX_WIFI_NETWORKS) {
    String fileContent = file.readStringUntil('\n');
    fileContent.trim();
    
    if(fileContent.length() == 0) {
      continue; // Skip empty lines
    }
    
    // Parse the content by splitting at colons: SSID:password[:ip][:gateway]
    int firstColonPos = fileContent.indexOf(':');
    if(firstColonPos > 0) {
      wifi[wifiCount].ssid = fileContent.substring(0, firstColonPos);
      
      // Get the rest of the string after the first colon
      String remaining = fileContent.substring(firstColonPos + 1);
      
      // Check if there's another colon for IP address
      int secondColonPos = remaining.indexOf(':');
      if(secondColonPos > 0) {
        wifi[wifiCount].password = remaining.substring(0, secondColonPos);
        remaining = remaining.substring(secondColonPos + 1);
        
        // Check if there's another colon for Gateway
        int thirdColonPos = remaining.indexOf(':');
        if(thirdColonPos > 0) {
          wifi[wifiCount].ip = remaining.substring(0, thirdColonPos);
          wifi[wifiCount].gateway = remaining.substring(thirdColonPos + 1);
          wifi[wifiCount].gateway.trim();
        } else {
          // Only IP is present, no gateway
          wifi[wifiCount].ip = remaining;
          wifi[wifiCount].ip.trim();
          wifi[wifiCount].gateway = "";
        }
      } else {
        // Only SSID and password
        wifi[wifiCount].password = remaining;
        wifi[wifiCount].password.trim();
        wifi[wifiCount].ip = "";
        wifi[wifiCount].gateway = "";
      }
      
      char logBuf[256];
      snprintf(logBuf, sizeof(logBuf), "WiFi[%d] - SSID: %.32s, Password length: %d, IP: %.15s, Gateway: %.15s",
               wifiCount, wifi[wifiCount].ssid.c_str(), wifi[wifiCount].password.length(),
               wifi[wifiCount].ip.c_str(), wifi[wifiCount].gateway.c_str());
      log::toAll(String(logBuf));
      
      wifiCount++;
    }
  }
  
  file.close();
  
  if(wifiCount > 0) {
    char logBuf[64];
    snprintf(logBuf, sizeof(logBuf), "Loaded %d WiFi network(s)", wifiCount);
    log::toAll(String(logBuf));
    return true;
  }
  
  log::toAll("No valid WiFi credentials found in wifi.txt");
  return false;
}

#ifdef WIFI
// for captive portal
DNSServer dnsServer;

// Search for parameter in HTTP POST request
const char* PARAM_HOSTNAME = "hostname";
const char* PARAM_SSID = "ssid";
const char* PARAM_PASSWORD = "pass";
const char* PARAM_IP = "ip";
const char* PARAM_GATEWAY = "gateway";

void setUpDNSServer(DNSServer &dnsServer, const IPAddress &localIP) {
  // Set the TTL for DNS response and start the DNS server
  log::toAll("starting DNS server for captive portal");
  dnsServer.setTTL(3600);
  dnsServer.start(53, "*", localIP);
  // we modify loop() delay here; not sure if this is necessary
  loopDelay = DNS_INTERVAL;
}

void startPortal() {
    // Connect to Wi-Fi network with SSID and password
    log::toAll("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("ESP-SETUP", NULL);

    IPAddress IP = WiFi.softAPIP();
    log::toAll("AP IP address: " + IP.toString());

    setUpDNSServer(dnsServer, IP);
  String localIPURL = "http://" + IP.toString();

  // https://github.com/CDFER/Captive-Portal-ESP32
  //======================== Webserver ========================
  // WARNING IOS (and maybe macos) WILL NOT POP UP IF IT CONTAINS THE WORD "Success" https://www.esp8266.com/viewtopic.php?f=34&t=4398
  // SAFARI (IOS) IS STUPID, G-ZIPPED FILES CAN'T END IN .GZ https://github.com/homieiot/homie-esp8266/issues/476 this is fixed by the webserver serve static function.
  // SAFARI (IOS) there is a 128KB limit to the size of the HTML. The HTML can reference external resources/images that bring the total over 128KB
  // SAFARI (IOS) popup browser has some severe limitations (javascript disabled, cookies disabled)

  // Required
  server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });  // windows 11 captive portal workaround
  server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });                // Honestly don't understand what this is but a 404 stops win 10 keep calling this repeatedly and panicking the esp32 :)

  // Background responses: Probably not all are Required, but some are. Others might speed things up?
  // A Tier (commonly used by modern systems)
  server.on("/generate_204", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });       // android captive portal redirect
  server.on("/redirect", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });           // microsoft redirect
  server.on("/hotspot-detect.html", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });  // apple call home
  server.on("/canonical.html", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });     // firefox captive portal call home
  server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });                   // firefox captive portal call home
  server.on("/ncsi.txt", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });           // windows call home

  // B Tier (uncommon)
  //  server.on("/chrome-variations/seed",[](AsyncWebServerRequest *request){request->send(200);}); //chrome captive portal call home
  //  server.on("/service/update2/json",[](AsyncWebServerRequest *request){request->send(200);}); //firefox?
  //  server.on("/chat",[](AsyncWebServerRequest *request){request->send(404);}); //No stop asking Whatsapp, there is no internet connection
  //  server.on("/startpage",[](AsyncWebServerRequest *request){request->redirect(localIPURL);});

  // return 404 to webpage icon
  server.on("/favicon.ico", [](AsyncWebServerRequest *request) { request->send(404); });  // webpage icon

  // the catch all
  server.onNotFound([localIPURL](AsyncWebServerRequest *request) {
    log::toAll("CAPTIVE PORTAL onNotFound: " + String(request->method()) + " " + String(request->host()) + " " + String(request->url()) + " -> redirecting to " + localIPURL);
    request->redirect(localIPURL);
  });

  // Web Server Root URL: serve wifimanager.html from captive portal
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/wifimanager.html", "text/html");
  });

  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      String ssid, password, ip, gateway;
      int params = request->params();
      for (int i=0; i<params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()) {
          if (p->name() == PARAM_HOSTNAME && p->value().length() > 0) {
            // hostname is special; goes into preferences instead of file
            host = p->value().c_str();
            preferences.putString("hostname", host);
            log::toAll("host set to: " + host);
          }
          if (p->name() == PARAM_SSID) {
            ssid = p->value().c_str();
            log::toAll("SSID set to: " + ssid);
          }
          if (p->name() == PARAM_PASSWORD) {
            password = p->value().c_str();
            log::toAll("Password set to: " + password);
          }
          if (p->name() == PARAM_IP) {
            ip = p->value().c_str();
            log::toAll("IP Address set to: " + ip);
          }
          if (p->name() == PARAM_GATEWAY) {
            gateway = p->value().c_str();
            log::toAll("Gateway set to: " + gateway);
          }
          log::toAll("POST[" + p->name() + "]: " + p->value());
        }
      }
      if (ssid.length() > 0 && password.length() > 0) {
        // Save the WiFi credentials to the file
        // Read existing lines first
        String existingLines = "";
        if(SPIFFS.exists(wifiPath)) {
          File readFile = SPIFFS.open(wifiPath, FILE_READ);
          if(readFile) {
            while(readFile.available()) {
              existingLines += readFile.readStringUntil('\n') + "\n";
            }
            readFile.close();
          }
        }
        
        // Write new credentials first, then append old ones
        File file = SPIFFS.open(wifiPath, FILE_WRITE);
        if(!file) {
          log::toAll("Failed to open wifi.txt for writing");
        } else {
          // Format: SSID:password:ip:gateway
          String newEntry = ssid + ":" + password;
          if (ip.length() > 0)
            newEntry += ":" + ip;
          if (gateway.length() > 0)
            newEntry += ":" + gateway;
          file.println(newEntry);
          // Write back existing lines
          if(existingLines.length() > 0) {
            file.print(existingLines);
          }
          
          file.close();
          log::toAll("WiFi credentials saved to file (newest first)");
        }
      } else
        log::toAll("must supply ssid and password");
    request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
    delay(3000);
    ESP.restart();
  }); // end lambda for "/"
  server.begin();
}

// WiFi event handlers
void onWiFiConnected() {
  log::toAll("WiFi connected!");
  log::toAll("ESP IP Address: http://" + WiFi.localIP().toString());
  //broadcastIP = WiFi.localIP();
  //broadcastIP[3] = 255;
  //log::toAll("Broadcast IP set to: " + broadcastIP.toString());
  wifiConnected = true;
  xTimerDelete(connectTimer,0);

#ifdef NTP
  // Configure NTP server and timezone
  log::toAll("Setting up time synchronization...");
  setupTime();
  
  // Wait for time synchronization (timeout after 10 seconds)
  waitForTimeSync(10);
  
  // If time sync was successful, log the current time
  if (isNtpSyncSuccessful()) {
    log::toAll("Current time: " + getFormattedTime());
  } else {
    log::toAll("Failed to sync time with NTP servers. Will use browser time if available.");
  }
#endif

  // Initialize mDNS
  String mdnsHost = host;
  mdnsHost.toLowerCase();
  mdnsHost.replace("_", "-");
  mdnsHost.replace(" ", "-");
  String sanitized = "";
  for (int i = 0; i < mdnsHost.length() && i < 63; i++) {
    char c = mdnsHost.charAt(i);
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
      sanitized += c;
    }
  }
  if (sanitized.length() > 0) {
    log::toAll("Starting MDNS with hostname: " + sanitized);
    if (!MDNS.begin(sanitized.c_str())) {
      log::toAll(F("Error starting MDNS responder"));
    } else {
      log::toAll("MDNS started successfully as " + sanitized + ".local");
      if (!MDNS.addService("http", "tcp", HTTP_PORT)) {
        log::toAll("MDNS add service failed");
      } else {
        log::toAll("MDNS service added successfully");
      }
      
      // Query for other mDNS services
      int n = MDNS.queryService("http", "tcp");
      if (n == 0) {
        log::toAll("No other mDNS services found");
      } else {
        log::toAll("Found " + String(n) + " mDNS services:");
        for (int i = 0; i < n; i++) {
          log::toAll("  - " + MDNS.hostname(i) + ".local");
        }
      }
    }
  }
  server.begin();
  // initialize UDP after wifi is connected
  // (only if we need a listening port)
  //#ifdef UDP_FORWARD
  //setupUDP();
  //#endif
#ifdef TCP_FORWARD
  setupTCP();
#endif
}

static int disConCount = 0;
void onWiFiDisconnected() {
  disConCount++;
  char logBuf[64];
  snprintf(logBuf, sizeof(logBuf), "WiFi disconnected disconnect count: %d", disConCount);
  log::toAll(String(logBuf));
  wifiConnected = false;
}

void tryConnect(TimerHandle_t xTimer) {
  if (wifiConnected) {
    log::toAll("Already connected, stopping connection attempts");
    xTimerStop(connectTimer, 0);
    return;
  }
  // Just set flag instead of doing heavy work in timer context
  needWifiConnect = true;
}

// Add this function to be called from main loop
void handleWifiConnect() {
  if (!needWifiConnect) return;
  needWifiConnect = false;
  
  // Use char buffer instead of String concatenation to reduce stack usage
  char logBuf[128];
  snprintf(logBuf, sizeof(logBuf), "connecting to: %.32s connect count: %d",
           wifi[connectIdx].ssid.c_str(), connectCount);
  log::toAll(String(logBuf));
  
  WiFi.begin(wifi[connectIdx].ssid, wifi[connectIdx].password); // TBD: IP/GW
  
  if (connectCount++ >= MAX_RETRY) {
    snprintf(logBuf, sizeof(logBuf), "Failed to connect to wifi after %d tries", connectCount);
    log::toAll(String(logBuf));
    xTimerDelete(connectTimer,0);
    connectTimer = NULL;
    // Stop STA mode to prevent further disconnect events
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    // Now start captive portal in AP mode
    log::toAll("Starting captive portal after connection failure");
    startPortal();
    return;
  }
  connectIdx++;   // index of ssid we're trying
  if (connectIdx >= wifiCount)
    connectIdx = 0;
}

bool setupWifi() {
  log::toAll("Starting WiFi...");
  // Set up WiFi event handlers
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    onWiFiConnected();
  }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    onWiFiDisconnected();
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  
  if (!gotWifiCreds) {
    log::toAll("No wifi credentials in wifi.txt, starting captive portal...");
    startPortal();
    return false;
  } else {
    log::toAll("starting wifi");
    WiFi.mode(WIFI_STA);
    //WiFi.begin(ssid, password);
    wifiStartTime = now; // Set global wifiStartTime when WiFi connection starts
    // set up tryConnect every 5 seconds
    // pdTRUE means auto-reload (periodic timer)
    connectTimer = xTimerCreate("connectTimer", 5000/portTICK_PERIOD_MS, pdTRUE, NULL, tryConnect);
    if (connectTimer == NULL) {
      log::toAll("Failed to create wifi connect timer");
      return false;
    }
    xTimerStart(connectTimer,0);
    log::toAll("WiFi connection initiated (async)...");
    
    // Start web server immediately (will be in AP mode if connection fails)
    // I'm skeptical that this will work but let's try
    //server.begin();
    
    return false; // Not connected yet, will be set by event handler
  }
}

void resetWifi() {
  log::toAll("Resetting WiFi settings...");
  WiFi.disconnect(true);
  if (SPIFFS.exists(wifiPath))
    if (SPIFFS.remove(wifiPath))
      log::toAll("removed wifi params");
  delay(3000);
  ESP.restart();
}
#endif // ifdef WIFI