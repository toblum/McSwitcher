#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include "definitions.h"

// ***************************************************************************
// Load libraries for: WebServer / WiFiManager / FauxmoESP / RCswitch
// ***************************************************************************
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <fauxmoESP.h>            //https://bitbucket.org/xoseperez/fauxmoesp
#define WIFI_MANAGER_USE_ASYNC_WEB_SERVER //See: https://bitbucket.org/xoseperez/fauxmoesp/issues/12/wifimanager-cannot-be-used-with-fauxmoesp
#include <WiFiManager.h>          //Alternative version from: https://github.com/btomer/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>
#include <RCSwitch.h>
#include <SPIFFSEditor.h>


// ***************************************************************************
// Global instances of libraries
// ***************************************************************************
RCSwitch mySwitch = RCSwitch();
WiFiClient espClient;
PubSubClient client(espClient);
fauxmoESP fauxmo;
AsyncWebServer server(80);


// ***************************************************************************
// Parameter definitions for WifiManager
// ***************************************************************************
//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40] = "raspberrypi2";
char mqtt_port[6] = "1883";
char mqtt_topic[40] = "mc_switcher";


// The extra parameters to be configured (can be either global or just in the setup)
// After connecting, parameter.getValue() will get you the configured value
// id/name placeholder/prompt default length
WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 40);
WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port, 5);
WiFiManagerParameter custom_mqtt_topic("topic", "MQTT Topic", mqtt_topic, 40);

// flag for saving data
bool shouldSaveConfig = false;


// ***************************************************************************
// WifiManager: callback notifying us of the need to save config
// ***************************************************************************
void saveConfigCallback () {
  DEBUG_PRINTLN("Should save config");
  shouldSaveConfig = true;
}


// ***************************************************************************
// PubSubClient: Callback when new message arrived
// ***************************************************************************
void callback_mqtt(char* topic, byte* payload, unsigned int length) {
  DEBUG_PRINT("Message arrived [");
  DEBUG_PRINT(topic);
  DEBUG_PRINT("] ");
  for (int i = 0; i < length; i++) {
    DEBUG_PRINT((char)payload[i]);
  }
  DEBUG_PRINTLN();

  String str_topic = topic;
  str_topic = str_topic.substring(strlen(mqtt_topic) + 5);

  DEBUG_PRINT("Subtopic:");
  DEBUG_PRINTLN(str_topic);
  DEBUG_PRINT("Length:");
  DEBUG_PRINTLN(length);

  String str_payload = (char *)payload;
  str_payload = str_payload.substring(0, length);
  DEBUG_PRINT("Sending:");
  DEBUG_PRINTLN(str_payload);

  int bits = str_topic.toInt();
  if (bits == 0) {
    bits = 24;
  }
  DEBUG_PRINT("Bits:");
  DEBUG_PRINTLN(bits);
  DEBUG_PRINT("Payload:");
  DEBUG_PRINTLN(str_payload.toInt());

  mySwitch.send(str_payload.toInt(), bits);
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
      client.publish(mqtt_topic, "Reconnected");
      
      // ... and resubscribe
      char topic[32];
      sprintf(topic, "%s/cmd/#", mqtt_topic);
      client.subscribe(topic);
    } else {
      DEBUG_PRINT("failed, rc=");
      DEBUG_PRINT(client.state());
      DEBUG_PRINTLN(" try again in 5 seconds");

      ESP.restart();
    }
  }
}


void setup() {
  Serial.begin(115200);
  DEBUG_PRINTLN();

  // ***************************************************************************
  // Init switches
  // ***************************************************************************
  mySwitch.enableReceive(PIN_RECEIVE);
  mySwitch.enableTransmit(PIN_TRANSMIT);
  mySwitch.setRepeatTransmit(NUM_TRANS_REPEATS);

  pinMode(PIN_MODEBUTTON, INPUT);  // PIN_MODEBUTTON as input for Config mode selection


  // ***************************************************************************
  // Read configuration from FS config.json / fauxmo_config.json
  // ***************************************************************************
  //clean FS, for testing
  //SPIFFS.format();
  
  DEBUG_PRINTLN("mounting FS...");

  if (SPIFFS.begin()) {
    DEBUG_PRINTLN("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      DEBUG_PRINTLN("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        DEBUG_PRINTLN("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          DEBUG_PRINTLN("\nparsed json");
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_topic, json["mqtt_topic"]);
        } else {
          DEBUG_PRINTLN("failed to load json config");
        }
      }
    }

    if (SPIFFS.exists("/fauxmo_config.json")) {
      File fauxmoConfigFile = SPIFFS.open("/fauxmo_config.json", "r");
      if (fauxmoConfigFile) {
        DEBUG_PRINTLN("opened fauxmo config file");
        size_t size = fauxmoConfigFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        fauxmoConfigFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonFauxmoBuffer;
        JsonObject& fauxmoJson = jsonFauxmoBuffer.parseObject(buf.get());
       
        fauxmoJson.printTo(Serial);

        if (fauxmoJson.success()) {
          DEBUG_PRINTLN("\nparsed fauxmo json");
          for (JsonObject::iterator it=fauxmoJson.begin(); it!=fauxmoJson.end(); ++it)
          {
            JsonObject& data = it->value;
            String key = it->key;
            int code_on = data["on"];
            int code_off = data["off"];
            
            switchstates[num_switches].title = key;
            switchstates[num_switches].code_on = code_on;
            switchstates[num_switches].code_off = code_off;
            num_switches++;
          }
        }
      }
    }
  } else {
    DEBUG_PRINTLN("failed to mount FS");
  }
  //end read


  // ***************************************************************************
  // Init WifiManager
  // ***************************************************************************
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_topic);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(180);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(AP_NAME, AP_PASS)) {
    DEBUG_PRINTLN("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  DEBUG_PRINTLN("connected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_topic, custom_mqtt_topic.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    DEBUG_PRINTLN("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_topic"] = mqtt_topic;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      DEBUG_PRINTLN("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  DEBUG_PRINT("Your local IP is: ");
  DEBUG_PRINTLN(WiFi.localIP());
  
  // ***************************************************************************
  // Init PubSubClient
  // ***************************************************************************
  client.setServer(mqtt_server, atoi(mqtt_port));
  client.setCallback(callback_mqtt);

  // ***************************************************************************
  // Include: Request handlers
  // ***************************************************************************
  #include "request_handlers.h"
  
  // ***************************************************************************
  // Init FauxmoESP
  // ***************************************************************************
  for (int i=0; i < num_switches; i++) {
    DEBUG_PRINT("Added fauxmoESP device: ");
    DEBUG_PRINTLN(switchstates[i].title);
    DEBUG_PRINTLN(switchstates[i].code_on);
    DEBUG_PRINTLN(switchstates[i].code_off);
    fauxmo.addDevice(switchstates[i].title.c_str());
  }

  fauxmo.onMessage([](unsigned char device_id, const char * device_name, bool state) {
    Serial.printf("[MAIN] Device #%d (%s) state: %s\n", device_id, device_name, state ? "ON" : "OFF");

    int code = (state) ? switchstates[device_id].code_on : switchstates[device_id].code_off;
    mySwitch.send(code, 24);
  });
}


void loop() {
  if ( digitalRead(PIN_MODEBUTTON) == LOW ) {
    DEBUG_PRINTLN("Mode button triggered, start config portal");
  
    //WiFiManager
    WiFiManager wifiManager;

    //add all your parameters here
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_topic);

    if (!wifiManager.startConfigPortal(AP_NAME, AP_PASS)) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
  }

  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();

  fauxmo.handle();

  if (mySwitch.available()) {
    int value = mySwitch.getReceivedValue();
    
    if (value == 0) {
      DEBUG_PRINT("Unknown encoding");
    } else {
      DEBUG_PRINT("Received ");
      DEBUG_PRINT( mySwitch.getReceivedValue() );
      DEBUG_PRINT(" / ");
      DEBUG_PRINT( mySwitch.getReceivedBitlength() );
      DEBUG_PRINT("bit ");
      DEBUG_PRINT("Protocol: ");
      DEBUG_PRINTLN( mySwitch.getReceivedProtocol() );

      char topic[64];
      sprintf(topic, "%s/incoming/%d", mqtt_topic, mySwitch.getReceivedBitlength());

      unsigned long val = mySwitch.getReceivedValue();
      client.publish(topic, String(val).c_str());
    }

    mySwitch.resetAvailable();
  }
}
