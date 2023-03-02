#include <EEPROM.h>

unsigned long SECONDS_PER_DAY = 86400UL;

unsigned long MS_PER_HOUR = 3600000UL;
unsigned long MS_PER_MINUTE = 60000UL;
unsigned long MS_PER_SECOND = 1000UL;

// Run four times per day
int numRuntimesPerDay = 18;

//const unsigned long DEFAULT_TTL = (unsigned long)(SECONDS_PER_DAY / numRuntimesPerDay) * MS_PER_SECOND;
const unsigned long DEFAULT_TTL = 120UL * MS_PER_SECOND; // two minutes
//const unsigned long INITIAL_TTL = 0;
unsigned long ttlMilliSec = DEFAULT_TTL;
bool justBooted = true;

// Specify in seconds
//unsigned long ttlMilliSec = 60 * MS_PER_SECOND;

// Specify in minutes
//unsigned long ttlMilliSec = 1 * MS_PER_MINUTE;

// Specify in hours
//unsigned long ttlMilliSec = 1 * MS_PER_HOUR;

int OUTPUT_LED = 13;

// Active Time
//const unsigned long INITIAL_RUNTIME_MS = 600000;  // 10 minutes
//const unsigned long DEFAULT_RUNTIME_MS = 600000;  // 10 minutes

const unsigned long INITIAL_RUNTIME_MS = 10000;  // 10 seconds
const unsigned long DEFAULT_RUNTIME_MS = 15000;  // 15 seconds

volatile unsigned long activeRuntimeMillis = INITIAL_RUNTIME_MS;
volatile unsigned long currRun = 0;

unsigned long lastStartTimeMS = 0;

#include <util/atomic.h>

// for testing only. 4294967290 is 5ms before rollover
void setMillis(unsigned long ms)
{
    extern unsigned long timer0_millis;
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE) {
        timer0_millis = ms;
    }
}

void setup() {
  //lcd.begin(16,2);
  pinMode(OUTPUT_LED, OUTPUT);
  digitalWrite(OUTPUT_LED, LOW);

  Serial.begin(115200);

  Serial.println("Current Time: " + String(millis()));
  Serial.println("Initial TTL: " + String(ttlMilliSec / MS_PER_SECOND) + "sec");
  Serial.println("Initial Runtime: " + String(activeRuntimeMillis / MS_PER_SECOND) + "sec");

  Serial.println("Sleep 2.5 seconds");
  while ((millis() - 0) < 2500UL) {
    
  }
  Serial.println("Done with setup");

}

void loop() {
  // Every five seconds
  unsigned long currentMillis = millis();

  if ((unsigned long)(currentMillis - lastStartTimeMS) >= ttlMilliSec || justBooted || currRun > 0) {
    justBooted = false;
    //ttlMilliSec = DEFAULT_TTL;

    Serial.println("cm " + String(currentMillis));
    Serial.println("Last Start " + String(lastStartTimeMS));

    // We go HIGH.. which means we're running...
    digitalWrite(OUTPUT_LED, HIGH);

    if (currRun < activeRuntimeMillis) {
      //Serial.println("Running");
      // stay running.
      //delay(10000);
      currRun += 1000;
      Serial.println("Stay running " + String((activeRuntimeMillis - currRun) / MS_PER_SECOND) + " more seconds");
    } else {
      activeRuntimeMillis = DEFAULT_RUNTIME_MS;
      currRun = 0;
      lastStartTimeMS = currentMillis;

      Serial.println("Done with Runtime");
      delay(10);
      digitalWrite(OUTPUT_LED, LOW);

      Serial.println("Stop");
      delay(10);

      Serial.println("Current TTL: " + String(ttlMilliSec / MS_PER_SECOND) + "sec");
      Serial.println("Next Runtime: " + String(activeRuntimeMillis / MS_PER_SECOND) + "sec");
    }

    //Serial.println("Resetting");
  }
  delay(1000);
  //Serial.println("Stopped");
}