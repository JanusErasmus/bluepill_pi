/*
 * mqtt.h
 *
 *  Created on: 19 Jul 2018
 *      Author: janus
 */

#ifndef MQTT_OBJECT_H_
#define MQTT_OBJECT_H_

#include "mqtt/mqtt.h"

class MQTT
{
	struct mqtt_client client;
	pthread_t client_daemon;



public:
	int sockfd;
	char address[128];
	char port[8];
	char topic[128];
	uint8_t sendbuf[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
	uint8_t recvbuf[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */

	static bool (*receivedCB)(int pipe, uint8_t *data, int len);
	MQTT(const char *topic, const char *addr, const char *port);
	virtual ~MQTT();

	void checkStatus();

	void setRXcb(bool(cb(int pipe, uint8_t *data, int len))){ receivedCB = cb; }

	bool publish(char *topic, char *application_message);
};

#endif /* MQTT_OBJECT_H_ */
