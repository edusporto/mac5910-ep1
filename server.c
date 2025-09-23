/* Código original por Prof. Daniel Batista <batista@ime.usp.br>
 * Em 03/8/2025
 * Versão finalizada do EP1 de MAC5910/2025 por Eduardo Sandalo Porto
 * <sandalo@ime.usp.br>
 * 
 * Broker MQTT simples.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <dirent.h>

#include "errors.h"
#include "mqtt.h"
#include "management.h"
#include "handlers.h"

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

#define DEFAULT_SERVER_PORT 1883
#define MAX_BASE_BUFFER 1024
#define MAX_MSG_SIZE 1024*1024

/* ========================================================= */

int main (int argc, char **argv) {
    // Server listening socket
    int listenfd;
    // Individual connection socket.
    // Must be open from the main process, and then closed after each fork.
    int connfd;

    // Socket information
    struct sockaddr_in servaddr;
    // Fork return
    pid_t childpid;
   
    // Choose server port
    uint16_t server_port;
    if (argc >= 2) {
        server_port = atoi(argv[1]);
    } else {
        server_port = DEFAULT_SERVER_PORT;
    }

    /* Setup: prepare FIFO directory, wait for previous children to _stop_ */
    signal(SIGINT, catch_int);
    fresh_dir(BASE_FOLDER);
    sleep(1); /* wait for orphan children to die */

    // IPv4, TCP, Internet socket creation
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket :(\n");
        exit(2);
    }

    /* Allow socket to be created when old TCP connection is in wait state */
    int optval = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Socket binding.
    // Uses IPv4, connects to any address, sets port to command line
    // argument.
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(server_port);
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("bind :(\n");
        exit(3);
    }

    // Sets listen socket to listening mode
    if (listen(listenfd, LISTENQ) == -1) {
        perror("listen :(\n");
        exit(4);
    }

    printf("[Server up. Waiting for connections in port %d]\n", server_port);
    printf("[To stop the server, do CTRL+C]\n");
   
    /* ===================== Server loop ======================= */
	for (;;) {
        // Accepts connection to the first socket in the listening queue
        if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
            perror("accept :(\n");
            exit(5);
        }
      
        // Forks to new process.
        // The child should treat the connection open on `connfd`, while the
        // parent should close this socket to listen for more connections.
        if ((childpid = fork()) == 0) {
            // Child process
            close(listenfd);
            int mypid = getpid();
            printf("[Connection open for user %d]\n", mypid);

            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 START                         */
            /* ========================================================= */
            /* ========================================================= */

            MqttControlPacket recv = { 0 };
            MqttControlPacket connack = { 0 };

            read_control_packet(connfd, &recv);

            /* Useful for debugging
            // FIXED HEADER
            printf("Fixed header:\n");
            printf("header.len = %d\n", packet.fixed_header.len);
            printf("header.type = %d\n", packet.fixed_header.type);
            printf("header.flags = %d\n", packet.fixed_header.flags);

            // VAR HEADER
            printf("Variable header:\n");
            TODO: print stuff
            printf("pack_id = %d\n", packet.var_header.pack_id);
            printf("props.len = %d\n", packet.var_header.props_len);
            printf("payload[1] = %c\n", packet.payload.content[1]);

            // PAYLOAD
            printf("Payload length: %d\n", packet.payload.len);
            printf("Payload:\n");
            for (ssize_t i = 0; i < packet.payload.len; i++) {
                printf("%d:%c\n", packet.payload.content[i], packet.payload.content[i]);
            }
            */

            /* First packet should be CONNECT */
            if (recv.fixed_header.type != CONNECT) {
                fprintf(stderr, "[Got invalid connection, probably not MQTT]\n");
                exit(ERROR_CLIENT);
            }
            destroy_control_packet(recv);
        
            /* Answer CONNECT with CONNACK */
            connack = create_connack();
            write_control_packet(connfd, &connack);
            destroy_control_packet(connack);

            /* Now, we treat any other packets this client may send */
            for (;;) {
                int stop = 0;

                memset(&recv, 0, sizeof(MqttControlPacket)); 

                /* This can fail with a weird message if the client
                 * suddenly closes the connection.
                 * The broker will still work, so won't fix for now. */
                read_control_packet(connfd, &recv);

                switch ((MqttControlType)recv.fixed_header.type) {
                    case SUBSCRIBE:
                        treat_subscribe(connfd, mypid, recv);
                        break;
                    case UNSUBSCRIBE:
                        treat_unsubscribe(connfd, mypid, recv);
                        break;
                    case PUBLISH:
                        /* We only accept PUBLISH with QoS = 0 */
                        treat_publish(recv);
                        stop = 1;
                        break;
                    case DISCONNECT:
                        treat_disconnect(mypid);
                        stop = 1;
                        break;
                    case PINGREQ:
                        treat_pingreq(connfd);
                        break;
                    default:
                        fprintf(stderr, "[Warning: packet type %d not implemented]\n", recv.fixed_header.type);
                }

                destroy_control_packet(recv);

                if (stop) { break; }
            }

            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 END                           */
            /* ========================================================= */
            /* ========================================================= */

            printf("[Connection closed for user %d]\n", mypid);
            exit(0);
        } else {
            close(connfd);
        }
    }

    /* I don't think the program can get here */
    remove_dir(BASE_FOLDER);
    exit(0);
}
