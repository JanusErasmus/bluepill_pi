#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "RPi/bcm2835.h"
#include "RPi/RF24_arch_config.h"
#include "interface_nrf24.h"
#include "mqtt_object.h"


//void exit_example(int status, int sockfd, pthread_t *client_daemon)
//{
//    if (sockfd != -1) close(sockfd);
//    if (client_daemon != NULL) pthread_cancel(*client_daemon);
//    exit(status);
//}

int main()
{
	printf("NRF24 listener\n");

	uint8_t netAddress[] = {0x00, 0x44, 0x55};
	InterfaceNRF24::init(netAddress, 3);

	const char *topic = "/node/#";
	const char *addr = "160.119.253.3";
	const char *port = "1883";
	MQTT mq(topic, addr, port);


	while(1)
	{
		printf("Run\n");
		InterfaceNRF24::get()->run();
//		usleep(100000);
	mq.publish("/pi", "object hi");
		sleep(5);
	}
	return 0;
}
