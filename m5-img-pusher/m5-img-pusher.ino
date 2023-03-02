#include "OV2640.h"
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include "CRtspSession.h"
#include "OV2640Streamer.h"
#include "SimStreamer.h"

#include "battery.h"

#define ssid     "ssid"
#define password "password"

OV2640 cam;

WiFiServer rtspServer(8554);
CStreamer *streamer;

#include "WiFiClientSecure.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <ArduinoJson.h>

#include "Base64.h"

/************************* WiFi Access Point *********************************/

#define WLAN_SSID "dirker"
#define WLAN_PASS "alphabit"

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "mqtt.showstream.us"

// Using port 8883 for MQTTS
#define AIO_SERVERPORT  8883

// Adafruit IO Account Configuration
// (to obtain these values, visit https://io.adafruit.com and click on Active Key)
#define AIO_USERNAME "client_user"
#define AIO_KEY      "client"

/************ Global State (you don't need to change this!) ******************/

// WiFiFlientSecure for SSL/TLS support
WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/

// Setup a feed called 'test' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
//Adafruit_MQTT_Publish test = Adafruit_MQTT_Publish(&mqtt, "/"); // = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/test");
char baseMacChr[18] = {0};

void setup() {
    bat_init();
    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);
    Serial.begin(115200);
    while (!Serial) {
        ;
    }

    camera_config_t timercam_config{

        .pin_pwdn     = -1,
        .pin_reset    = 15,
        .pin_xclk     = 27,
        .pin_sscb_sda = 25,
        .pin_sscb_scl = 23,

        .pin_d7       = 19,
        .pin_d6       = 36,
        .pin_d5       = 18,
        .pin_d4       = 39,
        .pin_d3       = 5,
        .pin_d2       = 34,
        .pin_d1       = 35,
        .pin_d0       = 32,
        .pin_vsync    = 22,
        .pin_href     = 26,
        .pin_pclk     = 21,
        .xclk_freq_hz = 20000000,
        .ledc_timer   = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size   = FRAMESIZE_VGA,
        .jpeg_quality = 12,  // 0-63 lower numbers are higher quality
        .fb_count = 2  // if more than one i2s runs in continous mode.  Use only
                       // with jpeg
    };

    cam.init(timercam_config);

    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1);
    s->set_hmirror(s, 0);

    IPAddress ip;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(F("."));
    }
    ip = WiFi.localIP();
    Serial.println(F("WiFi connected"));
    Serial.print("RTSP URL: rtsp://");
    Serial.print(ip);
    Serial.println(":8554/mjpeg/1");
    rtspServer.begin();
    // streamer = new SimStreamer(true);             // our streamer for UDP/TCP
    // based RTP transport
    streamer = new OV2640Streamer(cam);  // our streamer for UDP/TCP based RTP transport

    uint8_t baseMac[6];
    // Get MAC address for WiFi station
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);  

    // Set Adafruit IO's root CA
    //client.setCACert(adafruitio_root_ca);
    client.setInsecure();
}

void loop() {
    // If we have an active client connection, just service that until gone
    streamer->handleRequests(0);  // we don't use a timeout here,
    // instead we send only if we have new enough frames
    uint32_t now = millis();
    if (streamer->anySessions()) {
        streamer->streamImage(now);
    }

    WiFiClient rtspClient = rtspServer.accept();
    if (rtspClient) {
        Serial.print("client: ");
        Serial.print(rtspClient.remoteIP());
        Serial.println();
        streamer->addSession(rtspClient);
    }

  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();
  Adafruit_MQTT_Publish test = Adafruit_MQTT_Publish(&mqtt, baseMacChr);
  // Now we can publish stuff!
  Serial.print(F("\nSending val "));
  Serial.print(x);
  Serial.print(F(" to test feed..."));

  // encoding
  char inputString[] = "Base64EncodeExample";
  int inputStringLength = strlen(inputString);

  Serial.print("Input string is:\t");
  Serial.println(inputString);

  Serial.println();

  int encodedLength = Base64.encodedLength(inputStringLength);
  char encodedString[encodedLength + 1];
  Base64.encode(encodedString, inputString, inputStringLength);
  Serial.print("Encoded string is:\t");
  Serial.println(encodedString);

  const int capacity = JSON_OBJECT_SIZE(4);
  StaticJsonDocument<capacity> doc;
  doc["event"] = "newSnapshot";
  doc["macAddress"] = baseMacChr;
  doc["fmt"] = "jpg";
  doc["data"] = encodedString;

  //int asdf = encodedLength + 200;
  // 512k
  //char jsonDoc[asdf];
  std::string jsonDoc = ""; //new std::string();
  serializeJson(doc, jsonDoc);
  //if (! test.publish(x++)) {
  if (! test.publish(jsonDoc.c_str())) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }

  // wait a couple seconds to avoid rate limit
  delay(2000);

}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }

  Serial.println("MQTT Connected!");
}