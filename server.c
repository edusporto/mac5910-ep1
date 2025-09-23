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
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <dirent.h>

#include "errors.h"
#include "management.h"
#include "mqtt.h"

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

#define DEFAULT_SERVER_PORT 1883
#define MAX_BASE_BUFFER 1024
#define MAX_MSG_SIZE 1024*1024

/* ========================================================= */

/* Base folder to store topics and messages */
const char *BASE_FOLDER = "/tmp/temp.mac5910.1.11796510";

void catch_int(int dummy) {
    (void)dummy;
    remove_dir(BASE_FOLDER);
    exit(0);
}

void treat_subscribe(int connfd, int user_id, MqttControlPacket packet) {
    char file_name_buffer[MAX_BASE_BUFFER + 1];
    char topic_name_buffer[MAX_BASE_BUFFER + 1];
    uint16_t topic_name_size = 0;

    snprintf(file_name_buffer, MAX_BASE_BUFFER, "%s/%d", BASE_FOLDER, user_id);
    ensure_dir(file_name_buffer);

    for (ssize_t i = 0; i < packet.payload.subscribe.topic_amount; i++) {
        /* Check if user is already subscribed to this topic */
        snprintf(
            file_name_buffer,
            MAX_BASE_BUFFER,
            "%s/%d/%s",
            BASE_FOLDER, user_id, packet.payload.subscribe.topics[i].str.val
        );
        if (ensure_fifo(file_name_buffer)) {
            /* the pipe already existed, another process is reading it */
            continue;
        }

        /* before we lose `packet`, save the topic name for each fork */
        snprintf(topic_name_buffer, MAX_BASE_BUFFER, "%s", packet.payload.subscribe.topics[i].str.val);
        topic_name_size = packet.payload.subscribe.topics[i].str.len;

        int pid;
        if ((pid = fork()) == 0) {
            /* child process, read the buffer */
            int pipe_fd = open(file_name_buffer, O_RDONLY | O_NONBLOCK);
            if (pipe_fd == -1) {
                fprintf(stderr, "[%d failed to open pipe %s]\n", pipe_fd, file_name_buffer);
                exit(ERROR_SERVER);
            }

            for (;;) {
                fflush(stdout);

                fd_set read_fds;
                FD_ZERO(&read_fds);
                FD_SET(pipe_fd, &read_fds);

                /* timeout checking if pipe still exists */
                struct timeval timeout;
                timeout.tv_sec = 0;
                timeout.tv_usec = 50000; /* 0.05s */

                int ret = select(pipe_fd + 1, &read_fds, NULL, NULL, &timeout);

                if (ret > 0)
                {
                    /* Data available, send to client */
                    char msg_buffer[MAX_MSG_SIZE + 1];
                    ssize_t bytes_read = read(pipe_fd, msg_buffer, sizeof(msg_buffer));
                    if (bytes_read < 0) {
                        /* error, don't care */
                        break;
                    } else if (bytes_read == 0) {
                        /* some writer closed the pipe, but we keep on living */
                        continue;
                    }
                    msg_buffer[bytes_read] = '\0';
                    
                    /* finally send the publish packet */
                    String topic = { .val = topic_name_buffer, .len = topic_name_size};
                    MqttControlPacket send = create_publish(topic, msg_buffer, bytes_read);
                    write_control_packet(connfd, &send);
                    /* don't destroy `send` since it doesn't allocate anything new */
                }
                else if (ret == 0)
                {
                    /* Timeout occurred, check if pipe still exists */
                    struct stat file_stat;
                    if (stat(file_name_buffer, &file_stat) == -1) {
                        /* pipe deleted */
                        break;
                    }
                }
                else
                {
                    /* error, don't care */
                    exit(EXIT_SUCCESS);
                }
            }

            close(pipe_fd);
            close(connfd);
            remove_fifo(file_name_buffer);
            exit(0);
        } else { /* parent process */ }
    }

    /* All that's left is sending the SUBACK */
    MqttControlPacket send = create_suback(packet);
    write_control_packet(connfd, &send);
    /* we allocated for the payload */
    destroy_control_packet(send);
}

void treat_unsubscribe(int connfd, int user_id, MqttControlPacket packet) {
    char file_name_buffer[MAX_BASE_BUFFER + 1];

    for (ssize_t i = 0; i < packet.payload.unsubscribe.topic_amount; i++) {
        snprintf(
            file_name_buffer,
            MAX_BASE_BUFFER,
            "%s/%d/%s",
            BASE_FOLDER, user_id, packet.payload.unsubscribe.topics[i].val
        );

        /* Delete FIFO, making child processes exit */
        if (remove_fifo(file_name_buffer)) {
            printf("[User %d unsubscribed from topic: %s]\n", user_id, packet.payload.unsubscribe.topics[i].val);
        } else {
            // This isn't a critical error; the user might be unsubscribing from a non-existent topic.
            fprintf(stderr,
                "[Warning: User %d tried to unsubscribe from non-existent topic: %s]\n",
                user_id,
                packet.payload.unsubscribe.topics[i].val
            );
        }
    }

    /* Send UNSUBACK */
    MqttControlPacket send = create_unsuback(packet);
    write_control_packet(connfd, &send);
    destroy_control_packet(send);
}

void treat_publish(int connfd, int user_id, MqttControlPacket packet) {
    (void)connfd;
    (void)user_id;

    char *topic_name = packet.var_header.publish.topic_name.val;
    char *msg = (char*)packet.payload.other.content;
    ssize_t msg_len = packet.payload.other.len;

    DIR *base_dir = opendir(BASE_FOLDER);
    if (base_dir == NULL) {
        perror("[PUBLISH: Failed to open base directory]");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(base_dir)) != NULL) {
        // Skip '.' and '..'
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* assume all other dirs are users */
        char fifo_path[MAX_BASE_BUFFER + 1];
        snprintf(
            fifo_path,
            sizeof(fifo_path),
            "%s/%s/%s",
            BASE_FOLDER, entry->d_name, topic_name
        );

        /* check if current user is subscribed to this topic */
        struct stat st;
        if (stat(fifo_path, &st) == 0 && S_ISFIFO(st.st_mode)) {
            /* fifo exists, so user is subscriber */
            int pipe_fd = open(fifo_path, O_WRONLY | O_NONBLOCK);

            if (pipe_fd != -1) {
                write(pipe_fd, msg, msg_len);
                close(pipe_fd);
            } else {
                fprintf(stderr, "[PUBLISH: couldn't publish to %s, skipping]\n", fifo_path);
            }
        }
    }

    closedir(base_dir);
}

void treat_pingreq(int connfd) {
    MqttControlPacket send = create_pingresp();
    write_control_packet(connfd, &send);
}

void treat_disconnect(int user_id) {
    printf("[User %d sent DISCONNECT. Cleaning up resources.]\n", user_id);

    char user_dir_path[MAX_BASE_BUFFER + 1];
    snprintf(user_dir_path, sizeof(user_dir_path), "%s/%d", BASE_FOLDER, user_id);

    /* Remove the user's directory. This closes all user FIFOs, which should
     * stop all forked children for `user_id`. */
    remove_dir(user_dir_path);

    /* The server does not need to return a response. */
}

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
   
    // TODO: remove
    // if (argc != 2) {
    //     fprintf(stderr,"Uso: %s <Porta>\n", argv[0]);
    //     fprintf(stderr,"Vai rodar um servidor de echo na porta <Porta> TCP\n");
    //     exit(1);
    // }
    uint16_t server_port;
    if (argc >= 2) {
        server_port = atoi(argv[1]);
    } else {
        server_port = DEFAULT_SERVER_PORT;
    }

    /* ========================================================= */
    /* ================= Part of my solution =================== */

    /* ========================================================= */

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
    // TODO: fix
    // servaddr.sin_port        = htons(atoi(argv[1]));
    servaddr.sin_port        = htons(DEFAULT_SERVER_PORT);
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
            // close(listenfd); // TODO: uncomment

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
                    treat_subscribe(connfd, childpid, recv);
                    break;
                case UNSUBSCRIBE:
                    treat_unsubscribe(connfd, childpid, recv);
                    break;
                case PUBLISH:
                    /* We only accept PUBLISH with QoS = 0 */
                    treat_publish(connfd, childpid, recv);
                    break;
                case DISCONNECT:
                    treat_disconnect(childpid);
                    break;
                case PINGREQ:
                    treat_pingreq(connfd);
                    break;
                default:
                    fprintf(stderr, "[Warning: packet type %d not implemented]\n", recv.fixed_header.type);
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
            // exit(0);
        // }
        // else {
        //     close(connfd);
        // }
        // TODO: remove
        close(connfd);
    }
    remove_dir(BASE_FOLDER);
    exit(0);
}
