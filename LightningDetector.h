//#include "iomap_mhetesp32minikit.h"

#define VERSION          "5.0.0" // compiled on "__DATE__" at " __TIME__

#define SOUNDER_PIN         D5   //to make the beep
#define SOUNDER_PITCH       2048 //Hz
#define SOUNDER_DURATION    100  //milliseconds
#define CLEAR_LOG_LED_TIME  2000 //milliseconds to show LED when clearing log

#define LIGHTNING_LED_PIN LED_BUILTIN 
#define BLUE_LED_PIN    D4
#define RELAY_PIN       D2      //relay on digital 12

#define PHOTO_PIN       A0      //input from solar cell
#define ERROR_PIN       D1      //analog output for ambient light compensation
#define MAX_READING     1023    //the largest value from the A/D converter
#define MAX_STRIKES     200     //size of strike log
#define HOUR_MILLISECS 3600000L //60*60*1000
#define DAY_MILLISECS 86400000L //24*60*60*1000
#define MIN_STRIKE_WIDTH 1      //Strike must last at least this many milliseconds
#define MAX_STRIKE_WIDTH 400    //But no longer than this
#define HTML_CR         "<br>"
#define TEXT_CR         "\n\r"
#define OFF             HIGH
#define ON              LOW
#define TYPE_TEXT       0
#define TYPE_HTML       1
#define TYPE_TABLE      2
#define TYPE_JSON       3
#define LED_ON          0
#define LED_OFF         1
#define LED_CHECK       2

#define SSID_SIZE 32
#define PASSWORD_SIZE 64
#define MDNS_SIZE 256
#define ADDRESS_SIZE 18
#define USERNAME_SIZE 32

#define NIST_PORT     13
#define NIST_HOST     "pool.ntp.org"
#define NTP_PACKET_SIZE 48
#define DNS_SERVER0  8
#define DNS_SERVER1  8
#define DNS_SERVER2  8
#define DNS_SERVER3  8

#define AIO_SERVER      "192.168.1.118" //"broker.hivemq.com" //"192.168.1.99"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME    "southLightning"
#define AIO_KEY         "sFlash"
#define MQTT_TOPIC      "tinyhouse/lightning/southside/brightness"
#define MQTT_TOPIC_SIZE 64

#define DEBUG false  //to show plotter graph

// Error codes copied from the MQTT library
#define MQTT_CONNECTION_REFUSED            -2
#define MQTT_CONNECTION_TIMEOUT            -1
#define MQTT_SUCCESS                        0
#define MQTT_UNACCEPTABLE_PROTOCOL_VERSION  1
#define MQTT_IDENTIFIER_REJECTED            2
#define MQTT_SERVER_UNAVAILABLE             3
#define MQTT_BAD_USER_NAME_OR_PASSWORD      4
#define MQTT_NOT_AUTHORIZED                 5
