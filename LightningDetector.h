//#include "iomap_mhetesp32minikit.h"

#define VERSION          "Version 4.0" // compiled on "__DATE__" at " __TIME__
#define LIGHTNING_LED_PIN D4
#define RELAY_PIN       12      //relay on digital 12
#define PHOTO_PIN       A0      //input from solar cell
#define ERROR_PIN       D1      //analog output for ambient light compensation
#define MAX_SLEW_RATE   25       //limits the rate of compensation for ambient light, minimizes "bouncing"
#define MAX_READING     1023    //the largest value from the A/D converter
#define RESET_DELAY     100     //x10 ms after lightning is seen
#define MAX_STRIKES     1000    //size of strike log
#define HOUR_MILLISECS 3600000L //60*60*1000
#define DAY_MILLISECS 86400000L //24*60*60*1000
#define MIN_STRIKE_WIDTH 2      //Strike lasts at least this many milliseconds
#define MAX_STRIKE_WIDTH 50     //But no longer than this
#define HTML_CR         "<br>"
#define TEXT_CR         "\n\r"

//Commands
#define GET_STATUS    1  //print relevant variables
#define GET_HOUR      2  //print the number of strikes in the past hour
#define GET_DAY       3  //print the number of strikes in the past 24 hours
#define GET_WEEK      4  //print the number of strikes in the past 7 days
#define GET_MONTH     5  //print the number of strikes in the past 30 days
#define GET_ALL       6  //print the number of strikes since the last reset
#define CHANGE_SETTING 7 //change an internal setting (sensitivity, etc)
#define RESET         8  //same as pressing the reset button
#define RESET_TOTALS  9  //reset the strike counters
#define FACTORY_RESET 0  //initialize all variables and counters

//EEPROM addresses
#define EEPROM_SENSITIVITY 0 // 4 bytes for a float
#define EEPROM_SLEW_RATE   4 // 2 byte integer
#define EEPROM_RESET_DELAY 6 // 2 byte integer

//Strictly for ESP32
//#define analogWrite ledcWrite //comment this out for ESP8266
#define MY_MDNS   "lightning" //this will be our server name

#define DEBUG false  //to show plotter graph
