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

#define ERROR_BASE_FOLDER  10
#define ERROR_READ_FAILED  11
#define ERROR_WRITE_FAILED 12
#define ERROR_INVALID_INP  13
#define ERROR_SERVER       500
#define ERROR_CLIENT       400

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

typedef struct MqttVarHeader {
    uint16_t pack_id;
    // Won't send
    ssize_t stuff_len;
    // Won't send
    uint8_t *stuff;
    uint32_t props_len;
    MqttProperty *properties;
} MqttVarHeader;

typedef struct MqttPayload {
    ssize_t len;
    uint8_t *content;
} MqttPayload;

typedef struct MqttControlPacket {
    MqttFixedHeader fixed_header;
    MqttVarHeader var_header;
    MqttPayload payload;
} MqttControlPacket;

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


ssize_t read_uint8(int fd, uint8_t *byte) {
    ssize_t bytes_read = read(fd, byte, 1);
    if (bytes_read <= 0) {
        perror("[Socket reading failed]");
        exit(ERROR_READ_FAILED);
    }
    return bytes_read;
}

ssize_t write_uint8(int fd, uint8_t *byte) {
    ssize_t bytes_written = write(fd, byte, 1);
    if (bytes_written <= 0) {
        perror("[Socket writing failed]");
        exit(ERROR_WRITE_FAILED);
    }
}

ssize_t read_uint16(int fd, uint16_t *val) {
    ssize_t bytes_read = read(fd, val, 2);
    *val = ntohs(*val);
    if (bytes_read <= 0) {
        perror("[Socket reading failed]");
        exit(ERROR_READ_FAILED);
    }
    return bytes_read;
}

ssize_t write_uint16(int fd, uint16_t *val) {
    uint16_t local = htons(*val);
    ssize_t bytes_written = write(fd, &local, 2);
    if (bytes_written <= 0) {
        perror("[Socket writing failed]");
        exit(ERROR_WRITE_FAILED);
    }
    return bytes_written;
}

ssize_t read_uint32(int fd, uint32_t *val) {
    ssize_t bytes_read = read(fd, val, 4);
    *val = ntohl(*val);
    if (bytes_read <= 0) {
        perror("[Socket reading failed]");
        exit(ERROR_READ_FAILED);
    }
    return bytes_read;
}

ssize_t write_uint32(int fd, uint32_t *val) {
    uint32_t local = htonl(*val);
    ssize_t bytes_written = write(fd, &local, 4);
    if (bytes_written <= 0) {
        perror("[Socket writing failed]");
        exit(ERROR_WRITE_FAILED);
    }
    return bytes_written;
}

// Read a Variable Byte Integer from a file descriptor.
ssize_t read_variable_int(int fd, uint32_t *val) {
    ssize_t bytes_read = 0;
    uint32_t multiplier = 1;
    uint8_t byte = 0;

    *val = 0;
    do {
        bytes_read += read_uint8(fd, &byte);
        *val += (byte & 127) * multiplier;
        if (multiplier > 128 * 128 * 128) {
            // Invalid variable byte integer.
            fprintf(stderr, "[Read invalid Variable Byte Integer]\n");
            exit(ERROR_INVALID_INP);
        }
        multiplier *= 128;
    } while ((byte & 128) != 0);

    return bytes_read;
}

ssize_t write_variable_int(int fd, uint32_t *val) {
    ssize_t bytes_written = 0;
    uint32_t x = *val;
    do {
        uint8_t byte = x % 128;
        x = x / 128;
        if (x > 0) { byte = byte | 128; }
        bytes_written += write_uint8(fd, &byte);
    } while (x > 0);
    
    return bytes_written;
}

ssize_t read_binary_data(int fd, BinaryData *data) {
    ssize_t bytes_read = 0;
    bytes_read += read_uint16(fd, &(data->len));

    data->bytes = malloc(data->len * sizeof(uint8_t));
    for (uint32_t i = 0; i < data->len; i++) {
        bytes_read += read_uint8(fd, &(data->bytes[i]));
    }

    return bytes_read;
}

ssize_t write_binary_data(int fd, BinaryData *data) {
    ssize_t bytes_written = 0;
    bytes_written += write_uint16(fd, &(data->len));

    for (uint32_t i = 0; i < data->len; i++) {
        bytes_written += write_uint8(fd, &(data->bytes[i]));
    }

    return bytes_written;
}

void destroy_binary_data(BinaryData data) {
    free(data.bytes);
}

// MQTT protocol asks for UTF-8, but we'll do ASCII strings
ssize_t read_string(int fd, char **str) {
    ssize_t bytes_read = 0;
    uint32_t i = 0;
    uint16_t len;
    bytes_read += read_uint16(fd, &len);

    *str = malloc((len + 1) * sizeof(char));
    for (i = 0; i < len; i++) {
        bytes_read += read_uint8(fd, (uint8_t*)&(*str)[i]);
    }
    (*str)[i] = '\0';

    return bytes_read;
}

// Writing null-terminated strings, without the '\0'
ssize_t write_string(int fd, char **str) {
    ssize_t bytes_written = 0;

    uint16_t len = strlen(*str);

    bytes_written += write_uint16(fd, &len);
    for (uint16_t i = 0; i < len; i++) {
        bytes_written += write_uint8(fd, (uint8_t*)&(*str)[i]);
    }

    return bytes_written;
}

void destroy_string(char *string) {
    free(string);
}

ssize_t read_string_pair(int fd, StringPair *pair) {
    ssize_t bytes_read = 0;

    bytes_read += read_string(fd, &(pair->str1));
    bytes_read += read_string(fd, &(pair->str2));

    return bytes_read;
}

ssize_t write_string_pair(int fd, StringPair *pair) {
    ssize_t bytes_written = 0;

    bytes_written += write_string(fd, &(pair->str1));
    bytes_written += write_string(fd, &(pair->str2));

    return bytes_written;
}

void destroy_string_pair(StringPair pair) {
    free(pair.str1);
    free(pair.str2);
}

// Reads a packet identifier, or 0 if not needed.
ssize_t read_packet_identifier(int fd, uint16_t *id, MqttFixedHeader header) {
    ssize_t bytes_read = 0;

    // If type does not contain packet identifier, ID is 0
    *id = 0;

    if (header.type != MQTT_TYP_PUBLISH     &&
        header.type != MQTT_TYP_PUBACK      &&
        header.type != MQTT_TYP_PUBREC      &&
        header.type != MQTT_TYP_PUBREL      &&
        header.type != MQTT_TYP_PUBCOMP     &&
        header.type != MQTT_TYP_SUBSCRIBE   &&
        header.type != MQTT_TYP_SUBACK      &&
        header.type != MQTT_TYP_UNSUBSCRIBE &&
        header.type != MQTT_TYP_UNSUBACK    
    ) {
        return bytes_read;
    }

    if (header.type == MQTT_TYP_PUBLISH &&
        !(header.flags && 0b0110 > 0)) {
        // if Header is PUBLISH and QoS > 0, we read packet_id
        return bytes_read;
    }

    bytes_read = read_uint16(fd, id);
    return bytes_read;
}

// Writes a packet identifier, if needed.
ssize_t write_packet_identifier(int fd, uint16_t *id, MqttFixedHeader header) {
    ssize_t bytes_written = 0;

    // If type does not contain packet identifier, do nothing.
    if (header.type != MQTT_TYP_PUBLISH     &&
        header.type != MQTT_TYP_PUBACK      &&
        header.type != MQTT_TYP_PUBREC      &&
        header.type != MQTT_TYP_PUBREL      &&
        header.type != MQTT_TYP_PUBCOMP     &&
        header.type != MQTT_TYP_SUBSCRIBE   &&
        header.type != MQTT_TYP_SUBACK      &&
        header.type != MQTT_TYP_UNSUBSCRIBE &&
        header.type != MQTT_TYP_UNSUBACK    
    ) {
        return bytes_written;
    }

    // For PUBLISH packets, a Packet Identifier is only used when QoS > 0.
    // The QoS level is defined by bits 1 and 2 of the flags.
    if (header.type == MQTT_TYP_PUBLISH && (header.flags & 0b0110) == 0) {
        return bytes_written;
    }

    bytes_written = write_uint16(fd, id);
    return bytes_written;
}

int prop_id_to_type(uint16_t id) {
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

ssize_t read_var_header(int fd, MqttVarHeader *props, MqttFixedHeader header) {
    ssize_t bytes_read = 0;

    // If type does not contain properties, set them to 0
    props->props_len = 0;
    props->properties = NULL;

    bytes_read += read_packet_identifier(fd, &props->pack_id, header);

    if (header.type != MQTT_TYP_CONNECT     &&
        header.type != MQTT_TYP_CONNACK     &&
        header.type != MQTT_TYP_PUBLISH     &&
        header.type != MQTT_TYP_PUBACK      &&
        header.type != MQTT_TYP_PUBREC      &&
        header.type != MQTT_TYP_PUBCOMP     &&
        header.type != MQTT_TYP_SUBSCRIBE   &&
        header.type != MQTT_TYP_SUBACK      &&
        header.type != MQTT_TYP_UNSUBSCRIBE &&
        header.type != MQTT_TYP_UNSUBACK    &&
        header.type != MQTT_TYP_DISCONNECT  &&
        header.type != MQTT_TYP_AUTH
    ) {
        return bytes_read;
    }

    // TODO: read variable stuff into stuff
    props->stuff_len = 0;
    props->stuff = NULL;

    bytes_read += read_variable_int(fd, &(props->props_len));

    props->properties = malloc(props->props_len * sizeof(MqttProperty));
    if (props->props_len > 0 && !props) { 
        fprintf(stderr, "[Memory error, stopping client]\n");
        exit(ERROR_SERVER);
    }

    for (uint32_t i = 0; i < props->props_len; i++) {
        MqttProperty prop;

        bytes_read += read_variable_int(fd, &(prop.id));
        switch (prop_id_to_type(prop.id)) {
            case MQTT_PROP_BYTE:
                bytes_read += read_uint8(fd, &prop.content.byte);
                break;
            case MQTT_PROP_TWO_BYTE:
                bytes_read += read_uint16(fd, &prop.content.two_byte);
                break;
            case MQTT_PROP_FOUR_BYTE:
                bytes_read += read_uint32(fd, &prop.content.four_byte);
                break;
            case MQTT_PROP_VAR_INT:
                bytes_read += read_variable_int(fd, &prop.content.var_int);
                break;
            case MQTT_PROP_BIN_DATA:
                bytes_read += read_binary_data(fd, &prop.content.data);
                break;
            case MQTT_PROP_STR:
                bytes_read += read_string(fd, &prop.content.string);
                break;
            case MQTT_PROP_STR_PAIR:
                bytes_read += read_string_pair(fd, &prop.content.string_pair);
                break;
            default:
                // This case should not be reached if the packet is well-formed.
                fprintf(stderr, "[Attempted to read invalid property id %d]\n", prop.id);
                exit(ERROR_CLIENT);
                break;
        }

        props->properties[i] = prop;
    }

    return bytes_read;
}

ssize_t write_var_header(int fd, MqttVarHeader *props, MqttFixedHeader header) {
    ssize_t bytes_written = 0;

    // Write the packet identifier if the packet type requires one
    bytes_written += write_packet_identifier(fd, &props->pack_id, header);

    // Check if the packet type includes a properties section.
    if (header.type != MQTT_TYP_CONNECT     &&
        header.type != MQTT_TYP_CONNACK     &&
        header.type != MQTT_TYP_PUBLISH     &&
        header.type != MQTT_TYP_PUBACK      &&
        header.type != MQTT_TYP_PUBREC      &&
        header.type != MQTT_TYP_PUBCOMP     &&
        header.type != MQTT_TYP_SUBSCRIBE   &&
        header.type != MQTT_TYP_SUBACK      &&
        header.type != MQTT_TYP_UNSUBSCRIBE &&
        header.type != MQTT_TYP_UNSUBACK    &&
        header.type != MQTT_TYP_DISCONNECT  &&
        header.type != MQTT_TYP_AUTH
    ) {
        return bytes_written;
    }

    // Write stuff
    for (ssize_t i = 0; i < props->stuff_len; i++) {
        bytes_written += write_uint8(fd, &(props->stuff[i]));
    }

    // Write the length of the properties section as a Variable Byte Integer
    bytes_written += write_variable_int(fd, &(props->props_len));

    for (uint32_t i = 0; i < props->props_len; i++) {
        MqttProperty prop = props->properties[i];

        // Write the property identifier.
        bytes_written += write_variable_int(fd, &(prop.id));

        // Write the property value based on its type.
        switch (prop_id_to_type(prop.id)) {
            case MQTT_PROP_BYTE:
                bytes_written += write_uint8(fd, &prop.content.byte);
                break;
            case MQTT_PROP_TWO_BYTE:
                bytes_written += write_uint16(fd, &prop.content.two_byte);
                break;
            case MQTT_PROP_FOUR_BYTE:
                bytes_written += write_uint32(fd, &prop.content.four_byte);
                break;
            case MQTT_PROP_VAR_INT:
                bytes_written += write_variable_int(fd, &prop.content.var_int);
                break;
            case MQTT_PROP_BIN_DATA:
                bytes_written += write_binary_data(fd, &prop.content.data);
                break;
            case MQTT_PROP_STR:
                bytes_written += write_string(fd, &prop.content.string);
                break;
            case MQTT_PROP_STR_PAIR:
                bytes_written += write_string_pair(fd, &prop.content.string_pair);
                break;
            default:
                // This case should not be reached if the packet is well-formed.
                fprintf(stderr, "[Attempted to write invalid property id %d]\n", prop.id);
                exit(ERROR_SERVER);
                break;
        }
    }

    return bytes_written;
}

void destroy_var_header(MqttVarHeader props) {
    for (uint32_t i = 0; i < props.props_len; i++) {
        MqttProperty prop = props.properties[i];
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
    free(props.properties);
}

ssize_t read_control_packet(int fd, MqttControlPacket *packet) {
    ssize_t bytes_read = 0;

    // === MQTT Control Packet Fixed Header

    MqttFixedHeader header;
    uint8_t byte;
    bytes_read += read_uint8(fd, &byte);
    header.flags = byte & 0x0F;
    header.type  = byte >> 4;
    bytes_read += read_variable_int(fd, &header.len);

    // === MQTT Control Packet Variable Header

    MqttVarHeader var_header;
    ssize_t remaining_read = 0;
    remaining_read += read_var_header(fd, &var_header, header);
    bytes_read += remaining_read;

    // === MQTT Control Packet Payload

    MqttPayload payload;
    payload.len = (ssize_t)header.len - remaining_read;

    payload.content = malloc(payload.len * sizeof(uint8_t));
    for (ssize_t i = 0; i < payload.len; i++) {
        bytes_read += read_uint8(fd, &payload.content[i]);
    }

    packet->fixed_header = header;
    packet->var_header = var_header;
    packet->payload = payload;

    return bytes_read;
}

void update_remaining_length(MqttControlPacket *packet) {
    ssize_t remaining_length = 0;
    
    // We'll use a pipe as a temporary, in-memory buffer to "write" the
    // variable header and payload to, just so we can measure their total size.
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        perror("[Pipe creation for length calculation failed]");
        exit(ERROR_SERVER);
    }

    // Write the variable header to the write-end of the pipe.
    remaining_length += write_var_header(pipe_fds[1], &packet->var_header, packet->fixed_header);
    remaining_length += packet->payload.len;

    // Close both ends of the pipe as we are done with them.
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    
    packet->fixed_header.len = remaining_length;
}

ssize_t write_control_packet(int fd, MqttControlPacket *packet) {
    ssize_t total_bytes_written = 0;

    // Fix the Remaining Length
    update_remaining_length(packet);

    // === Fixed Header

    uint8_t first_byte = (packet->fixed_header.type << 4) | (packet->fixed_header.flags & 0x0F);
    total_bytes_written += write_uint8(fd, &first_byte);
    total_bytes_written += write_variable_int(fd, &packet->fixed_header.len);

    // === Variable Header

    total_bytes_written += write_var_header(fd, &packet->var_header, packet->fixed_header);

    // === Payload

    if (packet->payload.len > 0) {
        ssize_t payload_bytes_written = write(fd, packet->payload.content, packet->payload.len);
        if (payload_bytes_written != packet->payload.len) {
            perror("[Socket writing failed for payload]");
            exit(ERROR_WRITE_FAILED);
        }
        total_bytes_written += payload_bytes_written;
    }

    return total_bytes_written;
}


void destroy_control_packet(MqttControlPacket packet) {
    free(packet.var_header.stuff_len);
    destroy_var_header(packet.var_header);
    free(packet.payload.content);
}

MqttControlPacket create_connack() {
    MqttFixedHeader fixed_header;
    fixed_header.type = MQTT_TYP_CONNACK;
    fixed_header.flags = MQTT_FLG_CONNACK;
    fixed_header.len = 0; // Updated by the send function

    MqttVarHeader var_header;
    var_header.pack_id = 0;
    var_header.stuff_len = 2;
    var_header.stuff = malloc(2 * sizeof(uint8_t));
    var_header.stuff[0] = 0;
    var_header.stuff[1] = 0;

    var_header.props_len = 3;
    
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

        // FIXME/TODO: to debug on mac, disabled fork
        // if ((childpid = fork()) == 0) {
            // We'll use this to count the bytes we've read after the MQTT Fixed Header.
            // This is so we can calculate the payload size.
            ssize_t remaining_read = 0;

            // Child process
            printf("[Uma conexão aberta]\n");
            close(listenfd);

            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 INÍCIO                        */
            /* ========================================================= */
            /* ========================================================= */

            MqttControlPacket packet;
            read_control_packet(connfd, &packet);

            // FIXED HEADER
            // printf("Fixed header:\n");
            // printf("header.len = %d\n", packet.fixed_header.len);
            // printf("header.type = %d\n", packet.fixed_header.type);
            // printf("header.flags = %d\n", packet.fixed_header.flags);

            // VAR HEADER
            // printf("Variable header:\n");
            // printf("header.flags = %d\n", packet.fixed_header.flags);
            // printf("header.type = %d\n", packet.fixed_header.type);
            // printf("header.len = %d\n", packet.fixed_header.len);
            // printf("pack_id = %d\n", packet.var_header.pack_id);
            // printf("props.len = %d\n", packet.var_header.props_len);
            // printf("payload[1] = %c\n", packet.payload.content[1]);

            // PAYLOAD
            // printf("Payload length: %d\n", packet.payload.len);
            // printf("Payload:\n");
            // for (ssize_t i = 0; i < packet.payload.len; i++) {
            //     printf("%d:%c\n", packet.payload.content[i], packet.payload.content[i]);
            // }

            if (packet.fixed_header.type != MQTT_TYP_CONNECT) {
                fprintf(stderr, "[Got invalid connection, probably not MQTT]\n");
                exit(ERROR_CLIENT);
            }



            destroy_control_packet(packet);

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
        // }
        // else {
        //     close(connfd);
        // }
    }
    exit(0);
}
