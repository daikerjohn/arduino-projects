/*
This example takes range measurements with the VL53L1X and displays additional
details (status and signal/ambient rates) for each measurement, which can help
you determine whether the sensor is operating normally and the reported range is
valid. The range is in units of mm, and the rates are in units of MCPS (mega
counts per second).
*/

#include <WiFi.h>
//#include <AsyncTCP.h>
#include "libs/AsyncElegantOTA/AsyncElegantOTA.h"
#include <Wire.h>
#include <VL53L1X.h>
#include <RunningAverage.h>


// Replace with your network credentials
const char* ssid = "dirker";
const char* password = "alphabit";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create VL53L1X sensor object
VL53L1X sensor;

// Running average instance
const int RA_SIZE = 10;  // adjust based on how smooth you want it
RunningAverage ra(RA_SIZE);

int timerDelayMS = 5000;
int currentMillis = 0;
int lastTime = 0;

void setup()
{
  //while (!Serial) {}
  Serial.begin(115200);
  Wire.begin(21, 22, 400000);
  //Wire.setClock(400000); // use 400 kHz I2C

  delay(100);
  //I2Cscan();

  sensor.setTimeout(500);
  if (!sensor.init())
  {
    Serial.println("Failed to detect and initialize sensor!");
    //while (1);
  }
  // lower the return signal rate limit (default is 0.25 MCPS)
  //sensor.setSignalRateLimit(0.1);

  // Use long distance mode and allow up to 50000 us (50 ms) for a measurement.
  // You can change these settings to adjust the performance of the sensor, but
  // the minimum timing budget is 20 ms for short distance mode and 33 ms for
  // medium and long distance modes. See the VL53L1X datasheet for more
  // information on range and timing limits.
  sensor.setDistanceMode(VL53L1X::Long);
  // increase timing budget to 200 ms
  //sensor.setMeasurementTimingBudget(200000);
  sensor.setMeasurementTimingBudget(50000);

  // Start continuous readings at a rate of one measurement every 50 ms (the
  // inter-measurement period). This period should be at least as long as the
  // timing budget.
  //sensor.startContinuous(200);
  sensor.startContinuous(50);

  ra.clear();


  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // API endpoint for averaged distance
  server.on("/distance", HTTP_GET, [](AsyncWebServerRequest *request){
    uint16_t raw = sensor.read();
    if (sensor.timeoutOccurred()) {
      request->send(500, "application/json", "{\"error\": \"Timeout\"}");
      return;
    }

    ra.addValue(raw);
    float avg = ra.getAverage();
    String json = "{\"raw\":" + String(raw) + ",\"average\":" + String(avg, 1) + "}";
    request->send(200, "application/json", json);
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "VL53L1X Web Server with Averaging");
  });

  // Setup OTA
  AsyncElegantOTA.begin(&server);  // OTA at /update

  server.begin();
}

void loop() {
  // Optional: pre-fill average buffer for faster convergence
  static bool prefill = true;
  if (prefill) {
    for (int i = 0; i < RA_SIZE; i++) {
      ra.addValue(sensor.read());
      delay(50);  // match sensor interval
    }
    prefill = false;
  }

  currentMillis = millis();
  if ((currentMillis - lastTime) > timerDelayMS) {
    ra.addValue(sensor.read());
    lastTime = millis();
  }
}


// I2C scan function
void I2Cscan()
{
// scan for i2c devices
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for(address = 1; address < 127; address++ ) 
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address<16) 
        Serial.print("0");
      Serial.print(address,HEX);
      Serial.println("  !");

      nDevices++;
    }
    else if (error==4) 
    {
      Serial.print("Unknown error at address 0x");
      if (address<16) 
        Serial.print("0");
      Serial.println(address,HEX);
    }    
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
    
}