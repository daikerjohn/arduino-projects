#include <queue>
#include <deque>
#include <string>

#include <WiFi.h>
#include <WiFiUdp.h>

#include <WakeOnLan.h>

#include <ezButton.h>

#include <LiquidCrystal_I2C.h>

// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;

// set LCD address, number of columns and rows
// if you don't know your display address, run an I2C scanner sketch
LiquidCrystal_I2C lcd(0x3F, lcdColumns, lcdRows);


#define SHORT_PRESS_TIME 750

//ezButton button(13); // create ezButton object that attach to pin GIOP34
ezButton button(0); // create ezButton object that attach to BOOT button

unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;

WiFiUDP UDP;
WakeOnLan WOL(UDP);

const char* ssid     = "dirker";
const char* password = "alphabit";

int start = 0;
std::array<std::string, 3> hostnames = {
  "ryzen-d8:bb:c1:12:4d:d0",
  "msi-30:9c:23:83:77:97",
  "torrenter-8c:89:a5:c3:20:a8"};

long backlight_timeout = 5000;
long backlight_shutoff = millis();

long sleep_timeout = 5000;
long next_sleep = 0;
bool first_boot = true;

void Scanner ()
{
  Serial.println ();
  Serial.println ("I2C scanner. Scanning ...");
  byte count = 0;

  Wire.begin();
  for (byte i = 8; i < 120; i++)
  {
    Wire.beginTransmission (i);          // Begin I2C transmission Address (i)
    if (Wire.endTransmission () == 0)  // Receive 0 = success (ACK response) 
    {
      Serial.print ("Found address: ");
      Serial.print (i, DEC);
      Serial.print (" (0x");
      Serial.print (i, HEX);     // PCF8574 7 bit address
      Serial.println (")");
      count++;
    }
  }
  Serial.print ("Found ");      
  Serial.print (count, DEC);        // numbers of devices
  Serial.println (" device(s).");
}

std::string splitValue(int start, bool returnMac) {
  std::string next = hostnames[start];
  std::string delimiter = "-";
  int asdf = next.find(delimiter);
  //Serial.printf("%d\n", asdf);
  auto hst = next.substr(0, asdf);
  auto mac = next.substr(asdf+1, next.length()-next.find(delimiter)-1);
  //Serial.printf("%s\n", hst.c_str());
  //Serial.printf("%s\n", mac.c_str());
  if(returnMac) {
    return mac;
  }
  return hst;
}

void updateDisplay() {
    auto hst = splitValue(start, false);
    auto mac = splitValue(start, true);
    Serial.printf("%s - %s\n", hst.c_str(), mac.c_str());
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(String(hst.c_str()));
    lcd.setCursor(0, 1);
    lcd.print(String(mac.c_str()));
}

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void resetSleepTimer() {
  next_sleep = millis() + sleep_timeout;
}

void setup()
{
  Serial.begin(115200);

  Wire.begin (21, 22);   // sda= GPIO_21 /scl= GPIO_22

  //Scanner ();


  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  /*
  First we configure the wake up source
  We set our ESP32 to wake up for an external trigger.
  There are two types for ESP32, ext0 and ext1 .
  ext0 uses RTC_IO to wakeup thus requires RTC peripherals
  to be on while ext1 uses RTC Controller so doesnt need
  peripherals to be powered on.
  Note that using internal pullups/pulldowns also requires
  RTC peripherals to be turned on.
  */
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0); //1 = High, 0 = Low

  // initialize LCD
  lcd.init();
  // turn on LCD backlight                      
  lcd.backlight();

  // set cursor to first column, first row
  lcd.setCursor(0, 0);

  WOL.setRepeat(3, 100); // Optional, repeat the packet three times with 100ms between. WARNING delay() is used between send packet function.

  lcd.println("Connecting");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      lcd.print(".");
      Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.println(WiFi.localIP());
  Serial.println(WiFi.localIP());
  delay(5000);

  //WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask()); // Optional  => To calculate the broadcast address, otherwise 255.255.255.255 is used (which is denied in some networks).
  WOL.setBroadcastAddress(IPAddress(192, 168, 10, 255));
  
  button.setDebounceTime(50); // set debounce time to 50 milliseconds
  updateDisplay();
  //wakeMyPC();
  //wakeOfficePC();
  pressedTime = 0;
}

void loop()
{
  button.loop(); // MUST call the loop() function first

  // set cursor to first column, first row
  lcd.setCursor(0, 0);

  //if(backlight_shutoff != 0 && millis() > backlight_shutoff) {
  //  lcd.noBacklight();
  //  backlight_shutoff = 0;
  //}

  if (!first_boot && button.isPressed()) {
    resetSleepTimer();
    pressedTime = millis();
  }

  if (!first_boot && button.isReleased()) {
    resetSleepTimer();
    releasedTime = millis();

    long pressDuration = releasedTime - pressedTime;

    if ( pressDuration < SHORT_PRESS_TIME ) {
      lcd.backlight();
      backlight_shutoff = millis() + backlight_timeout;
      Serial.println("A short press is detected");
      start++;
      if(start > hostnames.size()-1) {
        start = 0;
      }
      updateDisplay();
    }

    if ( pressDuration >= SHORT_PRESS_TIME ) {
      Serial.println("A long press is detected");
      auto mac = splitValue(start, true);

      Serial.printf("%s\n", mac.c_str());

      if(WiFi.status()== WL_CONNECTED){
        if(WOL.sendMagicPacket(mac.c_str())) { // Send Wake On Lan packet with the above MAC address. Default to port 9.
        //if(true) {
          lcd.setCursor(0, 1);
          lcd.println("Sent            ");
          Serial.println("Sent");
        } else {
          lcd.setCursor(0, 1);
          lcd.println("Sending...      ");
          Serial.println("Sending");
        }
        lcd.backlight();
        backlight_shutoff = millis() + backlight_timeout;
      } else {
        lcd.setCursor(0, 1);
        lcd.println("No WiFi         ");
        lcd.backlight();
        backlight_shutoff = millis() + backlight_timeout;
      }
      // WOL.sendMagicPacket(MACAddress, 7); // Change the port number
    }
  }

  if(next_sleep != 0 && millis() > next_sleep) {
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.println("Sleeping...     ");
    //Go to sleep now
    Serial.println("Going to sleep now");
    delay(5000);
    lcd.noBacklight();
    esp_deep_sleep_start();
  }
  first_boot = false;
}
