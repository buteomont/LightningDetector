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
#include "Arduino.h"
#include <pgmspace.h>
#include <ctype.h>
#include <EEPROM.h>
#include <stdio.h>
//#include "RTClib.h"
#include <Esp.h>
#include "LightningDetector.h"

int lastReading = 0;
int thisReading = 0;
int errval=128; //half way
int lastErrval=512;
int resetCounter=0;
int strikeCount=0;
boolean inStrike=false; //strike is in progress
int strikeBlinkPass=0;  //pass counter for LED on/off time
int strikeBlinkCount=0; //blink counter for LED on/off time
float sensitivity=1.02;  //successive readings have to increase by at least this factor to register as a strike
unsigned long strikes[MAX_STRIKES]; //strike log

int readPointer=0;
int readHistory[]={0,0,0,0,0,0,0,0,0,0};

char usage[] PROGMEM = "\n\rUsage:"
                      "\n\r1 = GET STATUS -print relevant variables"
                      "\n\r2 = GET HOUR - print the number of strikes in the past hour"
	              "\n\r3 = GET DAY -print the number of strikes in the past 24 hours"
		      "\n\r4 = GET WEEK -print the number of strikes in the past 7 days"
		      "\n\r5 = GET MONTH -print the number of strikes in the past 30 days"
		      "\n\r6 = GET ALL -print the number of strikes since the last reset"
		      "\n\r7 = CHANGE SETTING -change the sensitivity, slew rate, or reset delay"
		      "\n\r8 = RESET -same as pressing the reset button"
		      "\n\r9 = RESET TOTALS -reset the strike counters"
		      "\n\r0 = FACTORY RESET -initialize all variables and counters"
                      "\n\rH = HELP - print this help text"
                      "\n\r";

void setup()
  {
  //Set up the serial communications
  Serial.begin(115200);
  Serial.setTimeout(10000);
  
//  loadSettings(); //load the editable settings from eeprom
  
  dumpSettings(); //show all of the settings
  
  
  
  //use the 5v default reference
//  analogReference(DEFAULT);
//  pinMode(ERROR_PIN,OUTPUT);

  //initialze the sensors
//  pinMode(PHOTO_PIN, INPUT);
//  digitalWrite(PHOTO_PIN, LOW);   //disable the internal pullup resistor

  //initialze the outputs
  pinMode(LIGHTNING_LED_PIN, OUTPUT);  //we'll use the led to indicate detection
  digitalWrite(LIGHTNING_LED_PIN,HIGH); //High is off on the dorkboard
  pinMode(RELAY_PIN, OUTPUT);  //set up the relay
  digitalWrite(RELAY_PIN,LOW); //off

  //initialize variables
  resetCounter=0;
  thisReading=analogRead(PHOTO_PIN);
  lastReading=thisReading;
  
  printProgStr(usage);
  
  //Ready to loop
  Serial.println("Ready.");
  Serial.println("");
  }

void loop()
  {
  quiesce();
  doCommands();
  checkForStrike();
  }


void checkForStrike()
  {
  if (!inStrike)  //skip it if we are already counting at a strike
    {
    readSensor();
    if (thisReading > (lastReading+5)*sensitivity) //Is this reading brighter than the last one?
                                                   // (+5 is for when it's really really dark)
      {
      //make sure it's really a strike.  Wait a bit and look again
      delay(MIN_STRIKE_WIDTH);
      readSensor();
      if (thisReading > (lastReading+5)*sensitivity) //still going?
        {
        delay (MAX_STRIKE_WIDTH-MIN_STRIKE_WIDTH); //Someone may have just turned on the light
        readSensor();
        if (thisReading > (lastReading+5)*sensitivity) //should not still be going
          noStrike();  // nope, too long 
        else strike(); // yep, record it    
        }
      else noStrike();
      }
    else noStrike();
    
    lastReading=thisReading;
    }
  else noStrike();
  }

// Record a strike
void strike()
  {
  digitalWrite(LIGHTNING_LED_PIN, LOW);//turn on the blue LED
  digitalWrite(RELAY_PIN, HIGH);    //turn on the relay
  resetCounter=RESET_DELAY;        //"debounce" the strike
  int index=strikeCount++ % MAX_STRIKES;
  strikes[index]=millis();  //record the time of this strike
  inStrike=true;
  }

//Not a strike this time
void noStrike()
  {
  if (resetCounter>0)
    {
    resetCounter--;
    delay(10);
    }
  else //turn off the LED and relay
    {
    digitalWrite(LIGHTNING_LED_PIN, HIGH);
    digitalWrite(RELAY_PIN, LOW);
    inStrike=false;
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
* See if any commands are given.  Process them if so.
*/
void doCommands()
  {
  int command=readCommand();
  if (command==GET_STATUS) 
    {
    char buf[120];
    Serial.println(getStatus(buf));
    }
  else if (command==GET_HOUR) 
    {
    Serial.print(getRecent(HOUR_MILLISECS),DEC); //A day's worth of milliseconds
    Serial.println(F(" strikes in the past hour"));
    }
  else if (command==GET_DAY) 
    {
    Serial.print(getRecent(DAY_MILLISECS),DEC); //A day's worth of milliseconds
    Serial.println(F(" strikes in the past 24 hours"));
    }
  else if (command==GET_WEEK) 
    {
    Serial.print(getRecent(DAY_MILLISECS*7),DEC); //A week's worth of milliseconds
    Serial.println(F(" strikes in the past 7 days"));
    }
  else if (command==GET_MONTH) 
    {
    Serial.print(getRecent(DAY_MILLISECS*30L),DEC); //A week's worth of milliseconds
    Serial.println(F(" strikes in the past 30 days"));
    }
  else if (command==GET_ALL) 
    {
    Serial.print(strikeCount,DEC);
    Serial.println(F(" strikes since last reset"));
    }
//  else if (command==CHANGE_SETTING) 
//    {
//    changeSetting();
//    }
  else if (command==RESET) 
    {
    Serial.println(F("Resetting..."));
    delay(1000);
    ESP.restart(); //asm volatile ("jmp 0"); //pseudo software reset
    }   
  else if (command==RESET_TOTALS) 
    {
    reset_totals();
    Serial.println(F("All accumulators were reset."));
    }
  else if (command==FACTORY_RESET) 
    {
    reset_totals();
//    init_variables();
    Serial.println(F("Everything reset to factory defaults."));
    delay(1000);
    ESP.restart(); //asm volatile ("jmp 0"); //pseudo software reset    
    }
  }
  
/*
* Read, validate, and return a command from the serial port.
*/
int readCommand()
  {
  int command=-1;
  if (Serial.available())
    {
    char cmd=Serial.read(); //get a command
    Serial.println(cmd);    //echo it back to sender
    
    cmd-=48; //convert to number
   
    if ( cmd!=GET_STATUS
      && cmd!=GET_HOUR 
      && cmd!=GET_DAY
      && cmd!=GET_WEEK
      && cmd!=GET_MONTH
      && cmd!=GET_ALL
      && cmd!=CHANGE_SETTING
      && cmd!=RESET
      && cmd!=RESET_TOTALS
      && cmd!=FACTORY_RESET)
      {
      printProgStr(usage);
      }
    else command=cmd; //convert from ASCII to number      
    }
  return command;
  }
  
///*
//* Ask which setting to change and then change it in eeprom and memory
//*/
//void changeSetting()
//  {
//  Serial.println(F("\n\r1-Sensitivity\n\r2-Slew Rate\n\r3-Reset Delay\n\r:"));
//  char cmd=Serial.read(); //which setting?
//  Serial.println(cmd);    //echo it
//  cmd-=48; //convert to a number
//  
//  switch(cmd)
//    {
//    case 1:
//      Serial.println(F("Enter a number between 1 and 2:"));
//      char buf[6];
//      Serial.readBytesUntil('\n',buf,5);
//      
//      break;
//    case 2:
//    case 3:
//    default:
//      Serial.println(F("That's not one of the choices."));
//    }
//  }
  
/*
* Print all relevant variables to the serial port.
*/
char* getStatus(char* buf)
  {
  char temp[11];
  strcpy(buf,"Uptime: ");
  strcat(buf,ltoa(millis(),temp,10));
  strcat(buf,"\n\rIllumination: ");
  strcat(buf,itoa(thisReading,temp,10));
  strcat(buf,"\n\rCenter: ");
  strcat(buf,itoa(errval,temp,10));
  strcat(buf,"\n\rDelay: ");
  strcat(buf,itoa(resetCounter,temp,10));
  strcat(buf,"\n\rIn Strike: ");
  strcat(buf,itoa(inStrike,temp,10));
  strcat(buf,"\n\rTotal Strikes: ");
  strcat(buf,itoa(strikeCount,temp,10));
  strcat(buf,"  ");
  return buf;
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
  thisReading=(analogRead(PHOTO_PIN)
              +analogRead(PHOTO_PIN)
              +analogRead(PHOTO_PIN)
              +analogRead(PHOTO_PIN))/4;
  return thisReading;
  }


//Try to keep the input signal quiesced at approximately the midpoint of the range.
//This is done by adjusting the pulse width being sent to the compensation circuit.
//This routine calculates the value of the pulse width.
void quiesce()
    {
    readSensor();

    if (DEBUG)
      graphIt(thisReading);
      
//    if ((thisReading>lastReading && errval>lastErrval) //a rising light level and rising error value, or
//      ||(thisReading<lastReading && errval<lastErrval) //a dropping light level and dropping error value, or
//      || errval==lastErrval)                           //error value hasn't changed
      {                                                // then change the error level
      errval=thisReading-MAX_READING/2;             // Calculate error value from midpoint of max possible input
//      Serial.print(errval);
//      Serial.print("\t");
      if (errval>(lastErrval+MAX_SLEW_RATE))       // but limit the change to the maximum allowed
        errval=lastErrval+MAX_SLEW_RATE;
      else if (errval<(lastErrval-MAX_SLEW_RATE))
        errval=lastErrval-MAX_SLEW_RATE;
      if (errval<0) errval=0;                      // In no case go outside of one byte
      if (errval>255) errval=255;
//      Serial.print(thisReading);
//      Serial.print("\t");
//      Serial.print(lastErrval);
//      Serial.print("\t");
//      Serial.println(errval);
      lastErrval=errval;
      }
    analogWrite(ERROR_PIN, map(errval,0,255,0,MAX_READING)); //write the new value to the compensation circuit
    }

  void graphIt(int reading)
    {
    readPointer++;
    if (readPointer>9)
      readPointer=0;
    readHistory[readPointer]=reading;
    float readTotal=0;
    for (int i=0;i<10;i++)
      readTotal+=readHistory[i];
    Serial.println(readTotal/10); //for the serial plotter
//    Serial.println(reading); //for the serial plotter
    }

    
// given a PROGMEM string, use Serial.print() to send it out
void printProgStr(const char str[])
  {
  char c;
  if(!str) return;
  while((c = pgm_read_byte(str++)))
    Serial.print(c);
  }