/*
 * LightningDetector.c
 *
 *  Created on: May 9, 2014
 *  Updated on: July 20, 2019
 *      Author: David E. Powell
 *
 *  --------Hardware and Software Enhancements----------
 *  - Add reset command to clear all counters and restart
 *  - Add counters to eeprom for strike count in the last 24 hours, week, month, and YTD 
 *  - Add data on demand instead of streaming
 *  - Add configuration to eeprom for sensitivity, reset delay, slew rate
 *  - Write dedicated GUI for PC end of serial connection
 *  - 
 *
 *
 */
//#include "WProgram.h" instead of Arduino.h when outside of Arduino IDE
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "Arduino.h"
#include <pgmspace.h>
#include <ctype.h>
#include <ESP_EEPROM.h>
#include <stdio.h>
//#include "RTClib.h"
#include <Esp.h>
#include "LightningDetector.h"

#ifndef AP_SSID
  #define AP_SSID "lightning!"
  #define AP_PASS ""
  #define MY_MDNS "lightning" 
  #endif

IPAddress local_IP(192,168,1,1);
IPAddress gateway(192,168,1,254);
IPAddress subnet(255,255,255,0);

char ssid[SSID_SIZE] = AP_SSID;
char password[PASSWORD_SIZE] = AP_PASS;
char myMDNS[MDNS_SIZE]=MY_MDNS;

const char *htmlCr="<br>";
const char *textCr="\n\r";

int lastReading = 0;
int thisReading = 0;
int errval=128; //half way
int resetCounter=0; //keeps the LED lit long enough to see it when a strike happens
int resetDelay=100;  //this is how many milliseconds (approx) the LED stays lit after lightning is seen
int sensitivity=12;  //successive readings have to be at least this much larger than the last reading to register as a strike
unsigned long strikes[MAX_STRIKES];       //strike time log
unsigned int strikeIntensity[MAX_STRIKES];//strike brightness log
int strikeCount=0;  //number of strikes in the log
boolean done=false;
int strikeStage=0;   //to keep track of which part of the strike we are working
long strikeWaitTime; //to keep from having to use delay()
int intensity;       //temporary storage to record the intensity of the strike for the log
boolean configured=false;  //don't look for lightning until we are set up

static const char configPage[] PROGMEM = "<html>"
    "<head>"
    "<title>Lightning Detector Configuration</title>"
    "<style>body{background-color: #cccccc; Color: #000088; }</style>"
    "<script>function showSensitivity() {document.getElementById(\"sens\").innerHTML=document.getElementById(\"sensitivity\").value;}</script>"
    "</head>"
    "<body>"
    "<b><div style=\"text-align: center;\"><h1><a href=\"/\">Lightning Detector</a> Configuration</h1></div></b>"
    "<br><div id=\"message\">%s</div><br>"
    "<form action=\"/configure\" method=\"post\">"
    "SSID: <input type=\"text\" name=\"ssid\" value=\"%s\" maxlength=\"32\"><br>"
    "Password: <input type=\"text\" name=\"pword\" value=\"%s\" maxlength=\"255\"><br>"
    "mDNS: <input type=\"text\" name=\"mdns\" value=\"%s\" maxlength=\"64\">.local<br>"
    "Sensitivity: <sup><span style=\"font-size: smaller;\">(Max)</span></sup> "
    " <input type=\"range\" name=\"sensitivity\" id=\"sensitivity\" value=\"%s\" min=\"1\" max=\"25\" step=\"1\""
    " onchange=\"showSensitivity()\">"
    " <sup><span style=\"font-size: smaller;\">(Min)</span></sup>"
    " <div align=\"left\" style=\"display: inline; \" id=\"sens\"></div>"
    "<br><br>"
    "<script> showSensitivity();</script>"
    "<input type=\"checkbox\" name=\"factory_reset\" value=\"reset\" "
    "onchange=\"if (this.checked) alert('Checking this box will cause the configuration to be set to "
    "factory defaults and reset the detector! \\nOnce reset, you must connect your wifi to access point "
    "\\'lightning!\\' and browse to http://lightning.local to reconfigure it.');\">"
    "Factory Reset"
    "<br><br>"
    "<input type=\"submit\" name=\"Update Configuration\" value=\"Update\">"
    "</form>"
    "<script>"
    "var msg=new URLSearchParams(document.location.search.substring(1)).get(\"msg\");"
    "if (msg)"
    " document.getElementById(\"message\").innerHTML=msg;"
    "</script>"
    "</body>"
    "</html>"
    ;
//configBuf is defined here to keep it off the stack.
char configBuf[3072]; //I don't know why it has to be twice the size

ESP8266WebServer server(80);

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
  pinMode(RELAY_PIN, OUTPUT);  //set up the relay
  digitalWrite(RELAY_PIN,LOW); //off

  EEPROM.begin(512); //fire up the eeprom section of flash

  //look for a double-click on the reset button.  If reset is clicked, then clicked
  //again while the LED is on, then do a factory reset. We detect this by reading the
  //"configured" value from EEPROM and setting it to false, turning on the LED, then 
  //turning the LED off and writing the value back to EEPROM two seconds later.
  boolean conf;
  boolean noConf=false;
  EEPROM.get(EEPROM_ADDR_FLAG,conf);  //save original value
  EEPROM.put(EEPROM_ADDR_FLAG,noConf);//write "false" for non-configured
  EEPROM.commit();                    //do the write
  digitalWrite(LIGHTNING_LED_PIN,LOW);//Turn on the LED
  delay(2000);                        //wait a sec
  digitalWrite(LIGHTNING_LED_PIN,HIGH);//Turn the LED back off
  EEPROM.put(EEPROM_ADDR_FLAG,conf);  //If we are here then no double-click, write the original back
  EEPROM.commit();                    //do the write

  loadSettings(); //load the editable settings from eeprom

  //initialize variables
  
  resetCounter=0;
  thisReading=analogRead(PHOTO_PIN);
  lastReading=thisReading;
  
  //Get the WiFi going
  if (strcmp(AP_SSID,ssid)==0) //using the default SSID?  If so then open the configure page
    {
    Serial.print("Configuring soft access point. SSID is \""+String(ssid)+"\". ");
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(ssid, password);
  
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("IP address is ");
    Serial.println(myIP);
    }
  else
    {
    configured=true;  //we are configured
    WiFi.mode(WIFI_STA);
    Serial.println("Connecting to "+String(ssid)+" with password "+String(password));
    WiFi.begin(ssid, password);
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
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());       
    }

  if (MDNS.begin(myMDNS)) {
    Serial.print("MDNS responder started as ");
    Serial.print(myMDNS);
    Serial.println(".local");
  }

  server.on("/", documentRoot);
  server.on("/json", sendJson);
  server.on("/text", sendText);
  server.on("/configure", setConfig);
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

//  Serial.print("setup() running on core ");
//  Serial.println(xPortGetCoreID());

  //Ready to loop
  Serial.println(getStatus(TYPE_TEXT));
  Serial.println("Ready.");
  Serial.println("");
  }

void loop()
  {
  server.handleClient();
  MDNS.update();

  //Don't read the sensor too fast or else it will disconnect the wifi
  if(!configured || millis()%5 != 0)
    return;
  else
    {     
    checkForStrike();
    }
  }

char* getConfigPage(String message)
  {
  char temp[11];
  sprintf_P(configBuf,(PGM_P)FPSTR(configPage),
              message.c_str(),
              ssid,
              password,
              myMDNS,
              itoa(sensitivity,temp,10));
  return configBuf;
  }

void setConfig()
  {
  Serial.println("Received set config request");
  if (server.hasArg("plain")== false) //Check if body received
    {
    server.send(200, "text/html", getConfigPage(""));
    Serial.println("Configure page sent.");
    }
  else if (server.arg("factory_reset").compareTo("reset")==0) //factory reset
    {
    boolean valid=false;
    EEPROM.put(EEPROM_ADDR_FLAG,valid);
    EEPROM.commit();
    ESP.reset();   
    }
  else
    {
    String message="Configuration Updated"; //if all goes well
    Serial.println("POST data: "+server.arg("plain"));

    int sens=atoi(server.arg("sensitivity").c_str());

    if (strcmp(myMDNS,server.arg("mdns").c_str())==0
      &&strcmp(ssid,server.arg("ssid").c_str())==0
      &&strcmp(password,server.arg("pword").c_str())==0
      &&sensitivity==sens)
      {
      server.sendHeader("Location", String("/"), true);
      server.send(303, "text/plain", ""); //send them back to the configuration page
      }
    else 
      {
      sensitivity=sens;
      strcpy(myMDNS,server.arg("mdns").c_str());
      strcpy(ssid,server.arg("ssid").c_str());
      strcpy(password,server.arg("pword").c_str());
  
      boolean valid=true;
      EEPROM.put(EEPROM_ADDR_FLAG,valid);
      EEPROM.put(EEPROM_ADDR_SSID,ssid);
      EEPROM.put(EEPROM_ADDR_PASSWORD,password);
      EEPROM.put(EEPROM_ADDR_MDNS,myMDNS);
      EEPROM.put(EEPROM_ADDR_SENSITIVITY,sensitivity);
  
      //perform the actual write to eeprom
      if (!EEPROM.commit())
        {
        Serial.println("Storing to eeprom failed!");
        message="Could not store configuration values!";
        server.send(200, "text/html", getConfigPage(message)); //send them back to the configuration page
        }
      else
        {
        Serial.println("Configuration stored to EEPROM");
        if (configured)
          {
          server.sendHeader("Location", String("/"), true);
          server.send(303, "text/plain", ""); //send them back to the configuration page
          }
        else
          ESP.reset();
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
  Serial.println("Code 404 response sent.");
  }

void sendText()
  {
  Serial.println("Received request for text data");

  server.send(200, "text/plain",getStatus(TYPE_TEXT));
  
  Serial.println("Response sent for text data");
  };


void sendJson()
  {
  Serial.println("Received request for JSON data");
  
  server.send(200, "text/plain",getStatus(TYPE_JSON));

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
  String page="<html>\
    <head>\
      <meta http-equiv='refresh' content='15'/>\
      <title>Lightning Detector</title>\
      <style>\
        body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
      </style>\
    </head>\
    <body>\
      <div style=\"text-align: center;\"><h1>Optical Lightning Detector</h1></div>\
      <p><h2><a href=\"/configure\">Configuration</a> and Status</h2>\
      mDNS: \""+String(myMDNS)+".local\"<br>\
      Address: "+WiFi.localIP().toString()+"<br><br>"
      +getStatus(TYPE_HTML)
      +"</p>\
    </body>\
  </html>";
  server.send(200, "text/html", page);
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
      if (thisReading > lastReading+sensitivity) //Is this reading brighter than the last one?
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
        if (thisReading > lastReading+sensitivity) //still going?
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
          if (thisReading <= lastReading+sensitivity) //should not still be going, maybe someone turned on the light
            {
            strikeStage++;   //Take us to default case until this strike is over
            Serial.print("Strike confirmed! - ");
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
  strikes[index]=millis();  //record the time of this strike
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
    case TYPE_TABLE: //later
      result=name+": "+value+"<br>";
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

/*
* Return a string with all relevant variables.
* Type is 1 for text, 2 for HTML, 3 for JSON
*/
String getStatus(int type)
  {
  char temp[11];
  String cret=type==TYPE_HTML?"<br>":type==TYPE_JSON?"":"\n";
  String h2open=type==TYPE_HTML?"<h2>":"";
  String h2close=type==TYPE_HTML?"</h2>":"";
  String msg=type==TYPE_JSON?"{\"lightning\":{":"";
  
  msg+=fmt("Uptime",msToAge(millis()),type,false);
  msg+=fmt("Sensitivity",itoa(sensitivity,temp,10),type,false);
  msg+=fmt("Illumination",itoa(thisReading,temp,10),type,false);

  if (type!=TYPE_JSON)
    msg+=cret+h2open+"Strike Data:"+h2close+(type==TYPE_HTML?"":cret+cret);
  msg+=fmt("Past hour",itoa(getRecent(HOUR_MILLISECS),temp,10),type,false);
  msg+=fmt("Past 24 hours",itoa(getRecent(DAY_MILLISECS),temp,10),type,false);
  msg+=fmt("Past 7 days",itoa(getRecent(DAY_MILLISECS*7),temp,10),type,false);
  msg+=fmt("Past 30 days",itoa(getRecent(DAY_MILLISECS*30L),temp,10),type,false);
  msg+=fmt("Total Strikes",itoa(strikeCount,temp,10),type,false);
  
  String lbr=type==TYPE_JSON?"[":type==TYPE_HTML?"":"\n"; //left bracket
  String rbr=type==TYPE_JSON?"]":""; //you know
  if (type==TYPE_HTML)
    {
    msg+=h2open+"Strike Log:"+h2close;
    msg+=getStrikeLog(type);
    }
  else
    msg+=fmt(cret+h2open+"Strike Log"+h2close,lbr+getStrikeLog(type)+rbr,type,true);
  msg+=type==TYPE_JSON?"}}":"";
  return msg;
  }

String getStrikeLog(int type)
  {
  char temp[11];
  String msg="";
  long now=millis();
  boolean lastEntry=false;
  for (int i=strikeCount-1;i>=0;i--)
    {
    lastEntry=(i==0);
    int j=i % MAX_STRIKES; 
     
    if (type==TYPE_JSON)
      {
      msg+="{"+fmt(msToAge(now-strikes[j]),itoa(strikeIntensity[j],temp,10),type,true)+"}";
      msg+=(lastEntry?"":",");
      }
    else
      {
      msg+=itoa(i+1,temp,10);
      msg+=" - ";
      msg+=fmt(msToAge(now-strikes[j]),itoa(strikeIntensity[j],temp,10),type,lastEntry);
      }
    } 
  return msg;
  }



/*
* Return the number of strikes recorded in the past (age) milliseconds
*/
int getRecent(unsigned long age)
  {
  int count=0;  
    
  //get the run time and subtract age, but don't go below 0
  unsigned long oldest=millis();
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

/*
*  Initialize the settings from eeprom
*/
void loadSettings()
  {
  boolean valid=false;
  EEPROM.get(EEPROM_ADDR_FLAG,valid);
  if (valid)    //skip loading stuff if it's never been written
    {
    EEPROM.get(EEPROM_ADDR_SSID,ssid);
    EEPROM.get(EEPROM_ADDR_PASSWORD,password);
    EEPROM.get(EEPROM_ADDR_MDNS,myMDNS);
    EEPROM.get(EEPROM_ADDR_SENSITIVITY,sensitivity);
  
    Serial.println("myMDNS is "+String(myMDNS));
    }
  else
    Serial.println("Skipping load from EEPROM, device not configured.");
  }



///*
//*  Initialize the EEPROM settings to default values
//*/
//void init_variables()
//  {
//  // Sensitivity
//  sensitivity=DEFAULT_SENSITIVITY;
//  char* sens=(char*)&sensitivity; //get the location of the 4-byte float
//  EEPROM.write(EEPROM_SENSITIVITY+0,sens[0]);
//  EEPROM.write(EEPROM_SENSITIVITY+1,sens[1]);
//  EEPROM.write(EEPROM_SENSITIVITY+2,sens[2]);
//  EEPROM.write(EEPROM_SENSITIVITY+3,sens[3]);
//
//  // Max Slew Rate
//  
//  
//  // Reset Delay
//  
//  }


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
