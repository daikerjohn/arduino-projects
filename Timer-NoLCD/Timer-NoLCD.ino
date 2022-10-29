#include <EEPROM.h>

//#include <LiquidCrystal.h>
//LiquidCrystal lcd(3,4,5,6,7,8);

char lineOne[16] = {};
char lineTwo[16] = {};
char formatTtlBuffer[16] = {};

int RUNTIME_ADDRESS = 100;

int TTL_ADDRESS = 200;

unsigned long SECONDS_PER_DAY = 86400UL;

unsigned long MS_PER_HOUR = 3600000UL;
unsigned long MS_PER_MINUTE = 60000UL;
unsigned long MS_PER_SECOND = 1000UL;

// Run four times per day
int numRuntimesPerDay = 4;

unsigned long ttlMilliSec = (unsigned long)(SECONDS_PER_DAY / numRuntimesPerDay) * MS_PER_SECOND;
// Specify in seconds
//unsigned long ttlMilliSec = 60 * MS_PER_SECOND;

// Specify in minutes
//unsigned long ttlMilliSec = 1 * MS_PER_MINUTE;

// Specify in hours
//unsigned long ttlMilliSec = 1 * MS_PER_HOUR;

int OUTPUT_LED = 13;

int PROGRAMMING_MODE_PIN = 2;

// Active Time
const int INITIAL_RUNTIME_SEC = 720;  // 12 minutes
const int MAX_RUNTIME_SEC     = 1200; // 20 minutes
const int RUNTIME_INCREMENT   = 15;   // 15 seconds
volatile int activeRuntimeSec = INITIAL_RUNTIME_SEC;
volatile unsigned long activeRuntimeMillis = activeRuntimeSec * MS_PER_SECOND;

// Change this to 60 (one minute) for actual timing.
unsigned long REFRESH_INTERVAL = 60 * MS_PER_SECOND;

unsigned long previousMillis = 0;
unsigned long previousRefreshMillis = 0;

void setup() {
  //lcd.begin(16,2);
  pinMode(OUTPUT_LED, OUTPUT);
  digitalWrite(OUTPUT_LED, LOW);

  //pinMode(PROGRAMMING_MODE_PIN, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(PROGRAMMING_MODE_PIN), program_me, RISING);

  Serial.begin(115200);
  int eeprom_runtime = getActiveRuntime();
  if (eeprom_runtime > MAX_RUNTIME_SEC) {
    Serial.println("Greater than MAX");
    activeRuntimeSec = INITIAL_RUNTIME_SEC;
  } else if (eeprom_runtime <= 0) {
    Serial.println("Less than zero");
    activeRuntimeSec = INITIAL_RUNTIME_SEC;
  } else {
    // This will *always* wipeout what we have in EEPROM.  That's probably okay, though?
    activeRuntimeSec = INITIAL_RUNTIME_SEC;
    //activeRuntimeSec = eeprom_runtime;
  }
  setActiveRuntime(activeRuntimeSec);

  Serial.println("TTL: " + String(ttlMilliSec / MS_PER_SECOND) + "sec");
  Serial.println("Runtime: " + String(activeRuntimeSec) + "sec");
  activeRuntimeMillis = activeRuntimeSec * MS_PER_SECOND;
  previousMillis = millis();
  //delay(100);

  while ((millis() - previousMillis) < 5000UL) {
    // sleep for 5 seconds before we begin
  }
  Serial.println("Done with setup");
}

//ram monitor function I found on this forum
int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  int rtn = (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
  return rtn;
}

void formatTtl(unsigned long timeLeft) {
  //char str_temp[6];
  //double timeLeftFloat = 0;
  float divisor = (float)MS_PER_SECOND;
  char units[1] = {};

  if (timeLeft > MS_PER_HOUR) {
    divisor = (float)MS_PER_HOUR;
    units[0] = 'h';
  } else if (timeLeft > MS_PER_MINUTE) {
    divisor = (float)MS_PER_MINUTE;
    units[0] = 'm';
  } else {
    divisor = (float)MS_PER_SECOND;
    units[0] = 's';
  }
  float timeLeftFloat = (float)((float)timeLeft / (float)divisor);
  memset(formatTtlBuffer, 0, strlen(formatTtlBuffer));
  int remainder = (int)(timeLeftFloat * 60) % 60;
  if (units[0] == 's' && timeLeftFloat < 60.0F) {
    sprintf(formatTtlBuffer, "TTL: %d%c", (int)timeLeftFloat, units[0]);
  } else {
    sprintf(formatTtlBuffer, "TTL: %d:%02d%c", (int)timeLeftFloat, (int)remainder, units[0]);
  }
  Serial.println(formatTtlBuffer);
  //sprintf(formatTtlBuffer,"TTL: %d.%02d%c", (int)timeLeftFloat, (int)(timeLeftFloat*100)%100, units[0]);
}

void updateDisplayChar(char firstLine[]) {
  Serial.println(firstLine);
  delay(10);
}

void updateDisplayChar(char firstLine[], char secondLine[]) {
  Serial.println(firstLine);
  delay(10);

  Serial.println(secondLine);
  delay(10);

  Serial.println(freeRam());
}

void program_me() {
  Serial.println("program_me");
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200UL) {
    activeRuntimeSec += RUNTIME_INCREMENT;
    if (activeRuntimeSec > MAX_RUNTIME_SEC) {
      activeRuntimeSec = INITIAL_RUNTIME_SEC;
    }
    char theLine[16] = {};
    sprintf(theLine, "New Runtime %d", activeRuntimeSec);
    updateDisplayChar(theLine);
    setActiveRuntime(activeRuntimeSec);
  }
  last_interrupt_time = interrupt_time;
}

void EEPROMWriteInt(int address, int value)
{
  byte two = (value & 0xFF);
  byte one = ((value >> 8) & 0xFF);

  EEPROM.update(address, two);
  EEPROM.update(address + 1, one);
}

int EEPROMReadInt(int address)
{
  long two = EEPROM.read(address);
  long one = EEPROM.read(address + 1);

  return ((two << 0) & 0xFFFFFF) + ((one << 8) & 0xFFFFFFFF);
}

void setActiveRuntime(int runtime) {
  EEPROMWriteInt(RUNTIME_ADDRESS, runtime);
  activeRuntimeMillis = (unsigned long)(runtime * MS_PER_SECOND);
  Serial.println("Active Runtime " + activeRuntimeMillis);
  //Serial.println(activeRuntimeMillis);
}

int getActiveRuntime() {
  return EEPROMReadInt(RUNTIME_ADDRESS);
}

void(* resetFunc) (void) = 0;

unsigned long lastTick = 0;

void updateSerialCon(const char additionalText[]) {
  // While we're still supposed to be running...
  //while ((unsigned long)(millis() - previousMillis) <= activeRuntimeMillis) {
  //
  if ((unsigned long)(millis() - lastTick) >= REFRESH_INTERVAL) {
    long elapsed = millis() - lastTick;
    sprintf(lineOne, "Refresh tick %lu %s", elapsed, additionalText);
    Serial.println(lineOne);
    lastTick = millis();
  }
  //}
}

void loop() {
  // Every five seconds
  if ((unsigned long)(millis() - previousMillis) >= ttlMilliSec) {
    sprintf(lineTwo, "m() %lu", millis());
    updateSerialCon(lineTwo);

    sprintf(lineTwo, "pm %lu", previousMillis);
    updateSerialCon(lineTwo);

    sprintf(lineTwo, "math %lu", (millis() - previousMillis));
    updateSerialCon(lineTwo);

    sprintf(lineTwo, "ttlMilliSec %lu", ttlMilliSec);
    Serial.println(lineTwo);
    previousMillis = millis();
    //Serial.println("ttlMilliSec expired: " + String(previousMillis));

    // We go HIGH.. which means we're running...
    digitalWrite(OUTPUT_LED, HIGH);
    unsigned long startOfRuntime = millis();
    Serial.println("Running");
    while (millis() - startOfRuntime <= activeRuntimeMillis) {
      // stay running.
      updateSerialCon("Running");
    }
    startOfRuntime = 0;

    Serial.println("Done with Runtime");
    delay(10);
    digitalWrite(OUTPUT_LED, LOW);

    updateSerialCon("Stop");
    previousMillis = millis();
    delay(10);
    //Serial.println("New millis: " + String(previousMillis));

    //Serial.println("Resetting");
    //delay(1000);
    //resetFunc();
  }
  updateSerialCon("Stopped");
}
