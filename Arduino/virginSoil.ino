/* This is an initial sketch to be used as a "blueprint" to create apps which can be used with IOTappstory.com infrastructure
  Your code can be filled wherever it is marked.


  Copyright (c) [2016] [Andreas Spiess]

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*/

#define SKETCH "McSwitcher "
#define VERSION "V1.0"
#define FIRMWARE SKETCH VERSION

#define SERIALDEBUG         // Serial is used to present debugging messages 
#define REMOTEDEBUGGING     // UDP is used to transfer debug messages

#define LEDS_INVERSE   // LEDS on = GND

//#include "credentials.h"
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>        //https://github.com/kentaylor/WiFiManager
#include <Ticker.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <FS.h>

#ifdef REMOTEDEBUGGING
#include <WiFiUDP.h>
#endif

extern "C" {
#include "user_interface.h" // this is for the RTC memory read/write functions
}

//--------  Sketch Specific -------
#include <PubSubClient.h>
#include <ESPiLight.h>



// -------- PIN DEFINITIONS ------------------
#ifdef ARDUINO_ESP8266_ESP01           // Generic ESP's 
#define MODEBUTTON 0
#define LEDgreen 13
//#define LEDred 12
#else
#define MODEBUTTON D3
#define LEDgreen D7
//#define LEDred D6
#endif

// --- Sketch Specific -----



//---------- DEFINES for SKETCH ----------
#define STRUCT_CHAR_ARRAY_SIZE 50  // length of config variables
#define MAX_WIFI_RETRIES 50
#define RTCMEMBEGIN 68
#define MAGICBYTE 85

// --- Sketch Specific -----
// #define SERVICENAME "VIRGIN"  // name of the MDNS service used in this group of ESPs


//-------- SERVICES --------------


// --- Sketch Specific -----



//--------- ENUMS AND STRUCTURES  -------------------

typedef struct {
  char ssid[STRUCT_CHAR_ARRAY_SIZE];
  char password[STRUCT_CHAR_ARRAY_SIZE];
  char boardName[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStory1[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStoryPHP1[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStory2[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStoryPHP2[STRUCT_CHAR_ARRAY_SIZE];
  char automaticUpdate[2];   // right after boot
  // insert NEW CONSTANTS according boardname example HERE!

  char mqttHost[STRUCT_CHAR_ARRAY_SIZE];
  char mqttPort[STRUCT_CHAR_ARRAY_SIZE];
  char mqttTopic[STRUCT_CHAR_ARRAY_SIZE];
  char pinReceiver[STRUCT_CHAR_ARRAY_SIZE];
  char pinSender[STRUCT_CHAR_ARRAY_SIZE];

  char magicBytes[4];

} strConfig;

strConfig config = {
  "",
  "",
  "yourFirstApp",
  "iotappstory.com",
  "/ota/esp8266-v1.php",
  "iotappstory.com",
  "/ota/esp8266-v1.php",
  "0",
  "CFG"  // Magic Bytes
};

// --- Sketch Specific -----
ESPiLight rf(D1);
WiFiClient espClient;
PubSubClient client(espClient);


//---------- VARIABLES ----------

unsigned long debugEntry;

String sysMessage;

// --- Sketch Specific -----
// String xx; // add NEW CONSTANTS for WiFiManager according the variable "boardname"




//---------- FUNCTIONS ----------
// to help the compiler, sometimes, functions have  to be declared here
void loopWiFiManager(void);
void readFullConfiguration(void);
bool readRTCmem(void);
void printRTCmem(void);
void initialize(void);
void sendDebugMessage(void);

//---------- OTHER .H FILES ----------
#include "ESP_Helpers.h"           // General helpers for all IOTappStory sketches
#include "IOTappStoryHelpers.h"    // Sketch specific helpers for all IOTappStory sketches




// ================================== SETUP ================================================

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 5; i++) DEBUG_PRINTLN("");
  DEBUG_PRINTLN("Start "FIRMWARE);


  // ----------- PINS ----------------
  pinMode(MODEBUTTON, INPUT_PULLUP);  // MODEBUTTON as input for Config mode selection

#ifdef LEDgreen
  pinMode(LEDgreen, OUTPUT);
  digitalWrite(LEDgreen, LEDOFF);
#endif
#ifdef LEDred
  pinMode(LEDred, OUTPUT);
  digitalWrite(LEDred, LEDOFF);
#endif

  // --- Sketch Specific -----



  // ------------- INTERRUPTS ----------------------------
  attachInterrupt(MODEBUTTON, ISRbuttonStateChanged, CHANGE);
  blink.detach();


  //------------- LED and DISPLAYS ------------------------
  LEDswitch(GreenBlink);


  // --------- BOOT STATISTICS ------------------------
  // read and increase boot statistics (optional)
  readRTCmem();
  rtcMem.bootTimes++;
  writeRTCmem();
  printRTCmem();


  //---------- SELECT BOARD MODE -----------------------------

  system_rtc_mem_read(RTCMEMBEGIN + 100, &boardMode, 1);   // Read the "boardMode" flag RTC memory to decide, if to go to config
  if (boardMode == 'C') configESP();

  readFullConfiguration();

  // --------- START WIFI --------------------------

  connectNetwork();

  sendSysLogMessage(2, 1, config.boardName, FIRMWARE, 10, counter++, "------------- Normal Mode -------------------");

  if (atoi(config.automaticUpdate) == 1) IOTappStory(false);  // replace false with true if you want tu update the SPIFFS, too



  // ----------- SPECIFIC SETUP CODE ----------------------------

  // add a DNS service
  // MDNS.addService(SERVICENAME, "tcp", 8080);  // just as an example
  
  // Init RF receiver
  rf.setCallback(rfCallback);
  rf.initReceiver(D2);

  // Init PubSubClint
  client.setServer(config.mqttHost, atoi(config.mqttPort));
  client.setCallback(callback_mqtt);

  // ----------- END SPECIFIC SETUP CODE ----------------------------

  LEDswitch(None);
  pinMode(MODEBUTTON, INPUT_PULLUP);  // MODEBUTTON as input for Config mode selection

  sendSysLogMessage(7, 1, config.boardName, FIRMWARE, 10, counter++, "Setup done");
}





//======================= LOOP =========================
void loop() {
  //-------- IOTappStory Block ---------------
  yield();
  handleModeButton();   // this routine handles the reaction of the Flash button. If short press: update of skethc, long press: Configuration

  // Normal blink (1 sec): Connecting to network
  // fast blink: Configuration mode. Please connect to ESP network
  // Slow Blink: IOTappStore Update in progress

  if (millis() - debugEntry > 5000) { // Non-Blocking second counter
    debugEntry = millis();
    sendDebugMessage();
  }


  //-------- Your Sketch ---------------
  rf.loop();

  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();

}
//------------------------- END LOOP --------------------------------------------



void rfCallback(const String &protocol, const String &message, int status, int repeats, const String &deviceID) {
  DEBUG_PRINT("RF signal arrived [");
  DEBUG_PRINT(protocol); //protocoll used to parse
  DEBUG_PRINT("][");
  DEBUG_PRINT(deviceID); //value of id key in json message
  DEBUG_PRINT("] (");
  DEBUG_PRINT(status);  //status of message, depending on repeat, either: 
                         // FIRST   - first message of this protocoll within the last 0.5 s
                         // INVALID - message repeat is not equal to the previous message
                         // VALID   - message is equal to the previous message
                         // KNOWN   - repeat of a already valid message
  DEBUG_PRINT(") ");
  DEBUG_PRINT(message); // message in json format
  DEBUG_PRINTLN();

  // check if message is valid and process it
  if(status==VALID) {
    DEBUG_PRINT("Valid message: [");
    DEBUG_PRINT(protocol);
    DEBUG_PRINT("] ");
    DEBUG_PRINT(message);
    DEBUG_PRINTLN();

    char topic[64];
    sprintf(topic, "%s/incoming/%s", config.mqttTopic, protocol.c_str());

    //char buffer[128];
    //String escaped_message = message;    
    //escaped_message.replace('"', '\"');
    //sprintf(buffer, "{\"protocol\":\"%s\",\"message\":\"%s\"}", protocol.c_str(), escaped_message.c_str());

    client.publish(topic, message.c_str());
    DEBUG_PRINT("Published ");
    DEBUG_PRINT(message);
    DEBUG_PRINT(" to ");
    DEBUG_PRINTLN(topic);
  }
}

void reconnect_mqtt() {
  // Loop until we're reconnected
  while (!client.connected()) {
    DEBUG_PRINT("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      DEBUG_PRINTLN("connected");
      // Once connected, publish an announcement...
      client.publish(config.mqttTopic, "Reconnected");
      
      // ... and resubscribe
      char topic[32];
      sprintf(topic, "%s/cmd/#", config.mqttTopic);
      client.subscribe(topic);
    } else {
      DEBUG_PRINT("failed, rc=");
      DEBUG_PRINT(client.state());
      DEBUG_PRINTLN(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback_mqtt(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  String str_topic = topic;
  str_topic = str_topic.substring(strlen(config.mqttTopic) + 5);

  DEBUG_PRINT("Subtopic:");
  DEBUG_PRINTLN(str_topic);

  String str_payload = (char *)payload;
  DEBUG_PRINT("Sending:");
  DEBUG_PRINTLN(str_payload);
  rf.send(str_topic, str_payload);


  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}




void sendDebugMessage() {
  // ------- Syslog Message --------

  /* severity: 2 critical, 6 info, 7 debug
    facility: 1 user level
    String hostName: Board Name
    app: FIRMWARE
    procID: unddefined
    msgID: counter
    message: Your message
  */

  sysMessage = "";
  long h1 = ESP.getFreeHeap();
  sysMessage += " Heap ";
  sysMessage += h1;
  sendSysLogMessage(6, 1, config.boardName, FIRMWARE, 10, counter++, sysMessage);
}


bool readRTCmem() {
  bool ret = true;
  system_rtc_mem_read(RTCMEMBEGIN, &rtcMem, sizeof(rtcMem));
  if (rtcMem.markerFlag != MAGICBYTE) {
    rtcMem.markerFlag = MAGICBYTE;
    rtcMem.bootTimes = 0;
    system_rtc_mem_write(RTCMEMBEGIN, &rtcMem, sizeof(rtcMem));
    ret = false;
  }
  return ret;
}

void printRTCmem() {
  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("rtcMem ");
  DEBUG_PRINT("markerFlag ");
  DEBUG_PRINTLN(rtcMem.markerFlag);
  DEBUG_PRINT("bootTimes ");
  DEBUG_PRINTLN(rtcMem.bootTimes);
}


