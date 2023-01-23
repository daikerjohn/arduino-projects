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

// https://github.com/syvic/ModbusMaster
#include <ModbusMaster.h>
ModbusMaster node;

// Set web server port number to 80
WiFiServer server(80);
// Variable to store the HTTP request
String header;
String status_str = "";

WiFiClientSecure client;  // or WiFiClientSecure for HTTPS
HTTPClient http;

const char* ssid = "dirker";
const char* password = "alphabit";

//Your Domain name with URL path or IP address with path
const char* serverName = "https://lhbqdvca46.execute-api.us-west-2.amazonaws.com/dev/solar";

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelayMS = 600000;
// Set timer to 5 seconds (5000)
//unsigned long timerDelayMS = 5000;
// Set timer to 60 seconds (60000)
unsigned long timerDelayMS = 60000;


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
int battery_capacity_ah = 20;
float max_battery_discharge = .5;

float minimum_shutoff_voltage = 11.5;
float minimum_starting_voltage = 12.5;

float sim_starting_battery_voltage = 13.12;
float sim_bat_volt_change = -0.11;

float minimum_shutoff_soc = 45;
float minimum_starting_soc = 55;

uint8_t sim_starting_battery_soc = 51.5;
float sim_bat_soc_change = -1;

bool load_running = false;

float sim_starting_solar_panel_watts = 20;
int loop_number = 0;


void setup()
{
  Serial.begin(115200);
  Serial.println("Started!");

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

    client.setInsecure();

  // create a second serial interface for modbus
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); 

  // my Renogy Wanderer has an (slave) address of 255! Not in docs??? 
  // Do all Renogy charge controllers use this address?
  int modbus_address = 255; 
  node.begin(modbus_address, Serial2);

  // Start webserver
  server.begin();
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
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

String automatic_decision = "";

void handle_webserver_connection() {
  WiFiClient one_client = server.available();   // Listen for incoming clients

  if (one_client) {                             // If a new client connects,
    harvest_data();
    update_decisions();

    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (one_client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (one_client.available()) {             // if there's bytes to read from the one_cclientlient,
        char c = one_client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the one_client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the one_client knows what's coming, then a blank line:
            one_client.println("HTTP/1.1 200 OK");
            one_client.println("Content-type:text/html");
            one_client.println("Connection: close");
            one_client.println();
            
            // turns the GPIOs on and off
            if (header.indexOf("GET /load/on") >= 0) {
              Serial.println("Turn load on");
              renogy_control_load(1);
              load_running = true;
              //digitalWrite(output26, HIGH);
            } else if (header.indexOf("GET /load/off") >= 0) {
              Serial.println("Turn load off");
              renogy_control_load(0);
              load_running = false;
              //digitalWrite(output26, LOW);
            }
            
            // Display the HTML web page
            one_client.println("<!DOCTYPE html><html>");
            one_client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            one_client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            one_client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            one_client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            one_client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            one_client.println(".button2 {background-color: #555555;}</style></head>");
            
            // Web Page Heading
            one_client.println("<body><h1>ESP32 Web Server</h1>");
            
            // Display current state of the Renogy load
            if(load_running) {
              one_client.println("<p>Renogy Load - Running @ " + String(renogy_data.load_watts) + " watts</p>");
            } else {
              one_client.println("<p>Renogy Load - Stopped - " + String(renogy_data.load_watts) + "</p>");
            }
            one_client.println("<p>Last Automatic: " + String(automatic_decision) + "</p>");
            one_client.println("<p>" + String(status_str) + "</p>");
            // If the load_running is on, it displays the OFF button
            if (load_running) {
              one_client.println("<p><a href=\"/load/off\"><button class=\"button button2\">OFF</button></a></p>");
            } else {
              one_client.println("<p><a href=\"/load/on\"><button class=\"button\">ON</button></a></p>");
            } 

            one_client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            one_client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    one_client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

void harvest_data() {
  static uint32_t i;
  i++;

  // set word 0 of TX buffer to least-significant word of counter (bits 15..0)
  node.setTransmitBuffer(0, lowWord(i));  
  // set word 1 of TX buffer to most-significant word of counter (bits 31..16)
  node.setTransmitBuffer(1, highWord(i));
}

void update_decisions() {
  if (renogy_data.battery_voltage < minimum_shutoff_voltage) {
    AppendStatus("Turn load off (voltage)");
    automatic_decision = "Turn off load (voltage)";
    renogy_control_load(0);
    load_running = false;
    sim_bat_volt_change = fabs(sim_bat_volt_change) * 1.0;
    sim_bat_soc_change = fabs(sim_bat_soc_change);
  } else {
    if (renogy_data.battery_soc < minimum_shutoff_soc) {
      AppendStatus("Turn load off (battery)");
      automatic_decision = "Turn off load (battery)";
      //status_str = "Turn load off (battery)";
      renogy_control_load(0);
      load_running = false;
      sim_bat_volt_change = fabs(sim_bat_volt_change) * 1.0;
      sim_bat_soc_change = fabs(sim_bat_soc_change);
    } else {
      //Serial.println("Load on");
      // Only if it's daytime!?
      automatic_decision = "Turn on load";
    }
  }
  if (renogy_data.load_watts > 0) {
    load_running = true;
  }
  if (renogy_data.load_watts == 0 && renogy_data.controller_connected && load_running) {
    load_running = false;
  }

  //if (renogy_data.solar_panel_watts > renogy_data.load_watts) {
  float panel_incoming_rate_ah = (renogy_data.solar_panel_watts*1.0) / (battery_voltage*1.0);
  float load_outgoing_rate_ah = renogy_data.load_watts / (battery_voltage*1.0);
  AppendStatus("Load is consuming " + String(load_outgoing_rate_ah) + " ah");
  float charging_rate_ah = panel_incoming_rate_ah - load_outgoing_rate_ah;
  if(charging_rate_ah == 0) {
    AppendStatus("Battery is idle at " + String(renogy_data.battery_soc) + "%");
    AppendStatus("Panel is at " + String(renogy_data.solar_panel_amps) + " amps and " + String(renogy_data.solar_panel_watts) + " watts");
  } else if(charging_rate_ah > 0) {
    AppendStatus("Battery is charging at " + String(charging_rate_ah) + " ah");  
    float ah_left_to_charge = ((battery_capacity_ah*1.0) * (100-renogy_data.battery_soc))/100;
    AppendStatus("Battery is at " + String(renogy_data.battery_soc) + "%. So " + String(ah_left_to_charge) + "ah left to charge");
    float time_to_full = ah_left_to_charge / charging_rate_ah;
    AppendStatus("Battery should be full in " + String(time_to_full) + " hours");
  } else {
    AppendStatus("Battery is discharging at " + String(charging_rate_ah) + " ah");  
    float time_to_empty = ((battery_capacity_ah*1.0*max_battery_discharge)/(charging_rate_ah*1.0));
    AppendStatus("Battery will be empty in " + String(time_to_empty) + " hours");  
  }

  //float charging_rate_ah = renogy.battery_charging_amps
  //AppendStatus("Battery is charging at " + String(charging_rate_ah) + " ah");
  //float ah_left_to_charge = ((battery_capacity_ah*1.0) - ((battery_capacity_ah*1.0) * (renogy_data.battery_soc / 100)));

  if(renogy_data.battery_voltage > minimum_starting_voltage && renogy_data.battery_soc > minimum_starting_soc && !load_running) {
    AppendStatus("Load would automatically turn on here");
    if(simulator_mode) {
      renogy_data.load_watts = 8;
    }
    sim_bat_volt_change = fabs(sim_bat_volt_change) * -1.0;
    sim_bat_soc_change = -fabs(sim_bat_soc_change);
  }
}

void loop()
{
  status_str = "";
  loop_number++;

  //harvest_data();

  //update_decisions();
  
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

  // Only read info every 10th loop
  if (loop_number % 10 == 0) {
    harvest_data();

    Serial.println("Battery voltage: " + String(renogy_data.battery_voltage));
    Serial.println("Battery charge level: " + String(renogy_data.battery_soc) + "%");
    Serial.println("Panel wattage: " + String(renogy_data.solar_panel_watts));

    update_decisions();

    Serial.println("---");

    //Send an HTTP POST request every 10 minutes
    if ((millis() - lastTime) > timerDelayMS) {
      //Check WiFi connection status
      if(WiFi.status()== WL_CONNECTED){
        //WiFiClient client;
        //HTTPClient http;
      
        // Your Domain name with URL path or IP address with path
        http.begin(client, serverName);
        
        // If you need Node-RED/server authentication, insert user and password below
        //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
        
        // Specify content-type header
        http.addHeader("Content-Type", "application/json");
        String theStr = structToString(renogy_data);
        Serial.println(theStr);
        // Data to send with HTTP POST
        String httpRequestData = theStr;
        int httpResponseCode = 0;
        // Send HTTP POST request
        if(renogy_data.controller_connected) {
          Serial.print("POSTing http data");
          httpResponseCode = http.POST(httpRequestData);
        }
        
        // If you need an HTTP request with a content type: application/json, use the following:
        //http.addHeader("Content-Type", "application/json");
        //int httpResponseCode = http.POST("{\"api_key\":\"tPmAT5Ab3j7F9\",\"sensor\":\"BME280\",\"value1\":\"24.25\",\"value2\":\"49.54\",\"value3\":\"1005.14\"}");

        // If you need an HTTP request with a content type: text/plain
        //http.addHeader("Content-Type", "text/plain");
        //int httpResponseCode = http.POST("Hello, World!");
      
        Serial.print("HTTP Response code: ");
        AppendStatus("Latest Posting Reponse Code: " + String(httpResponseCode));
          
        // Free resources
        http.end();
      }
      else {
        Serial.println("WiFi Disconnected");
      }
      lastTime = millis();
    }
  }

  handle_webserver_connection();

  delay(6000);
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