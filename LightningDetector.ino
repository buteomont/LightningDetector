/*
 * LightningDetector.c
 *
 *  Created on: May 9, 2014
 *  Updated on: July 20, 2019
 *      Author: David E. Powell
 *
 *  --------Hardware and Software Enhancements----------
 *  - Move strike log to EEPROM
 *  - Add button/REST command to clear strike log
 *  - Read NIST time on boot and use clock time in log instead of elapsed time
 *  - Configure static or DHCP address
 *
 */
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "Arduino.h"
#include <pgmspace.h>
#include <ctype.h>
#include <ESP_EEPROM.h>
#include <stdio.h>
#include <Esp.h>
#include <TimeLib.h>
#include "LightningDetector.h"
#include "configpage.h"

#ifndef AP_SSID
  #define AP_SSID "lightning!"
  #define AP_PASS ""
  #define MY_MDNS "lightning" 
  #endif

IPAddress local_IP(192,168,1,1);
IPAddress gateway(192,168,1,254);
IPAddress subnet(255,255,255,0);

//char ssid[SSID_SIZE] = AP_SSID;
//char password[PASSWORD_SIZE] = AP_PASS;
//char myMDNS[MDNS_SIZE]=MY_MDNS;

const char *htmlCr="<br>";
const char *textCr="\n\r";

int lastReading = 0;
int thisReading = 0;
int errval=128; //half way
int resetCounter=0; //keeps the LED lit long enough to see it when a strike happens
int resetDelay=100;  //this is how many milliseconds (approx) the LED stays lit after lightning is seen
//int sensitivity=10;  //successive readings have to be at least this much larger than the last reading to register as a strike
unsigned long strikes[MAX_STRIKES];       //strike time log
unsigned int strikeIntensity[MAX_STRIKES];//strike brightness log
int strikeCount=0;  //number of strikes in the log
boolean done=false;
int strikeStage=0;   //to keep track of which part of the strike we are working
long strikeWaitTime; //to keep from having to use delay()
int intensity;       //temporary storage to record the intensity of the strike for the log
boolean configured=false;  //don't look for lightning until we are set up
time_t tod=0;         //time of day

IPAddress timeServer(132,163,96,2);  //time.nist.gov

ESP8266WebServer server(80);
WiFiUDP Udp; //for getting time from NIST

unsigned int localPort = 8888;  // local port to listen for UDP packets
const int timeZone = -4;  // Eastern Daylight Time (USA)

typedef struct 
  {
  boolean valid=false; //***** This must remain the first item in this structure! *******
  char ssid[SSID_SIZE] = AP_SSID;
  char password[PASSWORD_SIZE] = AP_PASS;
  char myMDNS[MDNS_SIZE]=MY_MDNS;
  int sensitivity=10;  //successive readings have to be at least this much larger than the last reading to register as a strike
  boolean useStatic=false;
  int statIP[4]={0,0,0,0};
  int statGW[4]={0,0,0,0};
  } conf;

conf settings; //all settings in one struct makes it easier to store in EEPROM

void setup()
  {
  //Set up the serial communications
  Serial.begin(115200); 
  Serial.setTimeout(10000);
  Serial.println();
  delay(500);
  Serial.println("Starting up...");
 
  //initialze the outputs
  pinMode(LIGHTNING_LED_PIN, OUTPUT);  //we'll use the led to indicate detection
  digitalWrite(LIGHTNING_LED_PIN,HIGH); //High is off on the dorkboard
  pinMode(BLUE_LED_PIN, OUTPUT);  //we'll use this led to indicate when we're looking for server connections
  digitalWrite(BLUE_LED_PIN,HIGH); //
  pinMode(RELAY_PIN, OUTPUT);  //set up the relay
  digitalWrite(RELAY_PIN,LOW); //off

  EEPROM.begin(512); //fire up the eeprom section of flash

  //look for a double-click on the reset button.  If reset is clicked, then clicked
  //again while the LED is on, then do a factory reset. We detect this by reading the
  //"configured" value from EEPROM and setting it to false, turning on the LED, then 
  //turning the LED off and writing the value back to EEPROM two seconds later.
  boolean conf;
  boolean noConf=false;
  EEPROM.get(0,conf);                 //save original value
  EEPROM.put(0,noConf);               //write "false" for non-configured
  EEPROM.commit();                    //do the write
  digitalWrite(LIGHTNING_LED_PIN,LOW);//Turn on the LED
  delay(2000);                        //wait a sec
  digitalWrite(LIGHTNING_LED_PIN,HIGH);//Turn the LED back off
  EEPROM.put(0,conf);                 //If we are here then no double-click, write the original back
  EEPROM.commit();                    //do the write

  loadSettings(); //load the editable settings from eeprom

  //initialize variables
  
  resetCounter=0;
  thisReading=analogRead(PHOTO_PIN);
  lastReading=thisReading;
  
  //Get the WiFi going
  if (strcmp(AP_SSID,settings.ssid)==0) //using the default SSID?  If so then open the configure page
    {
    Serial.print("Configuring soft access point. SSID is \""+String(settings.ssid)+"\". ");
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(settings.ssid, settings.password);
  
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("IP address is ");
    Serial.println(myIP);
    }
  else
    {
    configured=true;  //we are configured
    WiFi.disconnect();
    WiFi.hostname(settings.myMDNS);
    if (settings.useStatic)
      {
      String adr=String(settings.statIP[0])+"."+settings.statIP[1]+"."+settings.statIP[2]+"."+settings.statIP[3];
      Serial.println("Using static addressing: "+adr);
      IPAddress staticIP(settings.statIP[0],settings.statIP[1],settings.statIP[2],settings.statIP[3]); //ESP static ip
      IPAddress gateway(settings.statGW[0],settings.statGW[1],settings.statGW[2],settings.statGW[3]);   //IP Address of your WiFi Router (Gateway)
      IPAddress subnet(255, 255, 255, 0);  //Subnet mask
      WiFi.config(staticIP, subnet, gateway);
      }
      
    WiFi.mode(WIFI_STA);
    Serial.println("Connecting to "+String(settings.ssid)+" with password "+String(settings.password));
    WiFi.begin(settings.ssid, settings.password);
    Serial.print("Connecting");
  
    // Wait for connection
    int waitCount=100;
    while (WiFi.status() != WL_CONNECTED) 
      {
      delay(500);
      Serial.print(".");
      if (--waitCount<=0)
        {
        Serial.println("\n\nTimeout waiting to connect, resetting.");
        ESP.restart();
        }
      }
  
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(settings.ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());       
    }

  if (MDNS.begin(settings.myMDNS)) {
    Serial.print("MDNS responder started as ");
    Serial.print(settings.myMDNS);
    Serial.println(".local");
  }

  ArduinoOTA.onStart([]() 
    {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) 
      {
      type = "sketch";
      }
    else 
      { // U_SPIFFS
      type = "filesystem";
      }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
    });
  ArduinoOTA.onEnd([]() {Serial.println("\nEnd");});
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) 
    {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
  ArduinoOTA.onError([](ota_error_t error) 
    {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) 
      {
      Serial.println("Auth Failed");
      }
    else if (error == OTA_BEGIN_ERROR) 
      {
      Serial.println("Begin Failed");
      }
    else if (error == OTA_CONNECT_ERROR) 
      {
      Serial.println("Connect Failed");
      }
    else if (error == OTA_RECEIVE_ERROR) 
      {
      Serial.println("Receive Failed");
      }
    else if (error == OTA_END_ERROR) 
      {
      Serial.println("End Failed");
      }
    });
  ArduinoOTA.begin();


  server.on("/", documentRoot);
  server.on("/index.html", documentRoot);
  server.on("/json", sendJson);
  server.on("/text", sendText);
  server.on("/configure", setConfig);
//  server.on("/reset",factoryReset);
  server.on("/clear", clearLog);
  server.on("/bad", []() 
    {
    Serial.println("Received request for bad data");
    server.send(200, "text/plain", "This is a bad URL. Don't ever do that again.");
    Serial.println("Response sent for bad data");
    });
  server.onNotFound(pageNotFound);
  server.begin();
  Serial.println("HTTP server started");
  strikeWaitTime=millis(); //initialize the delay counter

  if (configured)
    {
    Udp.begin(localPort);
    tod=getNtpTime(); //initialize the TOD clock
    setTime(tod);
    }
    
  Serial.print("millis() is ");
  Serial.println(millis());
  Serial.print("now() is ");
  Serial.println(now());
  
//  Serial.print("The time is ");
//  if (tod>0)
//    {
//    char timebuf[25];
//    Serial.println(displayTime(tod,timebuf," "));
//    }
//  else
//    Serial.println("unknown.");

//  Serial.print("setup() running on core ");
//  Serial.println(xPortGetCoreID());

  //Ready to loop
  Serial.println(getConfiguration(TYPE_TEXT));
  Serial.println(getStatus(TYPE_TEXT));
  Serial.println(getStrikes(TYPE_TEXT));

  Serial.println("Ready.");
  Serial.println("");
  }

void loop()
  {
  server.handleClient();
  MDNS.update();
  ArduinoOTA.handle();

  //Don't read the sensor too fast or else it will disconnect the wifi
  if(!configured || millis()%10 != 0)
    return;
  else
    {
    checkForStrike();
    }
  }

//format the time to be human-readable
char* displayTime(time_t mytime, char* buf, String separator)
  {
  // digital clock display of the time
  String tmplate="%d/%d/%d"+separator+"%d:%02d:%02d";
  sprintf(buf,tmplate.c_str(), month(mytime),
                        day(mytime),
                        year(mytime),
                        hour(mytime),
                        minute(mytime),
                        second(mytime)); 
  return buf;
  }



/*-------- NTP code ----------*/

byte packetBuffer[NTP_PACKET_SIZE]; // NTP time is in the first 48 bytes of message

time_t getNtpTime()
  {
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 5000) 
    {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) 
      {
      Serial.println("Received NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      }
    if ((millis()-beginWait) % 1000==0) //re-request the time every second until we get it
      {
      Serial.println("Resending NTP Request");
      sendNTPpacket(timeServer);
      }
    }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
  }

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
  {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
  }


char* getConfigPage(String message)
  {
  char temp[11];
  sprintf_P(configBuf,(PGM_P)FPSTR(configPage),
              message.c_str(),
              settings.ssid,
              settings.password,
              settings.myMDNS,
              settings.useStatic?" checked":"",
              settings.statIP[0],
              settings.statIP[1],
              settings.statIP[2],
              settings.statIP[3],
              settings.statGW[0],
              settings.statGW[1],
              settings.statGW[2],
              settings.statGW[3],
              settings.sensitivity);
  return configBuf;
  }

void clearLog()
  {
  Serial.println("Received clear log request");
  Serial.println("POST data: "+server.arg("plain"));
  char res[]="CLEAR";
  if (server.hasArg("plain")== false
    ||strcmp(res,server.arg("clear_log").c_str())!=0) 
    {
    server.send(400, "text/html", "Request method must be POST with clear_log=true"); //not a POST request
    Serial.println("Not a POST or invalid request, failed.");
    }
  else
    {
    strikeCount=0;  //reset the pointer to the log head
    Serial.println("Log cleared.");
    documentRoot();
    }
  }
  
//void factoryReset()
//  {
//  Serial.println("Received factory reset request");
//  Serial.println("POST data: "+server.arg("plain"));
//  
//  char res[]="true";
//  if (server.hasArg("plain")== false
//    ||strcmp(res,server.arg("factory_reset").c_str())!=0 ) 
//    {
//    server.send(200, "text/html", "Request method must be POST with factory_reset=true" ); //not a POST request
//    Serial.println("Not a POST, failed.");
//    }
//  else
//    {
//    settings.valid=false;
//    saveSettings();
//    server.sendHeader("Location", String("http://lightning.local/"), true);
//    server.send(303, "text/plain", ""); //gonna need to configure it again
//    delay(500);
//    ESP.restart();   
//    }
//  }

void setConfig()
  {
  Serial.println("Received set config request");
  if (server.hasArg("plain")== false) //Check if body received
    {
    server.send(200, "text/html", getConfigPage(""));
    Serial.println("Configure page sent.");
    }
  else
    {
    String message="Configuration Updated"; //if all goes well
    Serial.println("POST data: "+server.arg("plain"));

    String new_ssid=server.arg("ssid");
    String new_password=server.arg("pword");
    String new_myMDNS=server.arg("mdns");
    int new_sensitivity=atoi(server.arg("sensitivity").c_str());
    boolean new_useStatic=strcmp(server.arg("Static").c_str(),"true")==0;

    int new_statIP0=atoi(server.arg("addrOctet0").c_str());
    int new_statIP1=atoi(server.arg("addrOctet1").c_str());
    int new_statIP2=atoi(server.arg("addrOctet2").c_str());
    int new_statIP3=atoi(server.arg("addrOctet3").c_str());
    
    int new_statGW0=atoi(server.arg("gwOctet0").c_str());
    int new_statGW1=atoi(server.arg("gwOctet1").c_str());
    int new_statGW2=atoi(server.arg("gwOctet2").c_str());
    int new_statGW3=atoi(server.arg("gwOctet3").c_str());

    if (strcmp(settings.myMDNS,new_myMDNS.c_str())==0
      &&strcmp(settings.ssid,new_ssid.c_str())==0
      &&strcmp(settings.password,new_password.c_str())==0
      &&settings.useStatic==new_useStatic
      && settings.statIP[0]==new_statIP0
      && settings.statIP[1]==new_statIP1
      && settings.statIP[2]==new_statIP2
      && settings.statIP[3]==new_statIP3
      && settings.statGW[0]==new_statGW0 
      && settings.statGW[1]==new_statGW1 
      && settings.statGW[2]==new_statGW2 
      && settings.statGW[3]==new_statGW3 
      &&settings.sensitivity==new_sensitivity)
      {
      server.sendHeader("Location", String("/"), true);
      server.send(303, "text/plain", ""); //send them back to the configuration page
      }
    else 
      {
      boolean needsReboot=strcmp(settings.myMDNS,new_myMDNS.c_str())!=0
                        ||strcmp(settings.ssid,new_ssid.c_str())!=0
                        ||settings.useStatic!=new_useStatic
                        ||settings.statIP[0]!=new_statIP0
                        ||settings.statIP[1]!=new_statIP1
                        ||settings.statIP[2]!=new_statIP2
                        ||settings.statIP[3]!=new_statIP3
                        ||settings.statGW[0]!=new_statGW0 
                        ||settings.statGW[1]!=new_statGW1 
                        ||settings.statGW[2]!=new_statGW2 
                        ||settings.statGW[3]!=new_statGW3;

      settings.sensitivity=new_sensitivity;
      strcpy(settings.myMDNS,new_myMDNS.c_str());
      strcpy(settings.ssid,new_ssid.c_str());
      strcpy(settings.password,new_password.c_str());
      settings.useStatic=new_useStatic;
      if (settings.useStatic)
        {
        settings.statIP[0]=new_statIP0;
        settings.statIP[1]=new_statIP1;
        settings.statIP[2]=new_statIP2;
        settings.statIP[3]=new_statIP3;
        settings.statGW[0]=new_statGW0;
        settings.statGW[1]=new_statGW1;
        settings.statGW[2]=new_statGW2;
        settings.statGW[3]=new_statGW3;
        }
      settings.valid=true;
  
      //perform the actual write to eeprom
      if (!saveSettings())
        {
        Serial.println("Storing to eeprom failed!");
        message="Could not store configuration values!";
        server.send(200, "text/html", getConfigPage(message)); //send them back to the configuration page
        }
      else
        {
        Serial.println("Configuration stored to EEPROM");
        if (needsReboot)
          {
          server.sendHeader("Location", String("http://"+server.arg("mdns")+".local/"), true);
          server.send(303, "text/plain", ""); //send them to the factory reset page
          delay(1000);
          ESP.restart();
          }
        else
          {
          server.sendHeader("Location", String("/"), true);
          server.send(303, "text/plain", ""); //send them to the main page
          }
        }
      }
    }
  }

void pageNotFound() 
  {
  Serial.println("Received invalid URL");
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) 
    {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }

  server.send(404, "text/plain", message);
  Serial.println(message);
  Serial.println("Code 404 response sent.");
  }

void sendText()
  {
  Serial.println("Received request for text data");

  server.send(200, "text/plain","---Status---\n"
                                +getStatus(TYPE_TEXT)
                                +"\n---Configuration---\n"
                                +getConfiguration(TYPE_TEXT)
                                +"\n---Strikes---\n"
                                +getStrikes(TYPE_TEXT)
                                );
  
  Serial.println("Response sent for text data");
  };


void sendJson()
  {
  Serial.println("Received request for JSON data");
  
  server.send(200, "application/json","{\"Status\":{"
                                +getStatus(TYPE_JSON)
                                +"},\"Configuration\":{"
                                +getConfiguration(TYPE_JSON)
                                +"},\"Strikes\":{"
                                +getStrikes(TYPE_JSON)
                                +"}");

  Serial.println("Response sent for JSON data");
  }

void documentRoot() 
  {
  Serial.println("Received request for document root");
  if (!configured)
    {
    Serial.println("Detector not configured. Redirecting to configuration page.");
    setConfig();
    return;
    }
  String pageStart="<html>\
    <head>\
      <meta http-equiv='refresh' content='15; url=/'/>\
      <title>Lightning Detector</title>\
      <style>\
        body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
        table {border: 1px solid black;}\
        th, td {border: 1px solid black; padding: 0px 10px 0px 10px;}\
      </style>\
    </head>\
    <body>\
      <div style=\"text-align: center;\"><h1>Optical Lightning Logger</h1></div>\
      <p><h2>Status</h2>"
      +getStatus(TYPE_HTML)
      +"<h2><a href=\"/configure\">Configuration</a></h2>"
      +getConfiguration(TYPE_HTML)
      +"<h2>Strike Data:</h2>";
      
      
  String pageEnd="</p>\
    </body>\
  </html>";

  String strikes=getStrikes(TYPE_HTML);

  int len=pageStart.length()+strikes.length()+pageEnd.length();
  
  server.setContentLength(len);
  server.send(200, "text/html", pageStart);
  server.sendContent(strikes);
  server.sendContent(pageEnd);
  Serial.println("Response sent for document root");
  }

/*
 * Check to see if we are in a strike.  There are four "zones": 
 * 0 - No strike detected
 * 1 - Increase in brightness above a threshold
 * 2 - Maintain that brightness for a set period
 * 3 - Drop the brightness before a set time has passed
 */
void checkForStrike()
  {
  switch(strikeStage)
    {
    case 0:
      readSensor(); //take a reading
      if (thisReading > lastReading+settings.sensitivity) //Is this reading brighter than the last one?
        {
        intensity=thisReading-lastReading; //save the brightness
        strikeStage++;   //may be a real strike, go to stage 1
        strikeWaitTime=millis()+MIN_STRIKE_WIDTH;
        Serial.print("Strike going to stage 1 - ");
        Serial.print(lastReading);
        Serial.print("/");
        Serial.println(thisReading);
        }
      else
        noStrike();
      break;
      
    case 1:
      //make sure it's really a strike.  Wait a bit and look again       
      if (millis()>=strikeWaitTime) 
        {
        readSensor();
        if (thisReading > lastReading+settings.sensitivity) //still going?
          {
          strikeStage++;   //looking like a real strike, go to stage 2
          strikeWaitTime=millis()+MAX_STRIKE_WIDTH;
          Serial.print("Strike going to stage 2 - ");
          Serial.print(lastReading);
          Serial.print("/");
          Serial.println(thisReading);
          intensity=max(intensity,thisReading-lastReading); //save the brightness if brighter
          }
        else 
          noStrike(); // nope, too long
        }
        break;

      case 2:
        if (millis()>=strikeWaitTime) 
          {
          readSensor();
          if (thisReading <= lastReading+settings.sensitivity) //should not still be going, maybe someone turned on the light?
            {
            strikeStage++;   //Take us to default case until this strike is over
            Serial.print("********** Strike confirmed! - ");
            Serial.println(intensity);
            strike(intensity);  //yep, record it    
            }
          else 
            noStrike(); // nope, too long
          }
          break;
          
    default:
      noStrike(); // nope, too long
    }
  }
  

// Record a strike
void strike(unsigned int brightness)
  {
  digitalWrite(LIGHTNING_LED_PIN, LOW);//turn on the blue LED
  digitalWrite(RELAY_PIN, HIGH);    //turn on the relay
  resetCounter=resetDelay;        //"debounce" the strike
  int index=strikeCount++ % MAX_STRIKES;
  strikes[index]=now();  //record the time of this strike
  strikeIntensity[index]=brightness;
  }

//Not a strike this time
void noStrike()
  {
  if (resetCounter>0)
    {
    resetCounter--;
    delay(1);
    }
  else //turn off the LED and relay
    {
    digitalWrite(LIGHTNING_LED_PIN, HIGH);
    digitalWrite(RELAY_PIN, LOW);
    lastReading=thisReading;
    if (strikeStage>0)
      Serial.println("Strike resetting to stage 0");
    strikeStage=0;   //Reset for the next strike
    }
  }
  
/*
 * Convert a long that represents age in milliseconds to a readable string
 */
String msToAge(long age)
  {
  char temp[11];
  int sec=age/1000;
  int min=sec/60;
  int hr=min/60;
  int day=hr/24;
  hr=hr%24;
  min=min%60;
  sec=sec%60;
  
  String msg="";
  if (day<10)
    msg+="0";
  msg+=itoa(day,temp,10);
  msg+=":";
  if (hr<10)
    msg+="0";
  msg+=itoa(hr,temp,10);
  msg+=":";
  if (min<10)
    msg+="0";
  msg+=itoa(min,temp,10);
  msg+=":";
  if (sec<10)
    msg+="0";
  msg+=itoa(sec,temp,10);
  return msg;
  }


/*
 * Format a name value pair
 */
String fmt(String name, String value, int type, boolean lastOne)
  {
  String result;
  String quote=(strncmp(value.c_str(), "[", 1)==0)?"":"\""; //don't quote if it's an array
  switch(type)
    {
    case TYPE_TEXT:
      result=name+": "+value+"\n";
      break;
    case TYPE_HTML:
      result=name+": "+value+"<br>";
      break;
    case TYPE_TABLE: //later
      result=name+"</td><td>"+value+"</td></tr>";
      break;
    case TYPE_JSON:
      result="\""+name+"\""+": "+quote+value+quote+(lastOne?"":",");
      break;
    default:
      result=name+": "+value+"\n";
      break;
    }
    return result;
  }

String getStatus(int type)
  {
  char temp[11];
  String cret=type==TYPE_HTML?"<br>":type==TYPE_JSON?"":"\n";
  char timebuf[25];

  String addr=WiFi.localIP().toString()+(settings.useStatic?" (static)":"");
  String msg=fmt("Address",addr,type,false);
  msg+=fmt("Time",displayTime(now(), timebuf, " "),type,false);
  msg+=fmt("Uptime",msToAge(millis()),type,false);
  msg+=fmt("Illumination",itoa(thisReading,temp,10),type,true);
  return msg;
  }


/*
* Return a string with all relevant variables.
* Type is 1 for text, 2 for HTML, 3 for JSON
*/
String getConfiguration(int type)
  {
  char temp[11];
  String cret=type==TYPE_HTML?"<br>":type==TYPE_JSON?"":"\n";
  String h2open=type==TYPE_HTML?"<h2>":"";
  String h2close=type==TYPE_HTML?"</h2>":"";

  String msg=fmt("SSID",String(settings.ssid),type,false);
  msg+=fmt("mDNS",String(settings.myMDNS)+".local",type,false);
  msg+=fmt("Sensitivity",itoa(settings.sensitivity,temp,10),type,true);
  return msg;
  }

  
String getStrikes(int type)
  {
  char temp[11];
  String cret=type==TYPE_HTML?"<br>":type==TYPE_JSON?"":"\n";
  String h2open=type==TYPE_HTML?"<h2>":"";
  String h2close=type==TYPE_HTML?"</h2>":"";
  
  String msg=fmt("Past hour",itoa(getRecent(HOUR_MILLISECS/1000),temp,10),type,false);
  msg+=fmt("Past 24 hours",itoa(getRecent(DAY_MILLISECS/1000),temp,10),type,false);
  msg+=fmt("Past 7 days",itoa(getRecent((DAY_MILLISECS/1000)*7),temp,10),type,false);
  msg+=fmt("Past 30 days",itoa(getRecent((DAY_MILLISECS/1000)*30L),temp,10),type,false);
  msg+=fmt("Total Strikes",itoa(strikeCount,temp,10),type,false);
  
  String lbr=type==TYPE_JSON?"[":type==TYPE_HTML?"":"\n"; //left bracket
  String rbr=type==TYPE_JSON?"]":""; //you know
  if (type==TYPE_HTML)
    {
    msg+=h2open+"<span style=\"display: inline;\">Strike Log ("+String(MAX_STRIKES)+" max): ";
    msg+="<form method=\"POST\" action=\"/clear\"><input type=\"submit\" name=\"clear_log\" value=\"CLEAR\"></form>";
    msg+="</span>"+h2close;
    msg+="<table><tr><th>#</th><th>Date</th><th>Time</th><th>Intensity</th></tr>";
    msg+=getStrikeLog(TYPE_TABLE);
    msg+="</table>";
    }
  else
    msg+=fmt(cret+h2open+"Strike Log"+h2close,lbr+getStrikeLog(type)+rbr,type,true);
  msg+=type==TYPE_JSON?"}":"";
  return msg;
  }


//Returns the strike log
String getStrikeLog(int type)
  {
  char temp[11];
  char timebuf[25];
  String msg="";
  boolean lastEntry=false;
  int wholeBuf=MAX_STRIKES;
  
  for (int i=strikeCount-1;i>=0;i--)
    {
    lastEntry=(i==0);
    int j=i % MAX_STRIKES; 
     
    if (type==TYPE_JSON)
      {
      msg+="{"+fmt(itoa(strikes[j],temp,10),itoa(strikeIntensity[j],temp,10),type,true)+"}";
      msg+=(lastEntry?"":",");
      }
    else if (type==TYPE_TABLE)
      {
      msg+="<tr><td>";
      msg+=itoa(i+1,temp,10);
      msg+="</td><td>";
      msg+=fmt(displayTime(strikes[j],timebuf,"</td><td>"),itoa(strikeIntensity[j],temp,10),type,lastEntry);
      }
    else
      {
      msg+=itoa(i+1,temp,10);
      msg+=" - ";
      msg+=fmt(displayTime(strikes[j],timebuf," "),itoa(strikeIntensity[j],temp,10),type,lastEntry);
      }
    if (--wholeBuf<=0)
      break;
    } 
  return msg;
  }



/*
* Return the number of strikes recorded in the past (age) seconds
*/
int getRecent(unsigned long age)
  {
  int count=0;  

  //get the run time and subtract age, but don't go below 0
  unsigned long oldest=now();
  if (oldest>age)  
    oldest-=age;
  else
    oldest=0;
    
  for (int i=strikeCount-1;i>=0;i--)
    {
    if (strikes[i]>oldest)
      count++;
    }
  return count;
  }
  
/*
* Reset the strike counters 
*/
void reset_totals()
  {
  for (strikeCount=MAX_STRIKES;strikeCount>=0;strikeCount--)
    {
    strikes[strikeCount]=0;  
    }
  strikeCount=0;
  }

boolean saveSettings()
  {
  EEPROM.put(0,settings);
  return EEPROM.commit();
  }


/*
*  Initialize the settings from eeprom
*/
void loadSettings()
  {
  EEPROM.get(0,settings);
  if (settings.valid)    //skip loading stuff if it's never been written
    {
    Serial.println("Loaded configuration values from EEPROM");
    }
  else
    {
    Serial.println("Skipping load from EEPROM, device not configured.");
    strcpy(settings.ssid,AP_SSID);
    strcpy(settings.password,AP_PASS);
    strcpy(settings.myMDNS,MY_MDNS);
    settings.sensitivity=10;  //successive readings have to be at least this much larger than the last reading to register as a strike
    settings.useStatic=false;
    settings.statIP[0]=0;
    settings.statIP[1]=0;
    settings.statIP[2]=0;
    settings.statIP[3]=0;
    settings.statGW[0]=0;
    settings.statGW[1]=0;
    settings.statGW[2]=0;
    settings.statGW[3]=0;
    }
  }


//Read the solar cell.  We take the average of several readings because the ADC on 
//the ESP8266 is real jumpy.  It stores the value in the global thisReading variable.
int readSensor()
  {
  thisReading=analogRead(PHOTO_PIN);
  return thisReading;
  }

    
// given a PROGMEM string, use Serial.print() to send it out
void printProgStr(const char str[])
  {
  char c;
  if(!str) return;
  while((c = pgm_read_byte(str++)))
    Serial.print(c);
  }
