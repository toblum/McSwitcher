#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>
#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

WiFiClient espClient;
PubSubClient client(espClient);

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


// Helper
#define MODEBUTTON 0
#define SERIALDEBUG true
#ifdef SERIALDEBUG
#define         DEBUG_PRINT(x)    Serial.print(x)
#define         DEBUG_PRINTLN(x)  Serial.println(x)
#else
#define         DEBUG_PRINT(x)
#define         DEBUG_PRINTLN(x)
#endif


// flag for saving data
bool shouldSaveConfig = false;

// callback notifying us of the need to save config
void saveConfigCallback () {
  DEBUG_PRINTLN("Should save config");
  shouldSaveConfig = true;
}

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
  //rf.send(str_topic, str_payload);

  int bits = str_topic.toInt();
  if (bits == 0) {
    bits = 24;
  }
  DEBUG_PRINT("Bits:");
  DEBUG_PRINTLN(bits);
  DEBUG_PRINT("Payload:");
  DEBUG_PRINTLN(str_payload.toInt());

  mySwitch.send(str_payload.toInt(), bits);

  /*
   * if (str_payload.startsWith("0")) {
    mySwitch.switchOff("10010", "00010"); 
  } else {
    mySwitch.switchOn("10010", "00010"); 
  }
  */
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
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}




void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  DEBUG_PRINTLN();

  mySwitch.enableReceive(D2);
  mySwitch.enableTransmit(D1);
  //mySwitch.setRepeatTransmit(5);

  pinMode(MODEBUTTON, INPUT);  // MODEBUTTON as input for Config mode selection

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
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
  } else {
    DEBUG_PRINTLN("failed to mount FS");
  }
  //end read


  //WiFiManager
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
  if (!wifiManager.autoConnect("AutoConnectAP", "SwitcherMc")) {
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

  DEBUG_PRINTLN("local ip");
  DEBUG_PRINTLN(WiFi.localIP());
  
  // Init PubSubClient
  client.setServer(mqtt_server, atoi(mqtt_port));
  client.setCallback(callback_mqtt);
}


void loop() {
  if ( digitalRead(MODEBUTTON) == LOW ) {
    DEBUG_PRINTLN("+++");
  
    //WiFiManager
    WiFiManager wifiManager;

    //add all your parameters here
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_topic);

    if (!wifiManager.startConfigPortal("OnDemandAP")) {
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

  if (mySwitch.available()) {
    int value = mySwitch.getReceivedValue();
    
    if (value == 0) {
      Serial.print("Unknown encoding");
    } else {
      Serial.print("Received ");
      Serial.print( mySwitch.getReceivedValue() );
      Serial.print(" / ");
      Serial.print( mySwitch.getReceivedBitlength() );
      Serial.print("bit ");
      Serial.print("Protocol: ");
      Serial.println( mySwitch.getReceivedProtocol() );

      char topic[64];
      sprintf(topic, "%s/incoming/%d", mqtt_topic, mySwitch.getReceivedBitlength());

      unsigned long val = mySwitch.getReceivedValue();
      client.publish(topic, String(val).c_str());
    }

    mySwitch.resetAvailable();
  }
}
