#include "mqtt_object.h"
#include "mqtt/templates/posix_sockets.h"
#include "json/Json.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>

void* client_refresher(void* client)
{
	int ping = 0;
    while(1)
    {
    	MQTTErrors result = mqtt_sync((struct mqtt_client*) client);
    	if (result != MQTT_OK)
    	{
    		printf("MQTT_sync %p KO, error: %s\n", client, mqtt_error_str(result));
    		sleep(5);
    	}

        //ping every 10 minutes (20 * 0.5 * 60)
        if(ping++ > 120)
        {
        	ping = 0;
        	result = mqtt_ping((struct mqtt_client*) client);
        	if (result != MQTT_OK)
        	{
        		printf("MQTT_ping %p KO, error: %s\n", client, mqtt_error_str(result));
        		sleep(5);
        	}
        }
        usleep(500000U); //every 500ms
    }
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

void publish_callback(void** unused, struct mqtt_response_publish *published)
{
    /* note that published->topic_name is NOT null-terminated (here we'll change it to a c-string) */
    char* topic_name = (char*) malloc(published->topic_name_size + 1);
    memcpy(topic_name, published->topic_name, published->topic_name_size);
    topic_name[published->topic_name_size] = '\0';

    uint8_t payload[128];
    memset(payload, 0, 128);
    memcpy(payload, published->application_message, published->application_message_size);

    printf("Received publish('%s'): %s\n", topic_name, payload);

    if(MQTT::receivedCB)
    {
    	char pipeString[32];
    	int k = 0;
    	char *ptr = topic_name;
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

    	//printf("pipe %s\n", pipeString);
    	int pipe = atoi(pipeString);

    	MQTT::receivedCB(pipe, payload, published->application_message_size + 1);
    }

    free(topic_name);
}

void reconnect(struct mqtt_client* client, void** arg)
{
	MQTT *_this = (MQTT*)*arg;

	printf("reconnect to %s:%s\n", _this->address, _this->port);


	/* setup a client */
	if(_this->sockfd != -1)
	{
		close(_this->sockfd);
		_this->sockfd = -1;
	}

	/* open the non-blocking TCP socket (connecting to the broker) */
	while(_this->sockfd == -1)
	{
		_this->sockfd = open_nb_socket(_this->address, _this->port);
		if (_this->sockfd == -1)
		{
			printf("Failed to open socket: waiting 5s");
			sleep(5);
		}
	}

	printf("opened socket %d\n", _this->sockfd);

	mqtt_reinit(client, _this->sockfd, _this->sendbuf, 2048, _this->recvbuf, 1024);
	MQTTErrors result = mqtt_connect(client, "janus_pi_client", 0, 0, 0, "janus", "Janus506", 0, 400);
	printf("MQTT %p connect: %s\n", client, mqtt_error_str(result));


    /* Subscribe to the topic. */
	result = mqtt_subscribe(client, _this->topic, 0);
	printf("Subscribe to %s: %s\n", _this->topic,  mqtt_error_str(result));
}


bool (*MQTT::receivedCB)(int pipe, uint8_t *data, int len) = 0;

MQTT::MQTT(const char *topic, const char *addr, const char *port)
{
	sockfd = -1;
	strcpy(address, addr);
	strcpy(this->port, port);
	strcpy(this->topic, topic);

	mqtt_init_reconnect(&client, reconnect, this, publish_callback);

	/* check that we don't have any errors */
//	if (result != MQTT_OK) {
//		printf("MQTT %p KO, error: %s\n", &client, mqtt_error_str(result));
//		//close(sockfd);
//		//return;
//	}

	/* start a thread to refresh the client (handle egress and ingree client traffic) */
	if(pthread_create(&client_daemon, NULL, client_refresher, &client))
	{
		printf("Failed to start client daemon.\n");
	}
}

MQTT::~MQTT()
{
	mqtt_disconnect(&client);

	if (sockfd != -1)
		close(sockfd);

	if (client_daemon != 0)
		pthread_cancel(client_daemon);
}


void MQTT::checkStatus()
{
	MQTTErrors result = mqtt_sync(&client);
	if(result != 1)
		printf("MQTT %p sync: (%d) %s\n", &client, result, mqtt_error_str(result));
}

bool MQTT::publish(char *topic, char *application_message)
{
	/* publish the time */
	MQTTErrors result = mqtt_publish(&client, (const char*)topic, application_message, strlen(application_message), MQTT_PUBLISH_QOS_0);

	/* check for errors */
	if (result != MQTT_OK)
	{
		printf( "publish error: %s\n", mqtt_error_str(result));
		return false;
	}

	return true;
}
