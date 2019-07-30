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
#include <EEPROM.h>
#include <stdio.h>
//#include "RTClib.h"
#include <Esp.h>
#include "LightningDetector.h"

#ifndef AP_SSID
  #define AP_SSID "Kif's Dream"
  #define AP_PASS "buteomontHill"
  #endif

const char *ssid = AP_SSID;
const char *password = AP_PASS;
const char *htmlCr="<br>";
const char *textCr="\n\r";

int lastReading = 0;
int thisReading = 0;
int errval=128; //half way
int lastErrval=512;
int resetCounter=0;
int strikeBlinkPass=0;  //pass counter for LED on/off time
int strikeBlinkCount=0; //blink counter for LED on/off time
int sensitivity=10;  //successive readings have to be at least this much larger than the last reading to register as a strike
unsigned long strikes[MAX_STRIKES];       //strike time log
unsigned int strikeIntensity[MAX_STRIKES];//strike brightness log
int strikeCount=0;  //number of strikes in the log
boolean done=false;
int strikeStage=0;   //to keep track of which part of the strike we are working
long strikeWaitTime;
int intensity;        //record the intensity of the strike for the log

static const char configPage[] PROGMEM = "This is a string stored in flash"
                                         "\nThis also";

ESP8266WebServer server(80);

void setup()
  {
  //Set up the serial communications
  Serial.begin(115200);
  Serial.setTimeout(10000);
    
//  loadSettings(); //load the editable settings from eeprom
  
  dumpSettings(); //show all of the settings
  
  //initialze the outputs
  pinMode(LIGHTNING_LED_PIN, OUTPUT);  //we'll use the led to indicate detection
  digitalWrite(LIGHTNING_LED_PIN,HIGH); //High is off on the dorkboard
  pinMode(RELAY_PIN, OUTPUT);  //set up the relay
  digitalWrite(RELAY_PIN,LOW); //off

  //initialize variables
  resetCounter=0;
  thisReading=analogRead(PHOTO_PIN);
  lastReading=thisReading;
  
  //Get the WiFi going
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) 
    {
    delay(500);
    Serial.print(".");
    }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin(MY_MDNS)) {
    Serial.print("MDNS responder started as ");
    Serial.print(MY_MDNS);
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
  Serial.println("Ready.");
  Serial.println("");
  }

void loop()
  {
  server.handleClient();
  MDNS.update();

  //Don't read the sensor too fast or else it will disconnect the wifi
  if(millis()%5 != 0)
    return;
  else
    {     
    checkForStrike();
    }
  }

void setConfig()
  {
  Serial.println("Received set config request");
  if (server.hasArg("plain")== false) //Check if body received
    {
    server.send(200, "text/plain", configPage);
    Serial.println("Configure page sent.");
    }
  else
    {
    String msg = "Body received:\n";
    msg+=server.arg("plain");
    msg+="\n";
    server.send(200, "text/plain", msg);
    Serial.println("POST data: "+server.arg("plain"));
    }
  Serial.println("Response sent for set config request");
  return;
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
  String page="<html>\
    <head>\
      <meta http-equiv='refresh' content='15'/>\
      <title>Lightning Detector</title>\
      <style>\
        body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
      </style>\
    </head>\
    <body>\
      <h1>Optical Lightning Detector</h1>\
      <p>Server Data: <br><br>"
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
//        Serial.print("0/");
//        Serial.print(lastReading);
//        Serial.print("/");
//        Serial.print(millis());
//        Serial.print("/");
//        Serial.println(strikeWaitTime);
      readSensor(); //take a reading
      if (thisReading > lastReading+sensitivity) //Is this reading brighter than the last one?
        {
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
          intensity=thisReading-lastReading; //save the brightness
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
            Serial.print(lastReading);
            Serial.print("/");
            Serial.println(thisReading);
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
  resetCounter=RESET_DELAY;        //"debounce" the strike
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
  


void dumpSettings()
  {
  Serial.println(" \n");
  Serial.println(VERSION);
  Serial.println(" ");
  Serial.print("ERROR_PIN: ");
  Serial.println(ERROR_PIN);
  Serial.print("LIGHTNING_LED_PIN: ");
  Serial.println(LIGHTNING_LED_PIN);
  Serial.print("MAX_SLEW_RATE: ");
  Serial.println(MAX_SLEW_RATE);
  Serial.print("PHOTO_PIN: ");
  Serial.println(PHOTO_PIN);
  Serial.print("RELAY_PIN: ");
  Serial.println(RELAY_PIN);
  Serial.print("RESET_DELAY: ");
  Serial.println(RESET_DELAY);
  }

  
/*
* Read, validate, and return a command from the serial port.
*/

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
  String msg=type==TYPE_JSON?"{\"lightning\":{":"";
  
  msg+=fmt("Uptime",msToAge(millis()),type,false);
  msg+=fmt("Sensitivity",itoa(sensitivity,temp,10),type,false);
  msg+=fmt("Illumination",itoa(thisReading,temp,10),type,false);

  if (type!=TYPE_JSON)
    msg+=cret+"Strike Data:"+cret+cret;
  msg+=fmt("Past hour",itoa(getRecent(HOUR_MILLISECS),temp,10),type,false);
  msg+=fmt("Past 24 hours",itoa(getRecent(DAY_MILLISECS),temp,10),type,false);
  msg+=fmt("Past 7 days",itoa(getRecent(DAY_MILLISECS*7),temp,10),type,false);
  msg+=fmt("Past 30 days",itoa(getRecent(DAY_MILLISECS*30L),temp,10),type,false);
  msg+=fmt("Total Strikes",itoa(strikeCount,temp,10),type,false);
  
  String lbr=type==TYPE_JSON?"[":type==TYPE_HTML?"<br>":"\n"; //left bracket
  String rbr=type==TYPE_JSON?"]":""; //you know
  msg+=fmt(cret+"Strike Log",lbr+getStrikeLog(type)+rbr,type,true);
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

///*
//*  Initialize the settings from eeprom
//*/
//void loadSettings()
//  {
//  // Sensitivity
//  char* sens=(char*)&sensitivity; //get the location of the 4-byte float
//  sens[0]=EEPROM.read(EEPROM_SENSITIVITY+0);
//  sens[1]=EEPROM.read(EEPROM_SENSITIVITY+1);
//  sens[2]=EEPROM.read(EEPROM_SENSITIVITY+2);
//  sens[3]=EEPROM.read(EEPROM_SENSITIVITY+3);
//
//  // Max Slew Rate
//  
//  
//  // Reset Delay
//  
//  }



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
//  thisReading=(analogRead(PHOTO_PIN)
//              +analogRead(PHOTO_PIN)
//              +analogRead(PHOTO_PIN)
//              +analogRead(PHOTO_PIN))/4;
//  if (thisReading<2)  //minimum value
//    thisReading=2;
  return thisReading;
  }


//Try to keep the input signal quiesced at approximately the midpoint of the range.
//This is done by adjusting the pulse width being sent to the compensation circuit.
//This routine calculates the value of the pulse width.
void quiesce()
    {
    readSensor();
    errval=thisReading-MAX_READING/2;            // Calculate error value from midpoint of max possible input
    if (errval>(lastErrval+MAX_SLEW_RATE))       // but limit the change to the maximum allowed
      errval=lastErrval+MAX_SLEW_RATE;
    else if (errval<(lastErrval-MAX_SLEW_RATE))
      errval=lastErrval-MAX_SLEW_RATE;
    if (errval<0) errval=0;                      // In no case go outside of one byte
    if (errval>255) errval=255;
    lastErrval=errval;
      
    analogWrite(ERROR_PIN, map(errval,0,255,0,MAX_READING)); //write the new value to the compensation circuit
    }
    
// given a PROGMEM string, use Serial.print() to send it out
void printProgStr(const char str[])
  {
  char c;
  if(!str) return;
  while((c = pgm_read_byte(str++)))
    Serial.print(c);
  }
