/*

Reads the data from the Renogy charge controller via it's RS232 port using an ESP32 or similar. Tested with Wanderer 30A (CTRL-WND30-LI) and Wanderer 10A

See my Github repo for notes on building the cable:
https://github.com/wrybread/ESP32ArduinoRenogy

Notes: 
- I don't think can power the ESP32 from the Renogy's USB port.. Maybe it's so low power that it shuts off?


To do:
- find out how much of a load the load port can handle... 
- test with an Arduino


*/


#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>

#include <WebServer.h>
#include <Update.h>
#include "time.h"
//#include <TimeLib.h>

// https://github.com/syvic/ModbusMaster
#include <ModbusMaster.h>
ModbusMaster node;

const char compile_date[] = __DATE__ " " __TIME__;

Preferences prefs;

// Set web server port number to 80
WebServer server(80);
// Variable to store the HTTP request
String header;
String status_str = "";

WiFiClientSecure client;  // or WiFiClientSecure for HTTPS
HTTPClient http;

bool flashing_ota = false;

const char* ssid = "dirker";
const char* password = "alphabit";

//Your Domain name with URL path or IP address with path
const char* serverName = "https://lhbqdvca46.execute-api.us-west-2.amazonaws.com/dev/solar";

unsigned long currentMillis = 0;
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
unsigned long lastWebTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelayMS = 600000;
// Set timer to 5 seconds (5000)
//unsigned long timerDelayMS = 5000;
// Set timer to 60 seconds (60000)
unsigned long timerDelayMS = 60000;

unsigned long previousMillis = 0;        // will store last time LED was updated
const long interval = 6000;           // interval at which to blink (milliseconds)

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
const uint32_t num_data_registers = 35;
const uint32_t num_info_registers = 17;

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


// if you don't have a charge controller to test with, can set this to true to get non 0 voltage readings
bool simulator_mode = false;

int battery_voltage = 12;
int batt_cap_ah = 40;
float max_battery_discharge = .5;

float minimum_shutoff_voltage = 11.5;
float minimum_starting_voltage = 12.5;

int batt_soc_min = 50;
int batt_soc_start = 65;

// 5 minutes
const long shutdown_cooldown_ms = 300000;
// Prevent any automatic power-on for 120 seconds
long next_available_startup = millis() + 120000;


float sim_starting_battery_voltage = 13.12;
float sim_bat_volt_change = -0.11;

uint8_t sim_starting_battery_soc = 51.5;
float sim_bat_soc_change = -1;

bool load_running = false;

float sim_starting_solar_panel_watts = 20;
int loop_number = 0;


void setup()
{
  Serial.begin(115200);
  Serial.println("Started!");

  prefs.begin("solar-app", false);
  batt_cap_ah = prefs.getInt("batt_cap_ah", 50);
  batt_soc_min = prefs.getInt("batt_soc_min", 50);
  batt_soc_start = prefs.getInt("batt_soc_start", 65);
  // Close the Preferences
  prefs.end();

    WiFi.begin(ssid, password);
    //WiFi.begin("Wokwi-GUEST", "", 6);
    while (WiFi.status() != WL_CONNECTED) {
      delay(250);
      Serial.println("connecting...");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    configTime(0, 0, "pool.ntp.org");

    client.setInsecure();

  // create a second serial interface for modbus
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); 

  // my Renogy Wanderer has an (slave) address of 255! Not in docs??? 
  // Do all Renogy charge controllers use this address?
  int modbus_address = 255; 
  node.begin(modbus_address, Serial2);

  // Start webserver
  server.begin();

  delay(500);
}

void AppendStatus(String asdf) {
  Serial.println(asdf);
  status_str += "<p>" + String(asdf) + "</p>\n";
}

String structToString(struct Controller_data myData){
    String asdf = "{";

    asdf += "\"battery_soc\": " + String(myData.battery_soc) + ",";
    asdf += "\"battery_voltage\": " + String(myData.battery_voltage) + ",";
    asdf += "\"battery_charging_amps\": " + String(myData.battery_charging_amps) + ",";
    asdf += "\"battery_temperature\": " + String(myData.battery_temperature) + ",";
    asdf += "\"controller_temperature\": " + String(myData.controller_temperature) + ",";
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


String statsIndex() {
  String data = "";
  // Display the HTML web page
  data += "<!DOCTYPE html><html>";
  data += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  data += "<link rel=\"icon\" href=\"data:,\">";
  // CSS to style the on/off buttons 
  // Feel free to change the background-color and font-size attributes to fit your preferences
  data += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  data += ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;";
  data += "text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
  data += ".button2 {background-color: #555555;}</style>";

  data += "<script>";
  data += "setTimeout(function(){ window.location.reload(); }, 15000);";
  data += "</script>";

  data += "</head>";
  
  // Web Page Heading
  data += "<body><h1>ESP32 Web Server - With OTAv3</h1>";
  data += "<p>Compiled: " + String(compile_date) + "</p>";
  
  // Display current state of the Renogy load
  if(load_running) {
    data += "<p>Renogy Load - Running @ " + String(renogy_data.load_watts) + " watts</p>";
  } else {
    data += "<p>Renogy Load - Stopped - " + String(renogy_data.load_watts) + "</p>";
  }
  data += "<p>Last Automatic: " + String(automatic_decision) + "</p>";
  data += "<p>" + String(status_str) + "</p>";
  // If the load_running is on, it displays the OFF button
  if (load_running) {
    data += "<p><a href=\"/load/off\"><button class=\"button button2\">OFF</button></a></p>";
  } else {
    data += "<p><a href=\"/load/on\"><button class=\"button\">ON</button></a></p>";
  }

  data += "<form action=\"/set\">      Enter an batt_cap_ah: <input type=\"text\" name=\"batt_cap_ah\" value=\"" + String(batt_cap_ah) + "\"><br/>";
  data += "<form action=\"/set\">      Enter an batt_soc_min: <input type=\"text\" name=\"batt_soc_min\" value=\"" + String(batt_soc_min) + "\"><br/>";
  data += "<form action=\"/set\">      Enter an batt_soc_start: <input type=\"text\" name=\"batt_soc_start\" value=\"" + String(batt_soc_start) + "\"><br/>";
  data += "<input type=\"submit\" value=\"Submit\"></form>";

  data += "</body></html>";
  return data;
}

const char* loginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";
 
/*
 * Server Index Page
 */
 
const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";


void handle_webserver_connection() {
  //Serial.println("Before handleClient()");
  server.handleClient();
  //Serial.println("After handleClient()");

  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    //harvest_data();
    update_decisions();
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", statsIndex());
  });
  server.on("/new", HTTP_GET, []() {
    harvest_data();
    update_decisions();
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", statsIndex());
  });
  server.on("/load/on", HTTP_GET, []() {
    Serial.println("Turn load on");
    renogy_control_load(1);
    load_running = true;
    server.sendHeader("Location", "/new", true);
    server.send(302, "text/plain", "");
    //server.redirect("/new");
    //server.sendHeader("Connection", "close");
    //server.send(200, "text/html", statsIndex());
  });
  server.on("/load/off", HTTP_GET, []() {
    Serial.println("Turn load off");
    renogy_control_load(0);
    load_running = false;
    next_available_startup = millis() + shutdown_cooldown_ms;
    server.sendHeader("Location", "/new", true);
    server.send(302, "text/plain", "");
    //server.sendHeader("Connection", "close");
    //server.send(200, "text/html", statsIndex());
  });

  server.on("/set", HTTP_GET, []() {
    int new_value;
    String input_value;
    if (server.hasArg("batt_cap_ah") || server.hasArg("batt_soc_min") || server.hasArg("batt_soc_start")) {
      if(prefs.begin("solar-app", false)) {
        if (server.hasArg("batt_cap_ah")) {
          input_value = server.arg("batt_cap_ah");
          batt_cap_ah = input_value.toInt();
          prefs.putInt("batt_cap_ah", batt_cap_ah);
        }
        if (server.hasArg("batt_soc_min")) {
          input_value = server.arg("batt_soc_min");
          batt_soc_min = input_value.toInt();
          prefs.putInt("batt_soc_min", batt_soc_min);
        }
        if (server.hasArg("batt_soc_start")) {
          input_value = server.arg("batt_soc_start");  
          batt_soc_start = input_value.toInt();
          prefs.putInt("batt_soc_start", batt_soc_start);
        }
        prefs.end();
        /*
        delay(10);

        if(prefs.begin("solar-app", true)) {
          batt_cap_ah = prefs.getInt("batt_cap_ah", 99);
          prefs.end();
          //server.sendHeader("Connection", "close");
          //server.send(200, "text/plain", String(batt_cap_ah));
        } else {
          server.sendHeader("Connection", "close");
          server.send(200, "text/plain", "Failed to being() prefs for read");
        }
        */
      } else {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", "Failed to being() prefs for write");
      }
    }
    server.sendHeader("Location", "/new", true);
    server.send(302, "text/plain", "");
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    flashing_ota = true;
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
}

String getTimestamp() {
  time_t    now;
  struct tm asdf;
  char      buf[80];
  if (!getLocalTime(&asdf)) {
    //Serial.println("Failed to obtain time");
    return "";
  } else {
    time(&now);
    asdf = *localtime(&now);
  }
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &asdf);
  return String(buf);
  //printf("%s\n", buf);
  //return now;
}

void harvest_data() {
  if(!flashing_ota) {
    static uint32_t i;
    i++;

    // set word 0 of TX buffer to least-significant word of counter (bits 15..0)
    node.setTransmitBuffer(0, lowWord(i));  
    // set word 1 of TX buffer to most-significant word of counter (bits 31..16)
    node.setTransmitBuffer(1, highWord(i));

    renogy_read_data_registers();

    renogy_read_info_registers();
  }
}

void turn_off_load(String decision = "") {
  if(decision != "") {
    automatic_decision = decision;
    AppendStatus(automatic_decision);
  }
  renogy_control_load(0);
  load_running = false;
  next_available_startup = millis() + shutdown_cooldown_ms;
}

void power_on_load(String decision = "") {
  if(decision != "") {
    automatic_decision = decision;
    AppendStatus(automatic_decision);
  }
  renogy_control_load(1);
  load_running = true;
}

void update_decisions() {
  if (renogy_data.battery_voltage < minimum_shutoff_voltage) {
    turn_off_load("Turn load off (voltage) " + String(renogy_data.battery_voltage) + " < " + String(minimum_shutoff_voltage));
    //automatic_decision = "Turn load off (voltage) " + String(renogy_data.battery_voltage) + " < " + String(minimum_shutoff_voltage);
    //AppendStatus(automatic_decision);
    //renogy_control_load(0);
    //load_running = false;
    //next_available_startup = millis() + shutdown_cooldown_ms;
    sim_bat_volt_change = fabs(sim_bat_volt_change) * 1.0;
    sim_bat_soc_change = fabs(sim_bat_soc_change);
  } else {
    if (renogy_data.battery_soc < batt_soc_min) {
      turn_off_load("Turn load off (battery soc) " + String(renogy_data.battery_soc) + " < " + String(batt_soc_min));
      //automatic_decision = "Turn load off (battery soc) " + String(renogy_data.battery_soc) + " < " + String(batt_soc_min);
      //AppendStatus(automatic_decision);
      //renogy_control_load(0);
      //load_running = false;
      //next_available_startup = millis() + shutdown_cooldown_ms;
      sim_bat_volt_change = fabs(sim_bat_volt_change) * 1.0;
      sim_bat_soc_change = fabs(sim_bat_soc_change);
    } else {
      if(renogy_data.battery_voltage > minimum_starting_voltage && renogy_data.battery_soc > batt_soc_start && !load_running) {
        if(next_available_startup != 0 && millis() < next_available_startup) {
          automatic_decision = "Wait for cooldown " + String(next_available_startup-millis());
          //AppendStatus(automatic_decision);
        } else {
          power_on_load("Turn on load @ " + String(getTimestamp()));
          //automatic_decision = "Turn on load @ " + String(getTimestamp());
          //renogy_control_load(1);
          //load_running = true;
        }
        //AppendStatus(automatic_decision);

        if(simulator_mode) {
          renogy_data.load_watts = 8;
        }
        sim_bat_volt_change = fabs(sim_bat_volt_change) * -1.0;
        sim_bat_soc_change = -fabs(sim_bat_soc_change);
      }
    }
  }
  if (renogy_data.load_watts > 0) {
    load_running = true;
  }
  if (renogy_data.load_watts == 0 && renogy_data.controller_connected && load_running) {
    load_running = false;
  }

  AppendStatus(getTimestamp());
  AppendStatus("Battery Capacity AH: " + String(batt_cap_ah) + " @ " + String(renogy_data.battery_soc) + "%");
  //if (renogy_data.solar_panel_watts > renogy_data.load_watts) {
  float panel_incoming_rate_ah = (renogy_data.solar_panel_watts*1.0) / (battery_voltage*1.0);
  AppendStatus("Panel is at " + String(renogy_data.solar_panel_amps) + " amps (r) and " + String(renogy_data.solar_panel_watts) + " watts (r)");
  AppendStatus("Panel is producing " + String(panel_incoming_rate_ah) + " ah (c)");
  float load_outgoing_rate_ah = renogy_data.load_watts / (battery_voltage*1.0);
  AppendStatus("Load is consuming " + String(load_outgoing_rate_ah) + " ah (c) and " + String(renogy_data.load_watts) + " watts (r)");
  float charging_rate_ah = panel_incoming_rate_ah - load_outgoing_rate_ah;
  AppendStatus("Battery drain or fill rate is " + String(charging_rate_ah) + " ah (c)");
  if(charging_rate_ah == 0) {
    AppendStatus("Battery is holding steady");
    //AppendStatus("Panel is at " + String(renogy_data.solar_panel_amps) + " amps and " + String(renogy_data.solar_panel_watts) + " watts");
  } else if(charging_rate_ah > 0) {
    AppendStatus("Battery is charging at " + String(charging_rate_ah) + " ah");  
    float ah_left_to_charge = ((batt_cap_ah*1.0) * (100-renogy_data.battery_soc))/100;
    AppendStatus("It's at " + String(renogy_data.battery_soc) + "% so just " + String(ah_left_to_charge) + "ah left to charge");
    float time_to_full = ah_left_to_charge / charging_rate_ah;
    AppendStatus("Battery should be full in " + String(time_to_full) + " hours");
  } else {
    AppendStatus("Battery is discharging at " + String(charging_rate_ah) + " ah");
    // We're discharging... so multiply by negative one so the time_to_empty is a positive value
    //(((51/100)-(1-0.5))*10)/(-0.25*-1.0)
    //          ((.51-.5)*10)/(-0.25*-1.0)
    //               (.01*10)/0.25
    // 0.1 / 0.25 = .4 == 24 minutes
    float time_to_empty = (((renogy_data.battery_soc/100.00)-(1-max_battery_discharge))*batt_cap_ah)/(charging_rate_ah*-1.0);
    AppendStatus("At that rate it will be empty in " + String(time_to_empty) + " hours");  
    if(time_to_empty <= 0.5) {
      turn_off_load("Battery will be dead in less than 30 minutes!  Kill the load!");
      //AppendStatus("Battery will be dead in less than 30 minutes!  Kill the load!");
    }
  }

  //float charging_rate_ah = renogy.battery_charging_amps
  //AppendStatus("Battery is charging at " + String(charging_rate_ah) + " ah");
  //float ah_left_to_charge = ((batt_cap_ah*1.0) - ((batt_cap_ah*1.0) * (renogy_data.battery_soc / 100)));


}

void loop()
{
  status_str = "";
  loop_number++;
  if(loop_number <= 1) {
    Serial.println("Entering loop");
  }
  
  if(simulator_mode) {
    renogy_data.battery_voltage += (sim_bat_volt_change*1.0);
    if(renogy_data.battery_voltage > 13.7) {
      renogy_data.battery_voltage = 13.7;
    }
    //if(sim_bat_soc_change > 0) {
      Serial.println("sim_bat_soc_change: " + String(sim_bat_soc_change));
    //}
    renogy_data.battery_soc += sim_bat_soc_change;
    if(renogy_data.battery_soc > 100) {
      renogy_data.battery_soc = 100;
    }
    if(load_running) {
      renogy_data.load_watts = 8;
    }
  }

  currentMillis = millis();
  // Only read info every 10th loop
  if ((currentMillis - lastTime) > timerDelayMS || loop_number <= 1) {
    Serial.println("Updating data");
    //if (loop_number % 10 == 0 && !flashing_ota) {
    harvest_data();
    update_decisions();

    //Serial.println("Battery voltage: " + String(renogy_data.battery_voltage));
    //Serial.println("Battery charge level: " + String(renogy_data.battery_soc) + "%");
    //Serial.println("Panel wattage: " + String(renogy_data.solar_panel_watts));

    //Serial.println("---");

    //Send an HTTP POST request every 10 minutes
    if ((currentMillis - lastTime) > timerDelayMS) {
      //Check WiFi connection status
      if(WiFi.status()== WL_CONNECTED){
        //WiFiClient client;
        //HTTPClient http;
      
        if(renogy_data.controller_connected) {
          // Your Domain name with URL path or IP address with path
          http.begin(client, serverName);
          
          // If you need Node-RED/server authentication, insert user and password below
          //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
          
          // Specify content-type header
          http.addHeader("Content-Type", "application/json");

          // Data to send with HTTP POST
          String httpRequestData = structToString(renogy_data);
          Serial.println("POSTing http data:");
          Serial.println(httpRequestData);
          int httpResponseCode = http.POST(httpRequestData);

          Serial.print("HTTP Response code: ");
          AppendStatus("Latest Posting Reponse Code: " + String(httpResponseCode));

          // Free resources
          http.end();
        }
      }
      else {
        Serial.println("WiFi Disconnected");
      }
      lastTime = millis();
    }
    lastTime = millis();
  }

  if ((currentMillis - lastWebTime) > 1000) {
    handle_webserver_connection();
    delay(1);
    lastWebTime = millis();
  }

  //loop to blink without delay
  currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    // if the LED is off turn it on and vice-versa:
    //ledState = not(ledState);

    // set the LED with the ledState of the variable:
    //digitalWrite(led, ledState);
  }
}



void renogy_read_data_registers() 
{
  uint8_t j, result;
  uint16_t data_registers[num_data_registers];
  char buffer1[40], buffer2[40];
  uint8_t raw_data;

  // prints data about each read to the console
  bool print_data=0; 
  
  result = node.readHoldingRegisters(0x100, num_data_registers);
  if (result == node.ku8MBSuccess)
  {
    if (print_data) {
      AppendStatus("Successfully read the data registers!");
    }
    renogy_data.controller_connected = true;
    for (j = 0; j < num_data_registers; j++)
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
      if(loop_number <= 1) {
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
    //Serial.println("raw ratings = " + String(raw_data));
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
    */

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


// control the load pins on Renogy charge controllers that have them
void renogy_control_load(bool state) {
  if (state==1) node.writeSingleRegister(0x010A, 1);  // turn on load
  else node.writeSingleRegister(0x010A, 0);  // turn off load
}