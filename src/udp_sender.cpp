#ifdef UDP_FORWARD
#include "include-general.h"
#include "windparse.h"
#include "BoatData.h"

// listen on serial port (AutopilotSerial), read sentences, transmit broadcast on UDP
// for 1-way forwarding of AIS and RTK traffic
// SignalK and Navionics will be listeners
// based on https://github.com/alvra/nmea-bridge/tree/main

uint16_t nmea_sentences_received = 0;
uint16_t nmea_sentences_sent = 0;

/* The buffer should be long enough for about two sentences
** so we can handle the case where a (single) newline is lost.
*/
//#define NMEA_SENTENCE_BUFFER_SIZE MAX_NMEA0183_MSG_BUF_LEN
#define NMEA_SENTENCE_BUFFER_SIZE MAX_NMEA0183_MSG_LEN
char nmea_sentence_buffer[NMEA_SENTENCE_BUFFER_SIZE + 1];
uint8_t nmea_sentence_buffer_filled = 0;  // number of chars in buffer

WiFiUDP udp_server;

// only call this if we are listening
// not needed if we are only transmitting/broadcasting
void setupUDP() {
  udp_server.begin(UDP_FORWARD_PORT);
  log::toAll("udp port open: " + String(UDP_FORWARD_PORT));
}

void handle_serial_event() {
    // on incoming data over the hardware serial port
    int serial_character_int;
    char serial_character;
    while (1) {
        serial_character_int = AISserial.read();
        if (serial_character_int < 0) {
            // no more data available
            return;
        }
        serial_character = (char)serial_character_int;
        // Debug output disabled - was causing garbled serial output
        // Serial.print(serial_character);
        // TODO sometimes this gives a large number of zero bytes
        // TODO then we also don't get the extra newline
        nmea_sentence_buffer[nmea_sentence_buffer_filled] = serial_character;
        nmea_sentence_buffer_filled ++;
        if (serial_character == '\n') {
            // filler
        } else if (nmea_sentence_buffer_filled >= NMEA_SENTENCE_BUFFER_SIZE) {
            Serial.println("nmea sentence buffer overflow");
            // A newline must have been lost since at least two sentences
            // should fit in the buffer, so send all buffered data immedately
            // to let the client decide how to handle this.
            nmea_sentence_buffer[NMEA_SENTENCE_BUFFER_SIZE] = '\n';
            nmea_sentence_buffer_filled = NMEA_SENTENCE_BUFFER_SIZE + 1;
        } else {
            continue;
        }
        handle_outgoing_sentence(nmea_sentence_buffer, nmea_sentence_buffer_filled);
        nmea_sentence_buffer_filled = 0;
    }
}

void handle_outgoing_sentence(const char *sentence, size_t length) {
  /* data is received from the nmea device
    sentence: always includes the newline and must be newline terminted
   */
  log::toAll(String(sentence));
  transmit_outgoing_over_udp(sentence, length);
  nmea_sentences_received ++;
}

IPAddress clientIP(192, 168, 68, 67);

// if UDP_FORWARD_TIMO then we send directly from nmea0183handlers.cpp
void transmit_outgoing_over_udp(const char *sentence, size_t length) {
    //int result = udp_server.beginPacket(INADDR_NONE, UDP_FORWARD_PORT);
    if (wifiConnected) {
        int result = udp_server.beginPacket(clientIP, UDP_FORWARD_PORT);
        if (result) {
            int wrote = udp_server.write((const uint8_t*)sentence, length);
            if (wrote != length)
                Serial.println("short write");
            else Serial.printf("udp: %s\n",sentence);
            udp_server.endPacket();
        } else Serial.println("udp init failed");
    }
}

#if NOTDEF  // keeping in case I want to change to multicast in future
void transmit_outgoing_over_udp(char *sentence, size_t length) {
    if (config.tx_mode == TransmitMode::multicast) {
        int result = udp_server.beginPacketMulticast(
            effective_tx_address, config.tx_port,
            get_device_ip_address());
    } else {
        int result = udp_server.beginPacket(
            effective_tx_address, config.tx_port);
    }
    udp_server.write(sentence, length);
    udp_server.endPacket();
}
#endif

#endif // UDP_FORWARD