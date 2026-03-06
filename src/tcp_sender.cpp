#ifdef TCP_FORWARD
#include "include-general.h"
#include "windparse.h"
#include "BoatData.h"

bool TCPdebug = false;

// listen on serial port (AISserial), read sentences, transmit over TCP
// for 1-way forwarding of AIS and RTK traffic
// SignalK and Navionics will be listeners

uint16_t nmea_sentences_received = 0;
uint16_t nmea_sentences_sent = 0;

char nmea_sentence_buffer[MAX_NMEA0183_MSG_LEN + 3];
uint8_t nmea_sentence_buffer_filled = 0;  // number of chars in buffer

#define MAX_TCP_CLIENTS 5
WiFiServer tcp_server(TCP_PORT);
WiFiClient tcp_clients[MAX_TCP_CLIENTS];
uint8_t tcp_client_count = 0;

// Function declarations
void cleanup_disconnected_clients();

// only call this if we are listening (as opposed to outbound connecting)
void setupTCP() {
  tcp_server.begin();
  tcp_client_count = 0;
  // Initialize all client slots as empty
  for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
    tcp_clients[i] = WiFiClient();
  }
  log::toAll("tcp port open: " + String(TCP_PORT) + " (max clients: " + String(MAX_TCP_CLIENTS) + ")");
}

void handle_tcp_connections() {
  // Check for new client connections
  WiFiClient new_client = tcp_server.available();
  if (new_client) {
    // Find an empty slot for the new client
    bool client_added = false;
    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
      if (!tcp_clients[i] || !tcp_clients[i].connected()) {
        // Clean up disconnected client if needed
        if (tcp_clients[i]) {
          String oldIP = tcp_clients[i].remoteIP().toString();
          uint16_t oldPort = tcp_clients[i].remotePort();
          tcp_clients[i].stop();
          log::toAll("TCP client disconnected: " + oldIP + ":" + String(oldPort));
          tcp_client_count--;
        }
        
        // Add new client
        tcp_clients[i] = new_client;
        String clientIP = new_client.remoteIP().toString();
        uint16_t clientPort = new_client.remotePort();
        tcp_client_count++;
        log::toAll("TCP client connected from: " + clientIP + ":" + String(clientPort) +
                   " (slot " + String(i) + ", total clients: " + String(tcp_client_count) + ")");
        client_added = true;
        break;
      }
    }
    
    if (!client_added) {
      // No available slots, reject the connection
      String clientIP = new_client.remoteIP().toString();
      uint16_t clientPort = new_client.remotePort();
      log::toAll("TCP connection rejected (max clients reached): " + clientIP + ":" + String(clientPort));
      new_client.stop();
    }
  }
  
  // Clean up any disconnected clients
  cleanup_disconnected_clients();
}

void cleanup_disconnected_clients() {
  for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
    if (tcp_clients[i] && !tcp_clients[i].connected()) {
      String clientIP = tcp_clients[i].remoteIP().toString();
      uint16_t clientPort = tcp_clients[i].remotePort();
      tcp_clients[i].stop();
      tcp_clients[i] = WiFiClient(); // Reset to empty client
      tcp_client_count--;
      log::toAll("TCP client disconnected: " + clientIP + ":" + String(clientPort) +
                 " (total clients: " + String(tcp_client_count) + ")");
    }
  }
}

#ifdef AIS_FORWARD
/*
Experimental change to handle_serial_event():
Set baud to 38400
read AIS data
if flags indicate Seatalk data needs to be sent,
    set baud to 4800
    transmit seatalk sentences
    flush

The experiment is working, at least on the bench. Since the AIS doesn't stop transmitting,
some incoming AIS data is garbled/lost, but that *shouldn't* matter, since any active
vessel will send frequent updates, and SK should cache the last position
(Any bytes received from AIS at 38400 when the UART is at 4800 will most likely be corrupted)

NOTE: AIS is now directly connected to Pi so none of this code is active...sigh
*/
void handle_serial_event() {
    static unsigned long lastSeatalkTX = 0;
    // on incoming data over the hardware serial port
    char serial_character;
    while (AISserial.available()) {
        serial_character = AISserial.read();
        // Reset buffer if we get a new sentence start
        if (serial_character == '$') {
            if (TCPdebug && nmea_sentence_buffer_filled > 0) {
                log::toAll("NEW SENTENCE START - resetting buffer (was at pos " + String(nmea_sentence_buffer_filled) + ")");
            }
            nmea_sentence_buffer_filled = 0;
            memset(nmea_sentence_buffer, 0, sizeof(nmea_sentence_buffer));
        }
        nmea_sentence_buffer[nmea_sentence_buffer_filled] = serial_character;
        nmea_sentence_buffer_filled++;
        if (serial_character == '\n') {
            // End of sentence - null terminate for logging
            nmea_sentence_buffer[nmea_sentence_buffer_filled] = '\0';
            if (TCPdebug) {
                log::toAll(nmea_sentence_buffer);
                /*log::toAll("Buffer length: " + String(nmea_sentence_buffer_filled));
                // Check if this looks like a valid NMEA sentence
                bool starts_valid = (nmea_sentence_buffer_filled > 0 &&
                                (nmea_sentence_buffer[0] == '$'));
                log::toAll("Starts with $/!: " + String(starts_valid ? "YES" : "NO"));
                if (!starts_valid && nmea_sentence_buffer_filled > 0) {
                    log::toAll("First char: '" + String(nmea_sentence_buffer[0]) + "' (ASCII " + String((int)nmea_sentence_buffer[0]) + ")");
                }
                */
            }
            handle_outgoing_sentence(nmea_sentence_buffer, nmea_sentence_buffer_filled);
            // Clear buffer completely
            nmea_sentence_buffer_filled = 0;
            memset(nmea_sentence_buffer, 0, sizeof(nmea_sentence_buffer));
            if (0 && TCPdebug) log::toAll("Buffer cleared and reset");
        } else if (nmea_sentence_buffer_filled >= MAX_NMEA0183_MSG_LEN) {
            log::toAll("handle_serial_event() nmea sentence buffer overflow");
            // A newline must have been lost
            nmea_sentence_buffer[MAX_NMEA0183_MSG_LEN] = '\n';
            nmea_sentence_buffer_filled = MAX_NMEA0183_MSG_LEN + 1;
            if (TCPdebug)
                log::toAll("overflow:" + String(nmea_sentence_buffer));
            handle_outgoing_sentence(nmea_sentence_buffer, nmea_sentence_buffer_filled);
            nmea_sentence_buffer_filled = 0;
        }
        // For all other characters, just continue accumulating in buffer
    }
    // no more AIS data; check for Seatalk data and transmit if needed
    if (sendSeaTalk && (now - lastSeatalkTX > ST_RATE)) {
        lastSeatalkTX = now;
        if (AutopilotSerial == AISserial)
            AutopilotSerial.updateBaudRate(STBAUD);
        sendSTwind();
        // this is a pretty crappy way to check if we have an active waypoint, TBD
        if (pBD->DistanceToWaypoint > 0.00000001) {
            sendRMB();
            sendAPB();
        }
        if (AutopilotSerial == AISserial) {
            AISserial.flush();
            AutopilotSerial.updateBaudRate(AISBAUD);
        }
        sendSeaTalk = false;
    }
}

void handle_outgoing_sentence(const char *sentence, size_t length) {
  /* data is received from the nmea device
    sentence: always includes the newline and must be newline terminted
   */
  log::toAll(String(sentence));
  transmit_outgoing_over_tcp(sentence, length);
  nmea_sentences_received++;
}
#endif

// used by NMEA0183Handlers in hybrid mode to forward all RTK sentences to SignalK
void transmit_outgoing_over_tcp(const char *sentence, size_t length) {
    if (TCPdebug) log::toAll(sentence);
    if (!wifiConnected) {
        return;
    }
    // Create a buffer to hold the sentence with proper CRLF termination
    char formatted_sentence[MAX_NMEA0183_MSG_LEN + 3]; // +2 for \r\n, +1 for null terminator
    strcpy(formatted_sentence, sentence);
    // Check if sentence ends with \r\n
    int len = strlen(formatted_sentence);
    if (!(len >= 2 && formatted_sentence[len-2] == '\r' && formatted_sentence[len-1] == '\n')) {
        // Remove any existing \n and add proper \r\n
        if (len > 0 && formatted_sentence[len-1] == '\n') {
            formatted_sentence[len-1] = '\0';
            len--;
        }
        strcat(formatted_sentence, "\r\n");
        len += 2;
    }
    // Check for new connections
    handle_tcp_connections();
    if (tcp_client_count == 0) {
        return; // No clients connected
    }
    // Send data to all connected clients
    int successful_sends = 0;
    size_t formatted_length = strlen(formatted_sentence);
    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
        if (tcp_clients[i] && tcp_clients[i].connected()) {
            size_t wrote = tcp_clients[i].write((const uint8_t*)formatted_sentence, formatted_length);
            if (wrote != formatted_length) {
                String clientIP = tcp_clients[i].remoteIP().toString();
                uint16_t clientPort = tcp_clients[i].remotePort();
                log::toAll("TCP short write to client " + clientIP + ":" + String(clientPort));
            } else {
                successful_sends++;
            }
        }
    }
    if (successful_sends > 0) {
        //Serial.printf("tcp: %s", sentence);
        nmea_sentences_sent += successful_sends;
    }
}

// Get current TCP connection statistics
String get_tcp_connection_stats() {
    String stats = "TCP Connections: " + String(tcp_client_count) + "/" + String(MAX_TCP_CLIENTS) + " [";
    bool first = true;
    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
        if (tcp_clients[i] && tcp_clients[i].connected()) {
            if (!first) stats += ", ";
            stats += tcp_clients[i].remoteIP().toString() + ":" + String(tcp_clients[i].remotePort());
            first = false;
        }
    }
    stats += "]";
    return stats;
}

// Force disconnect all clients (useful for cleanup)
void disconnect_all_tcp_clients() {
    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
        if (tcp_clients[i] && tcp_clients[i].connected()) {
            String clientIP = tcp_clients[i].remoteIP().toString();
            uint16_t clientPort = tcp_clients[i].remotePort();
            tcp_clients[i].stop();
            tcp_clients[i] = WiFiClient();
            log::toAll("TCP client forcibly disconnected: " + clientIP + ":" + String(clientPort));
        }
    }
    tcp_client_count = 0;
    log::toAll("All TCP clients disconnected");
}

#endif // TCP_FORWARD