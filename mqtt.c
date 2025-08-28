#include <unistd.h>

#include "mqtt.h"

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

    data->bytes = (uint8_t*)malloc(data->len * sizeof(uint8_t));
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

    *str = (char*)malloc((len + 1) * sizeof(char));
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

    props->properties = (MqttProperty *)malloc(props->props_len * sizeof(MqttProperty));
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

void destroy_properties(MqttVarHeader props) {
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

    payload.content = (uint8_t*)malloc(payload.len * sizeof(uint8_t));
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
    free((void*)packet.var_header.stuff_len);
    destroy_properties(packet.var_header);
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
    var_header.stuff = (uint8_t*)malloc(2 * sizeof(uint8_t));
    var_header.stuff[0] = 0;
    var_header.stuff[1] = 0;

    var_header.props_len = 3;
    
    // The function appears incomplete in the original header
    // Completing with minimal implementation to make it compile
    var_header.properties = NULL;

    MqttPayload payload;
    payload.len = 0;
    payload.content = NULL;

    MqttControlPacket packet;
    packet.fixed_header = fixed_header;
    packet.var_header = var_header;
    packet.payload = payload;

    return packet;
}
