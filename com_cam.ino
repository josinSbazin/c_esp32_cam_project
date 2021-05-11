#include <WiFiManager.h>
#include <ArduinoJson.h> //ArduinoJSON6
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "Esp32MQTTClient.h"
#include "SPIFFS.h"
#include "esp_camera.h"

DynamicJsonDocument DEVICE(2048);
DynamicJsonDocument CONFIG(2048);
DynamicJsonDocument CONFIGTEMP(2048);

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

#include "pins.h"

#define FLASH_LED_PIN 4

char const *cert PROGMEM = 
"-----BEGIN CERTIFICATE-----\n" \
"MIIEZTCCA02gAwIBAgIQQAF1BIMUpMghjISpDBbN3zANBgkqhkiG9w0BAQsFADA/\n" \
"MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n" \
"DkRTVCBSb290IENBIFgzMB4XDTIwMTAwNzE5MjE0MFoXDTIxMDkyOTE5MjE0MFow\n" \
"MjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxCzAJBgNVBAMT\n" \
"AlIzMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuwIVKMz2oJTTDxLs\n" \
"jVWSw/iC8ZmmekKIp10mqrUrucVMsa+Oa/l1yKPXD0eUFFU1V4yeqKI5GfWCPEKp\n" \
"Tm71O8Mu243AsFzzWTjn7c9p8FoLG77AlCQlh/o3cbMT5xys4Zvv2+Q7RVJFlqnB\n" \
"U840yFLuta7tj95gcOKlVKu2bQ6XpUA0ayvTvGbrZjR8+muLj1cpmfgwF126cm/7\n" \
"gcWt0oZYPRfH5wm78Sv3htzB2nFd1EbjzK0lwYi8YGd1ZrPxGPeiXOZT/zqItkel\n" \
"/xMY6pgJdz+dU/nPAeX1pnAXFK9jpP+Zs5Od3FOnBv5IhR2haa4ldbsTzFID9e1R\n" \
"oYvbFQIDAQABo4IBaDCCAWQwEgYDVR0TAQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8E\n" \
"BAMCAYYwSwYIKwYBBQUHAQEEPzA9MDsGCCsGAQUFBzAChi9odHRwOi8vYXBwcy5p\n" \
"ZGVudHJ1c3QuY29tL3Jvb3RzL2RzdHJvb3RjYXgzLnA3YzAfBgNVHSMEGDAWgBTE\n" \
"p7Gkeyxx+tvhS5B1/8QVYIWJEDBUBgNVHSAETTBLMAgGBmeBDAECATA/BgsrBgEE\n" \
"AYLfEwEBATAwMC4GCCsGAQUFBwIBFiJodHRwOi8vY3BzLnJvb3QteDEubGV0c2Vu\n" \
"Y3J5cHQub3JnMDwGA1UdHwQ1MDMwMaAvoC2GK2h0dHA6Ly9jcmwuaWRlbnRydXN0\n" \
"LmNvbS9EU1RST09UQ0FYM0NSTC5jcmwwHQYDVR0OBBYEFBQusxe3WFbLrlAJQOYf\n" \
"r52LFMLGMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjANBgkqhkiG9w0B\n" \
"AQsFAAOCAQEA2UzgyfWEiDcx27sT4rP8i2tiEmxYt0l+PAK3qB8oYevO4C5z70kH\n" \
"ejWEHx2taPDY/laBL21/WKZuNTYQHHPD5b1tXgHXbnL7KqC401dk5VvCadTQsvd8\n" \
"S8MXjohyc9z9/G2948kLjmE6Flh9dDYrVYA9x2O+hEPGOaEOa1eePynBgPayvUfL\n" \
"qjBstzLhWVQLGAkXXmNs+5ZnPBxzDJOLxhF2JIbeQAcH5H0tZrUlo5ZYyOqA7s9p\n" \
"O5b85o3AM/OJ+CktFBQtfvBhcJVd9wvlwPsk+uyOy2HI7mNxKKgsBTt375teA2Tw\n" \
"UdHkhVNcsAKX1H7GNNLOEADksd86wuoXvg==\n" \
"-----END CERTIFICATE-----\n";

char const *topic_TAKE_PHOTO_prefix = "cams/take_photo/";
char const *topic_CONFIG_prefix = "cams/config/";
char const *topic_SEND_PHOTO_prefix = "cams/photo/";
char const *topic_STATUS_prefix = "cams/status/";

char const *topic_TAKE_PHOTO, *topic_CONFIG, *topic_SEND_PHOTO, *topic_STATUS;

char const *client_id;
char const *ap_name, *ap_pass, *mqtt_server, *mqtt_user, *mqtt_pass;

WiFiClientSecure esp_client;
PubSubClient client(esp_client);

void callback(String topic, byte* message, unsigned int length) {
  Serial.println("In callback");
  Serial.println(topic);
  Serial.println(topic_TAKE_PHOTO);

  if (topic == topic_TAKE_PHOTO) {
    take_picture();
  }
  if (topic == topic_CONFIG) {
    deserializeJson(CONFIGTEMP, message, length);
    edit_config();
  }
}

void setup() {
  Serial.begin(115200);
  WiFiManager wifiManager;

  pinMode(FLASH_LED_PIN, OUTPUT);

  loadDeviceData();

  wifiManager.setHostname(client_id);
  
  //wifiManager.resetSettings();
  
  std::vector<const char *> menu = {"wifi","exit"};
  wifiManager.setMenu(menu);
  wifiManager.setClass("invert");
  wifiManager.setConfigPortalTimeout(120); 
  wifiManager.setTimeout(20);
  wifiManager.setAPStaticIPConfig(IPAddress(192,168,0,99), IPAddress(192,168,0,1), IPAddress(255,255,255,0));
  
  bool res = wifiManager.autoConnect(ap_name, ap_pass);

  if(!res) {
     Serial.println("Failed to connect");
     ESP.restart();
  } 
  else {
     Serial.println("connected to WiFi :)");

     camera_init();
     
     Serial.print("Set mqtt server -> ");
     Serial.println(mqtt_server);

     esp_client.setCACert(cert);
     client.setServer(mqtt_server, 8883);
     client.setCallback(callback);
     
     load_config();
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

bool loadDeviceData() {  
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return false;
  }

  File config_file = SPIFFS.open("/device.json", "r");
  if (!config_file) {
    Serial.println("Failed to open file for reading");
    return false;
  }
  
  String  config_string = config_file.readString();
  
  Serial.println("Open config");
  Serial.println(config_string);
  
  deserializeJson(DEVICE, config_string);
  config_file.close();
  
  ap_name = (const char*)DEVICE["ap_ssid"];
  ap_pass = (const char*)DEVICE["ap_pass"];
  mqtt_server = (const char*)DEVICE["mqtt_server"];
  mqtt_user = (const char*)DEVICE["mqtt_user"];
  mqtt_pass = (const char*)DEVICE["mqtt_pass"];
  
  String mac_address = WiFi.macAddress();
  
  Serial.print("Mac address: ");
  Serial.println(mac_address);

  String id = "cam>" + mac_address;

  client_id = strdup(id.c_str());
  
  return true;
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    Serial.print(mqtt_user);
    Serial.print(" // ");
    Serial.print(mqtt_pass);
    Serial.print("...");

//    char take_photo_buff[strlen(topic_TAKE_PHOTO_prefix) + strlen(client_id) + 1];
//    doConcat(topic_TAKE_PHOTO_prefix, client_id, take_photo_buff);
//    topic_TAKE_PHOTO = take_photo_buff;
//
//    char config_buff[strlen(topic_CONFIG_prefix) + strlen(client_id) + 1];
//    doConcat(topic_CONFIG_prefix, client_id, config_buff);
//    topic_CONFIG = config_buff;
//
//    char send_photo_buff[strlen(topic_SEND_PHOTO_prefix) + strlen(client_id) + 1];
//    doConcat(topic_SEND_PHOTO_prefix, client_id, send_photo_buff);
//    topic_SEND_PHOTO = send_photo_buff;
//
//    char status_buff[strlen(topic_STATUS_prefix) + strlen(client_id) + 1];
//    doConcat(topic_STATUS_prefix, client_id, status_buff);
//    topic_STATUS = status_buff;

    topic_TAKE_PHOTO = concat(topic_TAKE_PHOTO_prefix, client_id);
    topic_CONFIG = concat(topic_CONFIG_prefix, client_id);
    topic_SEND_PHOTO = concat(topic_SEND_PHOTO_prefix, client_id);
    topic_STATUS = concat(topic_STATUS_prefix, client_id);
      
    if (client.connect(client_id, mqtt_user, mqtt_pass, topic_STATUS, 0, true, "false")) {
      Serial.print("connected->");
      Serial.println(client_id);

      Serial.print("subscribe->");
      Serial.println(topic_TAKE_PHOTO);

      client.subscribe(topic_TAKE_PHOTO);

      Serial.print("subscribe->");
      Serial.println(topic_CONFIG);
   
      client.subscribe(topic_CONFIG);

      Serial.print("publish->");
      Serial.println(topic_STATUS);
      
      client.publish(topic_STATUS, "true", true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.print(" ");

      char err_buf[100];
      if (esp_client.lastError(err_buf, 100) < 0) {
          Serial.println(err_buf);
      } else {
          Serial.println("Connection error");
      }

      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void doConcat(const char *a, const char *b, char *out) {
    strcpy(out, a);
    strcat(out, b);
}

char* concat(const char *a, const char *b) {
    char *out = new char[strlen(a) + strlen(b) + 1];
    strcpy(out, a);
    strcat(out, b);
    return out;
}

char *strdup(const char *src_str) {
    char *new_str = new char[strlen(src_str) + 1];
    strcpy(new_str, src_str);
    return new_str;
}
