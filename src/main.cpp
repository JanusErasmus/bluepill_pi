#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include "Utils/utils.h"
#include "Utils/crc.h"
#include "RPi/bcm2835.h"
#include "RPi/RF24_arch_config.h"
#include "interface_nrf24.h"
#include "mqtt_object.h"

#define STREET_NODE_ADDRESS     0x00
#define UPS_NODE_ADDRESS        0x01
#define LIVING_NODE_ADDRESS     0x02
#define FERMENTER_NODE_ADDRESS  0x03
#define HOUSE_NODE_ADDRESS      0x04
#define GARAGE_NODE_ADDRESS     0x05
#define WATER_NODE_ADDRESS      0x06


InterfaceNRF24 *nrf = 0;
MQTT *mq = 0;

uint8_t netAddress[] =  {0x12, 0x3B, 0x45};
uint8_t nodeAddress[] = {0x23, 0x1B, 0x25};

static volatile bool running = 1;

void sig_handler(int signo)
{
	printf("Signal: ");
	switch(signo)
	{
	case SIGINT:
		printf(" - SIGINT\n");
		break;
	default:
		printf(" - %d\n", signo);
		break;
	}
	running = 0;
}


void catHEXstring(uint8_t *data, int len, char *string)
{
	for (int k = 0; k < len; ++k)
	{
		char hex[8];
		sprintf(hex, "%02X", data[k]);
		strcat(string, hex);
	}
}

char message[256];

enum nodeFrameType_e
{
	DATA = 0,
	COMMAND = 1,
	ACKNOWLEDGE = 2
};

//Node send 32 bytes of data, with the last byte being the 8-bit CRC
typedef struct {
	uint8_t nodeAddress;	//1
	uint8_t frameType;		//1
	uint32_t timestamp;		//4  6
	uint8_t inputs;			//1  7
	uint8_t outputs;		//1  8
	uint16_t voltages[4];	//8  16
	uint16_t temperature;	//2  18
	uint8_t reserved[13]; 	//13 31
	uint8_t crc;			//1  32
}__attribute__((packed, aligned(4))) nodeData_s;

uint8_t ackNodeAddress = 0xFF;
static int CBfailure = 0;

bool NRFreceivedCB(int pipe, uint8_t *data, int len)
{
	nodeData_s up;
	memcpy(&up, data ,32);

	if(CRC_8::crc(data, 32))
	{
		printf(RED("Main: CRC error\n"));

		if(CBfailure++ > 20)
		{
			printf("Something fucky with CRC\n");
			fflush(stdout);
			return false;
		}
	}
	CBfailure = 0;

	nodeData_s down;
	memcpy(&down, data, len);

	if(down.frameType == ACKNOWLEDGE)
	{
		printf(GREEN("ACK\n"));
		return true;
	}

	printf("Main: RCV NODE# %d\n", (int)up.nodeAddress);
	//printf(" PAYLOAD: %d\n", len);
	//diag_dump_buf(data, len);

	//This is a funky node address, that should not exist
	if(up.nodeAddress == 96)
	{
		printf("Received from node 96, STOP\n");
		return false;
	}

	if(mq)
	{
		char topic[32];
		message[0] = 0;

		sprintf(topic, "/node/up/%d", up.nodeAddress);

		sprintf(message, "{\"uptime\":%d,"
				"\"inputs\":%d,"
				"\"outputs\":%d,"
				"\"voltages\":[%d,%d,%d,%d],"
				"\"temperature\":%d",
				up.timestamp,
				up.inputs,
				up.outputs,
				up.voltages[0],
				up.voltages[1],
				up.voltages[2],
				up.voltages[3],
				up.temperature
				);

		strcat(message, "}");
		//printf("Message: %d\n", strlen(message) + 1);
		//diag_dump_buf(message, strlen(message) + 1);
		mq->publish(topic, message);

		ackNodeAddress = up.nodeAddress;
	}
	fflush(stdout);

	return true;
}

bool MQTTreceivedCB(int pipe, uint8_t *data, int len)
{
	printf("MQTT PIPE# %d\n", (int)pipe);
	printf(" PAYLOAD: %d\n", len);
	diag_dump_buf(data, len);

	time_t now = time(0);
	struct tm *tm_now = localtime(&now);

	nodeData_s down;
	memset(&down, 0, sizeof(down));
	down.timestamp = ((tm_now->tm_mon + 1) << 24) | (tm_now->tm_mday << 16) | (tm_now->tm_hour << 8) | tm_now->tm_min;

	if(!strcmp((const char*)data, "report"))
	{
		printf("Packing report frame %s", asctime(tm_now), tm_now->tm_mon, tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min);

		down.nodeAddress = 0xFF;//broadcast address
		down.frameType = DATA;
		down.crc = CRC_8::crc((uint8_t*)&down, 31);
		if(nrf)
			nrf->transmit(nodeAddress, (uint8_t*)&down, 32);

		fflush(stdout);
	}

	if(!strncmp((const char*)data, "l", 1))
	{

		int outputs = atoi((const char*)&data[2]);
		printf("Request to switch lights to: %02X\n", outputs);

		down.nodeAddress = LIVING_NODE_ADDRESS;
		down.frameType = COMMAND;
		down.outputs = outputs;
		down.crc = CRC_8::crc((uint8_t*)&down, 31);

		if(nrf)
			nrf->transmit(nodeAddress, (uint8_t*)&down, 32);
		fflush(stdout);
	}

	if(!strncmp((const char*)data, "s", 1))
	{
		int setpoint = atoi((const char*)&data[2]);
		printf("Request %d to set set-point to: %d\n", pipe, setpoint);

		down.nodeAddress = FERMENTER_NODE_ADDRESS;
		down.frameType = COMMAND;
		down.voltages[0] = setpoint;
		down.crc = CRC_8::crc((uint8_t*)&down, 31);

		if(nrf)
			nrf->transmit(nodeAddress, (uint8_t*)&down, 32);
		fflush(stdout);
	}

	if(!strncmp((const char*)data, "w", 1))
	{
		int setpoint = atoi((const char*)&data[2]);
		printf("Request %d to %s valve\n", pipe, setpoint?"open":"close");

		down.nodeAddress = WATER_NODE_ADDRESS;
		down.frameType = COMMAND;
		down.outputs = setpoint;
		down.crc = CRC_8::crc((uint8_t*)&down, 31);

		if(nrf)
			nrf->transmit(nodeAddress, (uint8_t*)&down, 32);
		fflush(stdout);
	}

	return false;
}

int main()
{
	printf("NRF24 listener\n");

	  if (signal(SIGINT, sig_handler) == SIG_ERR)
		  printf("\ncan't catch SIGINT\n");

	nrf = new InterfaceNRF24(netAddress, 3);
	if(!nrf->isInitialized())
	{
		printf("Could NOT initialize NRF24 radio\n");
		delete nrf;
		return -1;
	}
	nrf->setRXcb(NRFreceivedCB);

	const char *topic = "\/node\/down\/+";
	const char *addr = "160.119.253.3";
	const char *port = "1883";
	mq = new MQTT(topic, addr, port);
	mq->setRXcb(MQTTreceivedCB);

	fflush(stdout);
	while(running)
	{
		if(nrf)
		{
			running = nrf->run();

//			if(ackNodeAddress != 0xFF)
//			{
//				sleep(1);
//				nodeData_s down;
//				down.nodeAddress = ackNodeAddress;
//				down.frameType = ACKNOWLEDGE;
//				down.crc = CRC_8::crc((uint8_t*)&down, 31);
//				printf("Main: Sending ack to 0x%02X\n", ackNodeAddress);
//				fflush(stdout);
//				nrf->transmit(nodeAddress, (uint8_t*)&down, 32);
//
//				ackNodeAddress = 0xFF;
//			}

		}

		usleep(10000);
	}

	if(nrf)
		delete nrf;

	if(mq)
		delete mq;

	printf("DONE\n");
	fflush(stdout);
	return 0;
}
