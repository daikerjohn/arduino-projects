bool isPacketValid(byte *packet) //check if packet is valid
{
    if (packet == nullptr)
    {
        return false;
    }

    bmsPacketHeaderStruct *pHeader = (bmsPacketHeaderStruct *)packet;
    int checksumLen = pHeader->dataLen + 2; // status + data len + data

    if (pHeader->start != 0xDD)
    {
      str_ble_status += "packet did not start with 0xDD";
        return false;
    }

    int offset = 2; // header 0xDD and command type are skipped

    byte checksum = 0;
    for (int i = 0; i < checksumLen; i++)
    {
        checksum += packet[offset + i];
    }

    //printf("checksum: %x\n", checksum);

    checksum = ((checksum ^ 0xFF) + 1) & 0xFF;
    //printf("checksum v2: %x\n", checksum);

    byte rxChecksum = packet[offset + checksumLen + 1];

    if (checksum == rxChecksum)
    {
        //printf("Packet is valid\n");
        return true;
    }
    else
    {
        //printf("Packet is not valid\n");
        //printf("Expected value: %x\n", rxChecksum);
        return false;
    }
}

bool processBasicInfo(packBasicInfoStruct *output, byte *data, unsigned int dataLen)
{
  // https://github.com/FurTrader/OverkillSolarBMS/blob/master/Comm_Protocol_Documentation/JBD_REGISTER_MAP.md
  TelnetPrint.printf("dataLen: 0x%x\n", dataLen);
  // Expected data len
  if (dataLen != 0x1D && dataLen != 0x1B)
  {
    return false;
  }
  TelnetPrint.println("processing basic!");

  output->Volts = ((uint32_t)two_ints_into16(data[0], data[1])) * 10; // Resolution 10 mV -> convert to milivolts   eg 4895 > 48950mV
  output->Amps = ((int32_t)two_ints_into16(data[2], data[3])) * 10;   // Resolution 10 mA -> convert to miliamps

  output->Watts = output->Volts * output->Amps / 1000000; // W

  output->CapacityAh = ((uint32_t)two_ints_into16(data[6], data[7])) * 10;
  output->CapacityRemainAh = ((uint32_t)two_ints_into16(data[4], data[5])) * 10;
  output->CapacityRemainPercent = ((uint16_t)data[19]);
  output->Cycles = ((uint8_t)two_ints_into16(data[8], data[9]));

  //output->ManufactureDate = ((uint16_t)two_ints_into16(data[10], data[11]));  //bits 15:9=year - 2000    bits 8:5=month    bits 4:0=Day

  //output->CurrentErrors = ((uint16_t)two_ints_into16(data[16], data[17]));  // CurrentErrors

  //output->SoftwareVersion = ((byte)data[18]);  // Software Version
  //output->CellCount = ((byte)data[21]);  // Number of Cells
  //output->NtcCount = ((byte)data[22]);  // Number of NTC

  output->CapacityRemainWh = (output->CapacityRemainAh * c_cellNominalVoltage) / 1000000 * packCellInfo.NumOfCells;

  output->Temp1 = (((uint16_t)two_ints_into16(data[23], data[24])) - 2731);
  output->Temp2 = (((uint16_t)two_ints_into16(data[25], data[26])) - 2731);
  output->BalanceCodeLow = (two_ints_into16(data[12], data[13]));
  output->BalanceCodeHigh = (two_ints_into16(data[14], data[15]));
  output->MosfetStatus = ((byte)data[20]);

  return true;
};

bool processCellInfo(packCellInfoStruct *output, byte *data, unsigned int dataLen)
{
    uint16_t _cellSum = 0;
    uint16_t _cellMin = 5000;
    uint16_t _cellMax = 0;
    //uint16_t _cellAvg;
    //uint16_t _cellDiff;

    output->NumOfCells = dataLen / 2; // Data length * 2 is number of cells !!!!!!

    //go trough individual cells
    for (byte i = 0; i < dataLen / 2; i++)
    {
        output->CellVolt[i] = ((uint16_t)two_ints_into16(data[i * 2], data[i * 2 + 1])); // Resolution 1 mV
        _cellSum += output->CellVolt[i];
        if (output->CellVolt[i] > _cellMax)
        {
            _cellMax = output->CellVolt[i];
        }
        if (output->CellVolt[i] < _cellMin)
        {
            _cellMin = output->CellVolt[i];
        }

    }
    output->CellMin = _cellMin;
    output->CellMax = _cellMax;
    output->CellDiff = _cellMax - _cellMin; // Resolution 10 mV -> convert to volts
    output->CellAvg = _cellSum / output->NumOfCells;

    //----cell median calculation----
    uint16_t n = output->NumOfCells;
    uint16_t i, j;
    uint16_t temp;
    uint16_t x[n];

    for (uint8_t u = 0; u < n; u++)
    {
        x[u] = output->CellVolt[u];
    }

    for (i = 1; i <= n; ++i) //sort data
    {
        for (j = i + 1; j <= n; ++j)
        {
            if (x[i] > x[j])
            {
                temp = x[i];
                x[i] = x[j];
                x[j] = temp;
            }
        }
    }

    if (n % 2 == 0) //compute median
    {
        output->CellMedian = (x[n / 2] + x[n / 2 + 1]) / 2;
    }
    else
    {
        output->CellMedian = x[n / 2 + 1];
    }

    //for (uint8_t q = 0; q < output->NumOfCells; q++)
    //{
    //    uint32_t disbal = abs(output->CellMedian - output->CellVolt[q]);
    //}
    return true;
};

bool bmsProcessPacket(byte *packet)
{
    bool isValid = isPacketValid(packet);

    if (isValid != true)
    {
        TelnetPrint.println("Invalid packet received");
        //str_ble_status += "Packet is invalid\n";
        return false;
    }
    last_data_capture_bms = getTimestamp();

    bmsPacketHeaderStruct *pHeader = (bmsPacketHeaderStruct *)packet;
    byte *data = packet + sizeof(bmsPacketHeaderStruct); // TODO Fix this ugly hack
    unsigned int dataLen = pHeader->dataLen;

    bool result = false;

    // |Decision based on pac ket type (info3 or info4)
    switch (pHeader->type)
    {
    case cBasicInfo3:
    {
        TelnetPrint.println("Process basic info");
        result = processBasicInfo(&packBasicInfo, data, dataLen);
        newPacketReceived = true;
        break;
    }

    case cCellInfo4:
    {
        TelnetPrint.println("Process cell info");
        result = processCellInfo(&packCellInfo, data, dataLen);
        newPacketReceived = true;
        break;
    }

    default:
        result = false;
        TelnetPrint.printf("Unsupported packaet type %x\n", pHeader->type);
              //str_ble_status += "Unsupported packet type detected." + String(pHeader->type);
        //break;
        //commSerial.printf("Unsupported packet type detected. Type: %d", pHeader->type);
    }

    return result;
}

bool bleCollectPacket(char *data, uint32_t dataSize) // reconstruct packet from BLE incomming data, called by notifyCallback function
{
    static uint8_t packetstate = 0; //0 - empty, 1 - first half of packet received, 2- second half of packet received
    static uint8_t packetbuff[40] = {0x0};
    static uint32_t previousDataSize = 0;
    bool retVal = false;
    //hexDump(data,dataSize);

    if (data[0] == 0xdd && packetstate == 0) // probably got 1st half of packet
    {
        packetstate = 1;
        previousDataSize = dataSize;
        for (uint8_t i = 0; i < dataSize; i++)
        {
            packetbuff[i] = data[i];
        }
        retVal = false;
    }

    if (data[dataSize - 1] == 0x77 && packetstate == 1) //probably got 2nd half of the packet
    {
        packetstate = 2;
        for (uint8_t i = 0; i < dataSize; i++)
        {
            packetbuff[i + previousDataSize] = data[i];
        }
        retVal = false;
    }

    if (packetstate == 2) //got full packet
    {
        uint8_t packet[dataSize + previousDataSize];
        memcpy(packet, packetbuff, dataSize + previousDataSize);

        bmsProcessPacket(packet); //pass pointer to retrieved packet to processing function
        packetstate = 0;
        retVal = true;
        lastSuccessfulBluetooth = millis();
    }
    return retVal;
}
void bmsGetInfo3()
{
    // header status command length data checksum footer
    //                   DD    A5    03    00    FF    FD    77
    uint8_t data[7] = {0xdd, 0xa5, 0x03, 0x00, 0xff, 0xfd, 0x77};
    //bmsSerial.write(data, 7);
    TelnetPrint.println(getTimestamp() + " Sending info3 request");
    sendCommand(data, sizeof(data));
    TelnetPrint.println(getTimestamp() + " Request info3 sent");
}

void bmsGetInfo4()
{
    //                   DD    A5    04    00    FF    FC    77
    uint8_t data[7] = {0xdd, 0xa5, 0x04, 0x00, 0xff, 0xfc, 0x77};
    //bmsSerial.write(data, 7);
    TelnetPrint.println(getTimestamp() + " Sending info4 request");
    sendCommand(data, sizeof(data));
    TelnetPrint.println(getTimestamp() + " Request info4 sent");
}

/*
void printBasicInfo() //debug all data to uart
{
    TRACE;
    commSerial.printf("Total voltage: %f\n", (float)packBasicInfo.Volts / 1000);
    commSerial.printf("Amps: %f\n", (float)packBasicInfo.Amps / 1000);
    commSerial.printf("Watts: %f\n", (float)packBasicInfo.Watts); /// 1000);
    commSerial.printf("CapacityAh: %f\n", (float)packBasicInfo.CapacityAh / 1000);
    commSerial.printf("CapacityRemainAh: %f\n", (float)packBasicInfo.CapacityRemainAh / 1000);
    commSerial.printf("CapacityRemainPercent: %f\n", (float)packBasicInfo.CapacityRemainPercent);
    commSerial.printf("Cycles: %f\n", (float)packBasicInfo.Cycles);
    commSerial.printf("Temp1: %f\n", (float)packBasicInfo.Temp1 / 10);
    commSerial.printf("Temp2: %f\n", (float)packBasicInfo.Temp2 / 10);
    commSerial.printf("Balance Code Low: 0x%x\n", packBasicInfo.BalanceCodeLow);
    commSerial.printf("Balance Code High: 0x%x\n", packBasicInfo.BalanceCodeHigh);
    commSerial.printf("Mosfet Status: 0x%x\n", packBasicInfo.MosfetStatus);
}

void printCellInfo() //debug all data to uart
{
    TRACE;
    commSerial.printf("Number of cells: %u\n", packCellInfo.NumOfCells);
    for (byte i = 1; i <= packCellInfo.NumOfCells; i++)
    {
        commSerial.printf("Cell no. %u", i);
        commSerial.printf("   %f\n", (float)packCellInfo.CellVolt[i - 1] / 1000);
    }
    commSerial.printf("Max cell volt: %f\n", (float)packCellInfo.CellMax / 1000);
    commSerial.printf("Min cell volt: %f\n", (float)packCellInfo.CellMin / 1000);
    commSerial.printf("Difference cell volt: %f\n", (float)packCellInfo.CellDiff / 1000);
    commSerial.printf("Average cell volt: %f\n", (float)packCellInfo.CellAvg / 1000);
    commSerial.printf("Median cell volt: %f\n", (float)packCellInfo.CellMedian / 1000);
    commSerial.println();
}
*/

void hexDump(const char *data, uint32_t dataSize) //debug function
{
    TelnetPrint.printf("HEX data(%d):\n", dataSize);

    for (int i = 0; i < dataSize; i++)
    {
        TelnetPrint.printf("0x%x, ", data[i]);
        //commSerial.printf("0x%x, ", data[i]);
    }
    TelnetPrint.println("");
    //commSerial.println("");
}

int16_t two_ints_into16(int highbyte, int lowbyte) // turns two bytes into a single long integer
{
    int16_t result = (highbyte);
    result <<= 8;                //Left shift 8 bits,
    result = (result | lowbyte); //OR operation, merge the two
    return result;
}

