#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>

#include "json/Json.h"
#include "mqtt_object.h"

bool MQTT::mqtt_init = false;
static volatile bool running = true;

void* client_refresher(void* client)
{
	MQTT *_this = (MQTT*)client;

	while(running)
	{
		int rc = mosquitto_loop(_this->mosq, -1, 1);
		if(rc)
		{
			printf("MQTT: Connection error! Wait 10s...\n");
			sleep(10);
			mosquitto_reconnect(_this->mosq);
			mosquitto_subscribe(_this->mosq, NULL, _this->topic, 0);
		}
		sleep(1);
	}

	printf("MQTT: Refresher stopped\n");
	return NULL;
}


bool parse(char *jsonSource, const char *key, char *cityValue)
{
	int len = strlen ( jsonSource ) ;
	//printf("JSON(%d) %s:\n", len, jsonSource);

	Json json ( jsonSource, len);

	if ( json.isValidJson () )
	{
		//printf ( "Valid JSON: %s\n", jsonSource );

		if ( json.type (0) == JSMN_OBJECT )
		{
			//printf("Finding Key %s... \n", key);
			int keyIndex = json.findKeyIndexIn ( key, 0 );
			if ( keyIndex >= 0 )
			{
				int valueIndex = json.findChildIndexOf ( keyIndex, -1 );
				if ( valueIndex > 0 )
				{
					const char * valueStart  = json.tokenAddress ( valueIndex );
					int          valueLength = json.tokenLength ( valueIndex );
					strncpy ( cityValue, valueStart, valueLength );
					cityValue [ valueLength ] = 0; // NULL-terminate the string

					return true;
				}
			}
			else
				printf("Key does not exist..\n");

		}
		else
			printf("Invalid JSON.  ROOT element is not Object: %s\n", jsonSource);
	}
	else
		printf("Invalid JSON\n");

	return false;
}

void connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	printf("MQTT: connect callback, rc=%d\n", result);
	if(result == 0)
	{
		char topic[] = "/pi";
		char message[] = "online";

		mosquitto_publish(mosq, 0, topic, strlen(message), message, 0, 0);
	}
}

void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	printf("MQTT: got message '%.*s' for topic '%s'\n", message->payloadlen, (char*) message->payload, message->topic);
    if(MQTT::receivedCB)
    {
    	char pipeString[32];
    	int k = 0;
    	char *ptr = message->topic;
    	do {
    		ptr = strchr(ptr, '/');
    		if(ptr)
    		{
    			//printf("%d: %s\n",k , ptr);
    			if(k == 2)
    			{
    				strcpy(pipeString, ptr + 1);
    				break;
    			}
    			ptr++;
    			k++;
    		}
    	}while(ptr);

    	printf("pipe %s\n", pipeString);
    	int pipe = atoi(pipeString);

    	MQTT::receivedCB(pipe, (uint8_t*) message->payload, message->payloadlen + 1);
    }
}

bool (*MQTT::receivedCB)(int pipe, uint8_t *data, int len) = 0;

MQTT::MQTT(const char *topic, const char *addr, const char *port)
{
	strcpy(address, addr);
	strcpy(this->port, port);
	strcpy(this->topic, topic);

	if(!mqtt_init)
	{
		mqtt_init = true;
		mosquitto_lib_init();
	}

	mosq = mosquitto_new("janus_pi", true, 0);
	if(mosq)
	{
		mosquitto_connect_callback_set(mosq, connect_callback);
		mosquitto_message_callback_set(mosq, message_callback);
		mosquitto_username_pw_set(mosq, "janus", "Janus506");
		char will[] = "offline";
		mosquitto_will_set(mosq, "/pi", strlen(will), will, 0, 0);
		int rc = mosquitto_connect(mosq, addr, atoi(port), 60);
		if(rc)
		{
			printf("MQTT: Connection error! Wait 10s...\n");
		}
		mosquitto_subscribe(mosq, NULL, topic, 0);

	}

	/* start a thread to refresh the client (handle egress and ingree client traffic) */
	if(pthread_create(&client_daemon, NULL, client_refresher, this))
	{
		printf("MQTT: Failed to start client daemon.\n");
	}
}

MQTT::~MQTT()
{
	running = false;
	sleep(1);
	mosquitto_destroy(mosq);

	if (client_daemon != 0)
		pthread_cancel(client_daemon);
}

bool MQTT::publish(char *topic, char *application_message)
{
	mosquitto_publish(mosq, 0, topic, strlen(application_message), application_message, 0, 0);

	return true;
}
