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

//#include <ModbusMaster.h>
#include <RunningAverage.h>
#include <TelnetPrint.h>

//#define RS485_TX 1
//#define RS485_RX 3

#include "TracerData.h"
#include "EpeverTracer.h"

EpeverTracer tracer;

/*
"Real-time data" and "Real-time status" registers
    PV array voltage             3100
    PV array current             3101
    PV array power               3102-3103
    Battery voltage              3104
    Battery charging current     3105
    Battery charging power       3106-3107
    Load voltage                 310C
    Load current                 310D
    Load power                   310E-310F
    Battery temperature          3110
    Internal temperature         3111
    Heat sink temperature        3112
    Battery SOC                  311A
    Remote battery temperature   311B
    System rated voltage         311C
    Battery status               3200
    Charging equipment status    3201
    Discharging equipment status 3202
*/
/*
#define PANEL_VOLTS      0x00
#define PANEL_AMPS       0x01
#define PANEL_POWER_L    0x02
#define PANEL_POWER_H    0x03
#define BATT_VOLTS       0x04
#define BATT_AMPS        0x05
#define BATT_POWER_L     0x06
#define BATT_POWER_H     0x07
#define LOAD_VOLTS       0x0C
#define LOAD_AMPS        0x0D
#define LOAD_POWER_L     0x0E
#define LOAD_POWER_H     0x0F
#define BATT_TEMP        0x10
#define INT_TEMP         0x11
#define HEATSINK_TEMP    0x12
#define BATT_SOC         0x1A
#define REMOTE_BATT_TEMP 0x1B

const int realtimeMetrics = 11; //The number of realtime metrics we'll be collecting.
float realtime[realtimeMetrics];

byte avSamples = 10;
int failures = 0; //The number of failed WiFi or HTTP post attempts. Will automatically restart the ESP if too many failures occurr in a row.

RunningAverage realtimeAverage[realtimeMetrics] = {
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples)
};

byte collectedSamples = 0;
unsigned long lastUpdate = 0;

const char *realtimeMetricNames[] = {"PV_array_voltage",
                       "PV_array_current",
                       "PV_array_power",
                       "Battery_voltage",
                       "Battery_charging_current",
                       "Battery_charging_power",
                       "Load_voltage",
                       "Load_current",
                       "Load_power",
                       "Battery_temperature",
                       "Internal_temperature"};
*/

/*
"Statistical parameter registers
    Max input voltage today      3300
    Min input voltage today      3301
    Max battery voltage today    3302
    Min battery voltage today    3303
    Consumed energy today        3304-3305
    Consumed energy this month   3306-3307
    Consumed energy this year    3308-3309
    Total consumed energy        330A-330B
    Generated energy today       330C-330D
    Generated energy this moth   330E-330F
    Generated energy this year   3310-3311
    Total generated energy       3312-3313
    Carbon dioxide reduction     3314-3315
    Battery voltage              331A
    Net battery current          331B-331C
    Battery temperature          331D
    Ambient temperature          331E
*/

#define TRACE
//#include <Arduino.h>
#include <NimBLEDevice.h>
#include "mydatatypes.h"
//#include <Wire.h>

//---- global variables ----
static const NimBLEAdvertisedDevice* advDevice = nullptr;
static constexpr uint32_t bleScanTimeMs = 10 * 1000;

//static boolean doConnect = false;
static boolean BLE_client_connected = false;
static boolean doScan = false;

packBasicInfoStruct packBasicInfo;  //here shall be the latest data got from BMS
packEepromStruct packEeprom;        //here shall be the latest data got from BMS
packCellInfoStruct packCellInfo;    //here shall be the latest data got from BMS

const byte cBasicInfo3 = 3;  //type of packet 3= basic info
const byte cCellInfo4 = 4;   //type of packet 4= individual cell info

unsigned long previousMillis = 0;
//const long interval = 15000;

bool toggle = false;
bool newPacketReceived = false;

bool last_action_was_manual = false;


#include <queue>
#include <deque>

#include <WiFi.h>
//#include <WiFiClient.h>
//#include <WiFiClientSecure.h>
//#include <HTTPClient.h>
#include <Preferences.h>

//#include <AsyncTCP.h>
#include <ESPAsyncWebSrv.h>
//#include <ArduinoJson.h>

#ifdef ESP8266
#include <Updater.h>
#include <ESP8266mDNS.h>
#define U_PART U_FS
#else
//#include <Update.h>
//#include <ESPmDNS.h>
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

#define NUM_DEVICES 4

//int device_gpio_pins[NUM_DEVICES] = {21, 22, 0, 0};
//int device_gpio_pins[NUM_DEVICES] = {12, 13, 14, 0};
int device_gpio_pins[NUM_DEVICES] = {0, 0, 0, 0};
//#define RELAY_PIN_ONE 21
//#define RELAY_PIN_TWO 22

float latest_hashrates[NUM_DEVICES] = {0.0};
String device_ips[NUM_DEVICES] = {""};
int device_ports[NUM_DEVICES] = {44001};

String str_ble_status = "";
String str_http_status = "";
String last_data_capture_bms = "";
String last_data_capture_scc = "";
int last_data_size = 0;

//For UTC -8.00 : -8 * 60 * 60 : -28800
const long gmtOffset_sec = -28800;
//Set it to 3600 if your country observes Daylight saving time; otherwise, set it to 0.
const int daylightOffset_sec = 3600;

const char *ssid = "";
const char *password = "";

String ssid_str = "";
String password_str = "";

bool allow_auto = false;
bool ble_autoconn = false;

//const char *ssid = "TP-Link_7536";
//const char *password = "07570377";

//Your Domain name with URL path or IP address with path
//const String serverName = "http://192.168.10.61:3000/dev/solar";
String reportingUrl_str = "http://192.168.10.61:3000/dev/solar";


unsigned long currentMillis = 0;
unsigned long lastSuccessfulBluetooth = 0;
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
unsigned long lastWebTime = 0;


// Watchdog set for 10 minutes * 3 = 30 minutes
unsigned long bluetoothWatchdogMS = 600000 * 3;

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

// ESP32 - GPIO PIN 
// Arduino Nano ESP32
//#define RELAY_PIN 8

/*
A note about which pins to use: 
- I was originally using pins 17 and 18 (aka RX2 and TX2 on some ESP32 devboards) for RX and TX, 
which worked on an ESP32 Wroom but not an ESP32 Rover. So I switched to pins 13 and 14, which works on both.
I haven't tested on an Arduino board though.
*/
//#define RXD2 13
//#define TXD2 14

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
  //TelnetPrint.println(asdf);
  //status_str += "<p>" + String(asdf) + "</p>\n";
  snprintf(stat_char+strlen(stat_char),  STATUS_BUFFER_SIZE-strlen(stat_char), "<p>%s</p>\n", asdf.c_str());
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
  String html(bool reverse = false) {
    //int num_valid = 0;
    String asdf = "";
    if(reverse) {
      for (int y = this->size()-1; y >= 0; y--) {
        asdf += "<p>" + String(this->c[y]) + "</p>";
      }
    } else {
      for (int y = 0; y < this->size(); y++) {
        asdf += "<p>" + String(this->c[y]) + "</p>";
      }
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

int batt_cap_ah = 280;
//float max_battery_discharge = .2;

float batt_volt_min = 23;
int batt_voltage = 24;
float bat_volt_start = 24.5;

int batt_soc_min = 20;
int batt_soc_start = 50;

// 5 minutes
long shut_cool_ms = 600000;
// Prevent any automatic power-on for 120 seconds
long next_available_startup = millis() + 120000;

bool load_running = false;

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

void notFoundResp(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

template <class T>
String pnSuffix(T inp) {
  return String(inp > 0 ? "+" : "");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Started!");

  //control_gpio_pin(0);
  turn_off_load(-1, "Load Off - Startup");

  prefs.begin("solar-app", false);
  size_t whatsLeft = prefs.freeEntries();    // this method works regardless of the mode in which the namespace is opened.
  str_ble_status += String(whatsLeft);

  batt_cap_ah = prefs.getInt("batt_cap_ah", 150);
  batt_soc_min = prefs.getInt("batt_soc_min", 20);
  batt_soc_start = prefs.getInt("batt_soc_start", 50);
  batt_volt_min = prefs.getDouble("batt_volt_min", 23);
  batt_voltage = prefs.getDouble("batt_voltage", 24);
  bat_volt_start = prefs.getDouble("bat_volt_start", 24.5);
  shut_cool_ms = prefs.getLong("shut_cool_ms", 300000);

  //bool default_set = false;
  ssid_str = prefs.getString("ssid", "dirker");
  password_str = prefs.getString("password", "alphabit");
  //ssid_str = "dirker";
  //password_str = "alphabit";

  allow_auto = prefs.getBool("allow_auto", false);
  // Initial setup if key does not exist
  if (!prefs.isKey("ble_autoconn")) {
    str_ble_status += "Creating initial key!\n";
    if (prefs.putBool("ble_autoconn", false) != 1) {
      str_ble_status += "Error putting key initial key!\n";
    }
  }
  ble_autoconn = prefs.getBool("ble_autoconn", false);

  for(int x = 0; x < NUM_DEVICES; x++) {
    String ip_key = "device_ip_" + String(x);
    String port_key = "device_port_" + String(x);
    String pin_key = "device_pin_" + String(x);
    device_ips[x] = prefs.getString(ip_key.c_str(), "");
    device_ports[x] = prefs.getInt(port_key.c_str(), 0);
    device_gpio_pins[x] = prefs.getInt(pin_key.c_str(), 0);
  }
  for(int x = 0; x < NUM_DEVICES; x++) {
    if(device_gpio_pins[x] != 0) {
      int normalizedPinNumber = device_gpio_pins[x] > 100 ? device_gpio_pins[x]-100 : device_gpio_pins[x];
      //pinMode(device_gpio_pins[x], OUTPUT);
      pinMode(normalizedPinNumber, OUTPUT);
    }
  }
  reportingUrl_str = prefs.getString("report_url", "http://192.168.10.61:3000/dev/solar");
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

  next_available_startup = millis() + shut_cool_ms;

  WiFi.begin(ssid_str.c_str(), password_str.c_str());
  //WiFi.begin("dirker", "alphabit");
  //WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(2500);
    Serial.println("connecting...");
    Serial.println(ssid_str);
    Serial.println(password_str);
  }

  //Telnet log is accessible at port 23
  TelnetPrint.begin();

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  delay(100);

  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
  //configTime(0, 0, "pool.ntp.org");

  AsyncElegantOTA.begin(&server);  // Start AsyncElegantOTA
  Serial.println("After AsyncElegantOTA.begin()");

  handle_webserver_connection();
  //setupServer();

  // Start webserver
  server.begin();

  TelnetPrint.println("Starting Solar monitoring application...");

  delay(100);

  //str_ble_status += "BLE Autoconn: " + String(ble_autoconn);
  //if (ble_autoconn) {
  //  //bleStartup();
  //  bleRequestData();
  //}
  //Serial.println("After bleRequestData");

  tracer.begin();
  Serial.println("After tracer.begin()");

  boot_time_str = getTimestamp();
  Serial.println("After getTimestamp()");
  
  Serial.println("initBle");
  initBle();
}


String structToString() {
    String asdf = "{";

    asdf += "\"battery_soc\": " + String(packBasicInfo.CapacityRemainPercent) + ",";
    asdf += "\"battery_voltage\": " + String((float)packBasicInfo.Volts / 1000) + ",";
    asdf += "\"battery_charging_amps\": " + String((float)packBasicInfo.Amps / 1000) + ",";
    asdf += "\"battery_temperature\": " + String((float)packBasicInfo.Temp1 / 10) + ",";
    asdf += "\"battery_watts_net\": " + String((float)packBasicInfo.Watts) + ",";
    asdf += "\"controller_temperature\": " + String((float)packBasicInfo.Temp2 / 10) + ",";
      TracerData *data = tracer.getData();
      if (data->everythingRead) {
        asdf += "\"solar_panel_voltage\": " + String(data->realtimeData.pvVoltage) + ",";
        asdf += "\"solar_panel_amps\": " + String(data->realtimeData.pvCurrent) + ",";
        asdf += "\"solar_panel_watts\": " + String(data->realtimeData.pvPower) + ",";

        asdf += "\"solar_charging_voltage\": " + String(data->realtimeData.batteryVoltage) + ",";
        asdf += "\"solar_charging_amps\": " + String(data->realtimeData.batteryChargingCurrent) + ",";
        asdf += "\"solar_charging_watts\": " + String(data->realtimeData.batteryChargingPower) + ",";
      }
    if (latest_hashrates[0] > 0.0) {
      asdf += "\"latest_hashrate\": " + String(latest_hashrates[0], 3) + ",";
    }
    for (int x = 1; x < NUM_DEVICES; x++) {
      if(latest_hashrates[x] > 0.0) {
        asdf += "\"latest_hashrate_" + String(x) + "\": " + String(latest_hashrates[x], 3) + ",";
      }
    }
    bool isConnected = BLE_client_connected && ((millis() - lastSuccessfulBluetooth) < bluetoothWatchdogMS);
    asdf += "\"controller_connected\": " + String(isConnected);

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


String htmlHead(bool includeReload = true) {
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

  String data = String(headHtml);
  if (includeReload) {
    data += "setTimeout(function(){ window.location.reload(); }, 15000);";
  }
  data += "</script>";

  data += "</head>";
  return data;
}

String htmlFoot() {
  String data = "<p>Compiled: " + String(compile_date) + "</p>";
  data += "<p><a href='/'>Home</a> | <a href='/raw'>raw</a> | <a href='/json'>json</a> | <a href='/config'>Config</a> | <a href='/update'>Update</a> | <a href='/restart'>restart</a></p>";
  data += "<br/>";
  data += "<p><a href='/ble'>BLE</a> | <a href='/bledisc'>BLE Disconn</a> | <a href='/bleconn'>BLE Connect</a> | <a href='/blereq'>BLE Request</a> | <a href='/bleclearstr'>BLE ClearString</a> | <a href='/bleinit'>BLE Init</a></p>";
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

  data += "<p>Current Rate: " + String((float)packBasicInfo.Watts) + " watts</p>";
  data += "<p>Net Rate: " + String(net_avg_system_watts) + " watts</p>";
  if (automatic_decision != "") {
    data += "<p>Last Automatic: " + String(automatic_decision) + "</p>";
  }
  data += "<p>Automatics: " + String(allow_auto ? "On" : "Off") + "</p>";
  data += automatic_decisions.html(true);

  data += "<p>Current Time: " + getTimestamp() + "</p>";
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

  data += htmlFoot();
  return data;
}

String configIndex() {
  String data = htmlHead(false);

  data += "<form action=\"/set\">Enter an batt_cap_ah: <input type=\"text\" name=\"batt_cap_ah\" value=\"" + String(batt_cap_ah) + "\"><br/>";
  data += "Enter an batt_soc_min: <input type=\"text\" name=\"batt_soc_min\" value=\"" + String(batt_soc_min) + "\"><br/>";
  data += "Enter an batt_soc_start: <input type=\"text\" name=\"batt_soc_start\" value=\"" + String(batt_soc_start) + "\"><br/>";
  data += "Enter an batt_volt_min: <input type=\"text\" name=\"batt_volt_min\" value=\"" + String(batt_volt_min) + "\"><br/>";
  data += "Enter an batt_voltage: <input type=\"text\" name=\"batt_voltage\" value=\"" + String(batt_voltage) + "\"><br/>";
  data += "Enter an bat_volt_start: <input type=\"text\" name=\"bat_volt_start\" value=\"" + String(bat_volt_start) + "\"><br/>";
  data += "Enter an shut_cool_ms: <input type=\"text\" name=\"shut_cool_ms\" value=\"" + String(shut_cool_ms) + "\"><br/>";
  data += "<input type=\"submit\" value=\"Submit\"></form>";

  data += "<form action=\"/set\">Enter a ssid: <input type=\"text\" name=\"ssid\" value=\"" + String(ssid_str) + "\"><br/>";
  data += "Enter a password: <input type=\"text\" name=\"password\" value=\"" + String(password_str) + "\"><br/>";
  data += "<input type=\"submit\" value=\"Submit\"></form>";

  data += "<form action=\"/set\">Allow Automatic: <input type=\"text\" name=\"allow_auto\" value=\"" + String(allow_auto) + "\"><br/>";
  data += "BLE Auto Connect: <input type=\"text\" name=\"ble_autoconn\" value=\"" + String(ble_autoconn) + "\"><br/>";
  data += "<input type=\"submit\" value=\"Submit\"></form>";

  data += "<form action=\"/set\">";
  for(int x = 0; x < NUM_DEVICES; x++) {
    data += "Device IP: <input type=\"text\" name=\"device_ip_" + String(x) + "\" value=\"" + device_ips[x] + "\"><br/>";
    data += "Device Ports: <input type=\"text\" name=\"device_port_" + String(x) + "\" value=\"" + String(device_ports[x]) + "\"><br/>";  
    data += "Device Pins: <input type=\"text\" name=\"device_pin_" + String(x) + "\" value=\"" + String(device_gpio_pins[x]) + "\"><br/>";  
  }
  data += "<input type=\"submit\" value=\"Submit\"></form>";

  data += "<form action=\"/set\">Reporting URL: <input type=\"text\" name=\"report_url\" value=\"" + String(reportingUrl_str) + "\"><br/>";
  data += "<input type=\"submit\" value=\"Submit\"></form>";
  //data += "<form action=\"/set\">BLE Auto Connect: <input type=\"text\" name=\"ble_autoconn\" value=\"" + String(ble_autoconn) + "\"><br/>";
  //data += "<input type=\"submit\" value=\"Submit\"></form>";

  data += htmlFoot();
  return data;
}

String rawIndex(bool asPlaintext = true) {
  const int BUFFERSIZE = 750;
  char sBuff[BUFFERSIZE] = "";
  char endline[5] = "<br>";
  if(asPlaintext) {
    endline[0] = '\n';
    endline[1] = '\0';
  }
  sBuff[0] = '\0'; //clear old data
  //sprintf(sBuff, "Total voltage: %f %s", (float)packBasicInfo.Volts / 1000, endline);
  //sprintf(sBuff, "Amps: %f %s", (float)packBasicInfo.Amps / 1000, endline);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "BMS Captured: %s", last_data_capture_bms.c_str());
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "SCC Captured: %s", last_data_capture_scc.c_str());
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

  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "Latest data size: %d", last_data_size);
  snprintf(sBuff + strlen(sBuff), BUFFERSIZE - strlen(sBuff), "%s", endline);

  String data = sBuff;

  return data;
}

void turn_off_load(int device_number, String decision) {
  if (decision != "") {
    automatic_decision = decision + " dev " + String(device_number) + " @ " + getTimestamp();
    automatic_decisions.push(automatic_decision);
    AppendStatus(automatic_decision);
  }
  // all devices
  if(device_number == -1) {
    for(int x = 0; x < NUM_DEVICES; x++) {
      control_gpio_pin(LOW, device_gpio_pins[x]);
    }
  } else {
    control_gpio_pin(LOW, device_gpio_pins[device_number]);
  }
  //control_gpio_pin(LOW, RELAY_PIN_ONE);
  //control_gpio_pin(LOW, RELAY_PIN_TWO);
}

void power_on_load(int device_number, String decision) {
  if (decision != "") {
    String decisionStr = decision + " device " + String(device_number);
    if(device_number != -1) {
      decisionStr += " (pin " + String(device_gpio_pins[device_number]) + ")";
    }
    decisionStr += " @ " + getTimestamp();
    automatic_decisions.push(decisionStr);
    AppendStatus(decisionStr);
  }
  // all devices
  if(device_number == -1) {
    for(int x = 0; x < NUM_DEVICES; x++) {
      control_gpio_pin(HIGH, device_gpio_pins[x]);
    }
  } else {
    control_gpio_pin(HIGH, device_gpio_pins[device_number]);
  }
  //control_gpio_pin(HIGH, RELAY_PIN_ONE);
  //control_gpio_pin(HIGH, RELAY_PIN_TWO);
}

void handle_webserver_connection() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    status_str = "";
    stat_char[0] = '\0';
    update_decisions(false);
    request->send(200, "text/html", statsIndex());
  });
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
    stat_char[0] = '\0';
    request->send(200, "text/html", configIndex());
  });

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
    //bleStartup();
    initBle();
    request->redirect("/");
  });

  server.on("/bleconn", HTTP_GET, [](AsyncWebServerRequest *request) {
    str_ble_status += getTimestamp() + " - BLE - Connect\n";
    //if(!ble_autoconn) {
    //  bleStartup();
    //}
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
    bleDisconnect();

    str_ble_status = getTimestamp() + " - BLE - Disconnect\n";
    last_data_capture_bms = "Disconnected";
    //turn_off_load("Load Off - Manual");
    request->redirect("/");
  });


  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    //int new_value;
    String input_value;
    if (request->hasArg("batt_cap_ah") || request->hasArg("batt_soc_min") || request->hasArg("batt_soc_start") || request->hasArg("batt_volt_min") || request->hasArg("batt_voltage") || request->hasArg("bat_volt_start") || request->hasArg("shut_cool_ms")
        || request->hasArg("ssid")
        || request->hasArg("password")
        || request->hasArg("allow_auto")
        || request->hasArg("ble_autoconn")
        || request->hasArg("device_ip_0") || request->hasArg("device_port_0") || request->hasArg("device_pin_0") 
        || request->hasArg("device_ip_1") || request->hasArg("device_port_1") || request->hasArg("device_pin_1") 
        || request->hasArg("device_ip_2") || request->hasArg("device_port_2") || request->hasArg("device_pin_2") 
        || request->hasArg("device_ip_3") || request->hasArg("device_port_3") || request->hasArg("device_pin_3") 
        || request->hasArg("report_url")
        ) {
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
        if (request->hasArg("batt_voltage")) {
          input_value = request->arg("batt_voltage");
          batt_voltage = input_value.toFloat();
          prefs.putFloat("batt_voltage", batt_voltage);
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
        if (request->hasArg("ble_autoconn")) {
          input_value = request->arg("ble_autoconn");
          //str_ble_status += "input_value ble_autoconn = " + String(input_value);
          ble_autoconn = std::stoi(input_value.c_str());
          //str_ble_status += "storing ble_autoconn = " + String(ble_autoconn);
          prefs.putBool("ble_autoconn", ble_autoconn);
        }
        for(int x = 0; x < NUM_DEVICES; x++) {
          String ip_key = "device_ip_" + String(x);
          if (request->hasArg(ip_key.c_str())) {
            input_value = request->arg(ip_key.c_str());
            device_ips[x] = input_value;
            prefs.putString(ip_key.c_str(), device_ips[x]);
          }
          String port_key = "device_port_" + String(x);
          if (request->hasArg(port_key.c_str())) {
            input_value = request->arg(port_key.c_str());
            device_ports[x] = std::stoi(input_value.c_str());
            prefs.putInt(port_key.c_str(), device_ports[x]);
          }
          String pin_key = "device_pin_" + String(x);
          if (request->hasArg(pin_key.c_str())) {
            input_value = request->arg(pin_key.c_str());
            device_gpio_pins[x] = std::stoi(input_value.c_str());
            prefs.putInt(pin_key.c_str(), device_gpio_pins[x]);
          }
        }
        if (request->hasArg("report_url")) {
          input_value = request->arg("report_url");
          reportingUrl_str = input_value;
          prefs.putString("report_url", reportingUrl_str);
        }
        delay(100);
        prefs.end();
      } else {
        request->send(200, "text/plain", "Failed to being() prefs for write");
      }
    }
    request->redirect("/config");
  });

  server.onNotFound(notFoundResp);
}

String getTimestamp() {
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
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z", &asdf);
  return String(buf);
  //printf("%s\n", buf);
  //return now;
}

void update_decisions(bool allow_automatic) {
  if (!BLE_client_connected) {
    AppendStatus("Current Time: " + getTimestamp());
    AppendStatus("BLE Controller is not connected. No decisions will be made");
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

  // The new M4-ATX module seems to continue running even beyond an OTA update (and subsequent relay flapping on reboot)
  // So it's possible the load is actually still running after a reboot!?!?  This handles that case.
  // last_action_was_manual == true means we turned it *on* or *off* manually.
  // If something happened automatically then there's not need to override the 'load_running' variable.
  if (valid_data_points_load_watts > (num_data_points_decision/2)) {
    //if (net_avg_system_watts < 0.0 && !last_action_was_manual) {  
    if (net_avg_system_watts < 0.0) {  
      load_running = true;
    }
  }

  //float net_rate_ah = (float)packBasicInfo.Amps / 1000;
  //float time_to_empty_hrs = (((avg_batt_soc / 100.00) - (1 - max_battery_discharge)) * batt_cap_ah) / (net_rate_ah * -1.0);
  //float time_to_empty_mins = time_to_empty_hrs * 60;
  //time_to_empty_queue.push(time_to_empty_mins);

  if (valid_data_points_battery_voltage > num_data_points_decision && valid_data_points_battery_soc > num_data_points_decision) {
    if (load_running && avg_batt_volts < batt_volt_min) {
      if (allow_automatic) {
        last_action_was_manual = false;
        turn_off_load(-1, "Turn load off (voltage) " + String(avg_batt_volts) + " < " + String(batt_volt_min));
      }
    } else {
      if (load_running && avg_batt_soc < batt_soc_min) {
        if (allow_automatic) {
          last_action_was_manual = false;
          turn_off_load(-1, "Turn load off (battery soc) " + String(avg_batt_soc) + " < " + String(batt_soc_min));
        }
      } else {
        if (load_running) {
          current_status = "";
          //AppendStatus("Load already on");
        } else {
          if (avg_batt_volts > bat_volt_start) {
            if (avg_batt_soc > batt_soc_start) {
              if (next_available_startup != 0 && millis() > next_available_startup) {
                if (allow_automatic && net_avg_system_amps > 0) {
                  last_action_was_manual = false;
                  power_on_load(-1, "Turn on loads");
                }
                current_status = "";
              } else {
                AppendStatus("Wait for cooldown: " + String(next_available_startup - millis()));
                current_status = "Wait for cooldown " + String(next_available_startup - millis());
              }
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
  //float panel_incoming_rate_ah = ((float)packBasicInfo.Watts) / (batt_voltage * 1.0);
  //AppendStatus("Panel is at " + String(renogy_data.solar_panel_amps) + " amps (r) and " + String(renogy_data.solar_panel_watts) + " watts (r)");
  //AppendStatus("Panel is producing " + String(panel_incoming_rate_ah) + " ah (c)");

  if (valid_data_points_load_watts > num_data_points_decision) {
    float load_outgoing_rate_ah = net_avg_system_watts / (batt_voltage * 1.0);
    AppendStatus("Net load is " + pnSuffix(load_outgoing_rate_ah) + String(load_outgoing_rate_ah) + " ah (c) @ " + String(batt_voltage) + "v (" + String(net_avg_system_watts) + " watts)");

    //float charging_rate_ah = panel_incoming_rate_ah - load_outgoing_rate_ah;
    AppendStatus("Net System Rate is " + pnSuffix(net_avg_system_amps) + String(net_avg_system_amps) + " ah (c) @ " + String(avg_batt_volts) + "v");

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
      if (valid_data_points_tte > num_data_points_decision) {
        AppendStatus("Battery will be empty in " + String(avg_time_to_empty_mins/60) + " hours (" + String(avg_time_to_empty_mins) + " min)");
        /*
        if (avg_time_to_empty_mins > 0) {
          if (avg_time_to_empty_mins < 60) {
            //if (load_running && avg_time_to_empty_mins < 60) {
            //  if (allow_automatic) {
            //    turn_off_load("Battery will be dead in less than 60 minutes!  Kill the load! " + String(avg_time_to_empty_mins) + " to go");
            //  }
            //  //AppendStatus("Battery will be dead in less than 30 minutes!  Kill the load!");
            //}
          } else {
            // No-op. Running normally but discharging.
          }
        } else {
          AppendStatus("Should not see this? net_avg_system_amps says discharging but avg_time_to_empty_mins says charging");
        }
        */
      } else {
        //AppendStatus("At that rate it will be empty in " + String(avg_time_to_empty_mins / 60) + " hours");
        AppendStatus("Not enough data points");
      }
    }
  } else {
    AppendStatus("Not enough load points");
  }

  //float charging_rate_ah = renogy.battery_charging_amps
  //AppendStatus("Battery is charging at " + String(charging_rate_ah) + " ah");
  //float ah_left_to_charge = ((batt_cap_ah*1.0) - ((batt_cap_ah*1.0) * (renogy_data.battery_soc / 100)));
}

bool loopOneTime = false;
void loop() {
  // I think this just has to run all the time?
  tracer.update();
  if (first_loop) {
    TelnetPrint.println("Entering loop");
    TelnetPrint.println("Entering loop...");
  }

  currentMillis = millis();
  // ten minutes
  
  if ((currentMillis - lastSuccessfulBluetooth) > bluetoothWatchdogMS && !first_loop) {
    str_ble_status = getTimestamp() + " - BLE Watchdog - Disconnect\n";
    TelnetPrint.println(str_ble_status);
    automatic_decisions.push(str_ble_status);
    bleDisconnect();
    last_data_capture_bms = "Disconnected";
    /*
    bleDisconnect();
    TelnetPrint.println("Bluetooth Watchdog - Delay");
    delay(500);
    TelnetPrint.println("Bluetooth Watchdog - Connect");
    if(connectToServer()) {
      lastSuccessfulBluetooth = millis();
    } else {
      delay(5000);
    }
    */
  }

  // Only read info every 10th loop
  if ((currentMillis - lastTime) > timerDelayMS || first_loop) {
    loopOneTime = true;
    TelnetPrint.println("Updating data");
    status_str = "";
    stat_char[0] = '\0';
    bleRequestData();
    delay(250);
    if(first_loop && !BLE_client_connected) {
      power_on_load(0, "Firstboot: turn on first GPIO"); 
    }
    
    if(requestFromEPEver()) {
      last_data_capture_scc = getTimestamp();
    }
    
    //}

    if((float)packBasicInfo.CapacityRemainPercent > 0.0) {
      battery_voltage_queue.push((float)packBasicInfo.Volts / 1000);
      battery_soc_queue.push((float)packBasicInfo.CapacityRemainPercent);
      net_system_watts_queue.push((float)packBasicInfo.Watts);
      net_system_amps_queue.push((float)packBasicInfo.Amps / 1000);
    }

    int valid_data_points_battery_soc = 0;
    avg_batt_soc = battery_soc_queue.average(&valid_data_points_battery_soc, "Battery SOC");

    int valid_data_points_net_amps = 0;
    net_avg_system_amps = net_system_amps_queue.average(&valid_data_points_net_amps, "Avg Net System Amps");

    if(valid_data_points_battery_soc > num_data_points_decision && valid_data_points_net_amps > num_data_points_decision) {
      //float net_rate_ah = (float)packBasicInfo.Amps / 1000;
      float time_to_empty_hrs = (((avg_batt_soc / 100.00) - (batt_soc_min / 100.00)) * batt_cap_ah) / (net_avg_system_amps * -1.0);
      float time_to_empty_mins = time_to_empty_hrs * 60;
      time_to_empty_queue.push(time_to_empty_mins);
    }

    update_decisions(allow_auto);
    lastTime = millis();
  }

  first_loop = false;
}

// control the load pins on Renogy charge controllers that have them
void control_gpio_pin(int desiredHIGHLOW, int pin) {
  if(pin == 0) {
    return;
  }
  bool origState = desiredHIGHLOW;
  // Trick for handling inverted pins
  if(pin > 100) {
    pin -= 100;
    desiredHIGHLOW = !desiredHIGHLOW;
  }
  // The relays I use are wired backwards!?  So this HIGH/LOW are switched from what they'd normally be...
  // Moderately annoying.
  String stat = String("Setting pin " + String(pin) + " to ");
  if(desiredHIGHLOW == 0) {
    stat += "Low";
  } else {
    stat += "High";
  }
  TelnetPrint.println(stat);
  if (origState) {
    //digitalWrite(pin, LOW);  // turn on load
    digitalWrite(pin, desiredHIGHLOW);  // turn off load
    load_running = true;
  } else {
    //digitalWrite(pin, HIGH);  // turn off load
    digitalWrite(pin, desiredHIGHLOW);  // turn off load
    load_running = false;
  }
  /*
  // The MOSFETs are wired correctly... so this HIGH/LOW are as expected
  if (state) {
    digitalWrite(pin, HIGH);  // turn on load
    load_running = true;
  } else {
    digitalWrite(pin, LOW);  // turn off load
    load_running = false;
  }
  */
  next_available_startup = millis() + shut_cool_ms;
  //if (state==1) node.writeSingleRegister(0x010A, 1);  // turn on load
  //else node.writeSingleRegister(0x010A, 0);  // turn off load
}

bool requestFromEPEver() {
  //float rssi = WiFi.RSSI();
  //TelnetPrint.print("WiFi signal strength is: "); TelnetPrint.println(rssi);
  //TelnetPrint.print("WiFi signal strength is: "); TelnetPrint.println(rssi);

  TelnetPrint.println("Reading the EPEver...");

  if (tracer.update()) {
      TelnetPrint.println("EPEver update succeeded...");
      TracerData *data = tracer.getData();
      if (data->everythingRead) {
          TelnetPrint.println("MODBUS everything read!");

          /*
          TelnetPrint.print("pvPower: ");
          TelnetPrint.println(data->realtimeData.pvPower);
          TelnetPrint.print("pvCurrent: ");
          TelnetPrint.println(data->realtimeData.pvCurrent);
          TelnetPrint.print("pvVoltage: ");
          TelnetPrint.println(data->realtimeData.pvVoltage);

          TelnetPrint.print("batteryChargingPower: ");
          TelnetPrint.println(data->realtimeData.batteryChargingPower);
          TelnetPrint.print("batteryChargingCurrent: ");
          TelnetPrint.println(data->realtimeData.batteryChargingCurrent);
          TelnetPrint.print("batteryVoltage: ");
          TelnetPrint.println(data->realtimeData.batteryVoltage);

          TelnetPrint.print("equipmentTemp: ");
          TelnetPrint.println(data->realtimeData.equipmentTemp);
          TelnetPrint.print("heatsinkTemp: ");
          TelnetPrint.println(data->realtimeData.heatsinkTemp);
          */
      } else {
        TelnetPrint.println("requestFromEPEver->MODBUS missing something...");
        return false;
      }
      //delay(1000);
      return true;
  }
  TelnetPrint.println("EPEver update failed...");
  return false;
}

/*
// Store common strings in flash memory with FPSTR access
const char HTML_DOCTYPE[] PROGMEM = "<!DOCTYPE html><html>";
const char HTML_HEAD[] PROGMEM = "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><link rel=\"icon\" href=\"data:,\">";
const char HTML_STYLE[] PROGMEM = "<style>html{font-family:Helvetica;display:inline-block;margin:0px auto;text-align:center;}.button{background-color:#4CAF50;border:none;color:white;padding:16px 40px;text-decoration:none;font-size:30px;margin:2px;cursor:pointer;}.button2{background-color:#555555;}</style>";
const char HTML_SCRIPT_START[] PROGMEM = "<script>";
const char HTML_SCRIPT_RELOAD[] PROGMEM = "setTimeout(function(){ window.location.reload(); }, 5000);";
const char HTML_SCRIPT_END[] PROGMEM = "</script>";
const char HTML_HEAD_END[] PROGMEM = "</head>";
const char HTML_BODY_START[] PROGMEM = "<body>";
const char HTML_BODY_END[] PROGMEM = "</body></html>";

// Template function for HTML head that works with any stream-like class
template <typename T>
void sendHtmlHead(T& client, bool includeReload = true) {
  client.print(FPSTR(HTML_DOCTYPE));
  client.print(FPSTR(HTML_HEAD));
  client.print(FPSTR(HTML_STYLE));
  client.print(FPSTR(HTML_SCRIPT_START));
  
  if (includeReload) {
    client.print(FPSTR(HTML_SCRIPT_RELOAD));
  }
  
  client.print(FPSTR(HTML_SCRIPT_END));
  client.print(FPSTR(HTML_HEAD_END));
  client.print(FPSTR(HTML_BODY_START));
}

// Template function for HTML footer
template <typename T>
void sendHtmlFoot(T& client) {
  client.print("<p>Compiled: ");
  client.print(compile_date);
  client.print("</p><p><a href='/'>Home</a> | <a href='/raw'>raw</a> | <a href='/json'>json</a> | <a href='/config'>Config</a> | <a href='/update'>Update</a> | <a href='/restart'>restart</a></p><br/>");
  client.print("<p><a href='/ble'>BLE</a> | <a href='/bledisc'>BLE Disconn</a> | <a href='/bleconn'>BLE Connect</a> | <a href='/blereq'>BLE Request</a> | <a href='/bleclearstr'>BLE ClearString</a> | <a href='/bleinit'>BLE Init</a></p>");
  client.print(FPSTR(HTML_BODY_END));
}

// Template function for Stats page
template <typename T>
void handleStatsRequest(T& client) {
  sendHtmlHead(client);
  
  // Web Page Heading
  client.print("<h1>ESP32 Web Server - With OTAv3</h1>");
  client.print("<p>Boot Time: ");
  client.print(boot_time_str);
  client.print("</p>");
  
  // Display current state of the load
  client.print("<p>Relay Load - ");
  client.print(load_running ? "Running" : "Stopped");
  client.print("</p>");
  
  client.print("<p>Current Rate: ");
  client.print((float)packBasicInfo.Watts);
  client.print(" watts</p>");
  
  client.print("<p>Net Rate: ");
  client.print(net_avg_system_watts);
  client.print(" watts</p>");
  
  if (automatic_decision.length() > 0) {
    client.print("<p>Last Automatic: ");
    client.print(automatic_decision);
    client.print("</p>");
  }
  
  client.print("<p>Automatics: ");
  client.print(allow_auto ? "On" : "Off");
  client.print("</p>");
  
  // Automatic decisions
  //automatic_decisions.htmlDirect(client, true);
  String asdf = automatic_decisions.html(true);
  client.print(asdf);
  
  client.print("<p>Current Time: ");
  client.print(getTimestamp());
  client.print("</p>");
  
  client.print("<p>Last BMS Data Capture: ");
  client.print(last_data_capture_bms);
  client.print("</p>");
  
  client.print("<p>Last SCC Data Capture: ");
  client.print(last_data_capture_scc);
  client.print("</p>");
  
  if (current_status.length() > 0) {
    client.print("<p>Current: ");
    client.print(current_status);
    client.print("</p>");
  }
  
  client.print("<p>");
  client.print(status_str);
  client.print("</p>");
  
  client.print("<p>***");
  client.print(stat_char);
  client.print("***</p>");
  
  // Buttons for load control
  client.print("<p><a href=\"/load/off\"><button class=\"button button2\">All OFF</button></a><a href=\"/load/on\"><button class=\"button\">All ON</button></a></p>");
  
  for(int x = 0; x < NUM_DEVICES; x++) {
    client.print("<p><a href=\"/load/off?device=");
    client.print(x);
    client.print("\"><button class=\"button button2\">");
    client.print(x);
    client.print(" OFF</button></a>");
    
    client.print("<a href=\"/load/on?device=");
    client.print(x);
    client.print("\"><button class=\"button\">");
    client.print(x);
    client.print(" ON</button></a></p>");
  }
  
  sendHtmlFoot(client);
}

// Template function for Config page
template <typename T>
void handleConfigRequest(T& client) {
  sendHtmlHead(client, false);
  
  // Battery configuration form
  client.print("<form action=\"/set\">Enter an batt_cap_ah: <input type=\"text\" name=\"batt_cap_ah\" value=\"");
  client.print(batt_cap_ah);
  client.print("\"><br/>Enter an batt_soc_min: <input type=\"text\" name=\"batt_soc_min\" value=\"");
  client.print(batt_soc_min);
  client.print("\"><br/>Enter an batt_soc_start: <input type=\"text\" name=\"batt_soc_start\" value=\"");
  client.print(batt_soc_start);
  client.print("\"><br/>Enter an batt_volt_min: <input type=\"text\" name=\"batt_volt_min\" value=\"");
  client.print(batt_volt_min);
  client.print("\"><br/>Enter an batt_voltage: <input type=\"text\" name=\"batt_voltage\" value=\"");
  client.print(batt_voltage);
  client.print("\"><br/>Enter an bat_volt_start: <input type=\"text\" name=\"bat_volt_start\" value=\"");
  client.print(bat_volt_start);
  client.print("\"><br/>Enter an shut_cool_ms: <input type=\"text\" name=\"shut_cool_ms\" value=\"");
  client.print(shut_cool_ms);
  client.print("\"><br/><input type=\"submit\" value=\"Submit\"></form>");
  
  // WiFi configuration form
  client.print("<form action=\"/set\">Enter a ssid: <input type=\"text\" name=\"ssid\" value=\"");
  client.print(ssid_str);
  client.print("\"><br/>Enter a password: <input type=\"text\" name=\"password\" value=\"");
  client.print(password_str);
  client.print("\"><br/><input type=\"submit\" value=\"Submit\"></form>");
  
  // Auto settings form
  client.print("<form action=\"/set\">Allow Automatic: <input type=\"text\" name=\"allow_auto\" value=\"");
  client.print(allow_auto);
  client.print("\"><br/>BLE Auto Connect: <input type=\"text\" name=\"ble_autoconn\" value=\"");
  client.print(ble_autoconn);
  client.print("\"><br/><input type=\"submit\" value=\"Submit\"></form>");
  
  // Device configuration form
  client.print("<form action=\"/set\">");
  for(int x = 0; x < NUM_DEVICES; x++) {
    client.print("Device IP: <input type=\"text\" name=\"device_ip_");
    client.print(x);
    client.print("\" value=\"");
    client.print(device_ips[x]);
    client.print("\"><br/>Device Ports: <input type=\"text\" name=\"device_port_");
    client.print(x);
    client.print("\" value=\"");
    client.print(device_ports[x]);
    client.print("\"><br/>");
  }
  client.print("<input type=\"submit\" value=\"Submit\"></form>");
  
  // Reporting URL form
  client.print("<form action=\"/set\">Reporting URL: <input type=\"text\" name=\"report_url\" value=\"");
  client.print(reportingUrl_str);
  client.print("\"><br/><input type=\"submit\" value=\"Submit\"></form>");
  
  sendHtmlFoot(client);
}

// Template function for Raw data page
template <typename T>
void handleRawRequest(T& client, bool asPlaintext = true) {
  const char* endline = asPlaintext ? "\n" : "<br>";
  
  // Write directly to client to avoid buffer overflow
  client.print("BMS Captured: ");
  client.print(last_data_capture_bms);
  client.print(endline);
  
  client.print("SCC Captured: ");
  client.print(last_data_capture_scc);
  client.print(endline);
  
  client.print("Total voltage: ");
  client.print((float)packBasicInfo.Volts / 1000, 2);
  client.print(endline);
  
  client.print("Amps: ");
  client.print((float)packBasicInfo.Amps / 1000, 2);
  client.print(endline);
  
  client.print("Watts: ");
  client.print((float)packBasicInfo.Watts, 2);
  client.print(endline);
  
  client.print("CapacityRemainAh: ");
  client.print((float)packBasicInfo.CapacityRemainAh / 1000, 3);
  client.print(endline);
  
  client.print("CapacityRemainPercent: ");
  client.print(packBasicInfo.CapacityRemainPercent);
  client.print(endline);
  
  client.print("Temp1: ");
  client.print((float)packBasicInfo.Temp1 / 10, 1);
  client.print(endline);
  
  client.print("Temp2: ");
  client.print((float)packBasicInfo.Temp2 / 10, 1);
  client.print(endline);
  
  client.print("Balance Code Low: 0x");
  client.print(packBasicInfo.BalanceCodeLow, HEX);
  client.print(endline);
  
  client.print("Balance Code High: 0x");
  client.print(packBasicInfo.BalanceCodeHigh, HEX);
  client.print(endline);
  
  client.print("Mosfet Status: 0x");
  client.print(packBasicInfo.MosfetStatus, HEX);
  client.print(endline);
  
  client.print("Number of cells: ");
  client.print(packCellInfo.NumOfCells);
  client.print(endline);
  
  // Print cell information
  for (byte i = 1; i <= packCellInfo.NumOfCells; i++) {
    client.print("Cell no. ");
    client.print(i);
    client.print("    ");
    client.print((float)packCellInfo.CellVolt[i - 1] / 1000, 3);
    client.print(endline);
  }
  
  client.print("Max cell volt: ");
  client.print((float)packCellInfo.CellMax / 1000, 3);
  client.print(endline);
  
  client.print("Min cell volt: ");
  client.print((float)packCellInfo.CellMin / 1000, 3);
  client.print(endline);
  
  client.print("Difference cell volt: ");
  client.print((float)packCellInfo.CellDiff / 1000, 4);
  client.print(endline);
  
  client.print("Average cell volt: ");
  client.print((float)packCellInfo.CellAvg / 1000, 3);
  client.print(endline);
  
  client.print("Median cell volt: ");
  client.print((float)packCellInfo.CellMedian / 1000, 3);
  client.print(endline);
  
  client.print("Latest data size: ");
  client.print(last_data_size);
  client.print(endline);
}

void writeJsonToStream(AsyncResponseStream *response) {
  // Start the JSON object
  response->print("{");
  
  // Battery data
  response->printf("\"battery_soc\":%d,", packBasicInfo.CapacityRemainPercent);
  response->printf("\"battery_voltage\":%.3f,", (float)packBasicInfo.Volts / 1000);
  response->printf("\"battery_charging_amps\":%.3f,", (float)packBasicInfo.Amps / 1000);
  response->printf("\"battery_temperature\":%.1f,", (float)packBasicInfo.Temp1 / 10);
  response->printf("\"battery_watts_net\":%.1f,", (float)packBasicInfo.Watts);
  response->printf("\"controller_temperature\":%.1f,", (float)packBasicInfo.Temp2 / 10);
  
  // Solar data - only add if available
  TracerData *data = tracer.getData();
  if (data->everythingRead) {
    response->printf("\"solar_panel_voltage\":%.2f,", data->realtimeData.pvVoltage);
    response->printf("\"solar_panel_amps\":%.2f,", data->realtimeData.pvCurrent);
    response->printf("\"solar_panel_watts\":%.2f,", data->realtimeData.pvPower);
    
    response->printf("\"solar_charging_voltage\":%.2f,", data->realtimeData.batteryVoltage);
    response->printf("\"solar_charging_amps\":%.2f,", data->realtimeData.batteryChargingCurrent);
    response->printf("\"solar_charging_watts\":%.2f,", data->realtimeData.batteryChargingPower);
  }
  
  // Hashrate data - only add if available
  if (latest_hashrates[0] > 0.0) {
    response->printf("\"latest_hashrate\":%.3f,", latest_hashrates[0]);
  }
  
  for (int x = 1; x < NUM_DEVICES; x++) {
    if (latest_hashrates[x] > 0.0) {
      response->printf("\"latest_hashrate_%d\":%.3f,", x, latest_hashrates[x]);
    }
  }
  
  // Connection status - last field (no trailing comma)
  bool isConnected = BLE_client_connected && ((millis() - lastSuccessfulBluetooth) < bluetoothWatchdogMS);
  response->printf("\"controller_connected\":%s", isConnected ? "true" : "false");
  
  // Close the JSON object
  response->print("}");
}

// Setup AsyncWebServer to use our new template functions
void setupServer() {
  AsyncWebServer server(80);
  
  // Home page handler
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    handleStatsRequest(*response);
    request->send(response);
  });
  
  // Config page handler
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    status_str = "";
    stat_char[0] = '\0';
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    handleConfigRequest(*response);
    request->send(response);
  });
  
  // Raw data handler (plaintext)
  server.on("/raw", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/plain");
    handleRawRequest(*response, true);
    request->send(response);
  });
  
  // Raw data handler (HTML)
  server.on("/raw-html", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    handleRawRequest(*response, false);
    request->send(response);
  });
  
  // JSON data handler
  server.on("/json", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    writeJsonToStream(response);
    request->send(response);
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

  // Control endpoints would be similar
  server.on("/load/on", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Get device parameter if present
    int device = -1;
    if (request->hasParam("device")) {
      device = request->getParam("device")->value().toInt();
    }
    
    // Set load state
    //setLoadState(device, true);
    power_on_load(device, "Load On - Manual");
    
    // Redirect back to home page
    request->redirect("/");
  });
  
  server.on("/load/off", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Get device parameter if present
    int device = -1;
    if (request->hasParam("device")) {
      device = request->getParam("device")->value().toInt();
    }
    
    // Set load state
    //setLoadState(device, false);
    turn_off_load(device, "Load Off - Manual");
    
    // Redirect back to home page
    request->redirect("/");
  });


  server.on("/bleinit", HTTP_GET, [](AsyncWebServerRequest *request) {
    str_ble_status += getTimestamp() + " - BLE - Connect\n";
    //bleStartup();
    initBle();
    request->redirect("/");
  });

  server.on("/bleconn", HTTP_GET, [](AsyncWebServerRequest *request) {
    str_ble_status += getTimestamp() + " - BLE - Connect\n";
    //if(!ble_autoconn) {
    //  bleStartup();
    //}
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
    bleDisconnect();

    str_ble_status = getTimestamp() + " - BLE - Disconnect\n";
    last_data_capture_bms = "Disconnected";
    //turn_off_load("Load Off - Manual");
    request->redirect("/");
  });


  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    //int new_value;
    String input_value;
    if (request->hasArg("batt_cap_ah") || request->hasArg("batt_soc_min") || request->hasArg("batt_soc_start") || request->hasArg("batt_volt_min") || request->hasArg("batt_voltage") || request->hasArg("bat_volt_start") || request->hasArg("shut_cool_ms")
        || request->hasArg("ssid")
        || request->hasArg("password")
        || request->hasArg("allow_auto")
        || request->hasArg("ble_autoconn")
        || request->hasArg("device_ip_0") || request->hasArg("device_port_0")
        || request->hasArg("device_ip_1") || request->hasArg("device_port_1")
        || request->hasArg("device_ip_2") || request->hasArg("device_port_2")
        || request->hasArg("device_ip_3") || request->hasArg("device_port_3")
        || request->hasArg("report_url")
        ) {
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
        if (request->hasArg("batt_voltage")) {
          input_value = request->arg("batt_voltage");
          batt_voltage = input_value.toFloat();
          prefs.putFloat("batt_voltage", batt_voltage);
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
        if (request->hasArg("ble_autoconn")) {
          input_value = request->arg("ble_autoconn");
          //str_ble_status += "input_value ble_autoconn = " + String(input_value);
          ble_autoconn = std::stoi(input_value.c_str());
          //str_ble_status += "storing ble_autoconn = " + String(ble_autoconn);
          prefs.putBool("ble_autoconn", ble_autoconn);
        }
        for(int x = 0; x < NUM_DEVICES; x++) {
          String ip_key = "device_ip_" + String(x);
          if (request->hasArg(ip_key.c_str())) {
            input_value = request->arg(ip_key.c_str());
            device_ips[x] = input_value;
            prefs.putString(ip_key.c_str(), device_ips[x]);
          }
          String port_key = "device_port_" + String(x);
          if (request->hasArg(port_key.c_str())) {
            input_value = request->arg(port_key.c_str());
            device_ports[x] = std::stoi(input_value.c_str());
            prefs.putInt(port_key.c_str(), device_ports[x]);
          }
        }
        if (request->hasArg("report_url")) {
          input_value = request->arg("report_url");
          reportingUrl_str = input_value;
          prefs.putString("report_url", reportingUrl_str);
        }
        delay(100);
        prefs.end();
      } else {
        request->send(200, "text/plain", "Failed to being() prefs for write");
      }
    }
    request->redirect("/config");
  });

  server.onNotFound(notFoundResp);
  
  // Start server
  server.begin();
}
*/