
/**
 *  NimBLE_Async_client Demo:
 *
 *  Demonstrates asynchronous client operations.
 *
 *  Created: on November 4, 2024
 *      Author: H2zero
 */

const char compile_date[] = __DATE__ " " __TIME__;

#include "overkill_datatypes.h"

//#include <Arduino.h>
#include <NimBLEDevice.h>

#include <WiFi.h>
//#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <TelnetPrint.h>
//#include <ArduinoJson.h>
#include <Preferences.h>

#include <ESPAsyncWebSrv.h>

// Needs mod to work with ESPAsyncWebSrv above.  Original version wants ESPAsyncWeb*Server* not Srv
// https://github.com/ayushsharma82/AsyncElegantOTA
#include "libs/AsyncElegantOTA/AsyncElegantOTA.h"

static NimBLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms original module
//static NimBLEUUID serviceUUID("0xff00"); //xiaoxiang bms original module
// https://github.com/FurTrader/OverkillSolarBMS/blob/master/Comm_Protocol_Documentation/BLE%20_bluetooth_protocol.md#executive-summary
// * Characteristic FF01 is Read/Notify from the UART
//   The module will notify when this value is changed by the UART RX
static NimBLEUUID charUUID_rx("0000ff01-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms original module

// * Characteristic FF02 is Write Without Response to the UART
//   The module will transmit on the UART TX when this value changes
//   Can also be used to send instructions to the BLE module itself
static NimBLEUUID charUUID_tx("0000ff02-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms original module

static constexpr uint32_t scanTimeMs = 10 * 1000;

int last_data_size = 0;
bool BLE_client_connected = false;
String last_data_capture_bms = "";
String str_ble_status = "";

NimBLERemoteService *pRemoteService = nullptr;

unsigned long previousMillis = 0;
const long interval = 45000;
bool toggle = false;
bool newPacketReceived = false;

static const NimBLEAdvertisedDevice* advDevice = nullptr;
static bool                          doConnect  = false;


/** Notification / Indication receiving handler callback */
static void notifyCallbackTwo(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length,bool isNotify) {
  TelnetPrint.println("Notify callback for characteristic");
  TelnetPrint.printf("Notify callback for characteristic %s of data length %d\n",
          pBLERemoteCharacteristic->getUUID().toString().c_str(), length);
  hexDump((char*)pData, length);
  last_data_size = length;
  bleCollectPacket((char *)pData, length);
}


String getTimestamp(bool doShort=false) {
  time_t now;
  struct tm asdf;
  char buf[80];
  if (!getLocalTime(&asdf)) {
    //TelnetPrint.println("Failed to obtain time");
    return "";
  } else {
    time(&now);
    asdf = *localtime(&now);
  }
  if(doShort) {
    strftime(buf, sizeof(buf), "%m-%d %H:%M:%S", &asdf);
  } else {
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z", &asdf);
  }
  return String(buf);
  //printf("%s\n", buf);
  //return now;
}




/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override { TelnetPrint.printf("Connected\n"); }

    void onDisconnect(NimBLEClient* pClient, int reason) override {
        TelnetPrint.printf("%s Disconnected, reason = %d - Starting scan\n", pClient->getPeerAddress().toString().c_str(), reason);
        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }

    /********************* Security handled here *********************/
    void onPassKeyEntry(NimBLEConnInfo& connInfo) override {
        TelnetPrint.printf("Server Passkey Entry\n");
        /**
         * This should prompt the user to enter the passkey displayed
         * on the peer device.
         */
        NimBLEDevice::injectPassKey(connInfo, 123456);
    }

    void onConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t pass_key) override {
        TelnetPrint.printf("The passkey YES/NO number: %" PRIu32 "\n", pass_key);
        /** Inject false if passkeys don't match. */
        NimBLEDevice::injectConfirmPasskey(connInfo, true);
    }

    /** Pairing process complete, we can check the results in connInfo */
    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
        if (!connInfo.isEncrypted()) {
            TelnetPrint.printf("Encrypt connection failed - disconnecting\n");
            /** Find the client with the connection handle provided in connInfo */
            NimBLEDevice::getClientByHandle(connInfo.getConnHandle())->disconnect();
            return;
        }
    }
} clientCallbacks;

/** Define a class to handle the callbacks when scan events are received */
class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        TelnetPrint.printf("Advertised Device found: %s\n", advertisedDevice->toString().c_str());
        if (advertisedDevice->isAdvertisingService(serviceUUID)) {
            TelnetPrint.printf("Found Our Service\n");
            /** stop scan before connecting */
            NimBLEDevice::getScan()->stop();
            /** Save the device reference in a global for the client to use*/
            advDevice = advertisedDevice;
            /** Ready to connect now */
            doConnect = true;
        }
    }

    /** Callback to process the results of the completed scan or restart it */
    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        TelnetPrint.printf("Scan Ended, reason: %d, device count: %d\n", reason, results.getCount());
        //TelnetPrint.printf("Restarting scan\n");
        //NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
} scanCallbacks;

/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    TelnetPrint.printf("notifyCB\n");
    std::string str  = (isNotify == true) ? "Notification" : "Indication";
    str             += " from ";
    str             += pRemoteCharacteristic->getClient()->getPeerAddress().toString();
    str             += ": Service = " + pRemoteCharacteristic->getRemoteService()->getUUID().toString();
    str             += ", Characteristic = " + pRemoteCharacteristic->getUUID().toString();
    str             += ", Value = " + std::string((char*)pData, length);
    TelnetPrint.printf("%s\n", str.c_str());
}





/** Handles the provisioning of clients and connects / interfaces with the server */
bool connectToServer() {
    NimBLEClient* pClient = nullptr;

    /** Check if we have a client we should reuse first **/
    if (NimBLEDevice::getCreatedClientCount()) {
        /**
         *  Special case when we already know this device, we send false as the
         *  second argument in connect() to prevent refreshing the service database.
         *  This saves considerable time and power.
         */
        if (!advDevice) {
            TelnetPrint.printf("Special - No advDevice\n");
            return false;          
        }
        pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
        if (pClient) {
            if (!pClient->connect(advDevice, false)) {
                TelnetPrint.printf("Reconnect failed\n");
                return false;
            }
            TelnetPrint.printf("Reconnected client\n");
        } else {
            /**
             *  We don't already have a client that knows this device,
             *  check for a client that is disconnected that we can use.
             */
            pClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    /** No client to reuse? Create a new one. */
    if (!pClient) {
        if (NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS) {
            TelnetPrint.printf("Max clients reached - no more connections available\n");
            return false;
        }

        pClient = NimBLEDevice::createClient();

        TelnetPrint.printf("New client created\n");

        pClient->setClientCallbacks(&clientCallbacks, false);
        /**
         *  Set initial connection parameters:
         *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
         *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
         *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 150 * 10ms = 1500ms timeout
         */
        pClient->setConnectionParams(12, 12, 0, 150);

        /** Set how long we are willing to wait for the connection to complete (milliseconds), default is 30000. */
        pClient->setConnectTimeout(5 * 1000);

        if (!advDevice) {
            TelnetPrint.printf("No advDevice\n");
            return false;
        }
        if (!pClient->connect(advDevice)) {
            /** Created a client but failed to connect, don't need to keep it as it has no data */
            NimBLEDevice::deleteClient(pClient);
            TelnetPrint.printf("Failed to connect, deleted client\n");
            return false;
        }
    }

    if (!pClient->isConnected()) {
        if (!advDevice) {
            TelnetPrint.printf("No advDevice\n");
            return false;
        }
        if (!pClient->connect(advDevice)) {
            TelnetPrint.printf("Failed to connect\n");
            return false;
        }
    }

    TelnetPrint.printf("Connected to: %s RSSI: %d\n", pClient->getPeerAddress().toString().c_str(), pClient->getRssi());

    /** Now we can read/write/subscribe the characteristics of the services we are interested in */
    NimBLERemoteService*        pSvc = nullptr;
    NimBLERemoteCharacteristic* pChr = nullptr;
    NimBLERemoteDescriptor*     pDsc = nullptr;

    pRemoteService = pClient->getService(serviceUUID);

    pSvc = pClient->getService(serviceUUID);
    if (pSvc) {
        pChr = pSvc->getCharacteristic(charUUID_rx);
    }

    if (pChr) {
        if (pChr->canRead()) {
            TelnetPrint.printf("%s Value: %s\n", pChr->getUUID().toString().c_str(), pChr->readValue().c_str());
        }
        TelnetPrint.printf("Asdf\n");
        /*
        if (pChr->canWrite()) {
            if (pChr->writeValue("Tasty")) {
                TelnetPrint.printf("Wrote new value to: %s\n", pChr->getUUID().toString().c_str());
            } else {
                pClient->disconnect();
                return false;
            }

            if (pChr->canRead()) {
                TelnetPrint.printf("The value of: %s is now: %s\n", pChr->getUUID().toString().c_str(), pChr->readValue().c_str());
            }
        }
        */
        pChr->subscribe(true, notifyCallbackTwo);
        pChr->subscribe(false, notifyCallbackTwo);
/*
        if (pChr->canNotify()) {
            if (!pChr->subscribe(true, notifyCB)) {
                pClient->disconnect();
                return false;
            }
        } else if (pChr->canIndicate()) {
            // Send false as first argument to subscribe to indications instead of notifications
            if (!pChr->subscribe(false, notifyCB)) {
                pClient->disconnect();
                return false;
            }
        }
        */
    } else {
        TelnetPrint.printf("DEAD service not found.\n");
    }

/*
    pSvc = pClient->getService("BAAD");
    if (pSvc) {
        pChr = pSvc->getCharacteristic("F00D");
        if (pChr) {
            if (pChr->canRead()) {
                TelnetPrint.printf("%s Value: %s\n", pChr->getUUID().toString().c_str(), pChr->readValue().c_str());
            }

            pDsc = pChr->getDescriptor(NimBLEUUID("C01D"));
            if (pDsc) {
                TelnetPrint.printf("Descriptor: %s  Value: %s\n", pDsc->getUUID().toString().c_str(), pDsc->readValue().c_str());
            }

            if (pChr->canWrite()) {
                if (pChr->writeValue("No tip!")) {
                    TelnetPrint.printf("Wrote new value to: %s\n", pChr->getUUID().toString().c_str());
                } else {
                    pClient->disconnect();
                    return false;
                }

                if (pChr->canRead()) {
                    TelnetPrint.printf("The value of: %s is now: %s\n",
                                  pChr->getUUID().toString().c_str(),
                                  pChr->readValue().c_str());
                }
            }

            if (pChr->canNotify()) {
                if (!pChr->subscribe(true, notifyCB)) {
                    pClient->disconnect();
                    return false;
                }
            } else if (pChr->canIndicate()) {
                // Send false as first argument to subscribe to indications instead of notifications
                if (!pChr->subscribe(false, notifyCB)) {
                    pClient->disconnect();
                    return false;
                }
            }
        }
    } else {
        TelnetPrint.printf("BAAD service not found.\n");
    }
*/
    TelnetPrint.printf("Done with this device!\n");
    return true;
}



void bleRequestData()
{
    // If the flag "doConnect" is true then we have scanned for and found the desired
    // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
    // connected we set the connected flag to be true.
    
    if (!BLE_client_connected) {
        doConnect = false;
        /** Found a device we want to connect to, do it now */
        if (connectToServer()) {
            BLE_client_connected = true;
            str_ble_status += getTimestamp() + " - Connected\n";
            TelnetPrint.printf("Success! we should now be getting notifications, scanning for more!\n");
        } else {
            TelnetPrint.printf("Failed to connect, starting scan\n");
            NimBLEDevice::getScan()->start(scanTimeMs, false, true);
        }
    }

    // If we are connected to a peer BLE Server, update the characteristic each time we are reached
    // with the current time since boot.
    if (BLE_client_connected == true)
    {
        TelnetPrint.println("BLE_client_connected ==true");
        unsigned long currentMillis = millis();
        //if ((currentMillis - previousMillis >= interval || newPacketReceived)) //every time period or when packet is received
        //{
            previousMillis = currentMillis;

            if (toggle) //alternate info3 and info4
            {
                TelnetPrint.println("bmsGetInfo3");
                bmsGetInfo3();
                //showBasicInfo();
                newPacketReceived = false;
            }
            else
            {
                TelnetPrint.println("bmsGetInfo4");
                bmsGetInfo4();
                //showCellInfo();
                newPacketReceived = false;
            }
            toggle = !toggle;
        //}
    }
    //else if (doScan)
    //{
    //  TelnetPrint.println("doScan");
    //  BLEDevice::getScan()->start(0); // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
    //}
}




// Set web server port number to 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

//ESPAsyncHTTPUpdateServer _updateServer;

size_t content_len;

String boot_time_str = "";

const char *headHtml = R"literal(
<!DOCTYPE html><html>
<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">
<link rel=\"icon\" href=\"data:,\">
<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}
.button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;
text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}
.button2 {background-color: #555555;}</style>
<script>

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
// Init web socket when the page loads
window.addEventListener('load', onload);

function onload(event) {
    //initWebSocket();
}

function getReadings(){
    websocket.send("getReadings");
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

// When websocket is established, call the getReadings() function
function onOpen(event) {
    console.log('Connection opened');
    getReadings();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

// Function that receives the message from the ESP32 with the readings
function onMessage(event) {
    console.log(event.data);
    var myObj = JSON.parse(event.data);
    var keys = Object.keys(myObj);

    for (var i = 0; i < keys.length; i++){
        var key = keys[i];
        document.getElementById(key).innerHTML = myObj[key];
    }
}

)literal";

String htmlHead(unsigned long reloadDelayMs = 0) {
  String data = String(headHtml);
  if (reloadDelayMs > 5000) {
    data += "setTimeout(function(){ window.location.reload(); }, " + String(reloadDelayMs) + ");";
  }
  data += "</script>";

  data += "</head>";
  return data;
}

String htmlFoot() {
  String data = "<p>Compiled: " + String(compile_date) + "</p>";
  data += "<p><a href='/'>Home</a> | <a href='/config'>Config</a> | <a href='/update'>Update</a> | <a href='/restart'>restart</a></p>";
  //data += "<p><a href='/'>Home</a> | <a href='/raw'>raw</a> | <a href='/json'>json</a> | <a href='/config'>Config</a> | <a href='/update'>Update</a> | <a href='/restart'>restart</a></p>";
  data += "<br/>";
  data += "<p><a href='/ble'>BLE</a> | <a href='/bledisc'>BLE Disconn</a> | <a href='/bleconn'>BLE Connect</a> | <a href='/blereq'>BLE Request</a> | <a href='/bleclearstr'>BLE ClearString</a> | <a href='/bleinit'>BLE Init</a></p>";
  data += "</body></html>";
  return data;
}

String configIndex() {
  String data = htmlHead(0);

/*
  data += "<form action=\"/set\">Refresh interval: <input type=\"text\" name=\"refresh_int\" value=\"" + String(refresh_interval_min) + "\"><br/>";
  data += "Coin Delay: <input type=\"text\" name=\"coin_delay\" value=\"" + String(coin_delay_ms) + "\"><br/>";
  
  data += "<input type=\"submit\" value=\"Submit\"></form>";

  data += "<form action=\"/set\">";
  for(int x = 0; x < NUM_COINS; x++) {
    data += "Coin Symbol: <input type=\"text\" name=\"coin_sym_" + String(x) + "\" value=\"" + coin_symbols[x] + "\"><br/>";
    data += "Coin Balances: <input type=\"text\" name=\"coin_bal_" + String(x) + "\" value=\"" + String(coin_balances[x]) + "\"><br/>";  
  }
  data += "<input type=\"submit\" value=\"Submit\"></form>";
  */

  //data += "<form action=\"/set\">BLE Auto Connect: <input type=\"text\" name=\"ble_autoconn\" value=\"" + String(ble_autoconn) + "\"><br/>";
  //data += "<input type=\"submit\" value=\"Submit\"></form>";

  data += htmlFoot();
  return data;
}

String statsIndex() {
  String data = htmlHead(0);

  // Web Page Heading
  data += "<body><h1>ESP32 Web Server - With OTAv3</h1>";

  data += "<p>Boot Time: " + boot_time_str + "</p>";

  unsigned long totalBal=0.0;
  /*
  for(int x = 0; x < NUM_COINS && coin_symbols[x] != ""; x++) {
    data += "<p>" + coin_symbols[x] + ": <span id=" + coin_symbols[x] + ">" + coinStrings[x][1] + "</span></p>";
    //for(int y = 0; y < NUM_LINES; y++) {
    //  data += "<p><span id=" + coin_symbols[x] + ">" + coinStrings[x][y] + "</span></p>";
    //}

    //delay(coin_delay_ms);
    totalBal += coinBalancesByCoin[x];
  }
  */

  data += "</br><p>" + getTimestamp(true) + "</p>";
  data += "<p>$" + String(totalBal, 2) + "</p>";

  /*
  // Display current state of the Renogy load
  data += "<p>Relay Load - " + String(load_running ? "Running" : "Stopped") + "</p>";
  data += "<p>Current Rate: " + String((float)packBasicInfo.Watts) + " watts</p>";
  data += "<p>Net Rate: " + String(net_avg_system_watts) + " watts</p>";
  if (automatic_decision != "") {
    data += "<p>Last Automatic: " + String(automatic_decision) + "</p>";
  }
  data += "<p>Automatics: " + String(allow_auto ? "On" : "Off") + "</p>";
  data += automatic_decisions.html(true);

  data += "<p>Last BMS Data Capture: " + last_data_capture_bms + "</p>";
  data += "<p>Last SCC Data Capture: " + last_data_capture_scc + "</p>";
  if (current_status != "") {
    data += "<p>Current: " + String(current_status) + "</p>";
  }
  data += "<p>" + String(status_str) + "</p>";
  data += "<p>***" + String(stat_char) + "***</p>";

  // If the load_running is on, it displays the OFF button
  //if (load_running) {
    data += "<p><a href=\"/load/off\"><button class=\"button button2\">All OFF</button></a><a href=\"/load/on\"><button class=\"button\">All ON</button></a></p>";
  //}
  for(int x = 0; x < NUM_DEVICES; x++) {
    data += "<p><a href=\"/load/off?device=" + String(x) + "\"><button class=\"button button2\">" + String(x) + " OFF</button></a>";
    data += "<a href=\"/load/on?device=" + String(x) + "\"><button class=\"button\">" + String(x) + " ON</button></a></p>";
  }
  */

  data += htmlFoot();
  return data;
}

void notFoundResp(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void handle_webserver_connection() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    //status_str = "";
    //stat_char[0] = '\0';
    //update_decisions(false);
    request->send(200, "text/html", statsIndex());
  });
  server.on("/ble", HTTP_GET, [](AsyncWebServerRequest *request) {
    //update_decisions(false);
    request->send(200, "text/plain", str_ble_status);
  });
  /*
  server.on("/http", HTTP_GET, [](AsyncWebServerRequest *request) {
    //update_decisions(false);
    request->send(200, "text/html", str_http_status);
  });
  server.on("/raw", HTTP_GET, [](AsyncWebServerRequest *request) {
    status_str = "";
    stat_char[0] = '\0';
    update_decisions(false);
    request->send(200, "text/plain", rawIndex(true));
  });
  server.on("/json", HTTP_GET, [](AsyncWebServerRequest *request) {
    status_str = "";
    stat_char[0] = '\0';
    update_decisions(false);
    request->send(200, "application/json", structToString());
  });
  */
  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
    ESP.restart();
    request->redirect("/");
  });
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
  server.on("/max", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(ESP.getMaxAllocHeap()));
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    //status_str = "";
    //stat_char[0] = '\0';
    request->send(200, "text/html", configIndex());
  });

  /*
  // Load control
  server.on("/load/on", HTTP_GET, [](AsyncWebServerRequest *request) {
    int device = -1;
    if(request->hasParam("device")) {
      device = std::stoi(request->getParam("device")->value().c_str());
    }
    TelnetPrint.println("Device: " + String(device));
    power_on_load(device, "Load On - Manual");
    last_action_was_manual = true;
    request->redirect("/");
  });
  server.on("/load/off", HTTP_GET, [](AsyncWebServerRequest *request) {
    int device = -1;
    if(request->hasParam("device")) {
      device = std::stoi(request->getParam("device")->value().c_str());
    }
    TelnetPrint.println("Device: " + String(device));
    //AsyncWebParameter* p = request->getParam("device")->value();
    //if(p->name() == "device") {
    //  TelnetPrint.println("Device:" + String(request->getParam("device")->value());
    //}
    turn_off_load(device, "Load Off - Manual");
    last_action_was_manual = true;
    request->redirect("/");
  });
  */

  server.on("/bleinit", HTTP_GET, [](AsyncWebServerRequest *request) {
    str_ble_status += getTimestamp() + " - BLE - Init\n";
    initBle();
    //NimBLEDevice::getScan()->start(scanTimeMs);
    //bleStartup();
    request->redirect("/");
  });

  server.on("/bleconn", HTTP_GET, [](AsyncWebServerRequest *request) {
    str_ble_status += getTimestamp() + " - BLE - Connect\n";
  //  if(!ble_autoconn) {
  //    bleStartup();
  //  }
    connectToServer();
    //turn_off_load("Load Off - Manual");
    request->redirect("/");
  });
  server.on("/blereq", HTTP_GET, [](AsyncWebServerRequest *request) {
    str_ble_status += getTimestamp() + " - BLE - Request\n";
    bleRequestData();
    //turn_off_load("Load Off - Manual");
    request->redirect("/");
  });
  
  server.on("/bleclearstr", HTTP_GET, [](AsyncWebServerRequest *request) {
    str_ble_status = getTimestamp() + " - Clear Str\n";
    request->redirect("/");
  });

  server.on("/bledisc", HTTP_GET, [](AsyncWebServerRequest *request) {
    auto pClients = NimBLEDevice::getConnectedClients();
    //if (!pClients.size()) {
    //    return;
    //}

    for (auto& pClient : pClients) {
        TelnetPrint.printf("%s\n", pClient->toString().c_str());
        NimBLEDevice::deleteClient(pClient);
    }
    str_ble_status += getTimestamp() + " - BLE - Disconnect\n";
    request->redirect("/");
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    /*
    //int new_value;
    String input_value;
    // TODO: loop here
    if (request->hasArg("refresh_int") || request->hasArg("coin_delay")
        || request->hasArg("coin_sym_0") || request->hasArg("coin_bal_0")
        || request->hasArg("coin_sym_1") || request->hasArg("coin_bal_1")
        || request->hasArg("coin_sym_2") || request->hasArg("coin_bal_2")
        || request->hasArg("coin_sym_3") || request->hasArg("coin_bal_3")
        ) {
      if (prefs.begin("crypto-app", false)) {
        if (request->hasArg("refresh_int")) {
          input_value = request->arg("refresh_int");
          refresh_interval_min = input_value.toInt();
          timerWebDelayMS = refresh_interval_min * 1000 * 60;
          prefs.putInt("refresh_int", refresh_interval_min);
        }
        if (request->hasArg("coin_delay")) {
          input_value = request->arg("coin_delay");
          coin_delay_ms = input_value.toInt();
          prefs.putInt("coin_delay", coin_delay_ms);
        }
        for(int x = 0; x < NUM_COINS; x++) {
          String sym_key = "coin_sym_" + String(x);
          if (request->hasArg(sym_key.c_str())) {
            input_value = request->arg(sym_key.c_str());
            coin_symbols[x] = input_value;
            prefs.putString(sym_key.c_str(), coin_symbols[x]);
          }
          String bal_key = "coin_bal_" + String(x);
          if (request->hasArg(bal_key.c_str())) {
            input_value = request->arg(bal_key.c_str());
            coin_balances[x] = std::stod(input_value.c_str());
            prefs.putDouble(bal_key.c_str(), coin_balances[x]);
          }
        }
        delay(100);
        prefs.end();
      } else {
        request->send(500, "text/plain", "Failed to begin() prefs for write");
      }
    }
    */
    request->redirect("/");
  });

  server.onNotFound(notFoundResp);
}

// Websockets Start

void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    //data[len] = 0;
    //String message = (char*)data;
    // Check if the message is "getReadings"
    if (strcmp((char*)data, "getReadings") == 0) {
      //if it is, send current sensor readings
      String sensorReadings = "asdf"; //getSensorReadings();
      TelnetPrint.print(sensorReadings);
      //notifyClients(sensorReadings);
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      TelnetPrint.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      TelnetPrint.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

// Websockets End

void initBle() {
    TelnetPrint.printf("Starting NimBLE Async Client\n");
    /*
    NimBLEDevice::init("Async-Client");
    NimBLEDevice::setPower(3);  // +3db

    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(&scanCallbacks, false);
    pScan->setInterval(45);
    pScan->setWindow(45);
    pScan->setActiveScan(true);
    pScan->start(scanTimeMs);
    */

    /** Initialize NimBLE and set the device name */
    NimBLEDevice::init("NimBLE-Client");

    /**
     * Set the IO capabilities of the device, each option will trigger a different pairing method.
     *  BLE_HS_IO_KEYBOARD_ONLY   - Passkey pairing
     *  BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
     *  BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
     */
    // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY); // use passkey
    // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison

    /**
     * 2 different ways to set security - both calls achieve the same result.
     *  no bonding, no man in the middle protection, BLE secure connections.
     *  These are the default values, only shown here for demonstration.
     */
    // NimBLEDevice::setSecurityAuth(false, false, true);

    //NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);

    /** Optional: set the transmit power */
    NimBLEDevice::setPower(3); /** 3dbm */
    NimBLEScan* pScan = NimBLEDevice::getScan();

    /** Set the callbacks to call when scan events occur, no duplicates */
    pScan->setScanCallbacks(&scanCallbacks, false);

    /** Set scan interval (how often) and window (how long) in milliseconds */
    pScan->setInterval(45);
    pScan->setWindow(45);

    /**
     * Active scan will gather scan response data from advertisers
     *  but will use more energy from both devices
     */
    pScan->setActiveScan(true);

    /** Start scanning for advertisers */
    pScan->start(scanTimeMs);
    TelnetPrint.printf("Scanning for peripherals\n");
}


bool first_loop = true;
unsigned long currentMillis = 0;
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;

void setup() {
    Serial.begin(115200);


  //lcd.begin(16, 2);
  //lcd.print("Scrolling Text Example");

  Serial.begin(115200);
  Serial.println("Started!");
  
  String ssid_str = "dirker";
  String password_str = "alphabit";
  //ssid_str = "TP-Link_7536";
  //password_str = "07570377";

  //next_available_startup = millis() + shut_cool_ms;

  WiFi.begin(ssid_str.c_str(), password_str.c_str());
  //WiFi.begin("dirker", "alphabit");
  //WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(2500);
    Serial.println("connecting...");
    Serial.println(ssid_str);
    Serial.println(password_str);
  }
  Serial.println(WiFi.localIP().toString());

  //Telnet log is accessible at port 23
  TelnetPrint.begin();

  //For UTC -8.00 : -8 * 60 * 60 : -28800
  const long gmtOffset_sec = -8 * 60 * 60;
  //Set it to 3600 if your country observes Daylight saving time; otherwise, set it to 0.
  const int daylightOffset_sec = 3600;
  Serial.println("configTime");
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");

  Serial.println("handle_webserver_connection");
  handle_webserver_connection();
  //initWebSocket();

  // Start webserver
  Serial.println("server.begin();");
  server.begin();
  Serial.println("AsyncElegantOTA.begin();");
  AsyncElegantOTA.begin(&server);  // Start AsyncElegantOTA

  boot_time_str = getTimestamp();

  Serial.println("initBle");
  initBle();
  //delay(500);
  //bleRequestData();
  //delay(5000);
}

// Set timer to 60 seconds (60000)
unsigned long timerDelayMS = 30000;
void loop() {
  if (first_loop) {
    TelnetPrint.println("Entering loop");
    TelnetPrint.println("Entering loop...");
  }

  currentMillis = millis();
  // Only read info every 10th loop
  if ((currentMillis - lastTime) > timerDelayMS || first_loop) {
    bleRequestData();
    //auto pClients = NimBLEDevice::getConnectedClients();
    //if (!pClients.size()) {
    //    return;
    //}
    Serial.println("Ping");
    TelnetPrint.println("Ping");

    //for (auto& pClient : pClients) {
    //    TelnetPrint.printf("%s\n", pClient->toString().c_str());
    //    //NimBLEDevice::deleteClient(pClient);
    //}
    lastTime = millis();
  }

  first_loop = false;

    //NimBLEDevice::getScan()->start(scanTimeMs);


    //ws.cleanupClients();
}
