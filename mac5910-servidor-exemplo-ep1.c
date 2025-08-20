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
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

/* ========================================================= */
/* ================= Part of my solution =================== */

#define MQTT_PORT
#define MAX_BASE_BUFFER 1024

#define ERROR_BASE_FOLDER 10
#define ERROR_READ_FAILED 11
#define ERROR_INVALID_INP 12
#define ERROR_SERVER      500
#define ERROR_CLIENT      400

// MQTT Control Packet types
#define MQTT_TYP_CONNECT      1
#define MQTT_TYP_CONNACK      2
#define MQTT_TYP_PUBLISH      3
#define MQTT_TYP_PUBACK       4
#define MQTT_TYP_PUBREC       5
#define MQTT_TYP_PUBREL       6
#define MQTT_TYP_PUBCOMP      7
#define MQTT_TYP_SUBSCRIBE    8
#define MQTT_TYP_SUBACK       9
#define MQTT_TYP_UNSUBSCRIBE 10
#define MQTT_TYP_UNSUBACK    11
#define MQTT_TYP_PINGREQ     12
#define MQTT_TYP_PINGRESP    13
#define MQTT_TYP_DISCONNECT  14
#define MQTT_TYP_AUTH        15

// MQTT Control Packet flags
#define MQTT_FLG_CONNECT 0b0000
#define MQTT_FLG_CONNACK 0b0000
typedef struct MqttFlgPublish {
    uint8_t retain : 1;
    uint8_t qos    : 1;
    uint8_t dup    : 1;
} MqttFlgPublish;
#define MQTT_FLG_PUBACK      0b0000
#define MQTT_FLG_PUBREC      0b0000
#define MQTT_FLG_PUBREL      0b0010
#define MQTT_FLG_PUBCOMP     0b0000
#define MQTT_FLG_SUBSCRIBE   0b0010
#define MQTT_FLG_SUBACK      0b0000
#define MQTT_FLG_UNSUBSCRIBE 0b0010
#define MQTT_FLG_UNSUBACK    0b0000
#define MQTT_FLG_PINGREQ     0b0000
#define MQTT_FLG_PINGRESP    0b0000
#define MQTT_FLG_DISCONNECT  0b0000
#define MQTT_FLG_AUTH        0b0000

#define MQTT_PROP_BYTE      0
#define MQTT_PROP_TWO_BYTE  1
#define MQTT_PROP_FOUR_BYTE 2
#define MQTT_PROP_VAR_INT   3
#define MQTT_PROP_BIN_DATA  4
#define MQTT_PROP_STR       5
#define MQTT_PROP_STR_PAIR  6

typedef struct BinaryData {
    uint16_t len;
    uint8_t *bytes;
} BinaryData;

typedef struct StringPair {
    char *str1;
    char *str2;
} StringPair;

typedef struct MqttFixedHeader {
    uint8_t flags : 4;  // bits 0-3
    uint8_t type  : 4;  // bits 4-7
    uint32_t len;
} MqttFixedHeader;

union MqttPropertyContent {
    uint8_t byte;
    uint16_t two_byte;
    uint32_t four_byte;
    uint32_t var_int;
    BinaryData data;
    char *string;
    StringPair string_pair;
};

typedef struct MqttProperty {
    uint32_t id;
    union MqttPropertyContent content;
} MqttProperty;

typedef struct MqttProperties {
    uint16_t len;
    MqttProperty *properties;
} MqttProperties;

// Base folder to store topics and messages
const char *base_folder = "/tmp/temp.mac5910.1.11796510/";

int directory_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

// Creates base folder to store topics and messages.
void create_base_folder() {
    // It should not exist - if it does, it was probably kept from an earlier execution.
    if (directory_exists(base_folder)) {
        if (rmdir(base_folder) == -1) {
            fprintf(stderr, "[ERROR: could not delete %s]\n", base_folder);
            exit(ERROR_BASE_FOLDER);
        }

        if (mkdir(base_folder, 0744) == -1) {
            fprintf(stderr, "[ERROR: could not create %s]\n", base_folder);
            exit(ERROR_BASE_FOLDER);
        }
    } else {
        if (mkdir(base_folder, 0744) == -1) {
            fprintf(stderr, "[ERROR: could not create %s]\n", base_folder);
            exit(ERROR_BASE_FOLDER);
        }
    }
}

uint8_t read_uint8(int fd) {
    uint8_t byte;
    if (read(fd, &byte, 1) <= 0) {
        perror("[Socket reading failed]");
        exit(ERROR_READ_FAILED);
    }
    return byte;
}

uint16_t read_uint16(int fd) {
    uint16_t val;
    if (read(fd, &val, 2) <= 0) {
        perror("[Socket reading failed]");
        exit(ERROR_READ_FAILED);
    }
    return ntohs(val);
}

uint32_t read_uint32(int fd) {
    uint32_t val;
    if (read(fd, &val, 4) <= 0) {
        perror("[Socket reading failed]");
        exit(ERROR_READ_FAILED);
    }
    return ntohl(val);
}

// Read a Variable Byte Integer from a file descriptor.
uint32_t read_variable_int(int fd) {
    uint32_t multiplier = 1;
    uint32_t value = 0;
    uint8_t byte = 0;
    
    do {
        byte = read_uint8(fd);
        value += (byte & 127) * multiplier;
        if (multiplier > 128 * 128 * 128) {
            // Invalid variable byte integer.
            exit(ERROR_INVALID_INP);
        }
        multiplier *= 128;
    } while ((byte & 128) != 0);

    return value;
}

BinaryData read_binary_data(int fd) {
    BinaryData data;
    data.len = read_uint16(fd);

    data.bytes = malloc(data.len * sizeof(uint8_t));
    for (uint32_t i = 0; i < data.len; i++) {
        data.bytes[i] = read_uint8(fd);
    }

    return data;
}

void destroy_binary_data(BinaryData data) {
    free(data.bytes);
}

// MQTT protocol asks for UTF-8, but we'll do ASCII strings
char *read_string(int fd) {
    uint32_t i;
    uint16_t len = read_uint16(fd);

    char *string = malloc((len + 1) * sizeof(char));
    for (i = 0; i < len; i++) {
        string[i] = (char)read_uint8(fd);
    }
    string[i] = '\0';

    return string;
}

void destroy_string(char *string) {
    free(string);
}

StringPair read_string_pair(int fd) {
    StringPair pair;

    pair.str1 = read_string(fd);
    pair.str2 = read_string(fd);

    return pair;
}

void destroy_string_pair(StringPair pair) {
    free(pair.str1);
    free(pair.str2);
}

// Returns a packet identifier, or 0 if not needed.
uint16_t read_packet_identifier(int fd, MqttFixedHeader header) {
    if (header.type != MQTT_TYP_PUBLISH ||
        header.type != MQTT_TYP_PUBACK ||
        header.type != MQTT_TYP_PUBREC ||
        header.type != MQTT_TYP_PUBREL ||
        header.type != MQTT_TYP_PUBCOMP ||
        header.type != MQTT_TYP_SUBSCRIBE ||
        header.type != MQTT_TYP_SUBACK ||
        header.type != MQTT_TYP_UNSUBSCRIBE ||
        header.type != MQTT_TYP_UNSUBACK    
    ) {
        return 0;
    }

    if (header.type == MQTT_TYP_PUBLISH &&
        !(header.flags && 0b0110 > 0)) {
        // if Header is PUBLISH and QoS > 0, we read packet_id
        return 0;
    }

    return read_uint16(fd);
}

// TODO: write_variable_int

int prop_id_to_type(uint16_t id) {
    switch (id) {
        switch (id) {
            case  1:
            case 23:
            case 25:
            case 36:
            case 37:
            case 40:
            case 41:
            case 42:
                return MQTT_PROP_BYTE;
            case 19:
            case 33:
            case 34:
            case 35:
                return MQTT_PROP_TWO_BYTE;
            case 2:
            case 17:
            case 24:
            case 39:
                return MQTT_PROP_FOUR_BYTE;
            case 11:
                return MQTT_PROP_VAR_INT;
            case 9:
            case 22:
                return MQTT_PROP_BIN_DATA;
            case 3:
            case 8:
            case 18:
            case 21:
            case 26:
            case 28:
            case 31:
                return MQTT_PROP_STR;
            case 38:
                return MQTT_PROP_STR_PAIR;
            default:
                return -1;
        }
    }

    return -1;
}

MqttProperties *read_properties(int fd, MqttFixedHeader header) {
    if (header.type != MQTT_TYP_CONNECT     ||
        header.type != MQTT_TYP_CONNACK     ||
        header.type != MQTT_TYP_PUBLISH     ||
        header.type != MQTT_TYP_PUBACK      ||
        header.type != MQTT_TYP_PUBREC      ||
        header.type != MQTT_TYP_PUBCOMP     ||
        header.type != MQTT_TYP_SUBSCRIBE   ||
        header.type != MQTT_TYP_SUBACK      ||
        header.type != MQTT_TYP_UNSUBSCRIBE ||
        header.type != MQTT_TYP_UNSUBACK    ||
        header.type != MQTT_TYP_DISCONNECT  ||
        header.type != MQTT_TYP_AUTH) {
        return NULL;
    }

    MqttProperties *properties = malloc(1 * sizeof(MqttProperties));

    properties->len = read_variable_int(fd);
    properties->properties = malloc(properties->len * sizeof(MqttProperty));
    if (!properties) { 
        fprintf(stderr, "[Memory error, stopping client]\n");
        exit(ERROR_SERVER);
    }

    for (uint32_t i = 0; i < properties->len; i++) {
        MqttProperty property;

        property.id = read_variable_int(fd);
        switch (prop_id_to_type(property.id)) {
            case MQTT_PROP_BYTE:
                property.content.byte = read_uint8(fd);
                break;
            case MQTT_PROP_TWO_BYTE:
                property.content.two_byte = read_uint16(fd);
                break;
            case MQTT_PROP_FOUR_BYTE:
                property.content.four_byte = read_uint32(fd);
                break;
            case MQTT_PROP_VAR_INT:
                property.content.var_int = read_variable_int(fd);
                break;
            case MQTT_PROP_BIN_DATA:
                property.content.data = read_binary_data(fd);
                break;
            case MQTT_PROP_STR:
                property.content.string = read_string(fd);
                break;
            case MQTT_PROP_STR_PAIR:
                property.content.string_pair = read_string_pair(fd);
                break;
            default:
                fprintf(stderr, "[Invalid property id %d]\n", property.id);
                exit(ERROR_CLIENT);
                break;
        }
    }

    return properties;
}

void destroy_properties(MqttProperties *props) {
    for (uint32_t i = 0; i < props->len; i++) {
        MqttProperty prop = props->properties[i];
        switch (prop_id_to_type(prop.id)) {
            case MQTT_PROP_BIN_DATA:
                destroy_binary_data(prop.content.data);
                break;
            case MQTT_PROP_STR:
                destroy_string(prop.content.string);
                break;
            case MQTT_PROP_STR_PAIR:
                destroy_string_pair(prop.content.string_pair);
                break;
        }
    }
    free(props->properties);
    free(props);
}

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
    servaddr.sin_port        = htons(17170);
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
        if ((childpid = fork()) == 0) {
            // Child process
            printf("[Uma conexão aberta]\n");
            close(listenfd);

            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 INÍCIO                        */
            /* ========================================================= */
            /* ========================================================= */

            // === Read MQTT Control Packet Fixed Header

            MqttFixedHeader header;
            uint8_t byte = read_uint8(connfd);
            header.flags = byte & 0x0F;
            header.type  = byte >> 4;
            header.len = read_variable_int(connfd);

            uint16_t pack_id = read_packet_identifier(connfd, header);
            MqttProperties *properties = read_properties(connfd, header);

            printf("Ate agora, conseguimos:\n");
            printf("header.flags = %d\n", header.flags);
            printf("header.type = %d\n", header.type);
            printf("header.len = %d\n", header.len);
            printf("pack_id = %d\n", pack_id);
            printf("uepa\n");
            printf("properties = %d\n", properties);
            printf("papo\n");

            destroy_properties(properties);

            exit(0);

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
        }
        else {
            close(connfd);
        }
    }
    exit(0);
}
