//  ----- BLE stuff -----
//static NimBLEClient* pClient = nullptr;
static NimBLERemoteService *pRemoteService = nullptr;
static NimBLERemoteCharacteristic *pRemoteCharacteristic_overkill = nullptr;
//static NimBLERemoteCharacteristic *pChr = nullptr;

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
    void onConnect(NimBLEClient* oneClient) override { TelnetPrint.printf("Connected\n"); }

    void onDisconnect(NimBLEClient* oneClient, int reason) override {
        TelnetPrint.printf("%s Disconnected, reason = %d - Starting scan\n", oneClient->getPeerAddress().toString().c_str(), reason);
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
            //doConnect = true;
        }
        if(BLE_client_connected || advDevice) {
          TelnetPrint.println("Stopping scan because we are connected already");
          NimBLEDevice::getScan()->stop();
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
        TelnetPrint.printf("BLE not connected. Let's try\n");
        //doConnect = false;
        /** Found a device we want to connect to, do it now */
        if (connectToServer()) {
            BLE_client_connected = true;
            str_ble_status += getTimestamp() + " - Connected\n";
            TelnetPrint.printf("Success! we should now be getting notifications now!\n");
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
            TelnetPrint.println("Special - No advDevice");
            return false;          
        }
        pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
        if (pClient) {
            if(!pClient->isConnected()) {
                if (!pClient->connect(advDevice, false)) {
                    TelnetPrint.println("Reconnect failed");
                    return false;
                } else {
                    TelnetPrint.println("Reconnected succeeded");
                }
            } else {
                TelnetPrint.println("Client already connected");
            }
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
            TelnetPrint.println("Max clients reached - no more connections available");
            return false;
        }

        pClient = NimBLEDevice::createClient();

        TelnetPrint.println("New client created");

        pClient->setClientCallbacks(&clientCallbacks, false);
        /**
         *  Set initial connection parameters:
         *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
         *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
         *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 150 * 10ms = 1500ms timeout
         */
        pClient->setConnectionParams(12, 12, 0, 150);

        /** Set how long we are willing to wait for the connection to complete (milliseconds), default is 30000. */
        pClient->setConnectTimeout(10 * 1000);

        if (!advDevice) {
            TelnetPrint.println("No advDevice");
            return false;
        }
        if (!pClient->connect(advDevice)) {
            /** Created a client but failed to connect, don't need to keep it as it has no data */
            NimBLEDevice::deleteClient(pClient);
            TelnetPrint.println("Failed to connect, deleted client");
            return false;
        }
    }

    if (!pClient->isConnected()) {
        if (!advDevice) {
            TelnetPrint.println("No advDevice");
            return false;
        }
        if (!pClient->connect(advDevice)) {
            TelnetPrint.println("Failed to connect");
            return false;
        }
    }

    TelnetPrint.printf("Connected to: %s RSSI: %d\n", pClient->getPeerAddress().toString().c_str(), pClient->getRssi());

    /** Now we can read/write/subscribe the characteristics of the services we are interested in */
    //NimBLERemoteService*        pSvc = nullptr;
    NimBLERemoteCharacteristic* pChr = nullptr;
    //NimBLERemoteDescriptor*     pDsc = nullptr;

    pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService) {
        pChr = pRemoteService->getCharacteristic(charUUID_rx);
        pRemoteCharacteristic_overkill = pRemoteService->getCharacteristic(charUUID_tx);
    } else {
        TelnetPrint.println("No pRemoteService");
    }

    if (pChr) {
        if (pChr->canRead()) {
            TelnetPrint.printf("%s Value: %s\n", pChr->getUUID().toString().c_str(), pChr->readValue().c_str());
        }
        TelnetPrint.println("Asdf");
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
        //pChr->subscribe(true, notifyCallbackTwo);
        //pChr->subscribe(false, notifyCallbackTwo);

        if (pChr->canNotify()) {
            TelnetPrint.println("canNotify");
            if (!pChr->subscribe(true, notifyCallbackTwo)) {
                TelnetPrint.println("canNotify->disconnect");
                pClient->disconnect();
                return false;
            }
        } else if (pChr->canIndicate()) {
            TelnetPrint.println("canIndicate");
            // Send false as first argument to subscribe to indications instead of notifications
            if (!pChr->subscribe(false, notifyCallbackTwo)) {
                TelnetPrint.println("canIndicate->disconnect");
                pClient->disconnect();
                return false;
            }
        }
        
    } else {
        TelnetPrint.println("DEAD service not found.");
    }


    TelnetPrint.printf("Done with this device!\n");
    return true;
}

void sendCommand(uint8_t *data, size_t dataLen)
{
  //https://github.com/FurTrader/OverkillSolarBMS/blob/master/Comm_Protocol_Documentation/BLE%20_bluetooth_protocol.md#executive-summary
  //TelnetPrint.print("Characteristics: ");
  //TelnetPrint.println(pRemoteService->toString().c_str());
  //if(!pRemoteService && pClient) {
  //  TelnetPrint.println("pClient->getService");
  //  pRemoteService = pClient->getService(serviceUUID);
  //}
  if(!pRemoteCharacteristic_overkill) {
    if(!pRemoteService) {
      pRemoteCharacteristic_overkill = pRemoteService->getCharacteristic(charUUID_tx);
    } else {
      str_ble_status += "pRemoteService is nullptr\n";
      TelnetPrint.println("pRemoteService is nullptr");    
    }
  }

  if (pRemoteCharacteristic_overkill) {
    TelnetPrint.println(getTimestamp() + " Write to Regular");
    hexDump((char*)data, dataLen);
    //TelnetPrint.println(getTimestamp() + data + " " + dataLen);
    if(!pRemoteCharacteristic_overkill->writeValue(data, dataLen, false)) {
      TelnetPrint.println("Unable to send command");
    }
  } else {
    str_ble_status += "Remote TX characteristic not found\n";
    TelnetPrint.println("Remote TX characteristic not found");
  }
 

}

void bleDisconnect() {
    BLE_client_connected = false;
    pRemoteService = nullptr;
    pRemoteCharacteristic_overkill = nullptr;
    //pChr = nullptr;
    auto pClients = NimBLEDevice::getConnectedClients();
    for (auto& oneClient : pClients) {
        TelnetPrint.printf("%s\n", oneClient->toString().c_str());
        NimBLEDevice::deleteClient(oneClient);
    }
}
