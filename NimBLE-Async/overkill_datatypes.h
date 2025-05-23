/*
 * IncFile1.h
 *
 * Created: 6.3.2019 11:58:57
 *  Author: z003knyu
 */

#include <stdlib.h>

//#define unsigned char byte

#ifndef overkill_datatypes_H_
#define overkill_datatypes_H_

typedef struct
{
	byte start;
	byte type;
	byte status;
	byte dataLen;
} bmsPacketHeaderStruct;

typedef struct
{
	uint16_t Volts; // unit 1mV
	int32_t Amps;   // unit 1mA
	int32_t Watts;   // unit 1W
	uint32_t CapacityRemainAh;
	uint32_t CapacityAh;
	uint8_t Cycles;
	uint16_t CapacityRemainPercent; //unit 1%
	uint32_t CapacityRemainWh; 	//unit Wh
	uint16_t Temp1;				   //unit 0.1C
	uint16_t Temp2;				   //unit 0.1C
	uint16_t BalanceCodeLow;
	uint16_t BalanceCodeHigh;
	uint8_t MosfetStatus;
	uint16_t CurrentErrors;
	uint8_t SoftwareVersion;
	
} packBasicInfoStruct;

typedef struct
{
	uint8_t NumOfCells;
	uint16_t CellVolt[15]; //cell 1 has index 0 :-/
	uint16_t CellMax;
	uint16_t CellMin;
	uint16_t CellDiff; // difference between highest and lowest
	uint16_t CellAvg;
	uint16_t CellMedian;
	uint32_t CellColor[15];
	uint32_t CellColorDisbalance[15]; // green cell == median, red/violet cell => median + c_cellMaxDisbalance
} packCellInfoStruct;

struct packEepromStruct
{
	uint16_t POVP;
	uint16_t PUVP;
	uint16_t COVP;
	uint16_t CUVP;
	uint16_t POVPRelease;
	uint16_t PUVPRelease;
	uint16_t COVPRelease;
	uint16_t CUVPRelease;
	uint16_t CHGOC;
	uint16_t DSGOC;
};

const int32_t c_cellNominalVoltage = 3700; //mV

const uint16_t c_cellAbsMin = 3000;
const uint16_t c_cellAbsMax = 4200;

const int32_t c_packMaxWatt = 1250;

const uint16_t c_cellMaxDisbalance = 1500; //200; // cell different by this value from cell median is getting violet (worst) color

#endif /* overkill_datatypes_H_ */
