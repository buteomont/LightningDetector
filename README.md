# LightningDetector  
## An electronic optical lightning detector and strike logger

This device optically detects nearby lightning strikes, records them, and (optionally) energizes a relay for a specified period after each strike.  This version is based on the ESP8266 WEMOS board, but can be adapted to other processors as well.  Firmware development was done in the Arduino IDE.

### Hardware
The hardware is dead simple, as most of the heavy lifting is done by the ESP board and its libraries. An inexpensive solar cell is used as an optical sensor. This solar cell and the WEMOS board is really all that is needed to make a functioning lightning logger.  The output from the solar cell is fed to the processor's ADC (analog-to-digital converter) directly. An optional control circuit (not shown) can be added to energize a relay for a predetermined amount of time whenever lightning is detected.  The circuit can be battery powered, with the strike detector doing double-duty to charge the battery (but don't expect the battery to stay charged unless you are using a _huge_ solar cell).  Thanks to the small size of the WEMOS circuit board, the entire detector will fit inside the head of one of those solar path lights that can be purchased from Dollar Tree, with the solar cell already mounted!  I have included a 3D printer STL file that makes a mount to hold the Wemos board and sounder inside one of these lights. 

![Circuit Schematic](https://github.com/buteomont/LightningDetector/blob/master/lightningDetectorV4.0.png "Schematic")

### Firmware
The program to drive the lightning detector allows you to view or download the strike log and adjust the operating parameters. It operates as a basic web server, while concurrently watching for any lightning in the area.  When a lightning strike is detected, it will record the time and the intensity in the strike log.   

When powered up for the first time, the 8266 will go into "AP mode", since it doesn't know about any wifi routers in the area.  In AP mode, the ESP8266 *is* the router, with an SSID of "**_lightning!_**".  You will need to connect to that "router" and browse to _http://lightning.local_ to configure it.  Enter your household wifi SSID and password and press the _Update_ button.  You can also change the mDNS domain name and sensitivity setting.   

If you enter the SSID or password incorrectly, the lightning detector will never connect to your LAN, and also will not enter AP mode so you can change it!  The way around this is to press the _reset_ button on the WEMOS board and watch for the LED to illuminate.  When it does, press the _reset_ button again.  This will wipe the configuration and cause it to enter AP mode again.  

To reduce false detections, the program will ensure that the length of the strike is above a certain amplitude and within a specific duration window, no shorter and no longer. That window is from 1 to 200 milliseconds and may need to change if too many false positives are recorded or strikes are missed.  The amplitude threshold (sensitivity) of the detector is adjustable on the _/config_ page of the detector's web server.  I've found it's easy to find the right value by putting the detector into place and setting the sensitivity to 1 (maximum sensitivity), and letting it run for a while.  Then noting the values in the strike log makes it easy to adjust the sensitivity to just above the highest value.   

The optional sounder (buzzer) can be enabled or disabled.  The pitch of the beep it produces can be adjusted within a limited range, but be aware that most sounders have a narrow frequency band where they are resonant, so adjusting the pitch will likely affect the volume as well. 

There is no security implemented at this time, so anyone with access to the _/configure_ page can make changes. This will be problematic if you decide to make the logger world-accessible. I purposely did not include a link to the _/configure_ page from the main status page for this reason.  I have plans to add tighter security for this.

The firmware is fully OTA updatable. I would like to see any changes or improvements you come up with!

### Interface
There are several web endpoints that can be accessed when the detector is running and connected to the LAN:
* / or /index.html - The main status page
* /configure - The configuration page for WiFi, LAN, sensitivity, and other changes
* /text - The status and log information in plain text form
* /json - The status and log information in standard machine-readable JSON form

### Planned Enhancements
Some of the things I'd like to add when I get the time are
* Security features to require a userid and password when modifying the settings
* The ability to push the strike event to an external service, email, or SMS account
* More flexibility in configuring the internal parameters
* The ability to remove individual unwanted or error events from the strike log
* Automatically remove strike events from the log that are older than 30 days
* Store the strike log in EEPROM to make it non-volatile

