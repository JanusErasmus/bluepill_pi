#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "mqtt/mqtt.h"
#include "RPi/bcm2835.h"
#include "RPi/RF24_arch_config.h"
#include "interface_nrf24.h"


void publish_callback(void** unused, struct mqtt_response_publish *published)
{
    /* note that published->topic_name is NOT null-terminated (here we'll change it to a c-string) */
    char* topic_name = (char*) malloc(published->topic_name_size + 1);
    memcpy(topic_name, published->topic_name, published->topic_name_size);
    topic_name[published->topic_name_size] = '\0';

    printf("Received publish('%s'): %s\n", topic_name, (const char*) published->application_message);

    free(topic_name);
}

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
        usleep(100000U);
    }
    return NULL;
}


//void exit_example(int status, int sockfd, pthread_t *client_daemon)
//{
//    if (sockfd != -1) close(sockfd);
//    if (client_daemon != NULL) pthread_cancel(*client_daemon);
//    exit(status);
//}

int main()
{
	printf("NRF24 listener\n");

//	const char *topic = "/pi";
//    const char *addr = "160.119.253.3";
//    const char *port = "1883";
//	/* setup a client */
//    /* open the non-blocking TCP socket (connecting to the broker) */
//    int sockfd = open_nb_socket(addr, port);
//
//    if (sockfd == -1) {
//        perror("Failed to open socket: ");
//        return -1;
//    }
//
//	struct mqtt_client client;
//	uint8_t sendbuf[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
//	uint8_t recvbuf[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
//	mqtt_init(&client, sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), publish_callback);
//	mqtt_connect(&client, "janus_pi", NULL, NULL, 0, "janus", "Janus506", 0, 400);
//
//	/* check that we don't have any errors */
//	if (client.error != MQTT_OK) {
//		fprintf(stderr, "error: %s\n", mqtt_error_str(client.error));
//		return -2;
//	}
//
//    /* start a thread to refresh the client (handle egress and ingree client traffic) */
//    pthread_t client_daemon;
//    if(pthread_create(&client_daemon, NULL, client_refresher, &client)) {
//        fprintf(stderr, "Failed to start client daemon.\n");
//        return -3;
//
//    }
//
//
//    /* subscribe */
//    mqtt_subscribe(&client, topic, 0);
//
//	char *application_message = "PI hi";
//	while(1)
//	{
//		/* publish the time */
//		mqtt_publish(&client, topic, application_message, strlen(application_message) + 1, MQTT_PUBLISH_QOS_0);
//		/* check for errors */
//		if (client.error != MQTT_OK) {
//			fprintf(stderr, "publish error: %s\n", mqtt_error_str(client.error));
//
//		}
//
//    sleep(5);
//	}

	InterfaceNRF24::init();

	while(1)
	{
		InterfaceNRF24::get()->run();
		usleep(100000);
	}
	return 0;
}
