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
	uint8_t sendbuf[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
	uint8_t recvbuf[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */

public:
	MQTT(const char *topic, const char *addr, const char *port);
	virtual ~MQTT();

	bool publish(const char *topic, char *application_message);
};

#endif /* MQTT_OBJECT_H_ */
