/**
Program to read out and display 
data from xiaoxiang Smart BMS 
over Bluetooth Low Energy
https://www.lithiumbatterypcb.com/
Tested with original BLE module provided.
Might work with generic BLE module when UUIDs are modified

Needs ESP32 and graphic display.
Tested on TTGO TS https://github.com/LilyGO/TTGO-TS

(c) Miroslav Kolinsky 2019
https://www.kolins.cz

thanks to Petr Jenik for big parts of code
thanks to Milan Petrzilka

known bugs:
-if BLE server is not available during startup, program hangs
-reconnection sort of works, sometimes ESP reboots
*/

#define TRACE
//#include <Arduino.h>
#include "BLEDevice.h"
#include "mydatatypes.h"
//#include <Wire.h>

HardwareSerial commSerial(0);

//---- global variables ----

static boolean doConnect = false;
static boolean BLE_client_connected = false;
static boolean doScan = false;

packBasicInfoStruct packBasicInfo;  //here shall be the latest data got from BMS
packEepromStruct packEeprom;        //here shall be the latest data got from BMS
packCellInfoStruct packCellInfo;    //here shall be the latest data got from BMS

const byte cBasicInfo3 = 3;  //type of packet 3= basic info
const byte cCellInfo4 = 4;   //type of packet 4= individual cell info

unsigned long previousMillis = 0;
const long interval = 5000;

bool toggle = false;
bool newPacketReceived = false;



#include <queue>
#include <deque>

#include <WiFi.h>
#include <WiFiClient.h>
//#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>

//#include <AsyncTCP.h>
#include <ESPAsyncWebSrv.h>

#ifdef ESP8266
#include <Updater.h>
#include <ESP8266mDNS.h>
#define U_PART U_FS
#else
#include <Update.h>
#include <ESPmDNS.h>
#define U_PART U_SPIFFS
#endif

// Needs mod to work with ESPAsyncWebSrv above.  Original version wants ESPAsyncWeb*Server* not Srv
// https://github.com/ayushsharma82/AsyncElegantOTA
#include "libs/AsyncElegantOTA/AsyncElegantOTA.h"

//#include <WebServer.h>
//#include <Update.h>
//#include "time.h"
//#include <TimeLib.h>

// https://github.com/syvic/ModbusMaster
//#include <ModbusMaster.h>
//ModbusMaster node;

const char compile_date[] = __DATE__ " " __TIME__;

Preferences prefs;

// Set web server port number to 80
AsyncWebServer server(80);
size_t content_len;

String boot_time_str = "";
// Variable to store the HTTP request
String header;
String status_str = "";
const int STATUS_BUFFER_SIZE = 1500;
char stat_char[STATUS_BUFFER_SIZE] = "";

String http_status_str = "";
String last_data_capture = "";

//For UTC -8.00 : -8 * 60 * 60 : -28800
const long gmtOffset_sec = -28800;
//Set it to 3600 if your country observes Daylight saving time; otherwise, set it to 0.
const int daylightOffset_sec = 3600;

const char *ssid = "";
const char *password = "";

String ssid_str = "";
String password_str = "";

bool allow_auto = false;

//const char *ssid = "TP-Link_7536";
//const char *password = "07570377";

//Your Domain name with URL path or IP address with path
//const char *serverName = "https://lhbqdvca46.execute-api.us-west-2.amazonaws.com/dev/solar";
//String serverName = "https://us-west-2.aws.data.mongodb-api.com/app/solar-0-cvgqn/endpoint/createSolar";
String serverName = "http://192.168.10.61:3000/dev/solar";


unsigned long currentMillis = 0;
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
unsigned long lastWebTime = 0;

// Timer set to 10 minutes (600000)
//unsigned long timerDelayMS = 600000;

// Set timer to 15 seconds (15000)
//unsigned long timerDelayMS = 15000;

// Set timer to 60 seconds (60000)
unsigned long timerDelayMS = 60000;

// We'll upload to the web every sixty seconds
unsigned long timerWebDelayMS = 60000;

const int num_data_points_sma = 10;
const int num_data_points_decision = 8;

#define RELAY_PIN 21

/*
A note about which pins to use: 
- I was originally using pins 17 and 18 (aka RX2 and TX2 on some ESP32 devboards) for RX and TX, 
which worked on an ESP32 Wroom but not an ESP32 Rover. So I switched to pins 13 and 14, which works on both.
I haven't tested on an Arduino board though.
*/
#define RXD2 13
#define TXD2 14

/*
Number of registers to check. I think all Renogy controllers have 35
data registers (not all of which are used) and 17 info registers.
*/
//const uint32_t num_data_registers = 35;
//const uint32_t num_info_registers = 17;

/*
// A struct to hold the controller data
struct Controller_data {
  
  uint8_t battery_soc;               // percent
  float battery_voltage;             // volts
  float battery_charging_amps;       // amps
  uint8_t battery_temperature;       // celcius
  uint8_t controller_temperature;    // celcius
  float load_voltage;                // volts
  float load_amps;                   // amps
  uint8_t load_watts;                // watts
  float solar_panel_voltage;         // volts
  float solar_panel_amps;            // amps
  uint8_t solar_panel_watts;         // watts
  float min_battery_voltage_today;   // volts
  float max_battery_voltage_today;   // volts
  float max_charging_amps_today;     // amps
  float max_discharging_amps_today;  // amps
  uint8_t max_charge_watts_today;    // watts
  uint8_t max_discharge_watts_today; // watts
  uint8_t charge_amphours_today;     // amp hours
  uint8_t discharge_amphours_today;  // amp hours
  uint8_t charge_watthours_today;    // watt hours
  uint8_t discharge_watthours_today; // watt hours
  uint8_t controller_uptime_days;    // days
  uint8_t total_battery_overcharges; // count
  uint8_t total_battery_fullcharges; // count

  // convenience values
  float battery_temperatureF;        // fahrenheit
  float controller_temperatureF;     // fahrenheit
  float battery_charging_watts;      // watts. necessary? Does it ever differ from solar_panel_watts?
  long last_update_time;             // millis() of last update time
  bool controller_connected;         // bool if we successfully read data from the controller
};
Controller_data renogy_data;
*/

/*
// A struct to hold the controller info params
struct Controller_info {
  
  uint8_t voltage_rating;            // volts
  uint8_t amp_rating;                // amps
  uint8_t discharge_amp_rating;      // amps
  uint8_t type;
  uint8_t controller_name;
  char software_version[40];
  char hardware_version[40];
  char serial_number[40];
  uint8_t modbus_address;  

  float wattage_rating;
  long last_update_time;           // millis() of last update time
};
Controller_info renogy_info;
*/

void AppendStatus(String asdf) {
  Serial.println(asdf);
  status_str += "<p>" + String(asdf) + "</p>\n";
  sprintf(stat_char, "%s<p>%s</p>\n", stat_char, asdf.c_str());
}

template<typename T, int MaxLen, typename Container = std::deque<T>>
class FixedQueue : public std::queue<T, Container> {
public:
  void push(const T &value) {
    if (this->size() == MaxLen) {
      this->c.pop_front();
    }
    std::queue<T, Container>::push(value);
  }
  String html() {
    //int num_valid = 0;
    String asdf = "";
    for (int y = 0; y < this->size(); y++) {
      asdf += "<p>" + String(this->c[y]) + "</p>";
    }
    return asdf;
  }
  String csv() {
    //int num_valid = 0;
    String asdf = "";
    for (int y = 0; y < this->size(); y++) {
      asdf += String(this->c[y]);
      if (this->size() > 1 && y != (this->size()-1)) {
        asdf += ", ";
      }
    }
    return asdf;
  }
  T average(int *num_valid_ref, String txt) {
    T avg = 0;
    T sum = 0;
    //int num_valid = 0;
    //String asdf = "";
    for (int y = 0; y < this->size(); y++) {
      //if (this->c[y] > 0) {
        (*num_valid_ref) += 1;
        sum += this->c[y];
        //asdf += String(this->c[y]) + ", ";
      //}
    }
    //if (sum > 0) {
      avg = (sum / (*num_valid_ref)) * 1.0;
    //}
    if(txt != "") {
      AppendStatus(txt + " Avg: " + String(avg) + " (" + this->csv() + ")");
    }

    return avg;
  }
};

// if you don't have a charge controller to test with, can set this to true to get non 0 voltage readings
bool simulator_mode = false;

int battery_voltage = 12;
int batt_cap_ah = 280;
float max_battery_discharge = .2;

float batt_volt_min = 11.5;
float bat_volt_start = 12.5;

int batt_soc_min = 20;
int batt_soc_start = 50;

// 5 minutes
long shut_cool_ms = 600000;
// Prevent any automatic power-on for 120 seconds
long next_available_startup = millis() + 120000;


float sim_starting_battery_voltage = 13.12;
float sim_bat_volt_change = -0.11;

uint8_t sim_starting_battery_soc = 51.5;
float sim_bat_soc_change = -1;

bool load_running = false;

float sim_starting_solar_panel_watts = 20;
bool first_loop = true;


float avg_time_to_empty_mins = 0.0;
float net_avg_system_watts = 0.0;
float net_avg_system_amps = 0.0;
float avg_batt_soc = 0;
float avg_batt_volts = 0;

FixedQueue<float, num_data_points_sma> time_to_empty_queue;
FixedQueue<float, num_data_points_sma> net_system_watts_queue;
FixedQueue<float, num_data_points_sma> net_system_amps_queue;
FixedQueue<float, num_data_points_sma> battery_soc_queue;
FixedQueue<float, num_data_points_sma> battery_voltage_queue;
FixedQueue<String, num_data_points_sma> automatic_decisions;

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Started!");

  pinMode(RELAY_PIN, OUTPUT);

  renogy_control_load(0);

  prefs.begin("solar-app", false);
  batt_cap_ah = prefs.getInt("batt_cap_ah", 50);
  batt_soc_min = prefs.getInt("batt_soc_min", 50);
  batt_soc_start = prefs.getInt("batt_soc_start", 65);
  batt_volt_min = prefs.getDouble("batt_volt_min", 11.5);
  bat_volt_start = prefs.getDouble("bat_volt_start", 13);
  shut_cool_ms = prefs.getLong("shut_cool_ms", 300000);

  //bool default_set = false;
  ssid_str = prefs.getString("ssid", "dirker");
  password_str = prefs.getString("password", "alphabit");

  allow_auto = prefs.getBool("allow_auto", false);
  /*
  if(ssid == "" || strlen(ssid) < 3) {
    Serial.println("Setting default ssid...");
    prefs.putString("ssid", "dirker");
    default_set = true;
  }
  if(password == "" || strlen(password) < 3) {
    Serial.println("Setting default password...");
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

  next_available_startup = millis() + shut_cool_ms;

  WiFi.begin(ssid_str.c_str(), password_str.c_str());
  //WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(2500);
    Serial.println("connecting...");
    Serial.println(ssid_str);
    Serial.println(password_str);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);

  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
  //configTime(0, 0, "pool.ntp.org");

  // create a second serial interface for modbus
  //Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  // my Renogy Wanderer has an (slave) address of 255! Not in docs???
  // Do all Renogy charge controllers use this address?
  //int modbus_address = 255;
  //node.begin(modbus_address, Serial2);

  AsyncElegantOTA.begin(&server);  // Start AsyncElegantOTA

  handle_webserver_connection();

  // Start webserver
  server.begin();

  commSerial.begin(115200, SERIAL_8N1, 3, 1);
  delay(3000);
  commSerial.println("Starting ebike dashboard application...");

  delay(500);

  bleStartup();
  boot_time_str = getTimestamp();
}

String structToString(){
    String asdf = "{";

    asdf += "\"battery_soc\": " + String(packBasicInfo.CapacityRemainPercent) + ",";
    asdf += "\"battery_voltage\": " + String((float)packBasicInfo.Volts / 1000) + ",";
    asdf += "\"battery_charging_amps\": " + String((float)packBasicInfo.Amps / 1000) + ",";
    asdf += "\"battery_temperature\": " + String((float)packBasicInfo.Temp1 / 10) + ",";
    asdf += "\"battery_watts_net\": " + String((float)packBasicInfo.Watts) + ",";
    asdf += "\"controller_temperature\": " + String((float)packBasicInfo.Temp2 / 10) + ",";
    /*
    asdf += "\"load_voltage\": " + String(myData.load_voltage) + ",";
    asdf += "\"load_amps\": " + String(myData.load_amps) + ",";
    asdf += "\"load_watts\": " + String(myData.load_watts) + ",";
    asdf += "\"solar_panel_voltage\": " + String(myData.solar_panel_voltage) + ",";
    asdf += "\"solar_panel_amps\": " + String(myData.solar_panel_amps) + ",";
    asdf += "\"solar_panel_watts\": " + String(myData.solar_panel_watts) + ",";

    asdf += "\"min_battery_voltage_today\": " + String(myData.min_battery_voltage_today) + ",";
    asdf += "\"max_battery_voltage_today\": " + String(myData.max_battery_voltage_today) + ",";
    asdf += "\"max_charging_amps_today\": " + String(myData.max_charging_amps_today) + ",";
    asdf += "\"max_discharging_amps_today\": " + String(myData.max_discharging_amps_today) + ",";

    asdf += "\"max_charge_watts_today\": " + String(myData.max_charge_watts_today) + ",";
    asdf += "\"max_discharge_watts_today\": " + String(myData.max_discharge_watts_today) + ",";
    asdf += "\"charge_amphours_today\": " + String(myData.charge_amphours_today) + ",";
    asdf += "\"discharge_amphours_today\": " + String(myData.discharge_amphours_today) + ",";

    asdf += "\"charge_watthours_today\": " + String(myData.charge_watthours_today) + ",";
    asdf += "\"discharge_watthours_today\": " + String(myData.discharge_watthours_today) + ",";
    asdf += "\"controller_uptime_days\": " + String(myData.controller_uptime_days) + ",";
    asdf += "\"total_battery_overcharges\": " + String(myData.total_battery_overcharges) + ",";
    asdf += "\"total_battery_fullcharges\": " + String(myData.total_battery_fullcharges) + ",";

    asdf += "\"battery_temperatureF\": " + String(myData.battery_temperatureF) + ",";
    asdf += "\"controller_temperatureF\": " + String(myData.controller_temperatureF) + ",";
    asdf += "\"battery_charging_watts\": " + String(myData.battery_charging_watts) + ",";
    asdf += "\"last_update_time\": " + String(myData.last_update_time) + ",";
    asdf += "\"controller_connected\": " + String(myData.controller_connected);
    */
    asdf += "\"controller_connected\": " + String(BLE_client_connected);
    
    asdf += "}";
    return asdf;
}

// Current time
//unsigned long currentTime = millis();
// Previous time
//unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
//const long timeoutTime = 2000;

String automatic_decision = "";
String current_status = "";

const char *headHtml = R"literal(
<!DOCTYPE html><html>
<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">
<link rel=\"icon\" href=\"data:,\">
<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}
.button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;
text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}
.button2 {background-color: #555555;}</style>
<script>
)literal";

String htmlHead(bool includeReload = true) {
  // Display the HTML web page
  /*
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
  */

  String data = String(headHtml);
  if (includeReload) {
    data += "setTimeout(function(){ window.location.reload(); }, 5000);";
  }
  data += "</script>";

  data += "</head>";
  return data;
}

String htmlFoot() {
  String data = "<p>Compiled: " + String(compile_date) + "</p>";
  data += "<p><a href='/'>Home</a> | <a href='/raw'>raw</a> | <a href='/json'>json</a> | <a href='/config'>Config</a> | <a href='/update'>Update</a> | <a href='/restart'>restart</a></p>";
  data += "</body></html>";
  return data;
}

String statsIndex() {
  String data = htmlHead();

  // Web Page Heading
  data += "<body><h1>ESP32 Web Server - With OTAv3</h1>";

  data += "<p>Boot Time: " + boot_time_str + "</p>";

  // Display current state of the Renogy load
  data += "<p>Relay Load - " + String(load_running ? "Running" : "Stopped") + "</p>";
  /*
  if (load_running) {
    data += "<p>Relay Load - " + String(load_running ? "Running" : "Stopped"); // @ " + String((float)packBasicInfo.Watts) + " watts " + String(avg_load_watts) + " avg</p>";
  } else {
    data += "<p>Renogy Load - Stopped - Net rate: " + String((float)packBasicInfo.Watts) + "</p>";
  }
  */
  data += "<p>Current Rate: " + String((float)packBasicInfo.Watts) + " watts</p>";
  data += "<p>Net Rate: " + String(net_avg_system_watts) + " watts</p>";
  if (automatic_decision != "") {
    data += "<p>Last Automatic: " + String(automatic_decision) + "</p>";
  }
  data += "<p>Automatics: " + String(allow_auto ? "On" : "Off") + "</p>";
  data += automatic_decisions.html();

  data += "<p>Last Data Capture: " + last_data_capture + "</p>";
  if (current_status != "") {
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

  data += "<form action=\"/set\">Enter a ssid: <input type=\"text\" name=\"ssid\" value=\"" + String(ssid_str) + "\"><br/>";
  data += "<input type=\"submit\" value=\"Submit\"></form>";
  data += "<form action=\"/set\">Enter a password: <input type=\"text\" name=\"password\" value=\"" + String(password_str) + "\"><br/>";
  data += "<input type=\"submit\" value=\"Submit\"></form>";

  data += "<form action=\"/set\">Allow Automatic: <input type=\"text\" name=\"allow_auto\" value=\"" + String(allow_auto) + "\"><br/>";
  data += "<input type=\"submit\" value=\"Submit\"></form>";

  data += htmlFoot();
  return data;
}

String rawIndex(bool asPlaintext = true) {
  const int BUFFERSIZE = 500;
  char sBuff[BUFFERSIZE] = "";
  char endline[5] = "<br>";
  if(asPlaintext) {
    endline[0] = '\n';
    endline[1] = '\0';
  }
  sBuff[0] = '\0'; //clear old data
  //sprintf(sBuff, "Total voltage: %f %s", (float)packBasicInfo.Volts / 1000, endline);
  //sprintf(sBuff, "Amps: %f %s", (float)packBasicInfo.Amps / 1000, endline);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Captured: %s", last_data_capture.c_str());
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Total voltage: %.2f", (float)packBasicInfo.Volts / 1000);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Amps: %.2f", (float)packBasicInfo.Amps / 1000);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Watts: %.2f", (float)packBasicInfo.Watts);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "CapacityRemainAh: %f", (float)packBasicInfo.CapacityRemainAh / 1000);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "CapacityRemainPercent: %d", packBasicInfo.CapacityRemainPercent);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Temp1: %f", (float)packBasicInfo.Temp1 / 10);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Temp2: %f", (float)packBasicInfo.Temp2 / 10);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Balance Code Low: 0x%x", packBasicInfo.BalanceCodeLow);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Balance Code High: 0x%x", packBasicInfo.BalanceCodeHigh);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Mosfet Status: 0x%x", packBasicInfo.MosfetStatus);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Number of cells: %u", packCellInfo.NumOfCells);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);
  for (byte i = 1; i <= packCellInfo.NumOfCells; i++)
  {
      snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Cell no. %u    %.3f", i, (float)packCellInfo.CellVolt[i - 1] / 1000);
      snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);
      //snprintf(sBuff, STRINGBUFFERSIZE, "   %f\n", (float)packCellInfo.CellVolt[i - 1] / 1000);
  }
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Max cell volt: %.3f", (float)packCellInfo.CellMax / 1000);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Min cell volt: %.3f", (float)packCellInfo.CellMin / 1000);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Difference cell volt: %.4f", (float)packCellInfo.CellDiff / 1000);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Average cell volt: %.3f", (float)packCellInfo.CellAvg / 1000);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Median cell volt: %.3f", (float)packCellInfo.CellMedian / 1000);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  String data = sBuff;
  //return data;
  //String data = htmlHead(false);
  //data += std::printf("Total voltage: %f\n", (float)packBasicInfo.Volts / 1000);
  /*
    commSerial.printf("Amps: %f\n", (float)packBasicInfo.Amps / 1000);
    commSerial.printf("Watts: %f\n", (float)packBasicInfo.Watts); /// 1000);
    commSerial.printf("CapacityAh: %f\n", (float)packBasicInfo.CapacityAh / 1000);
    commSerial.printf("CapacityRemainAh: %f\n", (float)packBasicInfo.CapacityRemainAh / 1000);
    commSerial.printf("CapacityRemainPercent: %f\n", (float)packBasicInfo.CapacityRemainPercent);
    commSerial.printf("Cycles: %f\n", (float)packBasicInfo.Cycles);
    commSerial.printf("Temp1: %f\n", (float)packBasicInfo.Temp1 / 10);
    commSerial.printf("Temp2: %f\n", (float)packBasicInfo.Temp2 / 10);
    commSerial.printf("Balance Code Low: 0x%x\n", packBasicInfo.BalanceCodeLow);
    commSerial.printf("Balance Code High: 0x%x\n", packBasicInfo.BalanceCodeHigh);
    commSerial.printf("Mosfet Status: 0x%x\n", packBasicInfo.MosfetStatus);
    */
  //data += htmlFoot();
  return data;
}

void turn_off_load(String decision) {
  if (decision != "") {
    automatic_decision = decision + " @ " + getTimestamp();
    automatic_decisions.push(automatic_decision);
    AppendStatus(automatic_decision);
  }
  renogy_control_load(0);
}

void power_on_load(String decision) {
  if (decision != "") {
    automatic_decision = decision + " @ " + getTimestamp();
    automatic_decisions.push(automatic_decision);
    AppendStatus(automatic_decision);
  }
  renogy_control_load(1);
}

void handle_webserver_connection() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    //harvest_data();
    status_str = "";
    update_decisions(false);
    request->send(200, "text/html", statsIndex());
  });
  server.on("/http", HTTP_GET, [](AsyncWebServerRequest *request) {
    //harvest_data();
    //update_decisions(false);
    request->send(200, "text/html", http_status_str);
  });
  server.on("/raw", HTTP_GET, [](AsyncWebServerRequest *request) {
    //harvest_data();
    status_str = "";
    update_decisions(false);
    request->send(200, "text/plain", rawIndex(true));
  });
  server.on("/json", HTTP_GET, [](AsyncWebServerRequest *request) {
    //harvest_data();
    status_str = "";
    update_decisions(false);
    request->send(200, "text/plain", structToString());
  });
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
    status_str = "";
    request->send(200, "text/html", configIndex());
  });

  // Load control
  server.on("/load/on", HTTP_GET, [](AsyncWebServerRequest *request) {
    power_on_load("");
    request->redirect("/");
  });
  server.on("/load/off", HTTP_GET, [](AsyncWebServerRequest *request) {
    turn_off_load("");
    request->redirect("/");
  });


  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    //int new_value;
    String input_value;
    if (request->hasArg("batt_cap_ah") || request->hasArg("batt_soc_min") || request->hasArg("batt_soc_start") || request->hasArg("batt_volt_min") || request->hasArg("bat_volt_start") || request->hasArg("shut_cool_ms")
        || request->hasArg("ssid")
        || request->hasArg("password")
        || request->hasArg("allow_auto")) {
      if (prefs.begin("solar-app", false)) {
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
        if (request->hasArg("ssid")) {
          input_value = request->arg("ssid");
          ssid_str = input_value;
          prefs.putString("ssid", ssid_str);
        }
        if (request->hasArg("password")) {
          input_value = request->arg("password");
          password_str = input_value;
          prefs.putString("password", password_str);
        }
        if (request->hasArg("allow_auto")) {
          input_value = request->arg("allow_auto");
          allow_auto = std::stoi(input_value.c_str());
          prefs.putBool("allow_auto", allow_auto);
        }
        prefs.end();
      } else {
        request->send(200, "text/plain", "Failed to being() prefs for write");
      }
    }
    request->redirect("/config");
  });

  server.onNotFound(notFound);
}

String getTimestamp() {
  time_t now;
  struct tm asdf;
  char buf[80];
  if (!getLocalTime(&asdf)) {
    //Serial.println("Failed to obtain time");
    return "";
  } else {
    time(&now);
    asdf = *localtime(&now);
  }
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z", &asdf);
  return String(buf);
  //printf("%s\n", buf);
  //return now;
}

void harvest_data() {
  /*
  static uint32_t i;
  i++;

  // set word 0 of TX buffer to least-significant word of counter (bits 15..0)
  node.setTransmitBuffer(0, lowWord(i));  
  // set word 1 of TX buffer to most-significant word of counter (bits 31..16)
  node.setTransmitBuffer(1, highWord(i));

  renogy_read_data_registers();
  if(BLE_client_connected && renogy_data.battery_voltage >= 256) {
    // Likely bad data. Delay and try again
    delay(10);
    renogy_read_data_registers();
  }

  //renogy_read_info_registers();
  */
}

void update_decisions(bool allow_automatic) {
  AppendStatus("Current Time: " + getTimestamp());
  if (!BLE_client_connected && !simulator_mode) {
    AppendStatus("Controller is not connected. No decisions will be made");
    return;
  }

  int valid_data_points_battery_voltage = 0;
  avg_batt_volts = battery_voltage_queue.average(&valid_data_points_battery_voltage, "Battery Voltage");

  int valid_data_points_battery_soc = 0;
  avg_batt_soc = battery_soc_queue.average(&valid_data_points_battery_soc, "Battery SOC");

  int valid_data_points_load_watts = 0;
  net_avg_system_watts = net_system_watts_queue.average(&valid_data_points_load_watts, "Net System Watts");

  int valid_data_points_net_amps = 0;
  net_avg_system_amps = net_system_amps_queue.average(&valid_data_points_net_amps, "Avg Net System Amps");

  //float net_rate_ah = (float)packBasicInfo.Amps / 1000;
  //float time_to_empty_hrs = (((avg_batt_soc / 100.00) - (1 - max_battery_discharge)) * batt_cap_ah) / (net_rate_ah * -1.0);
  //float time_to_empty_mins = time_to_empty_hrs * 60;
  //time_to_empty_queue.push(time_to_empty_mins);

  if (allow_automatic) {
    if (valid_data_points_load_watts > num_data_points_decision) {
      if (net_avg_system_watts < 0.1) {
        load_running = true;
      }
    } else {
      if (load_running && net_avg_system_watts == 0 && BLE_client_connected) {
        load_running = false;
      }
    }
  }

  if (valid_data_points_battery_voltage > num_data_points_decision && valid_data_points_battery_soc > num_data_points_decision) {
    if (load_running && avg_batt_volts < batt_volt_min) {
      if (allow_automatic) {
        turn_off_load("Turn load off (voltage) " + String(avg_batt_volts) + " < " + String(batt_volt_min));
      }
      sim_bat_volt_change = fabs(sim_bat_volt_change) * 1.0;
      sim_bat_soc_change = fabs(sim_bat_soc_change);
    } else {
      if (load_running && avg_batt_soc < batt_soc_min) {
        if (allow_automatic) {
          turn_off_load("Turn load off (battery soc) " + String(avg_batt_soc) + " < " + String(batt_soc_min));
        }
        sim_bat_volt_change = fabs(sim_bat_volt_change) * 1.0;
        sim_bat_soc_change = fabs(sim_bat_soc_change);
      } else {
        if (load_running) {
          current_status = "";
          //AppendStatus("Load already on");
        } else {
          if (avg_batt_volts > bat_volt_start) {
            if (avg_batt_soc > batt_soc_start) {
              if (next_available_startup != 0 && millis() > next_available_startup) {
                if (allow_automatic) {
                  power_on_load("Turn on load");
                }
                current_status = "";
              } else {
                AppendStatus("Wait for cooldown: " + String(next_available_startup - millis()));
                current_status = "Wait for cooldown " + String(next_available_startup - millis());
              }

              //if(simulator_mode) {
              //  renogy_data.load_watts = 8;
              //}
              sim_bat_volt_change = fabs(sim_bat_volt_change) * -1.0;
              sim_bat_soc_change = -fabs(sim_bat_soc_change);
            } else {
              AppendStatus("SOC too low to start");
            }
          } else {
            AppendStatus("Volts too low to start");
          }
        }
      }
    }
  } else {
    AppendStatus("Not enough valid data. Skipping decision");
    return;
  }

  //AppendStatus(String(ESP.getFreeHeap()));
  AppendStatus("Battery Capacity AH: " + String(batt_cap_ah) + " @ " + String(avg_batt_soc) + "% @ " + String(avg_batt_volts) + "v");

  // P = I * V
  // I = P / V
  //float net_rate_ah = (float)packBasicInfo.Amps / 1000;
  //float panel_incoming_rate_ah = ((float)packBasicInfo.Watts) / (battery_voltage * 1.0);
  //AppendStatus("Panel is at " + String(renogy_data.solar_panel_amps) + " amps (r) and " + String(renogy_data.solar_panel_watts) + " watts (r)");
  //AppendStatus("Panel is producing " + String(panel_incoming_rate_ah) + " ah (c)");

  if (valid_data_points_load_watts > num_data_points_decision) {
    float load_outgoing_rate_ah = net_avg_system_watts / (battery_voltage * 1.0);
    AppendStatus("Load is consuming " + String(load_outgoing_rate_ah) + " ah (c) and " + String(net_avg_system_watts) + " watts (r)");

    //float charging_rate_ah = panel_incoming_rate_ah - load_outgoing_rate_ah;
    AppendStatus("Net System Rate is " + String(net_avg_system_amps) + " ah (c)");

    if (net_avg_system_amps == 0) {
      AppendStatus("Battery is holding steady");
      //AppendStatus("Panel is at " + String(renogy_data.solar_panel_amps) + " amps and " + String(renogy_data.solar_panel_watts) + " watts");
    } else if (net_avg_system_amps > 0) {
      AppendStatus("Battery is charging at " + String(net_avg_system_amps) + " ah");
      float ah_left_to_charge = ((batt_cap_ah * 1.0) * (100 - avg_batt_soc)) / 100;
      AppendStatus("It's at " + String(avg_batt_soc) + "% so just " + String(ah_left_to_charge) + "ah left to charge");
      float time_to_full = ah_left_to_charge / net_avg_system_amps;
      AppendStatus("Battery should be full in " + String(time_to_full) + " hours");
    } else {
      // We're discharging... so multiply by negative one so the time_to_empty is a positive value
      //(((51/100)-(1-0.5))*10)/(-0.25*-1.0)
      //          ((.51-.5)*10)/(-0.25*-1.0)
      //               (.01*10)/0.25
      // 0.1 / 0.25 = .4 == 24 minutes
      //float time_to_empty_hrs = (((avg_batt_soc / 100.00) - (1 - max_battery_discharge)) * batt_cap_ah) / (net_rate_ah * -1.0);
      //float time_to_empty_mins = time_to_empty_hrs * 60;

      //time_to_empty_queue.push(time_to_empty_mins);

      int valid_data_points_tte = 0;
      avg_time_to_empty_mins = time_to_empty_queue.average(&valid_data_points_tte, "Time-To-Empty");
      AppendStatus("Average TTE_Mins: " + String(avg_time_to_empty_mins) + " with " + String(valid_data_points_tte) + " points");
      /*
      String asdf = "";
      valid_data_points = 0;
      for(int y = 0; y<5; y++) {
        if(average_time_to_empty[y] > 0) {
          valid_data_points += 1;
          avg_time_to_empty_mins += average_time_to_empty[y];
          asdf += String(average_time_to_empty[y]) + ", ";          
        }
      }
      AppendStatus("TTE_Mins: " + asdf);
      avg_time_to_empty_mins = (avg_time_to_empty_mins/valid_data_points)*1.0;
      AppendStatus("Average TTE_Mins: " + String(avg_time_to_empty_mins));
      */

      AppendStatus("Battery is discharging at " + String(net_avg_system_amps) + " ah");
      if ((avg_time_to_empty_mins < 60 && valid_data_points_tte > num_data_points_decision)) {
        AppendStatus("At that rate it will be empty in " + String(avg_time_to_empty_mins) + " minutes");
        if (avg_time_to_empty_mins < 15) {
          if (allow_automatic) {
            turn_off_load("Battery will be dead in less than 15 minutes!  Kill the load!" + String(avg_time_to_empty_mins) + " to go");
          }
          //AppendStatus("Battery will be dead in less than 30 minutes!  Kill the load!");
        }
      } else {
        AppendStatus("At that rate it will be empty in " + String(avg_time_to_empty_mins / 60) + " hours");
      }
    }
  } else {
    AppendStatus("Not enough load points");
  }

  //float charging_rate_ah = renogy.battery_charging_amps
  //AppendStatus("Battery is charging at " + String(charging_rate_ah) + " ah");
  //float ah_left_to_charge = ((batt_cap_ah*1.0) - ((batt_cap_ah*1.0) * (renogy_data.battery_soc / 100)));
}

void loop() {
  if (first_loop) {
    Serial.println("Entering loop");
  }

  currentMillis = millis();
  // Only read info every 10th loop
  if ((currentMillis - lastTime) > timerDelayMS || first_loop) {
    Serial.println("Updating data");
    status_str = "";
    bleRequestData();
    //if(allow_automatic) {
    //battery_voltage_queue.push(renogy_data.battery_voltage);
    battery_voltage_queue.push((float)packBasicInfo.Volts / 1000);
    //battery_soc_queue.push(renogy_data.battery_soc);
    battery_soc_queue.push((float)packBasicInfo.CapacityRemainPercent);
    //net_system_watts_queue.push((float)renogy_data.load_watts);
    net_system_watts_queue.push((float)packBasicInfo.Watts);
    net_system_amps_queue.push((float)packBasicInfo.Amps / 1000);

    int valid_data_points_battery_soc = 0;
    avg_batt_soc = battery_soc_queue.average(&valid_data_points_battery_soc, "Battery SOC");

    int valid_data_points_net_amps = 0;
    net_avg_system_amps = net_system_amps_queue.average(&valid_data_points_net_amps, "Avg Net System Amps");

    if(valid_data_points_battery_soc > num_data_points_decision && valid_data_points_net_amps > num_data_points_decision) {
      //float net_rate_ah = (float)packBasicInfo.Amps / 1000;
      float time_to_empty_hrs = (((avg_batt_soc / 100.00) - (1 - max_battery_discharge)) * batt_cap_ah) / (net_avg_system_amps * -1.0);
      float time_to_empty_mins = time_to_empty_hrs * 60;
      time_to_empty_queue.push(time_to_empty_mins);
    }
    //}
    last_data_capture = getTimestamp();

    /*
    if(simulator_mode) {
      renogy_data.battery_voltage += (sim_bat_volt_change*1.0);
      if(renogy_data.battery_voltage > 13.7) {
        renogy_data.battery_voltage = 13.7;
      }
      //if(sim_bat_soc_change > 0) {
        //Serial.println("sim_bat_soc_change: " + String(sim_bat_soc_change));
      //}
      renogy_data.battery_soc += sim_bat_soc_change;
      if(renogy_data.battery_soc > 100) {
        renogy_data.battery_soc = 100;
      }
      if(load_running) {
        renogy_data.load_watts = 8;
      }
    }
    */

    update_decisions(allow_auto);
    lastTime = millis();
  }

  //Send an HTTP POST request every 10 minutes
  if ((currentMillis - lastWebTime) > timerWebDelayMS) {
    AppendStatus("Will POST HTTP");
    http_status_str = "Will POST HTTP @ " + getTimestamp() + "\n";
    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      if (BLE_client_connected) {
        //WiFiClientSecure wifiSecureCli;  // or WiFiClientSecure for HTTPS
        WiFiClient wifiCli;  // or WiFiClientSecure for HTTPS
        HTTPClient theHttpClient;
        //theHttpClient.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
        //wifiSecureCli.setInsecure();

        // Your Domain name with URL path or IP address with path
        theHttpClient.begin(wifiCli, serverName);

        // If you need Node-RED/server authentication, insert user and password below
        //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

        // Specify content-type header
        theHttpClient.addHeader("Content-Type", "application/json");

        // Data to send with HTTP POST
        String httpRequestData = structToString();
        Serial.println("POSTing http data:");
        Serial.println(httpRequestData);
        http_status_str += "\n";
        http_status_str += httpRequestData;
        http_status_str += "\n";
        int httpResponseCode = theHttpClient.POST(httpRequestData);
        //int httpResponseCode = 200;

        Serial.print("HTTP Response code: ");
        AppendStatus("Latest Posting Reponse Code: " + String(httpResponseCode));
        http_status_str += "Latest Posting Reponse Code: " + String(httpResponseCode);

        // Free resources
        theHttpClient.end();
        lastWebTime = millis();
      } else {
        AppendStatus("Latest Posting Reponse: No BLE");
        http_status_str += "Lastest Posting Response: No BLE";
      }
    } else {
      AppendStatus("Latest Posting Reponse: No WiFi");
      http_status_str += "Lastest Posting Response: No WiFi";
    }
  }

  first_loop = false;

  //server.handleClient();
  if (newPacketReceived == true) {
    printBasicInfo();
    printCellInfo();
  }
}


/*
void renogy_read_data_registers() 
{
  uint16_t data_registers[num_data_registers];
  //char buffer1[40], buffer2[40];
  //uint8_t raw_data;

  // prints data about each read to the console
  bool print_data=false; 
  
  uint8_t result = node.readHoldingRegisters(0x100, num_data_registers);
  if (result == node.ku8MBSuccess)
  {
    if (print_data) {
      AppendStatus("Successfully read the data registers!");
    }
    renogy_data.controller_connected = true;
    for (uint8_t j = 0; j < num_data_registers; j++)
    {
      data_registers[j] = node.getResponseBuffer(j);
      if (print_data) Serial.println(data_registers[j]);
    }

    if(!simulator_mode) {
      renogy_data.battery_soc = data_registers[0]; 
      renogy_data.battery_voltage = data_registers[1] * .1; // will it crash if data_registers[1] doesn't exist?
    }
    renogy_data.battery_charging_amps = data_registers[2] * .1;

    renogy_data.battery_charging_watts = renogy_data.battery_voltage * renogy_data.battery_charging_amps;
    
    //0x103 returns two bytes, one for battery and one for controller temp in c
    uint16_t raw_data = data_registers[3]; // eg 5913
    renogy_data.controller_temperature = raw_data/256;
    renogy_data.battery_temperature = raw_data%256; 
    // for convenience, fahrenheit versions of the temperatures
    renogy_data.controller_temperatureF = (renogy_data.controller_temperature * 1.8)+32;
    renogy_data.battery_temperatureF = (renogy_data.battery_temperature * 1.8)+32;

    renogy_data.load_voltage = data_registers[4] * .1;
    renogy_data.load_amps = data_registers[5] * .01;
    renogy_data.load_watts = data_registers[6];
    renogy_data.solar_panel_voltage = data_registers[7] * .1;
    renogy_data.solar_panel_amps = data_registers[8] * .01;
    if(!simulator_mode) {
      renogy_data.solar_panel_watts = data_registers[9];
    }
     //Register 0x10A - Turn on load, write register, unsupported in wanderer - 10
    renogy_data.min_battery_voltage_today = data_registers[11] * .1;
    renogy_data.max_battery_voltage_today = data_registers[12] * .1; 
    renogy_data.max_charging_amps_today = data_registers[13] * .01;
    renogy_data.max_discharging_amps_today = data_registers[14] * .1;
    renogy_data.max_charge_watts_today = data_registers[15];
    renogy_data.max_discharge_watts_today = data_registers[16];
    renogy_data.charge_amphours_today = data_registers[17];
    renogy_data.discharge_amphours_today = data_registers[18];
    renogy_data.charge_watthours_today = data_registers[19];
    renogy_data.discharge_watthours_today = data_registers[20];
    renogy_data.controller_uptime_days = data_registers[21];
    renogy_data.total_battery_overcharges = data_registers[22];
    renogy_data.total_battery_fullcharges = data_registers[23];
    renogy_data.last_update_time = millis();

    // Add these registers:
    //Registers 0x118 to 0x119- Total Charging Amp-Hours - 24/25    
    //Registers 0x11A to 0x11B- Total Discharging Amp-Hours - 26/27    
    //Registers 0x11C to 0x11D- Total Cumulative power generation (kWH) - 28/29    
    //Registers 0x11E to 0x11F- Total Cumulative power consumption (kWH) - 30/31    
    //Register 0x120 - Load Status, Load Brightness, Charging State - 32    
    //Registers 0x121 to 0x122 - Controller fault codes - 33/34

    if (print_data) Serial.println("---");
  }
  else 
  {
    if (result == 0xE2) 
    {
    AppendStatus("Timed out reading the data registers!");
    }
    else 
    {
      AppendStatus("Failed to read the data registers... ");
      Serial.print("Failed to read the data registers... ");
      Serial.println(result, HEX); // E2 is timeout
    }
    // Reset some values if we don't get a reading
    renogy_data.controller_connected = false;
    //renogy_data.battery_voltage = 0; 
    renogy_data.battery_charging_amps = 0;
    //renogy_data.battery_soc = 0;
    renogy_data.battery_charging_amps = 0;
    renogy_data.controller_temperature = 0;
    renogy_data.battery_temperature = 0;    
    renogy_data.solar_panel_amps = 0;
    //renogy_data.solar_panel_watts = 0;
    renogy_data.battery_charging_watts = 0;
    if (simulator_mode) {
      if(first_loop) {
        renogy_data.battery_voltage = sim_starting_battery_voltage;    
        renogy_data.battery_soc = sim_starting_battery_soc; 
        renogy_data.solar_panel_watts = sim_starting_solar_panel_watts;
      }
    } else {
      renogy_data.battery_voltage = 0;
      renogy_data.battery_soc = 0;
      renogy_data.solar_panel_watts = 0;
    }
  }
}
*/

/*
void renogy_read_info_registers() 
{
  uint8_t j, result;
  uint16_t info_registers[num_info_registers];
  char buffer1[40], buffer2[40];
  uint8_t raw_data;

  // prints data about the read to the console
  bool print_data=0;
  
  result = node.readHoldingRegisters(0x00A, num_info_registers);
  if (result == node.ku8MBSuccess)
  {
    if (print_data) {
      AppendStatus("Successfully read the info registers!");
    }
    for (j = 0; j < num_info_registers; j++)
    {
      info_registers[j] = node.getResponseBuffer(j);
      if (print_data) Serial.println(info_registers[j]);
    }

    // read and process each value
    //Register 0x0A - Controller voltage and Current Rating - 0
    // Not sure if this is correct. I get the correct amp rating for my Wanderer 30 (30 amps), but I get a voltage rating of 0 (should be 12v)
    raw_data = info_registers[0]; 
    renogy_info.voltage_rating = raw_data/256; 
    renogy_info.amp_rating = raw_data%256;
    renogy_info.wattage_rating = renogy_info.voltage_rating * renogy_info.amp_rating;
    //AppendStatus("raw = " + String(raw_data));
    //AppendStatus("raw volt = " + String(raw_data/256.0));
    //AppendStatus("raw amp = " + String(raw_data%256));
    //Serial.println("Voltage rating: " + String(renogy_info.voltage_rating));
    //Serial.println("amp rating: " + String(renogy_info.amp_rating));


    //Register 0x0B - Controller discharge current and type - 1
    raw_data = info_registers[1]; 
    renogy_info.discharge_amp_rating = raw_data/256; // not sure if this should be /256 or /100
    renogy_info.type = raw_data%256; // not sure if this should be /256 or /100

    //Registers 0x0C to 0x13 - Product Model String - 2-9
    // Here's how the nodeJS project handled this:
    /*
    let modelString = '';
    for (let i = 0; i <= 7; i++) {  
        rawData[i+2].toString(16).match(/.{1,2}/g).forEach( x => {
            modelString += String.fromCharCode(parseInt(x, 16));
        });
    }
    this.controllerModel = modelString.replace(' ','');
    /

    //Registers 0x014 to 0x015 - Software Version - 10-11
    itoa(info_registers[10],buffer1,10); 
    itoa(info_registers[11],buffer2,10);
    strcat(buffer1, buffer2); // should put a divider between the two strings?
    strcpy(renogy_info.software_version, buffer1); 
    //Serial.println("Software version: " + String(renogy_info.software_version));

    //Registers 0x016 to 0x017 - Hardware Version - 12-13
    itoa(info_registers[12],buffer1,10); 
    itoa(info_registers[13],buffer2,10);
    strcat(buffer1, buffer2); // should put a divider between the two strings?
    strcpy(renogy_info.hardware_version, buffer1);
    //Serial.println("Hardware version: " + String(renogy_info.hardware_version));

    //Registers 0x018 to 0x019 - Product Serial Number - 14-15
    // I don't think this is correct... Doesn't match serial number printed on my controller
    itoa(info_registers[14],buffer1,10); 
    itoa(info_registers[15],buffer2,10);
    strcat(buffer1, buffer2); // should put a divider between the two strings?
    strcpy(renogy_info.serial_number, buffer1);
    //Serial.println("Serial number: " + String(renogy_info.serial_number)); // (I don't think this is correct)

    renogy_info.modbus_address = info_registers[16];
    renogy_info.last_update_time = millis();
  
    if (print_data) Serial.println("---");
  }
  else
  {
    if (result == 0xE2) 
    {
      AppendStatus("Timed out reading the info registers!");
    }
    else 
    {
      AppendStatus("Failed to read the info registers... ");
      Serial.print("Failed to read the info registers... ");
      Serial.println(result, HEX); // E2 is timeout
    }
    // anything else to do if we fail to read the info reisters?
  }
}
*/


// control the load pins on Renogy charge controllers that have them
void renogy_control_load(bool state) {
  
  // The relays I use are wired backwards!?  So this HIGH/LOW are switched from what they'd normally be...
  // Moderately annoying.
  if (state) {
    digitalWrite(RELAY_PIN, LOW);  // turn on load
    load_running = true;
  } else {
    digitalWrite(RELAY_PIN, HIGH);  // turn off load
    load_running = false;
  }
  next_available_startup = millis() + shut_cool_ms;
  //if (state==1) node.writeSingleRegister(0x010A, 1);  // turn on load
  //else node.writeSingleRegister(0x010A, 0);  // turn off load
}