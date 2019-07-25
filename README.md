# LightningDetector  
## An electronic optical lightning detector and strike counter

This device optically detects nearby lightning strikes, records them, and (optionally) energizes a relay for a specified period after each strike.  This version is based on the ESP-12E ESP8266 development kit board, but can be adapted to other processors as well.  Firmware development was done in the Arduino IDE.

### Hardware
The hardware is simple and straightforward, as most of the heavy lifting is done by the ESP board and its libraries. An inexpensive solar cell is used both as an optical sensor and as a power source for charging the batteries.  The output from the solar cell is fed to the processor's ADC (analog-to-digital converter) via an ambient light compensation circuit.  The compensation circuit is continuously adjusted to counterbalance the effects of the ambient light, making the circuit useful for both day and night use.  

![Circuit Schematic](https://github.com/buteomont/LightningDetector/blob/master/lightningDetectorV4.0.png "Schematic")

### Firmware
The firmware 
