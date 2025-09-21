/* Por Prof. Daniel Batista <batista@ime.usp.br>
 * Em 03/8/2025
 * 
 * Um código simples de um servidor de eco a ser usado como base para
 * o EP1. Ele recebe uma linha de um cliente e devolve a mesma linha.
 * Teste ele assim depois de compilar:
 * 
 * ./redes-servidor-exemplo-ep1 8000
 * 
 * Com este comando o servidor ficará escutando por conexões na porta
 * 8000 TCP (Se você quiser fazer o servidor escutar em uma porta
 * menor que 1024 você precisará ser root ou ter as permissões
 * necessárias para rodar o código com 'sudo').
 *
 * Depois conecte no servidor via telnet. Rode em outro terminal:
 * 
 * telnet 127.0.0.1 8000
 * 
 * Escreva sequências de caracteres seguidas de ENTER. Você verá que o
 * telnet exibe a mesma linha em seguida. Esta repetição da linha é
 * enviada pelo servidor. O servidor também exibe no terminal onde ele
 * estiver rodando as linhas enviadas pelos clientes.
 * 
 * Obs.: Você pode conectar no servidor remotamente também. Basta
 * saber o endereço IP remoto da máquina onde o servidor está rodando
 * e não pode haver nenhum firewall no meio do caminho bloqueando
 * conexões na porta escolhida.
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

#include "errors.h"
#include "management.h"
#include "mqtt.h"

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

#define SERVER_PORT 1883
#define MAX_BASE_BUFFER 1024

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
    // Stores lines received by a client
    char recvline[MAXLINE + 1];
    // Stores size of lines read by client
    ssize_t n;
   
    // TODO: remove
    // if (argc != 2) {
    //     fprintf(stderr,"Uso: %s <Porta>\n", argv[0]);
    //     fprintf(stderr,"Vai rodar um servidor de echo na porta <Porta> TCP\n");
    //     exit(1);
    // }

    /* ========================================================= */
    /* ================= Part of my solution =================== */

    // Buffer to store paths within base folder.
    char base_buffer[MAX_BASE_BUFFER + 1];

    create_base_folder();

    /* ========================================================= */

    // IPv4, TCP, Internet socket creation
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket :(\n");
        exit(2);
    }

    // Socket binding.
    // Uses IPv4, connects to any address, sets port to command line
    // argument.
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    // TODO: fix
    // servaddr.sin_port        = htons(atoi(argv[1]));
    servaddr.sin_port        = htons(SERVER_PORT);
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("bind :(\n");
        exit(3);
    }

    // Sets listen socket to listening mode
    if (listen(listenfd, LISTENQ) == -1) {
        perror("listen :(\n");
        exit(4);
    }

    printf("[Servidor no ar. Aguardando conexões na porta %s]\n",argv[1]);
    printf("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");
   
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

        // FIXME/TODO: to debug on mac, disabled fork
        // if ((childpid = fork()) == 0) {
            // TODO: reenable fork
            childpid = 42;

            // Child process
            printf("[Uma conexão aberta]\n");
            close(listenfd);

            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 INÍCIO                        */
            /* ========================================================= */
            /* ========================================================= */

            MqttControlPacket recv = { 0 };
            MqttControlPacket send = { 0 };

            read_control_packet(connfd, &recv);

            // FIXED HEADER
            // printf("Fixed header:\n");
            // printf("header.len = %d\n", packet.fixed_header.len);
            // printf("header.type = %d\n", packet.fixed_header.type);
            // printf("header.flags = %d\n", packet.fixed_header.flags);

            // VAR HEADER
            // printf("Variable header:\n");
            // TODO: print stuff
            // printf("pack_id = %d\n", packet.var_header.pack_id);
            // printf("props.len = %d\n", packet.var_header.props_len);
            // printf("payload[1] = %c\n", packet.payload.content[1]);

            // PAYLOAD
            // printf("Payload length: %d\n", packet.payload.len);
            // printf("Payload:\n");
            // for (ssize_t i = 0; i < packet.payload.len; i++) {
            //     printf("%d:%c\n", packet.payload.content[i], packet.payload.content[i]);
            // }

            if (recv.fixed_header.type != CONNECT) {
                fprintf(stderr, "[Got invalid connection, probably not MQTT]\n");
                exit(ERROR_CLIENT);
            }
            destroy_control_packet(recv);
        
            send = create_connack();
            write_control_packet(connfd, &send);
            destroy_control_packet(send);

            read_control_packet(connfd, &recv);

            switch ((MqttControlType)recv.fixed_header.type) {
                case SUBSCRIBE:
                    
                    // TODO
                    break;
                case UNSUBSCRIBE:
                    // TODO
                    break;
                case PUBLISH:
                    /* We only accept PUBLISH with QoS = 0 */
                    // TODO
                    break;
                case DISCONNECT:
                    // TODO
                    break;
                case PINGREQ:
                    // TODO 
                    break;
                default:
                    fprintf(stderr, "[Warning: packet type %d still not implemented]\n", recv.fixed_header.type);
            }

            destroy_control_packet(recv);

            // while ((n=read(connfd, recvline, MAXLINE)) > 0) {
            //     recvline[n]=0;
            //     printf("[Cliente conectado no processo filho %d enviou:] ",getpid());
            //     if ((fputs(recvline, stdout)) == EOF) {
            //         perror("fputs :( \n");
            //         exit(6);
            //     }
            //     write(connfd, recvline, strlen(recvline));
            // }

            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 FIM                           */
            /* ========================================================= */
            /* ========================================================= */

            printf("[Uma conexão fechada]\n");
            exit(0);
        // }
        // else {
        //     close(connfd);
        // }
    }
    exit(0);
}
