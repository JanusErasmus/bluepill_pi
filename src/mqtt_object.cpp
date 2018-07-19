#include "mqtt_object.h"

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
    	printf(".");
    	fflush(stdout);
        mqtt_sync((struct mqtt_client*) client);
        usleep(500000U);
    }
    return NULL;
}


void publish_callback(void** unused, struct mqtt_response_publish *published)
{
    /* note that published->topic_name is NOT null-terminated (here we'll change it to a c-string) */
    char* topic_name = (char*) malloc(published->topic_name_size + 1);
    memcpy(topic_name, published->topic_name, published->topic_name_size);
    topic_name[published->topic_name_size] = '\0';

    printf("Received publish('%s'): %s\n", topic_name, (const char*) published->application_message);

    free(topic_name);
}

MQTT::MQTT(const char *topic, const char *addr, const char *port)
{
	/* setup a client */
	/* open the non-blocking TCP socket (connecting to the broker) */
	int sockfd = open_nb_socket(addr, port);
	if (sockfd == -1) {
		perror("Failed to open socket: ");
		return;
	}

	mqtt_init(&client, sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), publish_callback);
	mqtt_connect(&client, "janus_pi", NULL, NULL, 0, "janus", "Janus506", 0, 400);

	/* check that we don't have any errors */
	if (client.error != MQTT_OK) {
		printf("error: %s\n", mqtt_error_str(client.error));
		return;
	}

	/* start a thread to refresh the client (handle egress and ingree client traffic) */
	pthread_t client_daemon;
	if(pthread_create(&client_daemon, NULL, client_refresher, &client)) {
		printf("Failed to start client daemon.\n");
		return;
	}


	/* subscribe */
	mqtt_subscribe(&client, topic, 0);
}

MQTT::~MQTT()
{
	// TODO Auto-generated destructor stub
}

bool MQTT::publish(const char *topic, char *application_message)
{
	/* publish the time */
	mqtt_publish(&client, topic, application_message, strlen(application_message) + 1, MQTT_PUBLISH_QOS_0);

	/* check for errors */
	if (client.error != MQTT_OK) {
		printf( "publish error: %s\n", mqtt_error_str(client.error));
		return false;
	}

	return true;
}
