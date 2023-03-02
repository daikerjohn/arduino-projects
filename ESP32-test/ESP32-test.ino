#include <WiFi.h>

const int analogIn = 34;

const char* ssid = "dirker";
const char* password = "alphabit";

WiFiServer server(80);

String header;




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




void setup() {
  Serial.begin(115200);

  //String fv = WiFi.firmwareVersion();
  //Serial.println(fv);

  Serial.print("Connecting to Wifi Network");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Successfully connected to WiFi.");
  Serial.println("IP address of ESP32 is : ");
  Serial.println(WiFi.localIP());
  server.begin();
  Serial.println("Server started");

  Serial.println("Current Time: " + String(millis()));
  Serial.println("Initial TTL: " + String(ttlMilliSec / MS_PER_SECOND) + "sec");
  Serial.println("Initial Runtime: " + String(activeRuntimeMillis / MS_PER_SECOND) + "sec");
}

void loop() {



  // This line checks if web client is available or not
  WiFiClient client = server.available();
  // if client is available 'if' condition becomes true
  if (client)
  {
    Serial.println("Web Client connected ");   //print on serial monitor
    String request = client.readStringUntil('\r'); // wait untill http get request ends
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println();

    // This part send HTML page to web client
    // HTML page contains temperature values
    client.println("<!DOCTYPE html>\n");
    client.println("<html>\n");
    client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    client.println("<body>\n");
    client.println("<h1>Water Pump Web Server</h1>\n");
    //client.println("Status = ");
    if(currRun > 0) {
      client.println("Status = Running\n");
    } else {
      client.println("<p style=\"color:red\">Stopped</p>\n");
    }
    //client.println("</p>\n");
    //client.println("<p style=\"color:red\">Temperature in Farenheit =\"");
    //client.println(fahrenheit);
    //client.println("*F</p>\n");
    client.println("</body></html>");
    client.println();

    Serial.println("Client disconnected.");
    Serial.println("");
  }

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
}
