#include <WiFi.h>

const int analogIn = 34;

const char* ssid = "dirker";
const char* password = "alphabit";

WiFiServer server(80);

String header;

void setup() {
  Serial.begin(9600);

  String fv = WiFi.firmwareVersion();
  Serial.println(fv);

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
}

void loop() {
  int analogValue = analogRead(analogIn);
  Serial.print("analogV: ");
  Serial.print(analogValue);

  float millivolts = (analogValue / 1024.0) * 3300; //3300 is the voltage provided by NodeMCU
  //float millivolts = (analogValue / 1024.0) * 5000; //5000 is the voltage provided by NodeMCU
  Serial.print(" milli: ");
  Serial.print(millivolts);

  float celsius = millivolts / 10;
  Serial.print(" DegreeC: ");
  Serial.print(celsius);

  //---------- Here is the calculation for Fahrenheit ----------//

  float fahrenheit = ((celsius * 9) / 5 + 32);
  Serial.print(" Farenheit: ");
  Serial.println(fahrenheit);


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
    client.println("<h1>WEB SERVER LM35 SENSOR</h1>\n");
    client.println("<p style=\"color:red\">Temperature =\"");
    client.println("*C</p>\n");
    client.println(celsius);
    client.println("<p style=\"color:red\">Temperature in Farenheit =\"");
    client.println(fahrenheit);
    client.println("*F</p>\n");
    client.println("</body></html>");
    client.println();
    Serial.println("Client disconnected.");
    Serial.println("");
  }

  delay(1000);
}
