//  ----- BLE stuff -----
static BLERemoteCharacteristic *pRemoteCharacteristic;
static BLEAdvertisedDevice *myDevice;
BLERemoteService *pRemoteService;
// The remote service we wish to connect to. Needs check/change when other BLE module used.
static BLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms original module
static BLEUUID charUUID_tx("0000ff02-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms original module
static BLEUUID charUUID_rx("0000ff01-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms original module
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
        str_ble_status += getTimestamp() + " - onResult " + advertisedDevice->toString().c_str() + "\n";

        // We have found a device, let us now see if it contains the service we are looking for.
        if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(serviceUUID))
        {

            NimBLEDevice::getScan()->stop();
            myDevice = advertisedDevice;
            doConnect = true;
            doScan = true;

        } // Found our server
    }     // onResult
};        // MyAdvertisedDeviceCallbacks
class MyClientCallback : public BLEClientCallbacks
{ //this is called on connect / disconnect by some underlying magic+

    void onConnect(BLEClient *pclient)
    {
      str_ble_status += getTimestamp() + " - onConnect\n";
    }

    void onDisconnect(BLEClient *pclient)
    {
        BLE_client_connected = false;
        str_ble_status += getTimestamp() + " - onDisconnect\n";
        //commSerial.println("onDisconnect");
    }
};

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
            str_ble_status += getTimestamp() + " - Connected\n";
            doConnect = false;
        }
    }

    // If we are connected to a peer BLE Server, update the characteristic each time we are reached
    // with the current time since boot.
    if (BLE_client_connected == true)
    {

        unsigned long currentMillis = millis();
        if ((currentMillis - previousMillis >= interval || newPacketReceived)) //every time period or when packet is received
        {
            previousMillis = currentMillis;

            if (toggle) //alternate info3 and info4
            {
                bmsGetInfo3();
                //showBasicInfo();
                newPacketReceived = false;
            }
            else
            {
                bmsGetInfo4();
                //showCellInfo();
                newPacketReceived = false;
            }
            toggle = !toggle;
        }
        last_data_capture = getTimestamp();
    }
    else if (doScan)
    {
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
    str_ble_status += getTimestamp() + " - BLEDevice::init('')\n";

    // Retrieve a Scanner and set the callback we want to use to be informed when we
    // have detected a new device.  Specify that we want active scanning and start the
    // scan to run for 5 seconds.
    NimBLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, true);
    pBLEScan->clearResults();
#endif
}

static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) //this is called when BLE server sents data via notofication
{
    TRACE;
    //hexDump((char*)pData, length);
    bleCollectPacket((char *)pData, length);
}

bool connectToServer()
{
    TRACE;
    //commSerial.print("Forming a connection to ");
    //commSerial.println(myDevice->getAddress().toString().c_str());
    pClient = NimBLEDevice::createClient();
    //commSerial.println(" - Created client");
    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remote BLE Server.
    pClient->connect(myDevice); // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    //commSerial.println(" - Connected to server");
    // Obtain a reference to the service we are after in the remote BLE server.
    //BLERemoteService*
    pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr)
    {
        //commSerial.print("Failed to find our service UUID: ");
        //commSerial.println(serviceUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    //commSerial.println(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_rx);
    if (pRemoteCharacteristic == nullptr)
    {
        //commSerial.print("Failed to find our characteristic UUID: ");
        //commSerial.println(charUUID_rx.toString().c_str());
        pClient->disconnect();
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
        pRemoteCharacteristic->subscribe(true, notifyCallback);
    }

    BLE_client_connected = true;
    return BLE_client_connected;
}

void sendCommand(uint8_t *data, size_t dataLen)
{
    TRACE;

    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_tx);

    if (pRemoteCharacteristic)
    {
        pRemoteCharacteristic->writeValue(data, dataLen, false);
        //commSerial.println("bms request sent");
    }
    else
    {
        //commSerial.println("Remote TX characteristic not found");
    }
}
