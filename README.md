# LightningDetector  
## An electronic optical lightning detector and strike counter

This device optically detects nearby lightning strikes, records them, and (optionally) energizes a relay for a specified period after each strike.  This version is based on the ESP-12E ESP8266 development kit board, but can be adapted to other processors as well.  Firmware development was done in the Arduino IDE.

### Hardware
The hardware is dead simple, as most of the heavy lifting is done by the ESP board and its libraries. An inexpensive solar cell is used both as an optical sensor and as a power source for charging the batteries.  The output from the solar cell is fed to the processor's ADC (analog-to-digital converter) directly. An optional control circuit can be added to energize a relay for a predetermined amount of time whenever lightning is detected.  

![Circuit Schematic](https://github.com/buteomont/LightningDetector/blob/master/lightningDetectorV4.0.png "Schematic")

### Firmware
The program to drive the lightning detector allows you to view or download the strike log and adjust the operating parameters. It operates as a basic web server, while concurrently watching for any lightning in the area.  When a lightning strike is detected, it will record the relative time and the intensity in the strike log.   
To reduce false detections, the program will ensure that the length of the strike is within a user-definable size, no shorter and no longer.

--more to come--
