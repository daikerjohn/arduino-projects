//#include <EEPROM.h>

unsigned long SECONDS_PER_DAY = 86400UL;

unsigned long MS_PER_HOUR = 3600000UL;
unsigned long MS_PER_MINUTE = 60000UL;
unsigned long MS_PER_SECOND = 1000UL;

// Run four times per day
int numRuntimesPerDay = 18;

const unsigned long DEFAULT_TTL = (unsigned long)(SECONDS_PER_DAY / numRuntimesPerDay) * MS_PER_SECOND;
const unsigned long INITIAL_TTL = 0;
unsigned long ttlMilliSec = INITIAL_TTL;

// Specify in seconds
//unsigned long ttlMilliSec = 60 * MS_PER_SECOND;

// Specify in minutes
//unsigned long ttlMilliSec = 1 * MS_PER_MINUTE;

// Specify in hours
//unsigned long ttlMilliSec = 1 * MS_PER_HOUR;

int OUTPUT_LED = 13;

// Active Time
const unsigned long INITIAL_RUNTIME_MS = 600000;  // 10 minutes
//const unsigned long INITIAL_RUNTIME_MS = 10000;  // 10 seconds
const unsigned long DEFAULT_RUNTIME_MS = 600000;  // 10 minutes

volatile unsigned long activeRuntimeMillis = INITIAL_RUNTIME_MS;

unsigned long previousMillis = 0;

void setup() {
  //lcd.begin(16,2);
  pinMode(OUTPUT_LED, OUTPUT);
  digitalWrite(OUTPUT_LED, LOW);

  Serial.begin(115200);

  Serial.println("Initial TTL: " + String(ttlMilliSec / MS_PER_SECOND) + "sec");
  Serial.println("Initial Runtime: " + String(activeRuntimeMillis / MS_PER_SECOND) + "sec");

  while ((millis() - previousMillis) < 2500UL) {
    // sleep for 2.5 seconds before we begin
  }
  Serial.println("Done with setup");
}

void loop() {
  // Every five seconds
  unsigned long currentMillis = millis();

  if ((unsigned long)(currentMillis - previousMillis) >= ttlMilliSec) {
    ttlMilliSec = DEFAULT_TTL;

    Serial.println("Current TTL: " + String(ttlMilliSec / MS_PER_SECOND) + "sec");
    Serial.println("Current Runtime: " + String(activeRuntimeMillis / MS_PER_SECOND) + "sec");
    Serial.println("cm " + String(currentMillis));

    Serial.println("pm " + String(previousMillis));

    Serial.println("ttlMilliSec " + String(ttlMilliSec));

    // We go HIGH.. which means we're running...
    digitalWrite(OUTPUT_LED, HIGH);

    unsigned long currRun = 0;
    Serial.println("Running");
    while (currRun <= activeRuntimeMillis) {
      // stay running.
      delay(10000);
      currRun += 10000;
      Serial.println("Stay running " + String((activeRuntimeMillis - currRun) / MS_PER_SECOND) + "sec");
    }
    activeRuntimeMillis = DEFAULT_RUNTIME_MS;
    currRun = 0;
    previousMillis = currentMillis;

    Serial.println("Done with Runtime");
    delay(10);
    digitalWrite(OUTPUT_LED, LOW);

    Serial.println("Stop");
    delay(10);

    //Serial.println("Resetting");
    delay(1000);
  }
  //Serial.println("Stopped");
}
