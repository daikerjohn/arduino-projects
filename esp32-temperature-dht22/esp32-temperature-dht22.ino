/**
   ESP32 + DHT22 Example for Wokwi
   
   https://wokwi.com/arduino/projects/322410731508073042
*/

#include <WiFi.h>
#include "WiFiClientSecure.h"

#include <HTTPClient.h>

#include "DHTesp.h"

#include <Preferences.h>


const char compile_date[] = __DATE__ " " __TIME__;

Preferences prefs;

#include <AsyncTCP.h>
#include <ESPAsyncWebSrv.h>
// Needs mod to work with ESPAsyncWebSrv above.  Original version wants ESPAsyncWeb*Server* not Srv
// https://github.com/ayushsharma82/AsyncElegantOTA
#include "libs/AsyncElegantOTA/AsyncElegantOTA.h"
// Set web server port number to 80
AsyncWebServer server(80);

String header;
String status_str = "";
String current_status = "";

#define WLAN_SSID "dirker"
#define WLAN_PASS "alphabit"

//Your Domain name with URL path or IP address with path
const char* serverUrl = "https://lhbqdvca46.execute-api.us-west-2.amazonaws.com/dev/temp";

const int DHT_PIN = 15;
const int RELAY_PIN = 21;

DHTesp dhtSensor;
WiFiClientSecure client;

long sleep_timeout = 5000;
long next_sleep = 0;
bool first_boot = true;

// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;  // will store last time LED was updated

unsigned long currentMillis = 0;
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;

// Set timer to 5 seconds (5000)
const unsigned long timerDelayMS = 5000;
// Set timer to 60 seconds (60000)
//const unsigned long timerDelayMS = 60000;
// Timer set to 10 minutes (600000)
//const unsigned long timerDelayMS = 600000;


bool load_running = false;
// 5 minutes
long shut_cool_ms = 600000;
// Prevent any automatic power-on for 120 seconds
long next_available_startup = millis() + 120000;

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void resetSleepTimer() {
  next_sleep = millis() + sleep_timeout;
}

void turn_relay_on() {
  load_running = true;
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println("Load on");
}
void turn_relay_off() {
  load_running = false;
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("Load off");
}

void handle_webserver_connection() {
  server.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
    //harvest_data();
    status_str = "";
    //update_decisions(false);
    request->send(200, "text/html", statsIndex());
  });
  server.on("/heap", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
  server.on("/max", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(ESP.getMaxAllocHeap()));
  });

  server.on("/new", HTTP_GET, [] (AsyncWebServerRequest *request) {
    status_str = "";
    //harvest_data();
    //update_decisions(false);
    request->send(200, "text/html", statsIndex());
  });

  server.on("/config", HTTP_GET, [] (AsyncWebServerRequest *request) {
    status_str = "";
    //harvest_data();
    //update_decisions();
    request->send(200, "text/html", configIndex());
  });

  // Load control
  server.on("/load/on", HTTP_GET, [] (AsyncWebServerRequest *request) {
    turn_relay_on();
    request->redirect("/");
  });
  server.on("/load/off", HTTP_GET, [] (AsyncWebServerRequest *request) {
    turn_relay_off();
    request->redirect("/");
  });

  server.on("/set", HTTP_GET, [] (AsyncWebServerRequest *request) {
    //int new_value;
    String input_value;
    /*
    if (request->hasArg("batt_cap_ah") || request->hasArg("batt_soc_min") || request->hasArg("batt_soc_start") || request->hasArg("batt_volt_min") || request->hasArg("bat_volt_start") || request->hasArg("shut_cool_ms")) {
      if(prefs.begin("chicken-app", false)) {
        if (request->hasArg("batt_cap_ah")) {
          input_value = request->arg("batt_cap_ah");
          batt_cap_ah = input_value.toInt();
          prefs.putInt("batt_cap_ah", batt_cap_ah);
        }
        if (request->hasArg("batt_soc_min")) {
          input_value = request->arg("batt_soc_min");
          batt_soc_min = input_value.toInt();
          prefs.putInt("batt_soc_min", batt_soc_min);
        }
        if (request->hasArg("batt_soc_start")) {
          input_value = request->arg("batt_soc_start");  
          batt_soc_start = input_value.toInt();
          prefs.putInt("batt_soc_start", batt_soc_start);
        }
        if (request->hasArg("batt_volt_min")) {
          input_value = request->arg("batt_volt_min");  
          batt_volt_min = input_value.toFloat();
          prefs.putFloat("batt_volt_min", batt_volt_min);
        }
        if (request->hasArg("bat_volt_start")) {
          input_value = request->arg("bat_volt_start");  
          bat_volt_start = input_value.toFloat();
          prefs.putFloat("bat_volt_start", bat_volt_start);
        }
        if (request->hasArg("shut_cool_ms")) {
          input_value = request->arg("shut_cool_ms");  
          shut_cool_ms = input_value.toInt();
          prefs.putLong("shut_cool_ms", shut_cool_ms);
          next_available_startup = millis() + shut_cool_ms;
        }
        prefs.end();
      } else {
        request->send(200, "text/plain", "Failed to being() prefs for write");
      }
    }
    */
    request->redirect("/config");
  });

  server.onNotFound(notFound);
}

void AppendStatus(String asdf) {
  Serial.println(asdf);
  status_str += "<p>" + String(asdf) + "</p>\n";
}

void setup() {
  Serial.begin(115200);
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

  pinMode(RELAY_PIN, OUTPUT);
  turn_relay_off();

  prefs.begin("chicken-app", false);
  //batt_cap_ah = prefs.getInt("batt_cap_ah", 50);
  //batt_soc_min = prefs.getInt("batt_soc_min", 50);
  //batt_soc_start = prefs.getInt("batt_soc_start", 65);
  //batt_volt_min = prefs.getDouble("batt_volt_min", 11.5);
  //bat_volt_start = prefs.getDouble("bat_volt_start", 13);
  //shut_cool_ms = prefs.getLong("shut_cool_ms", 300000);
  // Close the Preferences
  prefs.end();

  /*
  First we configure the wake up source
  We set our ESP32 to wake up for an external trigger.
  There are two types for ESP32, ext0 and ext1 .
  ext0 uses RTC_IO to wakeup thus requires RTC peripherals
  to be on while ext1 uses RTC Controller so doesnt need
  peripherals to be powered on.
  Note that using internal pullups/pulldowns also requires
  RTC peripherals to be turned on.
  */
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0); //1 = High, 0 = Low

  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  //WiFi.begin("Wokwi-GUEST", "");
  delay(2000);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());
  Serial.println("MAC address: "); Serial.println(WiFi.macAddress());

  client.setInsecure();

  handle_webserver_connection();
  // Start webserver
  server.begin();
}

void loop() {
  if(first_boot) {
    Serial.println("Entering loop for first time");
  }

  currentMillis = millis();
   // Only read info every 10th loop
  if ((currentMillis - lastTime) > timerDelayMS || first_boot) {
    Serial.println("Updating data");
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    TempAndHumidity  data = dhtSensor.getTempAndHumidity();
    Serial.println("Temp: " + String(data.temperature, 2) + "°C");
    double tempF = (data.temperature * (9.0/5.0)) + 32.0;
    //if(tempF < 45) {
    //  turn_relay_on();
    //} else {
    //  turn_relay_off();
    //}
    Serial.println("TempF: " + String(tempF, 2) + "°F");
    Serial.println("Humidity: " + String(data.humidity, 1) + "%");
    Serial.println("---");

    String asdf = "{";

    asdf += "\"temp\":" + String(data.temperature, 2) + ",";
    asdf += "\"tempF\":" + String(tempF, 2) + ",";
    asdf += "\"humidity\":" + String(data.humidity, 1);

    asdf += "}";

    if ((WiFi.status() != WL_CONNECTED)) {
      Serial.print(millis());
      Serial.println("Reconnecting to WiFi...");
      WiFi.disconnect();
      WiFi.reconnect();
    }

    /*
    if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;
    
      // Your Domain name with URL path or IP address with path
      http.begin(client, serverUrl);
      
      // If you need Node-RED/server authentication, insert user and password below
      //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
      
      // Specify content-type header
      http.addHeader("Content-Type", "application/json");
      // Data to send with HTTP POST
      //String httpRequestData = theStr; //"api_key=tPmAT5Ab3j7F9&sensor=BME280&value1=24.25&value2=49.54&value3=1005.14";           
      // Send HTTP POST request
      Serial.println("POSTing http data");
      int httpResponseCode = http.POST(asdf);
      
      // If you need an HTTP request with a content type: application/json, use the following:
      //http.addHeader("Content-Type", "application/json");
      //int httpResponseCode = http.POST("{\"api_key\":\"tPmAT5Ab3j7F9\",\"sensor\":\"BME280\",\"value1\":\"24.25\",\"value2\":\"49.54\",\"value3\":\"1005.14\"}");

      // If you need an HTTP request with a content type: text/plain
      //http.addHeader("Content-Type", "text/plain");
      //int httpResponseCode = http.POST("Hello, World!");
      
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
        
      // Free resources
      http.end();
    } else {
      Serial.println("WiFi Disconnected");
    }
    */


    
    /*
    if(next_sleep != 0 && millis() > next_sleep) {
      //Go to sleep now
      Serial.println("Going to sleep now");
      delay(1000);
      esp_deep_sleep_start();
    }
    */
    lastTime = millis();
  }

  first_boot = false;
}

String htmlHead(bool includeReload = true) {
  // Display the HTML web page
  String data = "<!DOCTYPE html><html>";
  data += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  data += "<link rel=\"icon\" href=\"data:,\">";
  // CSS to style the on/off buttons 
  // Feel free to change the background-color and font-size attributes to fit your preferences
  data += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  data += ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;";
  data += "text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
  data += ".button2 {background-color: #555555;}</style>";

  data += "<script>";
  if(includeReload) {
    data += "setTimeout(function(){ window.location.reload(); }, 5000);";
  }
  data += "</script>";

  data += "</head>";
  return data;
}

String htmlFoot() {
  String data = "<p>Compiled: " + String(compile_date) + "</p>";
  data += "</body></html>";
  return data;
}

String statsIndex() {
  String data = htmlHead();
  
  // Web Page Heading
  data += "<body><h1>ESP32 Web Server - With OTAv3</h1>";
  
  // Display current state of the Renogy load
  if(load_running) {
    data += "<p>Light is On!</p>";
  } else {
    data += "<p>Light is off.</p>";
  }
  //if(automatic_decision != "") {
  //  data += "<p>Last Automatic: " + String(automatic_decision) + "</p>";
  //}
  data += "<p>Automatics</p>";
  //data += automatic_decisions.html();

  if(current_status != "") {
    data += "<p>Current: " + String(current_status) + "</p>";
  }
  data += "<p>" + String(status_str) + "</p>";
  // If the load_running is on, it displays the OFF button
  if (load_running) {
    data += "<p><a href=\"/load/off\"><button class=\"button button2\">OFF</button></a></p>";
  } else {
    data += "<p><a href=\"/load/on\"><button class=\"button\">ON</button></a></p>";
  }

  data += htmlFoot();
  return data;
}


String configIndex() {
  String data = htmlHead(false);

/*
  data += "<form action=\"/set\">Enter an batt_cap_ah: <input type=\"text\" name=\"batt_cap_ah\" value=\"" + String(batt_cap_ah) + "\"><br/>";
  data += "<input type=\"submit\" value=\"Submit\"></form>";
  data += "<form action=\"/set\">Enter an batt_soc_min: <input type=\"text\" name=\"batt_soc_min\" value=\"" + String(batt_soc_min) + "\"><br/>";
  data += "<input type=\"submit\" value=\"Submit\"></form>";
  data += "<form action=\"/set\">Enter an batt_soc_start: <input type=\"text\" name=\"batt_soc_start\" value=\"" + String(batt_soc_start) + "\"><br/>";
  data += "<input type=\"submit\" value=\"Submit\"></form>";
  data += "<form action=\"/set\">Enter an batt_volt_min: <input type=\"text\" name=\"batt_volt_min\" value=\"" + String(batt_volt_min) + "\"><br/>";
  data += "<input type=\"submit\" value=\"Submit\"></form>";
  data += "<form action=\"/set\">Enter an bat_volt_start: <input type=\"text\" name=\"bat_volt_start\" value=\"" + String(bat_volt_start) + "\"><br/>";
  data += "<input type=\"submit\" value=\"Submit\"></form>";
  data += "<form action=\"/set\">Enter an shut_cool_ms: <input type=\"text\" name=\"shut_cool_ms\" value=\"" + String(shut_cool_ms) + "\"><br/>";
  data += "<input type=\"submit\" value=\"Submit\"></form>";
  */

  data += htmlFoot();
  return data;
}