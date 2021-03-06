/*
 * LightningDetector.c
 *
 *  Created on: May 9, 2014
 *  Updated on: July 20, 2019
 *      Author: David E. Powell
 *
 *  --------Hardware and Software Enhancements----------
 *  - Move strike log to EEPROM
 *
 */

#include <ArduinoMqttClient.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
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

const char *htmlCr="<br>";
const char *textCr="\n\r";

int lastReading = 0;
int thisReading = 0;
int errval=128; //half way
int resetCounter=0; //keeps the LED lit long enough to see it when a strike happens
int resetDelay=100;  //this is how many milliseconds (approx) the LED stays lit after lightning is seen
unsigned long strikes[MAX_STRIKES];       //strike time log
unsigned int strikeIntensity[MAX_STRIKES];//strike brightness log
int strikeCount=0;  //number of strikes in the log
boolean done=false;
int strikeStage=0;   //to keep track of which part of the strike we are working
long strikeWaitTime; //to keep from having to use delay()
int intensity;       //temporary storage to record the intensity of the strike for the log
boolean configured=false;  //don't look for lightning until we are set up
time_t tod=0;         //time of day
unsigned long ledTime=millis(); //target time to turn off the LED
unsigned long mqttWait=millis(); //used to slow down MQTT connect retries
unsigned long looptime=0;

//IPAddress timeServer(132,163,96,2);  //time.nist.gov
IPAddress timeServer;

ESP8266WebServer server(80);
WiFiUDP Udp; //for getting time from NIST
unsigned int localPort = 8888;  // local port to listen for UDP packets for time query

typedef struct 
  {
  boolean valid=false; //***** This must remain the first item in this structure! *******
  char ssid[SSID_SIZE] = AP_SSID;
  char password[PASSWORD_SIZE] = AP_PASS;
  int statIP[4]={0,0,0,0};
  int statGW[4]={0,0,0,0};
  char myMDNS[MDNS_SIZE]=MY_MDNS;
  int tzOffset=-5;
  int sensitivity=10;  //successive readings have to be at least this much larger than the last reading to register as a strike
  boolean useStatic=false;
  boolean beepOnStrike=true;
  int beepPitch=SOUNDER_PITCH;
  boolean useMqtt=false;
  char mqttBrokerAddress[ADDRESS_SIZE]=AIO_SERVER;
  int mqttBrokerPort=AIO_SERVERPORT;
  char mqttUsername[USERNAME_SIZE]=AIO_USERNAME;
  char mqttPassword[PASSWORD_SIZE]=AIO_KEY;
  char mqttTopic[MQTT_TOPIC_SIZE]=MQTT_TOPIC;
  int dnsServer[4]={0,0,0,0};
  } conf;

conf settings; //all settings in one struct makes it easier to store in EEPROM

// Set up the MQTT client class by passing in the WiFi client and MQTT server and login details.
WiFiClient client;
MqttClient mqttClient(client);

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

  EEPROM.begin(sizeof(settings)); //fire up the eeprom section of flash
  Serial.print("Settings object size=");
  Serial.println(sizeof(settings));

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





//temporary///////////
settings.useMqtt=true;
sprintf(settings.mqttBrokerAddress,"%s",AIO_SERVER);
settings.mqttBrokerPort=AIO_SERVERPORT;
sprintf(settings.mqttUsername,"%s",AIO_USERNAME);
sprintf(settings.mqttPassword,"%s",AIO_KEY);
sprintf(settings.mqttTopic,"%s",MQTT_TOPIC); 
settings.dnsServer[0]=DNS_SERVER0;
settings.dnsServer[1]=DNS_SERVER1;
settings.dnsServer[2]=DNS_SERVER2;
settings.dnsServer[3]=DNS_SERVER3;
EEPROM.put(0,settings);
//////////////////////



  //initialize variables
  
  resetCounter=0;
  thisReading=analogRead(PHOTO_PIN);
  lastReading=thisReading;
  
  //Get the WiFi going
  if (!settings.valid) //Are we already configured?  If not then use AP mode
    {
    Serial.print("Configuring soft access point. SSID is \""+String(settings.ssid)+"\". ");
    WiFi.softAP(AP_SSID);
  
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("IP address is ");
    Serial.println(myIP);
    }
  else
    {
    configured=true;  //we are configured
    
    Serial.println("\n***** Settings ******");
    Serial.print("valid=");Serial.println(settings.valid?"true":"false");
    Serial.print("ssid="); Serial.println(settings.ssid);
    Serial.print("password="); Serial.println(settings.password);
    Serial.print("statIP="); Serial.print(settings.statIP[0]);Serial.print(".");Serial.print(settings.statIP[1]);Serial.print(".");Serial.print(settings.statIP[2]);Serial.print(".");Serial.println(settings.statIP[3]);
    Serial.print("statGW="); Serial.print(settings.statGW[0]);Serial.print(".");Serial.print(settings.statGW[1]);Serial.print(".");Serial.print(settings.statGW[2]);Serial.print(".");Serial.println(settings.statGW[3]);
    Serial.print("myMDNS="); Serial.println(settings.myMDNS);
    Serial.print("tzOffset="); Serial.println(settings.tzOffset);
    Serial.print("sensitivity="); Serial.println(settings.sensitivity);
    Serial.print("useStatic="); Serial.println(settings.useStatic?"true":"false");
    Serial.print("beepOnStrike="); Serial.println(settings.beepOnStrike?"true":"false");
    Serial.print("beepPitch="); Serial.println(settings.beepPitch);
    Serial.print("useMqtt="); Serial.println(settings.useMqtt?"true":"false");
    Serial.print("mqttBrokerAddress="); Serial.println(settings.mqttBrokerAddress);
    Serial.print("mqttBrokerPort="); Serial.println(settings.mqttBrokerPort);
    Serial.print("mqttUsername="); Serial.println(settings.mqttUsername);
    Serial.print("mqttPassword="); Serial.println(settings.mqttPassword);
    Serial.print("mqttTopic="); Serial.println(settings.mqttTopic);
    Serial.print("dnsServer="); Serial.print(settings.dnsServer[0]);Serial.print(".");Serial.print(settings.dnsServer[1]);Serial.print(".");Serial.print(settings.dnsServer[2]);Serial.print(".");Serial.println(settings.dnsServer[3]);
    Serial.println("***** Settings ******\n");


    WiFi.disconnect();
    WiFi.hostname(settings.myMDNS);
    if (settings.useStatic)
      {
      String adr=String(settings.statIP[0])+"."+settings.statIP[1]+"."+settings.statIP[2]+"."+settings.statIP[3];
      Serial.println("Using static addressing: "+adr);
      IPAddress staticIP(settings.statIP[0],settings.statIP[1],settings.statIP[2],settings.statIP[3]); //ESP static ip
      IPAddress gateway(settings.statGW[0],settings.statGW[1],settings.statGW[2],settings.statGW[3]);   //IP Address of your WiFi Router (Gateway)
      IPAddress subnet(255, 255, 255, 0);  //Subnet mask
      IPAddress dns(settings.dnsServer[0],settings.dnsServer[1],settings.dnsServer[2],settings.dnsServer[3]);   //IP Address of the DNS server
      WiFi.config(staticIP, gateway, subnet, dns);
      }
      
    WiFi.mode(WIFI_STA);
    Serial.println("Connecting to "+String(settings.ssid)+" with password "+String(settings.password));
    WiFi.begin(settings.ssid, settings.password);
    Serial.print("Connecting");
  
    // Wait for connection
    int waitCount=150;
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

  Serial.println("\n...Configuring OTA updater");
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

    // Set up the MQTT client class by passing in the WiFi client and MQTT server and login details.
    if (settings.useMqtt)
      {
      Serial.println("Attempting to connect to MQTT broker...");
//      mqttClient.setUsernamePassword(settings.mqttUsername, settings.mqttPassword);
      if (!mqttClient.connect(settings.mqttBrokerAddress, settings.mqttBrokerPort)) 
        {
        Serial.print("MQTT connection failed! Error code = ");
        Serial.println(mqttClient.connectError());
        }
      else
        {
        Serial.println("MQTT connected!");
        }
      }

  Serial.println("\n...Configuring HTTP server");
  server.on("/", documentRoot);
  server.on("/index.html", documentRoot);
  server.on("/json", sendJson);
  server.on("/text", sendText);
  server.on("/configure", setConfig);
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
  Serial.println("\n*** Configuration ***");
  Serial.println(getConfiguration(TYPE_TEXT));
  Serial.println("\n*** Status ***");
  Serial.println(getStatus(TYPE_TEXT));
  Serial.println("\n*** Events ***");
  Serial.println(getStrikes(TYPE_TEXT));

  Serial.println("Ready.");
  Serial.println("");
  }

void loop()
  {
  long lt=millis()-looptime;
  if (lt>3)
    Serial.println("---loop time: "+String(lt)+"milliseconds.");
  looptime=millis();
  server.handleClient();
  MDNS.update();
  ArduinoOTA.handle();

  if (settings.useMqtt)
    {
    if (!mqttClient.connected())
      {
      Serial.println("Attempting to connect to MQTT broker...");
      if (!mqttClient.connect(settings.mqttBrokerAddress, settings.mqttBrokerPort)) 
        {
        Serial.print("MQTT connection failed! Error code = ");
        Serial.println(mqttClient.connectError());
        }
      else
        {
        Serial.println("MQTT connected!");
        }
      }
    else
      {
      mqttClient.poll(); //keep the fire burning
      }
    }
    
  //If setting the clock failed in setup, try again
  if (tod==0 && configured)
    {
    tod=getNtpTime(); //initialize the TOD clock
    if (tod>0)
      setTime(tod);
    }
    
  manageLED(LED_CHECK,0); //turn off the LED if it's time
  
  //Don't read the sensor too fast or else it will disconnect the wifi
  if(!configured || millis()%10 != 0)
    return;
    
  checkForStrike();
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
  
  int hostFound=WiFi.hostByName(NIST_HOST, timeServer);
  if (hostFound!=1)
    {
    Serial.print("Error resolving address of time server ");
    Serial.print(NIST_HOST);
    Serial.print(":");
    Serial.println(hostFound);
    return 0;
    }
  Serial.print("NTP server is at ");
  Serial.println(timeServer);
    
  Serial.print("Transmit NTP Request to ");
  Serial.println(timeServer);
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 16000) 
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
      return secsSince1900 - 2208988800UL + settings.tzOffset * SECS_PER_HOUR;
      }
    if ((millis()-beginWait) % 3000==0) //re-request the time every 3 seconds until we get it
      {
      delay(1); //don't loop more than once per millisecond
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
              VERSION,
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
              settings.tzOffset,
              settings.sensitivity,
              settings.beepOnStrike?" checked":"",
              settings.beepPitch);
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
    server.sendHeader("Location", String("/"), true);
    server.send(303, "text/plain", ""); //send them to the main page
    manageLED(LED_ON,CLEAR_LOG_LED_TIME); //turn on the LED for a bit
    if (settings.beepOnStrike)
      tone(SOUNDER_PIN,settings.beepPitch,CLEAR_LOG_LED_TIME);
    }
  }
  
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
    int new_tzOffset=atoi(server.arg("timezone").c_str());
    boolean new_useStatic=strcmp(server.arg("Static").c_str(),"true")==0;
    boolean new_beepOnStrike=strcmp(server.arg("beep").c_str(),"true")==0;
    int new_beepPitch=atoi(server.arg("beepPitch").c_str());

    int new_statIP0=atoi(server.arg("addrOctet0").c_str());
    int new_statIP1=atoi(server.arg("addrOctet1").c_str());
    int new_statIP2=atoi(server.arg("addrOctet2").c_str());
    int new_statIP3=atoi(server.arg("addrOctet3").c_str());
    
    int new_statGW0=atoi(server.arg("gwOctet0").c_str());
    int new_statGW1=atoi(server.arg("gwOctet1").c_str());
    int new_statGW2=atoi(server.arg("gwOctet2").c_str());
    int new_statGW3=atoi(server.arg("gwOctet3").c_str());

    //see if anything changed.  If not, go back to the main page.
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
      && settings.sensitivity==new_sensitivity
      && settings.tzOffset==new_tzOffset
      && settings.beepOnStrike==new_beepOnStrike
      && settings.beepPitch==new_beepPitch)
      {
      server.sendHeader("Location", String("/"), true);
      server.send(303, "text/plain", ""); //send them back to the main page
      }
    else 
      {
      boolean needsReboot=strcmp(settings.myMDNS,new_myMDNS.c_str())!=0
                        ||strcmp(settings.ssid,new_ssid.c_str())!=0
                        ||settings.useStatic!=new_useStatic
                        ||settings.tzOffset !=new_tzOffset
                        ||settings.statIP[0]!=new_statIP0
                        ||settings.statIP[1]!=new_statIP1
                        ||settings.statIP[2]!=new_statIP2
                        ||settings.statIP[3]!=new_statIP3
                        ||settings.statGW[0]!=new_statGW0 
                        ||settings.statGW[1]!=new_statGW1 
                        ||settings.statGW[2]!=new_statGW2 
                        ||settings.statGW[3]!=new_statGW3;

      settings.sensitivity=new_sensitivity;
      settings.tzOffset=new_tzOffset;
      strcpy(settings.myMDNS,new_myMDNS.c_str());
      strcpy(settings.ssid,new_ssid.c_str());
      strcpy(settings.password,new_password.c_str());
      settings.useStatic=new_useStatic;
      settings.beepOnStrike=new_beepOnStrike;
      settings.beepPitch=new_beepPitch;
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
  String mdms=settings.myMDNS;
  String pageStart="<html>\
    <head>\
      <meta http-equiv='refresh' content='60; url=/'/>\
      <title>"
      +mdms
      +" Lightning Detector</title>\
      <style>\
        body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
        table {border: 1px solid black;}\
        th, td {border: 1px solid black; padding: 0px 10px 0px 10px;}\
      </style>\
    </head>\
    <body>\
      <div style=\"text-align: center;\"><h3>Optical Lightning Logger</h3></div>\
      <p><h4>Status</h4>"
      +getStatus(TYPE_HTML)
      +"<h4>Configuration</h4>"
      +getConfiguration(TYPE_HTML)
      +"<h4>Strike Data:</h4>";
      
      
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
          noStrike(); // nope, not long enough
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
      noStrike(); // not a strike
    }
  }
  

// Record a strike
void strike(unsigned int brightness)
  {
  if (settings.beepOnStrike)
    tone(SOUNDER_PIN,settings.beepPitch,SOUNDER_DURATION);
  manageLED(LED_ON,resetDelay); //turn on the blue LED
  digitalWrite(RELAY_PIN, HIGH);    //turn on the relay
  resetCounter=resetDelay;        //"debounce" the strike
  int index=strikeCount++ % MAX_STRIKES;
  strikes[index]=now();  //record the time of this strike
  strikeIntensity[index]=brightness;
  
  if (settings.useMqtt)
    {
    String payload=String(brightness);
    mqttClient.beginMessage(settings.mqttTopic,payload.length(),false,0,false);
    mqttClient.print(payload);
    mqttClient.endMessage();
    }
  }

//Not a strike this time
void noStrike()
  {
  if (resetCounter>0)
    {
    resetCounter--;
    delay(1);
    }
  else //turn off the relay
    {
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
  String headingOpen=type==TYPE_HTML?"<h4>":"";
  String headingClose=type==TYPE_HTML?"</h4>":"";

  String hz=" Hz";
  String msg=fmt("SSID",String(settings.ssid),type,false);
  msg+=fmt("mDNS",String(settings.myMDNS)+".local",type,false);
  msg+=fmt("GMT Offset",itoa(settings.tzOffset,temp,10),type,false);
  msg+=fmt("Sensitivity",itoa(settings.sensitivity,temp,10),type,false);
  msg+=fmt("Beep on detection",settings.beepOnStrike?"yes":"no",type,false);
  msg+=fmt("Beep pitch",itoa(settings.beepPitch,temp,10)+hz,type,true);
  return msg;
  }

  
String getStrikes(int type)
  {
  char temp[11];
  String cret=type==TYPE_HTML?"<br>":type==TYPE_JSON?"":"\n";
  String headingOpen=type==TYPE_HTML?"<h4>":"";
  String headingClose=type==TYPE_HTML?"</h4>":"";

  int h=getRecent(HOUR_MILLISECS/1000);
  int tfh=getRecent(DAY_MILLISECS/1000);
  int sd=getRecent((DAY_MILLISECS/1000)*7);
  int td=getRecent((DAY_MILLISECS/1000)*30L);
  String ms=itoa(MAX_STRIKES,temp,10);

  String msg=fmt("Past hour",h>=MAX_STRIKES?">"+ms:itoa(h,temp,10),type,false);
  msg+=fmt("Past 24 hours",tfh>=MAX_STRIKES?">"+ms:itoa(tfh,temp,10),type,false);
  msg+=fmt("Past 7 days",sd>=MAX_STRIKES?">"+ms:itoa(sd,temp,10),type,false);
  msg+=fmt("Past 30 days",td>=MAX_STRIKES?">"+ms:itoa(td,temp,10),type,false);
  msg+=fmt("Total Strikes",itoa(strikeCount,temp,10),type,false);
  
  String lbr=type==TYPE_JSON?"[":type==TYPE_HTML?"":"\n"; //left bracket
  String rbr=type==TYPE_JSON?"]":""; //you know
  if (type==TYPE_HTML)
    {
    msg+=headingOpen+"Strike Log ("+String(MAX_STRIKES)+" max): ";
    msg+=headingClose;
    msg+="<form method=\"POST\" action=\"/clear\">";
    msg+="<table><tr><th>#</th><th>Date</th><th>Time</th><th>Intensity</th></tr>";
    msg+=getStrikeLog(TYPE_TABLE);
    msg+="<tr><td colspan=4 align=center valign=middle border=0>";
    msg+="<input type=\"submit\" name=\"clear_log\" value=\"CLEAR\">";
    msg+="</td></tr>";
    msg+="</table>";
    msg+="</form>";
    }
  else
    msg+=fmt(cret+headingOpen+"Strike Log"+headingClose,lbr+getStrikeLog(type)+rbr,type,true);
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
  unsigned long oldest=now(); //gives us number of seconds since Jan 1 1970
  if (oldest>age)  
    oldest-=age;
  else
    oldest=0;

  for (int i=min(strikeCount, MAX_STRIKES)-1;i>=0;i--)
    {
    if (strikes[i]>oldest)
      count++;
    }
  return count;
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
    settings.tzOffset=-5;
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


//Read the solar cell and store the value in the global thisReading variable.
int readSensor()
  {
  thisReading=analogRead(PHOTO_PIN);
  return thisReading;
  }

/*
 * Turn on the built-in LED for a set period of time, or 
 * off when the time expires.  action is LED_ON, LED_OFF, or LED_CHECK.
 */
boolean manageLED(int action, long msec)
  {
  switch(action)
    {
    case LED_ON:
      digitalWrite(LIGHTNING_LED_PIN, LOW);
      ledTime=max(millis()+msec,ledTime); //if already on and longer, just keep it
      break;
    
    case LED_OFF:
      digitalWrite(LIGHTNING_LED_PIN, HIGH);
      break;

    case LED_CHECK:
      if (millis()>=ledTime)
        manageLED(LED_OFF,0);
      break;

    default:
      break;
    }
  }
