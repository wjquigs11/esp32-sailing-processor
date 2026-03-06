#ifdef NMEA0183
 
#ifndef _NMEA0183Handlers_H_
#define _NMEA0183Handlers_H_
#include "BoatData.h"
#include <Arduino.h>
//#include <Time.h>
#include <NMEA0183.h>
#include <NMEA0183Msg.h>
#ifdef N2K
#include <NMEA2000.h>
#endif

void DebugNMEA0183Handlers(Stream* _stream);

void HandleNMEA0183Msg(const tNMEA0183Msg &NMEA0183Msg);

struct tNMEA0183Handler {
  const char *Code;
  void (*Handler)(const tNMEA0183Msg &NMEA0183Msg); 
  int numMessages;
};

extern tNMEA0183Handler NMEA0183Handlers[];

#endif
#endif

