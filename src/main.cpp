#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include "Utils/utils.h"
#include "RPi/bcm2835.h"
#include "RPi/RF24_arch_config.h"
#include "interface_nrf24.h"
#include "mqtt_object.h"

InterfaceNRF24 *nrf = 0;
MQTT *mq = 0;
uint8_t netAddress[] = {0x00, 0x44, 0x55};

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

typedef struct {
	uint32_t timestamp;		//4
	uint8_t inputs;			//1
	uint8_t outputs;		//1
	uint16_t voltages[4];	//8
	uint16_t temperature;	//2
}__attribute__((packed, aligned(4))) nodeData_s;

bool NRFreceivedCB(int pipe, uint8_t *data, int len)
{
	printf("RCV PIPE# %d\n", (int)pipe);
	printf(" PAYLOAD: %d\n", len);
	//diag_dump_buf(data, len);

	if(mq)
	{
		char topic[32];
		message[0] = 0;

		sprintf(topic, "/node/up/%d", pipe);

		nodeData_s up;
		memcpy(&up, data ,16);
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
	}
	fflush(stdout);

	return false;
}

bool MQTTreceivedCB(int pipe, uint8_t *data, int len)
{
	//printf("MQTT PIPE# %d\n", (int)pipe);
	//printf(" PAYLOAD: %d\n", len);
	//diag_dump_buf(data, len);

	uint8_t address[5];
	memcpy(address, netAddress, 5);
	address[0] = pipe;

		time_t now = time(0);
		struct tm *tm_now = localtime(&now);

	if(!strcmp((const char*)data, "report"))
	{
		printf("packing report frame %s %d:%d\n", asctime(tm_now), tm_now->tm_hour, tm_now->tm_min);

		nodeData_s down;
		memset(&down, 0, sizeof(down));
		down.timestamp = (tm_now->tm_hour << 8) | tm_now->tm_min;

		if(nrf)
		nrf->transmit(address, (uint8_t*)&down, 16);

		fflush(stdout);
	}

	if(!strcmp((const char*)data, "light"))
	{
		printf("Request %d to switch lights on\n", pipe);

		nodeData_s down;
		memset(&down, 0, sizeof(down));
		down.timestamp = (tm_now->tm_hour << 8) | tm_now->tm_min;
		down.outputs = 0x01; //switch lights on

		if(nrf)
		nrf->transmit(address, (uint8_t*)&down, 16);
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
			running = nrf->run();


		usleep(10000);
	}

	if(nrf)
		delete nrf;

	if(mq)
		delete mq;

	fflush(stdout);
	return 0;
}
