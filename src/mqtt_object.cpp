#include "mqtt_object.h"
#include "json/Json.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>

/*
    A template for opening a non-blocking POSIX socket.
*/
int open_nb_socket(const char* addr, const char* port) {
    struct addrinfo hints = {0};

    hints.ai_family = AF_UNSPEC; /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Must be TCP */
    int sockfd = -1;
    int rv;
    struct addrinfo *p, *servinfo;

    /* get address information */
    rv = getaddrinfo(addr, port, &hints, &servinfo);
    if(rv != 0) {
        fprintf(stderr, "Failed to open socket (getaddrinfo): %s\n", gai_strerror(rv));
        return -1;
    }

    /* open the first possible socket */
    for(p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        /* connect to server */
        rv = connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
        if(rv == -1) continue;
        break;
    }

    /* free servinfo */
    freeaddrinfo(servinfo);

    /* make non-blocking */
    if (sockfd != -1) fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);

    /* return the new socket fd */
    return sockfd;
}

void* client_refresher(void* client)
{
    while(1)
    {
    	//printf(".");
    	//fflush(stdout);
        mqtt_sync((struct mqtt_client*) client);
        usleep(500000U);
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
    	int len = strlen(topic_name) - 1;
    	int pipe = atoi((const char*)&topic_name[len]);


    	MQTT::receivedCB(pipe, payload, published->application_message_size + 1);


    }

    free(topic_name);
}

bool (*MQTT::receivedCB)(int pipe, uint8_t *data, int len) = 0;

MQTT::MQTT(const char *topic, const char *addr, const char *port)
{

	/* setup a client */
	/* open the non-blocking TCP socket (connecting to the broker) */
	sockfd = open_nb_socket(addr, port);
	if (sockfd == -1) {
		perror("Failed to open socket: ");
		return;
	}

	mqtt_init(&client, sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), publish_callback);
	mqtt_connect(&client, "janus_pi", NULL, NULL, 0, "janus", "Janus506", 0, 400);

	/* check that we don't have any errors */
	if (client.error != MQTT_OK) {
		printf("error: %s\n", mqtt_error_str(client.error));
		close(sockfd);
		return;
	}

	/* start a thread to refresh the client (handle egress and ingree client traffic) */
	if(pthread_create(&client_daemon, NULL, client_refresher, &client)) {
		printf("Failed to start client daemon.\n");

		mqtt_disconnect(&client);
		close(sockfd);
		return;
	}


	/* subscribe */
	mqtt_subscribe(&client, topic, 0);
}

MQTT::~MQTT()
{
	mqtt_disconnect(&client);

	if (sockfd != -1)
		close(sockfd);

	if (client_daemon != 0)
		pthread_cancel(client_daemon);
}

bool MQTT::publish(char *topic, char *application_message)
{
	/* publish the time */
	mqtt_publish(&client, (const char*)topic, application_message, strlen(application_message), MQTT_PUBLISH_QOS_0);

	/* check for errors */
	if (client.error != MQTT_OK) {
		printf( "publish error: %s\n", mqtt_error_str(client.error));
		return false;
	}

	return true;
}
