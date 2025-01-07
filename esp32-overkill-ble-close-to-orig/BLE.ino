//  ----- BLE stuff -----
static NimBLERemoteService *pRemoteService = nullptr;
static NimBLERemoteCharacteristic *pRemoteCharacteristic_overkill = nullptr;

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




/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override { TelnetPrint.printf("Connected\n"); }

    void onDisconnect(NimBLEClient* pClient, int reason) override {
        TelnetPrint.printf("%s Disconnected, reason = %d - Starting scan\n", pClient->getPeerAddress().toString().c_str(), reason);
        NimBLEDevice::getScan()->start(bleScanTimeMs, false, true);
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
        //NimBLEDevice::getScan()->start(bleScanTimeMs, false, true);
    }
} scanCallbacks;


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
            NimBLEDevice::getScan()->start(bleScanTimeMs, false, true);
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
    pScan->start(bleScanTimeMs);
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
    pScan->start(bleScanTimeMs);
    TelnetPrint.printf("Scanning for peripherals\n");
}

/*
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

    // Set scan interval (how often) and window (how long) in milliseconds
    pBLEScan->setInterval(1500);
    pBLEScan->setWindow(500);

    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, scanEndedCB);
    //pBLEScan->clearResults();
#endif
}

// Define a class to handle the callbacks when advertisments are received
class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {

    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        TelnetPrint.print("Advertised Device found: ");
        TelnetPrint.println(advertisedDevice->toString().c_str());
        if(advertisedDevice->isAdvertisingService(serviceUUID))
        {
            TelnetPrint.println("Found Our Service");
            // stop scan before connecting
            NimBLEDevice::getScan()->stop();
            // Save the device reference in a global for the client to use
            myDevice = advertisedDevice;
            // Ready to connect now
            doConnect = true;
        }
    };
};
*/

/** Notification / Indication receiving handler callback */
static void notifyCallbackTwo(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length,bool isNotify) {
  TelnetPrint.printf("Notify callback for characteristic %s of data length %d\n",
          pBLERemoteCharacteristic->getUUID().toString().c_str(), length);
  hexDump((char*)pData, length);
  last_data_size = length;
  bleCollectPacket((char *)pData, length);
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
    //NimBLERemoteService*        pSvc = nullptr;
    NimBLERemoteCharacteristic* pChr = nullptr;
    NimBLERemoteDescriptor*     pDsc = nullptr;

    pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService) {
        pChr = pRemoteService->getCharacteristic(charUUID_rx);
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

void sendCommand(uint8_t *data, size_t dataLen)
{
  //https://github.com/FurTrader/OverkillSolarBMS/blob/master/Comm_Protocol_Documentation/BLE%20_bluetooth_protocol.md#executive-summary
  //TelnetPrint.print("Characteristics: ");
  //TelnetPrint.println(pRemoteService->toString().c_str());
  if(pRemoteService) {
    if(!pRemoteCharacteristic_overkill) {
      pRemoteCharacteristic_overkill = pRemoteService->getCharacteristic(charUUID_tx);
    }

    if (pRemoteCharacteristic_overkill) {
      TelnetPrint.println(getTimestamp() + " Write to Regular");
      if(!pRemoteCharacteristic_overkill->writeValue(data, dataLen, false)) {
        TelnetPrint.println("Unable to send command");
      }
    } else {
      str_ble_status += "Remote TX characteristic not found\n";
      TelnetPrint.println("Remote TX characteristic not found");
    }
  } else {
    str_ble_status += "pRemoteService is nullptr\n";
    TelnetPrint.println("pRemoteService is nullptr");    
  }
 
/*
  pRemoteCharacteristic_overkill = pRemoteService->getCharacteristic(charUUID_tx);
  if (pRemoteCharacteristic_overkill)
  {
    TelnetPrint.println(getTimestamp() + " Write to Regular");
    if(!pRemoteCharacteristic_overkill->writeValue(data, dataLen, false)) {
      TelnetPrint.println("Unable to send command");
    }
  }
  else
  {
    str_ble_status += "Remote TX characteristic not found\n";
    TelnetPrint.println("Remote TX characteristic not found");
  }
*/
}

void bleDisconnect() {
    pRemoteService = nullptr;
    pRemoteCharacteristic_overkill = nullptr;
    auto pClients = NimBLEDevice::getConnectedClients();
    for (auto& pClient : pClients) {
        TelnetPrint.printf("%s\n", pClient->toString().c_str());
        NimBLEDevice::deleteClient(pClient);
    }
}
