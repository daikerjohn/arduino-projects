//  ----- BLE stuff -----
static NimBLERemoteCharacteristic *pRemoteCharacteristic;
static NimBLEAdvertisedDevice *myDevice;
NimBLERemoteService *pRemoteService;
// The remote service we wish to connect to. Needs check/change when other BLE module used.
static NimBLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms original module
// https://github.com/FurTrader/OverkillSolarBMS/blob/master/Comm_Protocol_Documentation/BLE%20_bluetooth_protocol.md#executive-summary
// * Characteristic FF01 is Read/Notify from the UART
//   The module will notify when this value is changed by the UART RX
static NimBLEUUID charUUID_rx("0000ff01-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms original module

// * Characteristic FF02 is Write Without Response to the UART
//   The module will transmit on the UART TX when this value changes
//   Can also be used to send instructions to the BLE module itself
static NimBLEUUID charUUID_tx("0000ff02-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms original module

/** Callback to process the results of the last scan or restart it */
void scanEndedCB(NimBLEScanResults results){
    TelnetPrint.println("Scan Ended");
    // call bleRequestData here?
}

static uint32_t scanTime = 0; /** 0 = scan forever */

// 0000ff01-0000-1000-8000-00805f9b34fb
// NOTIFY, READ
// Notifications from this characteristic is received data from BMS

// 0000ff02-0000-1000-8000-00805f9b34fb
// Write this characteristic to send data to BMS
// READ, WRITE, WRITE NO RESPONSE

class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks
{ //this is called by some underlying magic
    /**
	* Called for each advertising BLE server.
	*/
    void onResult(NimBLEAdvertisedDevice* advertisedDevice)
    {
        //commSerial.print("BLE Advertised Device found: ");
        //commSerial.println(advertisedDevice->toString().c_str());
        TelnetPrint.println(getTimestamp() + " - onResult " + advertisedDevice->toString().c_str());

        // We have found a device, let us now see if it contains the service we are looking for.
        if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(serviceUUID))
        {
            TelnetPrint.println("overkill UUID matches");
            NimBLEDevice::getScan()->stop();
            myDevice = advertisedDevice;
            doConnect = true;
            doScan = true;
        } // Found our server
    }     // onResult
};        // MyAdvertisedDeviceCallbacks

class MyClientCallback : public NimBLEClientCallbacks
{ //this is called on connect / disconnect by some underlying magic+
    void onConnect(NimBLEClient* pClient) {
        TelnetPrint.println("Connected");
        /** After connection we should change the parameters if we don't need fast response times.
         *  These settings are 150ms interval, 0 latency, 450ms timout.
         *  Timeout should be a multiple of the interval, minimum is 100ms.
         *  I find a multiple of 3-5 * the interval works best for quick response/reconnect.
         *  Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
         */
        //pClient->updateConnParams(120,120,0,60);
    };

    void onDisconnect(NimBLEClient* pClient) {
        BLE_client_connected = false;
        TelnetPrint.print(pClient->getPeerAddress().toString().c_str());
        TelnetPrint.println(" Disconnected - Starting scan");
        NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
    };

    /** Called when the peripheral requests a change to the connection parameters.
     *  Return true to accept and apply them or false to reject and keep
     *  the currently used parameters. Default will return true.
     */
     /*
    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
        if(params->itvl_min < 24) { // 1.25ms units
            return false;
        } else if(params->itvl_max > 40) { // 1.25ms units
            return false;
        } else if(params->latency > 2) { // Number of intervals allowed to skip
            return false;
        } else if(params->supervision_timeout > 100) { // 10ms units
            return false;
        }

        return true;
    };
    */

    /********************* Security handled here **********************
    ****** Note: these are the same return values as defaults ********/
    uint32_t onPassKeyRequest(){
        TelnetPrint.println("Client Passkey Request");
        /** return the passkey to send to the server */
        return 123456;
    };

    bool onConfirmPIN(uint32_t pass_key){
        TelnetPrint.print("The passkey YES/NO number: ");
        TelnetPrint.println(pass_key);
    /** Return false if passkeys don't match. */
        return true;
    };

    /** Pairing process complete, we can check the results in ble_gap_conn_desc */
    void onAuthenticationComplete(ble_gap_conn_desc* desc){
        if(!desc->sec_state.encrypted) {
            TelnetPrint.println("Encrypt connection failed - disconnecting");
            /** Find the client with the connection handle provided in desc */
            NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
            return;
        }
    };
};

/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic* pRemoteChar, uint8_t* pData, size_t length, bool isNotify){
    bleCollectPacket((char *)pData, length);
    std::string str = (isNotify == true) ? "Notification" : "Indication";
    str += " from ";
    /** NimBLEAddress and NimBLEUUID have std::string operators */
    str += std::string(pRemoteChar->getRemoteService()->getClient()->getPeerAddress());
    str += ": Service = " + std::string(pRemoteChar->getRemoteService()->getUUID());
    str += ", Characteristic = " + std::string(pRemoteChar->getUUID());
    str += ", Value = " + std::string((char*)pData, length);
    TelnetPrint.println(str.c_str());
}

void bleRequestData()
{
#ifndef SIMULATION

    // If the flag "doConnect" is true then we have scanned for and found the desired
    // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
    // connected we set the connected flag to be true.
    if (doConnect == true)
    {
        if (connectToServer())
        {
            //commSerial.println("We are now connected to the BLE Server.");
            TelnetPrint.println("We are now Connected in bleRequestData");
            str_ble_status += getTimestamp() + " - Connected\n";
            doConnect = false;
        }
    }

    // If we are connected to a peer BLE Server, update the characteristic each time we are reached
    // with the current time since boot.
    if (BLE_client_connected == true)
    {
                  TelnetPrint.println("BLE_client_connected ==true");

        unsigned long currentMillis = millis();
        if ((currentMillis - previousMillis >= interval || newPacketReceived)) //every time period or when packet is received
        {
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
        }
    }
    else if (doScan)
    {
      TelnetPrint.println("doScan");
      BLEDevice::getScan()->start(0); // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
    }
#endif

#ifdef SIMULATION
    bmsSimulate();
#endif
}

void bleStartup()
{
#ifndef SIMULATION
    doConnect = false;
    BLE_client_connected = false;
    doScan = false;

    TRACE;
    BLEDevice::init("");
    esp_err_t errRc=esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT,ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN ,ESP_PWR_LVL_P9); 
    int pwrAdv  = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);
    int pwrScan = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_SCAN);
    int pwrDef  = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);
    TelnetPrint.println("Power Settings: (ADV,SCAN,DEFAULT)");         //all should show index7, aka +9dbm
    TelnetPrint.println(pwrAdv);
    TelnetPrint.println(pwrScan);
    TelnetPrint.println(pwrDef);
    str_ble_status += getTimestamp() + " - BLEDevice::init('')\n";

    // Retrieve a Scanner and set the callback we want to use to be informed when we
    // have detected a new device.  Specify that we want active scanning and start the
    // scan to run for 5 seconds.
    NimBLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    //pBLEScan->setInterval(1349);
    //pBLEScan->setWindow(449);

    /** Set scan interval (how often) and window (how long) in milliseconds */
    pBLEScan->setInterval(1500);
    pBLEScan->setWindow(500);

    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, scanEndedCB);
    //pBLEScan->clearResults();
#endif
}

/** Define a class to handle the callbacks when advertisments are received */
class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {

    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        TelnetPrint.print("Advertised Device found: ");
        TelnetPrint.println(advertisedDevice->toString().c_str());
        if(advertisedDevice->isAdvertisingService(serviceUUID))
        {
            TelnetPrint.println("Found Our Service");
            /** stop scan before connecting */
            NimBLEDevice::getScan()->stop();
            /** Save the device reference in a global for the client to use*/
            myDevice = advertisedDevice;
            /** Ready to connect now */
            doConnect = true;
        }
    };
};

static void notifyCallbackTwo(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    TelnetPrint.printf("Notify callback for characteristic %s of data length %d\n",
           pBLERemoteCharacteristic->getUUID().toString().c_str(),
           length);
    hexDump((char*)pData, length);
    bleCollectPacket((char *)pData, length);
}

static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) //this is called when BLE server sents data via notofication
{
    TRACE;
    hexDump((char*)pData, length);
    bleCollectPacket((char *)pData, length);
}

bool connectToServer()
{
    TRACE;
    //commSerial.print("Forming a connection to ");
    //commSerial.println(advDevice->getAddress().toString().c_str());
    pClientOld = NimBLEDevice::createClient();
        TelnetPrint.println("New client created");

        
        //pClient->setClientCallbacks(&clientCB, false);
        //pClient->setClientCallbacks(new MyClientCallback());


    //commSerial.println(" - Created client");
    TelnetPrint.println("setClientCallbacks");
    delay(100);
    pClientOld->setClientCallbacks(new MyClientCallback());
    //pClientOld->setClientCallbacks(new ClientCallbacks());

        /** Set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout.
         *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
         *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
         *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
         */
        TelnetPrint.println("setConnectionParams");
        pClientOld->setConnectionParams(12,12,0,51);
        /** Set how long we are willing to wait for the connection to complete (seconds), default is 30. */
        TelnetPrint.println("setConnectTimeout");
        pClientOld->setConnectTimeout(5);

        TelnetPrint.println("Checking some things...");
        if(myDevice == nullptr) {
          TelnetPrint.println("myDevice == nullptr");
          return false;
        }
    // Connect to the remote BLE Server.
    if(pClientOld->isConnected()) {
      TelnetPrint.println("pClientOld->isConnected()");
    } else {
      TelnetPrint.println("pClientOld->connect");
      if (!pClientOld->connect(myDevice)) { // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)      
        TelnetPrint.println("Failed to connect");
        return false;
      }
    }
    //commSerial.println(" - Connected to server");
    // Obtain a reference to the service we are after in the remote BLE server.
    //BLERemoteService*
    //pRemoteService = pClientOld->getService(serviceUUID);
    TelnetPrint.println("pClientOld->getService");
    pRemoteService = pClientOld->getService(serviceUUID);
    if (pRemoteService == nullptr)
    {
        //commSerial.print("Failed to find our service UUID: ");
        //commSerial.println(serviceUUID.toString().c_str());
        TelnetPrint.println("pClientOld->disconnect");
        pClientOld->disconnect();
        return false;
    }
    //commSerial.println(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_rx);
    if (pRemoteCharacteristic == nullptr)
    {
        TelnetPrint.print("Failed to find our characteristic UUID: ");
        TelnetPrint.println(charUUID_rx.toString().c_str());
        //commSerial.print("Failed to find our characteristic UUID: ");
        //commSerial.println(charUUID_rx.toString().c_str());
        pClientOld->disconnect();
        return false;
    }
    //commSerial.println(" - Found our characteristic");
    // Read the value of the characteristic.
    if (pRemoteCharacteristic->canRead())
    {
        std::string value = pRemoteCharacteristic->readValue();
        //commSerial.print("The characteristic value was: ");
        //commSerial.println(value.c_str());
    }

    if (pRemoteCharacteristic->canNotify()) {
		    //commSerial.print("The Device can notify");
        //pRemoteCharacteristic->registerForNotify(notifyCallback);
        //pRemoteCharacteristic->subscribe(true, notifyCallback);
        pRemoteCharacteristic->subscribe(true, notifyCallbackTwo);
    } else {
      TelnetPrint.println("Cannot subscribe to rx characteristic");
    }

    BLE_client_connected = true;
    return BLE_client_connected;
}

void sendCommand(uint8_t *data, size_t dataLen)
{
    TRACE;
    //https://github.com/FurTrader/OverkillSolarBMS/blob/master/Comm_Protocol_Documentation/BLE%20_bluetooth_protocol.md#executive-summary
    //pRemoteService->getCharacteristics(true);
    //TelnetPrint.print("Characteristics: ");
    //TelnetPrint.println(pRemoteService->toString().c_str());
    BLERemoteCharacteristic *pRemoteCharacteristic_overkill;

    pRemoteCharacteristic_overkill = pRemoteService->getCharacteristic(charUUID_tx);
    if (pRemoteCharacteristic_overkill)
    {
        //flush();
        TelnetPrint.println(getTimestamp() + " Write to Regular");
        if(!pRemoteCharacteristic_overkill->writeValue(data, dataLen, false)) {
          TelnetPrint.println("Unable to send command");
        }
        //sleep(100);
        //commSerial.println("bms request sent");
        //str_ble_status += "bms request send\n";
        //TelnetPrint.println("bms request send");
    }
    else
    {
        str_ble_status += "Remote TX characteristic not found\n";
        TelnetPrint.println("Remote TX characteristic not found");
        //commSerial.println("Remote TX characteristic not found");
    }
    /*
    BLERemoteCharacteristic *pRemoteCharacteristic_overkill;
    pRemoteCharacteristic_overkill = pRemoteService->getCharacteristic(charUUID_tx);
    if (pRemoteCharacteristic_overkill)
    {
        TelnetPrint.println("Write to Overkill: canWrite: " + String(pRemoteCharacteristic_overkill->canWrite()));
        TelnetPrint.println("Write to Overkill: canWriteNoResponse: " + String(pRemoteCharacteristic_overkill->canWriteNoResponse()));
        //flush();
        TelnetPrint.println("Write to Overkill");
        if(!pRemoteCharacteristic_overkill->writeValue(data, dataLen, false)) {
          TelnetPrint.println("Unable to send command");
        }
        //commSerial.println("bms request sent");
        //str_ble_status += "bms request send\n";
        //TelnetPrint.println("bms request send");
    }
    else
    {
        str_ble_status += "Remote TX characteristic not found\n";
        TelnetPrint.println("Remote TX characteristic not found");
        //commSerial.println("Remote TX characteristic not found");
    }
    */
}
