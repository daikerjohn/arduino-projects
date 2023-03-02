#include "DHT.h"

#define DHT_PIN 2     
#define DHT_TYPE DHT22   // DHT 22  (AM2302), AM2321

#define RELAY_PIN 9

DHT dht(DHT_PIN, DHT_TYPE);

long sleep_timeout = 5000;
long next_sleep = 0;
bool first_boot = true;

// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;  // will store last time LED was updated

unsigned long currentMillis = 0;
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;

// Minimum runtime of 5 minutes.
unsigned long minimum_run_time_ms = 300000;

// Set timer to 5 seconds (5000)
const unsigned long timerDelayMS = 5000;
// Set timer to 60 seconds (60000)
//const unsigned long timerDelayMS = 60000;
// Timer set to 10 minutes (600000)
//const unsigned long timerDelayMS = 600000;

bool simulating = true;

bool load_running = false;
// 5 minutes
long shut_cooldown_ms = 600000;

long next_startup_time_ms = 0;
long next_shutdown_time_ms = 0;
// Prevent any automatic power-on for 120 seconds
//long next_available_startup = millis() + 120000;

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  Serial.begin(115200);
  Serial.println(F("DHT22 example!"));

  dht.begin();
}

float readTemp() {
  if(simulating) {
    // Celcius, remember!
    return random(0, 40);
  }
  return dht.readTemperature();
}

float readHum() {
  if(simulating) {
    return random(30, 50);
  }
  return dht.readHumidity();
}

void loop() {
  currentMillis = millis();
   // Only read info every 10th loop
  if ((currentMillis - lastTime) > timerDelayMS || first_boot) {
    previousMillis = currentMillis;
    Serial.println("Updating data");

    float temperature = readTemp();
    float humidity = readHum();

    // Check if any reads failed and exit early (to try again).
    if (isnan(temperature) || isnan(humidity)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }
    double tempF = (temperature * (9.0/5.0)) + 32.0;

    Serial.print(F("Humidity: "));
    Serial.print(humidity);
    Serial.print(F("%  Temperature: "));
    Serial.print(temperature);
    Serial.print(F("C  TempF: "));
    Serial.print(tempF);
    Serial.println(F("F "));

    if(load_running) {
      if(tempF < 45) {
        Serial.println("No-op. Running and should be.");
      } else {
        if(currentMillis > next_shutdown_time_ms) {
          Serial.println("We've warmed up. Let's save some power!");
          digitalWrite(RELAY_PIN, LOW);
          load_running = false;
          next_startup_time_ms = currentMillis + shut_cooldown_ms;
        } else {
          long how_long = next_shutdown_time_ms - currentMillis;
          Serial.println("Would shutdown but minimum runtime not elapsed. " + String(how_long) + "ms left");
        }
      }
    } else {
      if(tempF < 45) {
        if(currentMillis > next_startup_time_ms) {
          Serial.println("It's cold. Give me some heat!");
          digitalWrite(RELAY_PIN, HIGH);
          load_running = true;
          next_shutdown_time_ms = currentMillis + minimum_run_time_ms;
        } else {
          long how_long = next_startup_time_ms - currentMillis;
          Serial.println("Need to wait for startup " + String(how_long) + "ms left");
        }
      } else {
        Serial.println("No-op. It's not running, nor does it need to be");
        //digitalWrite(RELAY_PIN, LOW);
        //load_running = false;
      }
    }

    lastTime = millis();
  }

  first_boot = false;
}
