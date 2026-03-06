# Garmin N2K Mast Rotation Compensator

This project is a fork of the original mast rotation compensator from RandelO, modified to enable the code to be used with Garmin wind instruments.

## Project Overview

The solution uses an ESP32 microcontroller in a Raspberry Pi form factor with an RS232 CAN bus module. The ESP32 sits on an isolated N2K bus with the Nexus/Garmin wind instrument (GND10), processes wind packets, reads rotation from the Honeywell sensor, and sends corrected wind packets to the main N2K bus, where it's picked up by the Garmin display.

Because the ESP32 is relatively idle, I use it to process incoming NMEA0183 data from GPS and/or AIS, as well as from a dual-antenna RTK GPS (Unicore UM982) that allows precise heading measurement to compensate for errors in wind angle measurement. It can optionally log tracks to GPX files. It transmits wind data as NMEA0183 to a controller for my Autohelm tiller pilot so it works in "steer to wind" mode. There's an environmental sensor, and web pages to chart temperature, humidity, and barometric pressure. I process RTK GPS data using the ttlappalainen libraries, but the AIS data is simply broadcast via UDP, where it can be picked up by my navigation tablet running Navionics.[^1]

When there's no traffic but wifi is active, it runs at 0.2A, or about 1 watt.

## Do As I Say, Not As I Do

I wouldn't do it this way again! I found the Pi-shaped ESP32 to be quite difficult to work with, considering its triple indirection from Pi GPIOs to Pi header pins to ESP32 GPIOs (the documentation labels specific pins as "GPIOx", which you might think indicates that pin is connected to GPIO x. It's not. You need to manually map header pins to ESP32 IO pins, which leaves one wondering why they included the incorrect GPIO labels in the documentation). Once I got it working, I didn't want to touch it, but unfortunately the PICAN-M board stopped functioning and needed to be replaced. So why use it? When I started building this system, it was the only practical way to get two isolated CAN interfaces on a single ESP32, because almost no off-the shelf MCP2515 modules use 3.3v logic. I have since discovered this [WaveShare](https://www.waveshare.com/wiki/2-CH_CAN_HAT) module. If I build another sensor for anyone else, I will use an ESP32 devkit that can use 12V power like the [Acebot](https://www.waveshare.com/wiki/2-CH_CAN_HAT) with the Waveshare HAT (which obviously does not need to be plugged in as a HAT...it has separate pins for SPI and a jumper for 3.3V/5V power).

I also experimented with two BNO08X IMUs: one mounted on the mast and one in the ESP32 enclosure in the cabin. This is a decent solution if you're racing at night and you you just want to know if you're sailing close-hauled or if you've inadvertently veered away[^2], but the IMUs usually have several degrees of error, so I didn't consider it accurate enough for my needs. I would like to maximize upwind VMG and this requires accurate wind direction measurement (within a degree or two). It's cheaper than the Honeywell position sensor, but not worth the savings IMO. If you already have a good electronic compass like a B&G Precision 9 (±2°), then you can get another decent compass like the Garmin SteadyCast or B&G ZG100[^3]...both of those claim ±3°accuracy so you *shouldn't* be off by more than 5 degrees. This isn't sufficient to determine what heading is optimal for your boat upwind, and by the time you've bought two compasses you've spent more than one Honeywell, but it's an adequate solution. 

I also investigated the "pull string" solutions adapted from rudder position sensors that use rotary encoders. These also seem adequate but I break stuff on my boat all the time and I feel like it would last a few weeks at best. Also, they have a limited range of rotation and I routinely rotate my mast through 100 degrees.

Now that RTK GPS is getting a lot less expensive, it's also a viable option.[^4] I have a Witmotion module with Unicore UM982 dual antenna receiver. At times it has shown signal from 50-70 satellites! Installing one set of antennas on deck and another on the spreaders is possible, and at spreader distance of around 1M, heading accuracy is in tenths of a degree. The concept of measuring relative mast rotation by phase differences from satellites thousands of miles away is mind-boggling, but entirely feasible. What's next, packages delivered by drone? Artificial intelligence that acts like your friend? Flying cars?

## Hardware Requirements

- ESP32 microcontroller/devkit
- RS232 CAN bus module
- Honeywell position sensor (100° or 180° - 180° recommended if you rotate your mast fully)
- Voltage divider components for the Honeywell sensor
- ADS1015 or 1115 analog to digital converter for the Honeywell sensor

## Features

### Wind Correction
- Reads apparent wind data from NMEA 2000 bus
- Applies mast rotation compensation using Honeywell position sensor
- Calculates true wind angle and speed
- Transmits corrected wind data on NMEA 2000 bus

### Compass Integration
- BNO085 compass support for heading data
- Optional mast-mounted compass for rotation detection

### Data Logging
- Wind data logging capability (when WINDLOG flag enabled)
- Console logging

### Web Interface
- Real-time wind and navigation data display
- Settings configuration
- Calibration tools
- Multiple display pages (wind, compass, settings)

### Connectivity
- NMEA 2000 bus integration
- NMEA 0183 support
- WiFi connectivity for configuration and monitoring
- WebSerial for remote diagnostics

## Build Configuration

The project uses various build flags in [`platformio.ini`](platformio.ini) to enable/disable features and optimize the firmware. Here's an explanation of each flag:

#### Web and Network Features
- **`-D ELEGANTOTA`** - Enable ElegantOTA for over-the-air firmware updates via web interface.
- **`-D ELEGANTOTA_USE_ASYNC_WEBSERVER=1`** - Configure ElegantOTA to use the async web server.
- **`-D WEBSERIAL`** - Enable WebSerial for remote serial console access via web browser.
- **`-D APPHANDLER`** - Enable application-specific command handlers for WebSerial interface.
- **`-D WIFI`** - Enable WiFi connectivity for web interface and remote access.

#### Navigation and Positioning
- **`-D RTK`** - Enable RTK (Real-Time Kinematic) GPS support for high-precision positioning.
- **`-D RTK_HYBRID`** - Enable hybrid RTK mode where we process only HPR and GGA and forward all else (to SK on RPI).
- **`-D N2K`** - Enable NMEA 2000 bus communication for marine instrument integration.
- **`-D NMEA0183`** - Enable NMEA 0183 protocol support for legacy marine instruments.

#### Sensors and Hardware
- **`-D BNO08X`** - Enable BNO08X IMU (Inertial Measurement Unit) for compass and orientation data.
- **`-D BNOADDR=0x4A`** or **`-D BNOADDR=0x4B`** - Set I2C address for BNO08X sensor (varies by environment).
- **`-D HONEY`** - Enable Honeywell position sensor for mast rotation measurement.
- **`-D DISPLAYON`** - Enable OLED display output for local status information.

#### Communication Protocols
- **`-D TCP_FORWARD`** - Enable TCP forwarding of navigation data to external systems.
- **`-D SEATALK`** - Enable SeaTalk protocol support for Raymarine instrument communication.
- **`-D AIS_FORWARD`** - Enable AIS (Automatic Identification System) data forwarding.

#### Optional Flags

- **`-D NTP`** - Network Time Protocol synchronization.
- **`-D DEEPSLEEP`** - Deep sleep mode for power conservation.
- **`-D TEMPLATE`** - Template configuration mode (not used).

## Troubleshooting

Common issues and solutions:
- If no wind data appears, check N2K connections and device addresses
- For compass calibration issues, use the calibration page in the web interface
- If WiFi connection fails, the system will start in AP mode for configuration

## Credits

This project builds on work from:
- randelO: Original mast rotation compensator code
- ttlappalainen: N2K libraries
- mairas: Sailor Hat ESP32 hardware and gateway code
- buhhe: Parsing raw N2K data
- lkarsten: Library for Nexus FDX data parsing

## License

See LICENSE file for details

### Notes

 [^1]:My boat partner of long ago installed one of the first available recreational AIS transponders, the West Marine AIS1000. It worked great for 20 years and was still working when I decided to proactively replace it with a Digital Yacht AIT1500. I bought the 1500 because it's been superceded by newer models, so it was under $500, which in these days of Drumpf inflation, is a good price. It doesn't have native N2K, so I use a serial interface on the ESP32 to read NMEA0183 data from the AIS. I've since learned about another [transponder](https://www.quark-marine.com/product/a051t-class-b-wifi-nmea-0183-2000-ais-transponder/) from Quark Marine that has wifi and N2K, and that's what I would recommend, although I don't have direct experience. Regardless, I consider an AIS transponder to be as critical a piece of safety gear as PFDs and GPS, *especially* if you sail at night. The first time a ship hails you by name and asks where you're going or asks you to change course, you realize that it's nice to have. Subsequently, a 300-meter container ship near Whidbey Island in Puget Sound was not in the shipping channel and was completely invisible to us because of background lights. We were both traveling at least 12 knots, so our convergence speed was over 20 kts. That's when I realized a transponder was not optional any longer. How likely are you to respond to a VHF hail of "Sailing vessel 3 miles south southeast of Pigeon Point, please alter course" compared to "Tatiana, Tatiana, Tatiana, this is the vessel..."?
 If you already have a transceiver, don't throw it away, but make sure you have your chosen tablet app configured for CPA alarms. And don't consider another transceiver (as opposed to transponder) if your existing one dies.

[^2]:In 2019, my boat had only two instruments, depth and speed, and they were from a little-known manufacturer in the UK. They worked fine, but had no NMEA 0183 or 2000 output, and obviously didn't show even uncorrected wind angle. For a long time I was a bit of a Luddite with respect to sailing instruments. I thought not having instruments helped me "get my head out of the boat". I finally got to the point where I felt I was sailing my boat as fast as I could and needed feedback to improve. This was reinforced on the first night of the Race to Alaska in 2019, when the wind was light and shifty. Since boat speed on a multihull tends to be close to true wind speed (for a "cruising" trimaran like my Farrier F-32; racing multis usually sail much faster than TWS), it feels like the boat is almost always sailing upwind. With TWA up to 90 degrees, you're sailing upwind, and if you're any good at it, TWA of 90 degrees feels like AWA of 25-35 degrees. This means that it's easy to fall off the wind, especially at night, because boat speed goes down but AWA doesn't change. So it's hard to tell if you're dealing with a wind shift or if you just aren't pointing high enough. My friend Mark had installed glow-in-the-dark yarn on the jib luff for telltales, and he's so hardcore that the first night sailing past Nanaimo, he kept running up to the bow to shine a UV flashlight on the telltales to get them glowing. They would last for 5 minutes, then we would "miss" the wind and start the whole process over again. Luffing is obvious, even at night. But long before you luff a trimaran, you're sailing far slower than you should be. And bearing away is even worse, since stalling slows you down without any visual or audio indication that you're off the wind. That's when I realized I needed a wind instrument.

[^3]: Or, DM me and I will build you an RTK N2K compass for cost.

[^4]: As of now, the UM982 is available for around $100 in a module with serial output (meaning it can connect directly to your computer or microcontroller). Its heading accuracy is 0.1 degrees per meter of baseline, which means with antennas mounted on the deck of my 32-foot trimaran (around 2 meters apart), expected heading error is about 0.05 degrees RMS. Compare to a $900 B&G Precision 9 that gives ±2°. I'm not trying to dump on B&G, or any other traditional manufacturer, but RTK is an example of technology making amazing strides and simultaneously getting amazingly cheaper. And it's not just RTK. You can now replicate every single feature of a "sailing processor" or MFD that costs several thousand dollars, using a Raspberry Pi ($50) and free software. And in most cases, the free version works far better than the proprietary version, because a) the underlying tech has advanced far more than the proprietary tech and b) hundreds of talented people are donating a bit of their time to make it better. For example, [here](https://www.bandg.com/blog/calibrating-an-h5000-instrument-system/) is a link to B&G calibration instructions for wind, and they helpfully summarize the method: "If you are lifted from tack to tack, subtract half the difference. If headed from tack to tack, add half the difference." In the technology field, this is known as a [SWAG](https://en.wikipedia.org/wiki/Scientific_wild-ass_guess). Again, I have nothing against B&G. They have many talented engineers who have spent thousands of hours developing fine products. The problem is that they are working with technology that's fundamentally obsolete, and there isn't much they can do for now. Imagine how accurate calculations of leeway, upwash, downwash, and the impact of pointing higher or lower could be if you had a gadget that could measure roll, pitch, and yaw with an accuracy of 0.05° instead of ±2°?