//www.diyusthad.com
#include <WiFi.h>
//#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <TelnetPrint.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include <ESPAsyncWebSrv.h>

// Needs mod to work with ESPAsyncWebSrv above.  Original version wants ESPAsyncWeb*Server* not Srv
// https://github.com/ayushsharma82/AsyncElegantOTA
#include "libs/AsyncElegantOTA/AsyncElegantOTA.h"

String ssid_str = "";
String password_str = "";

unsigned long currentMillis = 0;
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
unsigned long lastWebTime = 0;

// We'll upload to the web every sixty seconds
unsigned long timerWebDelayMS = 60000;

/*
void loop() {
  lcd.scrollDisplayLeft();
  delay(500);
}
*/
// 3 coin, two lines each

int refresh_interval_min = 5;
unsigned long coin_delay_ms = 10000;

// Json Variable to Hold Sensor Readings
JsonDocument priceInfo;

#define NUM_COINS 10
String coin_symbols[NUM_COINS] = {""};
double coin_balances[NUM_COINS] = {44001};


#define COIN_SYMBOL 0
#define COIN_HOLDINGS 1
#define COIN_PRECISION 2
#define COIN_URL 3
#define COIN_END 4
String coinInfo[NUM_COINS][COIN_END] = {
  {"DERO", "163", "3", "https://api.nonkyc.io/api/v2/market/getbysymbol/DERO_USDT"},
  {"SPR", "500000", "6", "https://api.nonkyc.io/api/v2/market/getbysymbol/SPR_USDT"},
  {"XEL", "2.12", "3", "https://api.nonkyc.io/api/v2/market/getbysymbol/XEL_USDT"},
};

#define NUM_LINES 2
String coinStrings[NUM_COINS][NUM_LINES] = {
  {"coin one, line one", "coin one, line two"},
  {"coin two, line one", "coin two, line two"},
  {"coin three, line one", "coin three, line two"}
};

double coinBalancesByCoin[NUM_COINS] = {0.0, 0.0, 0.0};

String Hello="Hello World!";
bool first_loop = true;


String getTimestamp(bool doShort=false) {
  time_t now;
  struct tm asdf;
  char buf[80];
  if (!getLocalTime(&asdf)) {
    //TelnetPrint.println("Failed to obtain time");
    return "";
  } else {
    time(&now);
    asdf = *localtime(&now);
  }
  if(doShort) {
    strftime(buf, sizeof(buf), "%m-%d %H:%M:%S", &asdf);
  } else {
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z", &asdf);
  }
  return String(buf);
  //printf("%s\n", buf);
  //return now;
}

#include <LCD_I2C.h>

LCD_I2C lcd(0x3F, 16, 2); // Default address of most PCF8574 modules, change according

const char compile_date[] = __DATE__ " " __TIME__;

Preferences prefs;

// Set web server port number to 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

size_t content_len;




const char *headHtml = R"literal(
<!DOCTYPE html><html>
<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">
<link rel=\"icon\" href=\"data:,\">
<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}
.button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;
text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}
.button2 {background-color: #555555;}</style>
<script>

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
// Init web socket when the page loads
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
}

function getReadings(){
    websocket.send("getReadings");
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

// When websocket is established, call the getReadings() function
function onOpen(event) {
    console.log('Connection opened');
    //getReadings();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

// Function that receives the message from the ESP32 with the readings
function onMessage(event) {
    console.log(event.data);
    var myObj = JSON.parse(event.data);
    var keys = Object.keys(myObj);

    for (var i = 0; i < keys.length; i++){
        var key = keys[i];
        document.getElementById(key).innerHTML = myObj[key];
    }
}

)literal";

String htmlHead(unsigned long reloadDelayMs = 0) {
  String data = String(headHtml);
  if (reloadDelayMs > 5000) {
    data += "setTimeout(function(){ window.location.reload(); }, " + String(reloadDelayMs) + ");";
  }
  data += "</script>";

  data += "</head>";
  return data;
}

String htmlFoot() {
  String data = "<p>Compiled: " + String(compile_date) + "</p>";
  data += "<p><a href='/'>Home</a> | <a href='/config'>Config</a> | <a href='/update'>Update</a> | <a href='/restart'>restart</a></p>";
  //data += "<p><a href='/'>Home</a> | <a href='/raw'>raw</a> | <a href='/json'>json</a> | <a href='/config'>Config</a> | <a href='/update'>Update</a> | <a href='/restart'>restart</a></p>";
  //data += "<br/>";
  //data += "<p><a href='/ble'>BLE</a> | <a href='/bledisc'>BLE Disconn</a> | <a href='/bleconn'>BLE Connect</a> | <a href='/blereq'>BLE Request</a> | <a href='/bleclearstr'>BLE ClearString</a> | <a href='/bleinit'>BLE Init</a></p>";
  data += "</body></html>";
  return data;
}

String configIndex() {
  String data = htmlHead(0);

  data += "<form action=\"/set\">Refresh interval: <input type=\"text\" name=\"refresh_int\" value=\"" + String(refresh_interval_min) + "\"><br/>";
  data += "Coin Delay: <input type=\"text\" name=\"coin_delay\" value=\"" + String(coin_delay_ms) + "\"><br/>";
  
  data += "<input type=\"submit\" value=\"Submit\"></form>";

  data += "<form action=\"/set\">";
  for(int x = 0; x < NUM_COINS; x++) {
    data += "Coin Symbol: <input type=\"text\" name=\"coin_sym_" + String(x) + "\" value=\"" + coin_symbols[x] + "\"><br/>";
    data += "Coin Balances: <input type=\"text\" name=\"coin_bal_" + String(x) + "\" value=\"" + String(coin_balances[x]) + "\"><br/>";  
  }
  data += "<input type=\"submit\" value=\"Submit\"></form>";

  //data += "<form action=\"/set\">BLE Auto Connect: <input type=\"text\" name=\"ble_autoconn\" value=\"" + String(ble_autoconn) + "\"><br/>";
  //data += "<input type=\"submit\" value=\"Submit\"></form>";

  data += htmlFoot();
  return data;
}
String boot_time_str = "";

String statsIndex() {
  String data = htmlHead(0);

  // Web Page Heading
  data += "<body><h1>ESP32 Web Server - With OTAv3</h1>";

  data += "<p>Boot Time: " + boot_time_str + "</p>";

  unsigned long totalBal=0.0;
  for(int x = 0; x < NUM_COINS && coin_symbols[x] != ""; x++) {
    data += "<p>" + coin_symbols[x] + ": <span id=" + coin_symbols[x] + ">" + coinStrings[x][1] + "</span></p>";
    //for(int y = 0; y < NUM_LINES; y++) {
    //  data += "<p><span id=" + coin_symbols[x] + ">" + coinStrings[x][y] + "</span></p>";
    //}

    //delay(coin_delay_ms);
    totalBal += coinBalancesByCoin[x];
  }

  data += "</br><p>" + getTimestamp(true) + "</p>";
  data += "<p>$" + String(totalBal, 2) + "</p>";

  /*
  // Display current state of the Renogy load
  data += "<p>Relay Load - " + String(load_running ? "Running" : "Stopped") + "</p>";
  data += "<p>Current Rate: " + String((float)packBasicInfo.Watts) + " watts</p>";
  data += "<p>Net Rate: " + String(net_avg_system_watts) + " watts</p>";
  if (automatic_decision != "") {
    data += "<p>Last Automatic: " + String(automatic_decision) + "</p>";
  }
  data += "<p>Automatics: " + String(allow_auto ? "On" : "Off") + "</p>";
  data += automatic_decisions.html(true);

  data += "<p>Last BMS Data Capture: " + last_data_capture_bms + "</p>";
  data += "<p>Last SCC Data Capture: " + last_data_capture_scc + "</p>";
  if (current_status != "") {
    data += "<p>Current: " + String(current_status) + "</p>";
  }
  data += "<p>" + String(status_str) + "</p>";
  data += "<p>***" + String(stat_char) + "***</p>";

  // If the load_running is on, it displays the OFF button
  //if (load_running) {
    data += "<p><a href=\"/load/off\"><button class=\"button button2\">All OFF</button></a><a href=\"/load/on\"><button class=\"button\">All ON</button></a></p>";
  //}
  for(int x = 0; x < NUM_DEVICES; x++) {
    data += "<p><a href=\"/load/off?device=" + String(x) + "\"><button class=\"button button2\">" + String(x) + " OFF</button></a>";
    data += "<a href=\"/load/on?device=" + String(x) + "\"><button class=\"button\">" + String(x) + " ON</button></a></p>";
  }
  */

  data += htmlFoot();
  return data;
}

void notFoundResp(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void handle_webserver_connection() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    //status_str = "";
    //stat_char[0] = '\0';
    //update_decisions(false);
    request->send(200, "text/html", statsIndex());
  });
  /*
  server.on("/ble", HTTP_GET, [](AsyncWebServerRequest *request) {
    //update_decisions(false);
    request->send(200, "text/plain", str_ble_status);
  });
  server.on("/http", HTTP_GET, [](AsyncWebServerRequest *request) {
    //update_decisions(false);
    request->send(200, "text/html", str_http_status);
  });
  server.on("/raw", HTTP_GET, [](AsyncWebServerRequest *request) {
    status_str = "";
    stat_char[0] = '\0';
    update_decisions(false);
    request->send(200, "text/plain", rawIndex(true));
  });
  server.on("/json", HTTP_GET, [](AsyncWebServerRequest *request) {
    status_str = "";
    stat_char[0] = '\0';
    update_decisions(false);
    request->send(200, "application/json", structToString());
  });
  */
  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
    ESP.restart();
    request->redirect("/");
  });
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
  server.on("/max", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(ESP.getMaxAllocHeap()));
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    //status_str = "";
    //stat_char[0] = '\0';
    request->send(200, "text/html", configIndex());
  });

  /*
  // Load control
  server.on("/load/on", HTTP_GET, [](AsyncWebServerRequest *request) {
    int device = -1;
    if(request->hasParam("device")) {
      device = std::stoi(request->getParam("device")->value().c_str());
    }
    TelnetPrint.println("Device: " + String(device));
    power_on_load(device, "Load On - Manual");
    last_action_was_manual = true;
    request->redirect("/");
  });
  server.on("/load/off", HTTP_GET, [](AsyncWebServerRequest *request) {
    int device = -1;
    if(request->hasParam("device")) {
      device = std::stoi(request->getParam("device")->value().c_str());
    }
    TelnetPrint.println("Device: " + String(device));
    //AsyncWebParameter* p = request->getParam("device")->value();
    //if(p->name() == "device") {
    //  TelnetPrint.println("Device:" + String(request->getParam("device")->value());
    //}
    turn_off_load(device, "Load Off - Manual");
    last_action_was_manual = true;
    request->redirect("/");
  });

  server.on("/bleinit", HTTP_GET, [](AsyncWebServerRequest *request) {
    str_ble_status += getTimestamp() + " - BLE - Connect\n";
    bleStartup();
    request->redirect("/");
  });

  server.on("/bleconn", HTTP_GET, [](AsyncWebServerRequest *request) {
    str_ble_status += getTimestamp() + " - BLE - Connect\n";
    if(!ble_autoconn) {
      bleStartup();
    }
    connectToServer();
    //turn_off_load("Load Off - Manual");
    request->redirect("/");
  });
  server.on("/blereq", HTTP_GET, [](AsyncWebServerRequest *request) {
    str_ble_status += getTimestamp() + " - BLE - Request\n";
    bleRequestData();
    //turn_off_load("Load Off - Manual");
    request->redirect("/");
  });
  
  server.on("/bleclearstr", HTTP_GET, [](AsyncWebServerRequest *request) {
    str_ble_status = getTimestamp() + " - Clear Str\n";
    request->redirect("/");
  });

  server.on("/bledisc", HTTP_GET, [](AsyncWebServerRequest *request) {

    pClientOld->disconnect();
    //BLEDevice::deinit(false);
    //doConnect = false;
    //BLE_client_connected = false;
    //doScan = false;

    str_ble_status += getTimestamp() + " - BLE - Disconnect\n";
    last_data_capture_bms = "Disconnected";
    //turn_off_load("Load Off - Manual");
    request->redirect("/");
  });
  */

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    //int new_value;
    String input_value;
    // TODO: loop here
    if (request->hasArg("refresh_int") || request->hasArg("coin_delay")
        || request->hasArg("coin_sym_0") || request->hasArg("coin_bal_0")
        || request->hasArg("coin_sym_1") || request->hasArg("coin_bal_1")
        || request->hasArg("coin_sym_2") || request->hasArg("coin_bal_2")
        || request->hasArg("coin_sym_3") || request->hasArg("coin_bal_3")
        ) {
      if (prefs.begin("crypto-app", false)) {
        if (request->hasArg("refresh_int")) {
          input_value = request->arg("refresh_int");
          refresh_interval_min = input_value.toInt();
          timerWebDelayMS = refresh_interval_min * 1000 * 60;
          prefs.putInt("refresh_int", refresh_interval_min);
        }
        if (request->hasArg("coin_delay")) {
          input_value = request->arg("coin_delay");
          coin_delay_ms = input_value.toInt();
          prefs.putInt("coin_delay", coin_delay_ms);
        }
        for(int x = 0; x < NUM_COINS; x++) {
          String sym_key = "coin_sym_" + String(x);
          if (request->hasArg(sym_key.c_str())) {
            input_value = request->arg(sym_key.c_str());
            coin_symbols[x] = input_value;
            prefs.putString(sym_key.c_str(), coin_symbols[x]);
          }
          String bal_key = "coin_bal_" + String(x);
          if (request->hasArg(bal_key.c_str())) {
            input_value = request->arg(bal_key.c_str());
            coin_balances[x] = std::stod(input_value.c_str());
            prefs.putDouble(bal_key.c_str(), coin_balances[x]);
          }
        }
        delay(100);
        prefs.end();
      } else {
        request->send(500, "text/plain", "Failed to begin() prefs for write");
      }
    }
    request->redirect("/");
  });

  server.onNotFound(notFoundResp);
}

// Websockets Start

void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    //data[len] = 0;
    //String message = (char*)data;
    // Check if the message is "getReadings"
    if (strcmp((char*)data, "getReadings") == 0) {
      //if it is, send current sensor readings
      String sensorReadings = "asdf"; //getSensorReadings();
      Serial.print(sensorReadings);
      //notifyClients(sensorReadings);
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

// Websockets End

void setup() {
  //lcd.begin(16, 2);
  //lcd.print("Scrolling Text Example");

  Serial.begin(115200);
  Serial.println("Started!");
  
  ssid_str = "dirker";
  password_str = "alphabit";
  //ssid_str = "dirker";
  //password_str = "alphabit";

  //next_available_startup = millis() + shut_cool_ms;

  WiFi.begin(ssid_str.c_str(), password_str.c_str());
  //WiFi.begin("dirker", "alphabit");
  //WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(2500);
    Serial.println("connecting...");
    Serial.println(ssid_str);
    Serial.println(password_str);
  }
  Serial.println(WiFi.localIP().toString());

  //Telnet log is accessible at port 23
  TelnetPrint.begin();

  //For UTC -8.00 : -8 * 60 * 60 : -28800
  const long gmtOffset_sec = -8 * 60 * 60;
  //Set it to 3600 if your country observes Daylight saving time; otherwise, set it to 0.
  const int daylightOffset_sec = 3600;
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");

  handle_webserver_connection();
  initWebSocket();

  // Start webserver
  server.begin();
  AsyncElegantOTA.begin(&server);  // Start AsyncElegantOTA

  lcd.begin(); // If you are using more I2C devices using the Wire library use lcd.begin(false)
               // this stop the library(LCD_I2C) from calling Wire.begin()
  lcd.backlight();


  prefs.begin("crypto-app", false);
  size_t whatsLeft = prefs.freeEntries();    // this method works regardless of the mode in which the namespace is opened.

  refresh_interval_min = prefs.getInt("refresh_int", 5);
  timerWebDelayMS = refresh_interval_min * 1000 * 60;
  coin_delay_ms = prefs.getInt("coin_delay", 5000);
  
  for(int x = 0; x < NUM_COINS; x++) {
    String sym_key = "coin_sym_" + String(x);
    String bal_key = "coin_bal_" + String(x);
    coin_symbols[x] = prefs.getString(sym_key.c_str(), "");
    coin_balances[x] = prefs.getDouble(bal_key.c_str(), 0.00);
  }
  
/*
  if(ssid == "" || strlen(ssid) < 3) {
    TelnetPrint.println("Setting default ssid...");
    prefs.putString("ssid", "dirker");
    default_set = true;
  }
  if(password == "" || strlen(password) < 3) {
    TelnetPrint.println("Setting default password...");
    prefs.putString("password", "alphabit");
    default_set = true;
  }
  if(default_set) {
    prefs.end();
    prefs.begin("solar-app", false);
  }
  ssid = prefs.getString("ssid", "dirker").c_str();
  password = prefs.getString("password", "alphabit").c_str();
  */
  // Close the Preferences
  prefs.end();
  boot_time_str = getTimestamp();
}

void loop() {
  //Serial.println("Loop...");
  
  //String reportingUrl_str = "https://api.nonkyc.io/api/v2/market/getbysymbol/SPR_USDT";
  if (first_loop) {
    TelnetPrint.println("Entering loop");
    TelnetPrint.println("Entering loop...");
  }

  currentMillis = millis();

  //Send an HTTP POST request every 10 minutes
  if ((currentMillis - lastWebTime) > timerWebDelayMS || first_loop) {
    //AppendStatus("Will POST HTTP");
    TelnetPrint.println("HTTP GET @ " + getTimestamp());
    //str_http_status = "Will GET HTTP @ " + getTimestamp() + "\n";
    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      for(int x = 0; x < NUM_COINS && coin_symbols[x] != ""; x++) {
        WiFiClientSecure wifiSecureCli;  // or WiFiClientSecure for HTTPS
        //WiFiClient wifiCli;  // or WiFiClientSecure for HTTPS
        HTTPClient theHttpClient;
        theHttpClient.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
        wifiSecureCli.setInsecure();

        // Your Domain name with URL path or IP address with path
        theHttpClient.begin(wifiSecureCli, coinInfo[x][COIN_URL]);

        // If you need Node-RED/server authentication, insert user and password below
        //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

        // Specify content-type header
        theHttpClient.addHeader("Content-Type", "application/json");

        // Data to send with HTTP POST
        //String httpRequestData = structToString();
        TelnetPrint.println("GETting http data:");
        //TelnetPrint.println(httpRequestData);
        //str_http_status += "\n";
        //str_http_status += httpRequestData;
        //str_http_status += "\n";
        int httpResponseCode = theHttpClient.GET();
        //int httpResponseCode = 200;

        TelnetPrint.println("HTTP Response code: " + String(httpResponseCode));
        String response = theHttpClient.getString();
        // Free resources
        theHttpClient.end();

        JsonDocument doc;
        deserializeJson(doc, response.c_str());

        //const char* sensor = doc["sensor"];
        //long time          = doc["time"];
        double bidNum    = doc["bestBidNumber"];
        TelnetPrint.printf("bid: %06f\n", bidNum);
        //double numCoins = coinInfo[x][COIN_HOLDINGS].toDouble();
        double numCoins = coin_balances[x];
        coinBalancesByCoin[x] = bidNum * numCoins;
        TelnetPrint.println("dollarAmount: $" + String(coinBalancesByCoin[x]));
        coinStrings[x][0] = "$" + String(bidNum, coinInfo[x][COIN_PRECISION].toInt()) + " / " + coinInfo[x][COIN_SYMBOL];
        coinStrings[x][1] = "$" + String(coinBalancesByCoin[x], 2);
        char juststringval[17];
        sprintf(juststringval, "% 16s", coinStrings[x][1]);
        coinStrings[x][1] = String(juststringval);

        priceInfo[coinInfo[x][COIN_SYMBOL]] = "$" + String(coinBalancesByCoin[x], 2);
        String jsonString = "";
        serializeJson(priceInfo, jsonString);
        notifyClients(jsonString);
        //notifyClients("{" + coinInfo[x][COIN_SYMBOL] + ": \"" + String(coinBalancesByCoin[x], 2) + "\"}");

        delay(1000);
      }
      //TelnetPrint.println("HTTP Response code: " + String(httpResponseCode));
      //double longitude   = doc["data"][1];
      //AppendStatus("Latest Posting Reponse Code: " + String(httpResponseCode));
      //str_http_status += "Latest Posting Reponse Code: " + String(httpResponseCode);

      lastWebTime = millis();
    } else {
      TelnetPrint.println("Latest Posting Reponse: No WiFi");
    }
  }

  double totalBalance=0.0;
  for(int x = 0; x < NUM_COINS && coin_symbols[x] != ""; x++) {
    lcd.clear();
    for(int y = 0; y < NUM_LINES; y++) {
      lcd.setCursor(0, y);
      lcd.print(coinStrings[x][y]);
      TelnetPrint.println(coinStrings[x][y]);
    }
    //lcd.setCursor(0, 1);
    //lcd.print(coinStrings[x][1]);
    //TelnetPrint.println(coinStrings[x][1]);

    delay(coin_delay_ms);
    totalBalance += coinBalancesByCoin[x];
  }
  lcd.setCursor(0, 0);
  lcd.print(getTimestamp(true));
  TelnetPrint.println(" " + getTimestamp(true));

  lcd.setCursor(0, 1);
  //lcd.print("$" + String(totalBalance, 2));
  char justStringVal[17];
  sprintf(justStringVal, "% 16s", "$" + String(totalBalance, 2));
  lcd.print(String(justStringVal));
  TelnetPrint.println(justStringVal);
  delay(coin_delay_ms*1.33);

  first_loop = false;

  //String sensorReadings = "asdf"; //getSensorReadings();
  //Serial.print(sensorReadings);
  //notifyClients(sensorReadings);


  ws.cleanupClients();
  /*
  lcd.clear(); //d
  delay(4000);//
  lcd.blink();
  lcd.setCursor(0, 0);
  delay(6000); //2000

  for(int i=0;i<Hello.length();i++){
    lcd.print(Hello.charAt(i));
    delay(400);
  }  
  delay(6000);//2000 
  lcd.noBlink();
  delay(3000);
  lcd.clear();
  */
}
