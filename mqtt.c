#include <unistd.h>

#include "mqtt.h"
#include "sockets.h"

// Read a Variable Byte Integer from a file descriptor.
ssize_t read_var_int(int fd, uint32_t *val) {
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

ssize_t write_var_int(int fd, uint32_t *val) {
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
ssize_t read_string(int fd, String *str) {
    ssize_t bytes_read = 0;
    uint32_t i = 0;
    bytes_read += read_uint16(fd, &str->len);

    str->val = (char*)malloc((str->len + 1) * sizeof(char));
    for (i = 0; i < str->len; i++) {
        bytes_read += read_uint8(fd, (uint8_t*)&(str->val)[i]);
    }
    str->val[i] = '\0';

    return bytes_read;
}

// Writing null-terminated strings, without the '\0'
ssize_t write_string(int fd, String *str) {
    ssize_t bytes_written = 0;

    bytes_written += write_uint16(fd, &str->len);
    for (uint16_t i = 0; i < str->len; i++) {
        bytes_written += write_uint8(fd, (uint8_t*)&(str->val)[i]);
    }

    return bytes_written;
}

void destroy_string(String str) {
    free(str.val);
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
    destroy_string(pair.str1);
    destroy_string(pair.str2);
}

ssize_t read_packet_identifier(int fd, PacketID *id) {
    ssize_t bytes_read = 0;
    bytes_read = read_uint16(fd, (uint16_t*)id);
    return bytes_read;
}

ssize_t write_packet_identifier(int fd, PacketID *id) {
    ssize_t bytes_written = 0;
    bytes_written = write_uint16(fd, (uint16_t*)id);
    return bytes_written;
}

MqttPropType prop_id_to_type(uint16_t id) {
    switch (id) {
        case  1:
        case 23:
        case 25:
        case 36:
        case 37:
        case 40:
        case 41:
        case 42:
            return BYTE;
        case 19:
        case 33:
        case 34:
        case 35:
            return TWO_BYTE;
        case 2:
        case 17:
        case 24:
        case 39:
            return FOUR_BYTE;
        case 11:
            return VAR_INT;
        case 9:
        case 22:
            return BIN_DATA;
        case 3:
        case 8:
        case 18:
        case 21:
        case 26:
        case 28:
        case 31:
            return STR;
        case 38:
            return STR_PAIR;
        default:
            fprintf(stderr, "[Invalid prop id %d, stopping]\n", id);
            exit(ERROR_SERVER);
    }
}

ssize_t read_properties(int fd, MqttProperty **props, var_int len) {
    ssize_t bytes_read = 0;

    if (len <= 0) {
        *props = NULL;
        return bytes_read;
    }

    *props = (MqttProperty *)malloc(len * sizeof(MqttProperty));
    if (!*props && len > 0) { 
        fprintf(stderr, "[Memory error, stopping]\n");
        exit(ERROR_SERVER);
    }

    for (uint32_t i = 0; i < len; i++) {
        MqttProperty prop;

        bytes_read += read_var_int(fd, &(prop.id));
        switch (prop_id_to_type(prop.id)) {
            case BYTE:
                bytes_read += read_uint8(fd, &prop.content.byte);
                break;
            case TWO_BYTE:
                bytes_read += read_uint16(fd, &prop.content.two_byte);
                break;
            case FOUR_BYTE:
                bytes_read += read_uint32(fd, &prop.content.four_byte);
                break;
            case VAR_INT:
                bytes_read += read_var_int(fd, &prop.content.var_int);
                break;
            case BIN_DATA:
                bytes_read += read_binary_data(fd, &prop.content.data);
                break;
            case STR:
                bytes_read += read_string(fd, &prop.content.string);
                break;
            case STR_PAIR:
                bytes_read += read_string_pair(fd, &prop.content.string_pair);
                break;
            default:
                // This case should not be reached if the packet is well-formed.
                fprintf(stderr, "[Attempted to read invalid property id %d]\n", prop.id);
                exit(ERROR_CLIENT);
                break;
        }

        (*props)[i] = prop;
    }

    return bytes_read;
}

ssize_t write_properties(int fd, MqttProperty **props, var_int len) {
    ssize_t bytes_written = 0;

    if (len <= 0) {
        return bytes_written;
    }

    for (uint32_t i = 0; i < len; i++) {
        bytes_written += write_var_int(fd, &((*props)[i].id));
        switch (prop_id_to_type((*props)[i].id)) {
            case BYTE:
                bytes_written += write_uint8(fd, &((*props)[i].content.byte));
                break;
            case TWO_BYTE:
                bytes_written += write_uint16(fd, &((*props)[i].content.two_byte));
                break;
            case FOUR_BYTE:
                bytes_written += write_uint32(fd, &((*props)[i].content.four_byte));
                break;
            case VAR_INT:
                bytes_written += write_var_int(fd, &((*props)[i].content.var_int));
                break;
            case BIN_DATA:
                bytes_written += write_binary_data(fd, &((*props)[i].content.data));
                break;
            case STR:
                bytes_written += write_string(fd, &((*props)[i].content.string));
                break;
            case STR_PAIR:
                bytes_written += write_string_pair(fd, &((*props)[i].content.string_pair));
                break;
            default:
                // This case should not be reached if the packet is well-formed.
                fprintf(stderr, "[Attempted to read invalid property id %d]\n", (*props)[i].id);
                exit(ERROR_CLIENT);
                break;
        }
    }

    return bytes_written;
}

void destroy_properties(MqttProperty *props, var_int len) {
    for (uint32_t i = 0; i < len; i++) {
        MqttProperty prop = props[i];
        switch (prop_id_to_type(prop.id)) {
            case BIN_DATA:
                destroy_binary_data(prop.content.data);
                break;
            case STR:
                destroy_string(prop.content.string);
                break;
            case STR_PAIR:
                destroy_string_pair(prop.content.string_pair);
                break;
            default:
                /* other cases don't have to be freed */
                break;
        }
    }
    free(props);
}

ssize_t read_var_header(int fd, MqttVarHeader *var_header, MqttFixedHeader fixed_header) {
    ssize_t bytes_read = 0;

    switch ((MqttControlType)fixed_header.type) {
        case CONNECT:
            bytes_read += read_string(fd, &(var_header->connect.protocol_name));
            bytes_read += read_uint8(fd, &(var_header->connect.protocol_version));
            bytes_read += read_uint8(fd, &(var_header->connect.connect_flags));
            bytes_read += read_var_int(fd, &(var_header->connect.props_len));
            bytes_read += read_properties(fd, &(var_header->connect.props), var_header->connect.props_len);
            break;
        case CONNACK:
            bytes_read += read_uint8(fd, &(var_header->connack.ack_flags));
            bytes_read += read_uint8(fd, &(var_header->connack.reason_code));
            bytes_read += read_var_int(fd, &(var_header->connack.props_len));
            bytes_read += read_properties(fd, &(var_header->connack.props), var_header->connack.props_len);
            break;
        case PUBLISH:
            bytes_read += read_string(fd, &(var_header->publish.topic_name));
            /* note: 0x6 = 0b0110 */
            if ((fixed_header.flags & 0x6) > 0) {
                bytes_read += read_packet_identifier(fd, &(var_header->publish.packet_id));
            }
            bytes_read += read_var_int(fd, &(var_header->publish.props_len));
            bytes_read += read_properties(fd, &(var_header->publish.props), var_header->publish.props_len);
            break;
        case PUBACK:
            bytes_read += read_packet_identifier(fd, &(var_header->puback.packet_id));
            bytes_read += read_uint8(fd, &(var_header->puback.reason_code));
            if (fixed_header.len - bytes_read >= 4) {
                bytes_read += read_var_int(fd, &(var_header->puback.props_len));
                bytes_read += read_properties(fd, &(var_header->puback.props), var_header->puback.props_len);
            } else {
                var_header->puback.props_len = 0;
                var_header->puback.props = NULL;
            }
            break;
        case PUBREC:
            bytes_read += read_packet_identifier(fd, &(var_header->pubrec.packet_id));
            bytes_read += read_uint8(fd, &(var_header->pubrec.reason_code));
            if (fixed_header.len - bytes_read >= 4) {
                bytes_read += read_var_int(fd, &(var_header->pubrec.props_len));
                bytes_read += read_properties(fd, &(var_header->pubrec.props), var_header->pubrec.props_len);
            } else {
                var_header->pubrec.props_len = 0;
                var_header->pubrec.props = NULL;
            }
            break;
        case PUBREL:
            bytes_read += read_packet_identifier(fd, &(var_header->pubrel.packet_id));
            bytes_read += read_uint8(fd, &(var_header->pubrel.reason_code));
            if (fixed_header.len - bytes_read >= 4) {
                bytes_read += read_var_int(fd, &(var_header->pubrel.props_len));
                bytes_read += read_properties(fd, &(var_header->pubrel.props), var_header->pubrel.props_len);
            } else {
                var_header->pubrel.props_len = 0;
                var_header->pubrel.props = NULL;
            }
            break;
        case PUBCOMP:
            bytes_read += read_packet_identifier(fd, &(var_header->pubcomp.packet_id));
            bytes_read += read_uint8(fd, &(var_header->pubcomp.reason_code));
            if (fixed_header.len - bytes_read >= 4) {
                bytes_read += read_var_int(fd, &(var_header->pubcomp.props_len));
                bytes_read += read_properties(fd, &(var_header->pubcomp.props), var_header->pubcomp.props_len);
            } else {
                var_header->pubcomp.props_len = 0;
                var_header->pubcomp.props = NULL;
            }
            break;
        case SUBSCRIBE:
            bytes_read += read_packet_identifier(fd, &(var_header->subscribe.packet_id));
            bytes_read += read_var_int(fd, &(var_header->subscribe.props_len));
            bytes_read += read_properties(fd, &(var_header->subscribe.props), var_header->subscribe.props_len);
            break;
        case SUBACK:
            bytes_read += read_packet_identifier(fd, &(var_header->suback.packet_id));
            bytes_read += read_var_int(fd, &(var_header->suback.props_len));
            bytes_read += read_properties(fd, &(var_header->suback.props), var_header->suback.props_len);
            break;
        case UNSUBSCRIBE:
            bytes_read += read_packet_identifier(fd, &(var_header->unsubscribe.packet_id));
            bytes_read += read_var_int(fd, &(var_header->unsubscribe.props_len));
            bytes_read += read_properties(fd, &(var_header->unsubscribe.props), var_header->unsubscribe.props_len);
            break;
        case UNSUBACK:
            bytes_read += read_packet_identifier(fd, &(var_header->unsuback.packet_id));
            bytes_read += read_var_int(fd, &(var_header->unsuback.props_len));
            bytes_read += read_properties(fd, &(var_header->unsuback.props), var_header->unsuback.props_len);
            break;
        case PINGREQ:
            /* empty */
            break;
        case PINGRESP:
            /* empty */
            break;
        case DISCONNECT:
            bytes_read += read_uint8(fd, &(var_header->disconnect.reason_code));
            if (fixed_header.len - bytes_read >= 2) {
                bytes_read += read_var_int(fd, &(var_header->disconnect.props_len));
                bytes_read += read_properties(fd, &(var_header->disconnect.props), var_header->disconnect.props_len);
            } else {
                var_header->disconnect.props_len = 0;
                var_header->disconnect.props = NULL;
            }
            break;
        case AUTH:
            bytes_read += read_uint8(fd, &(var_header->auth.reason_code));
            bytes_read += read_var_int(fd, &(var_header->auth.props_len));
            bytes_read += read_properties(fd, &(var_header->auth.props), var_header->auth.props_len);
            break;
        default:
            /* This case should not be reached if the packet is well-formed. */
            fprintf(stderr, "[Attempted to read invalid var header type %d]\n", fixed_header.type);
            exit(ERROR_CLIENT);
            break;
    }

    return bytes_read;
}


ssize_t write_var_header(int fd, MqttVarHeader *var_header, MqttFixedHeader fixed_header) {
    ssize_t bytes_written = 0;

    switch ((MqttControlType)fixed_header.type) {
        case CONNECT:
            bytes_written += write_string(fd, &(var_header->connect.protocol_name));
            bytes_written += write_uint8(fd, &(var_header->connect.protocol_version));
            bytes_written += write_uint8(fd, &(var_header->connect.connect_flags));
            bytes_written += write_var_int(fd, &(var_header->connect.props_len));
            bytes_written += write_properties(fd, &(var_header->connect.props), var_header->connect.props_len);
            break;
        case CONNACK:
            bytes_written += write_uint8(fd, &(var_header->connack.ack_flags));
            bytes_written += write_uint8(fd, &(var_header->connack.reason_code));
            bytes_written += write_var_int(fd, &(var_header->connack.props_len));
            bytes_written += write_properties(fd, &(var_header->connack.props), var_header->connack.props_len);
            break;
        case PUBLISH:
            bytes_written += write_string(fd, &(var_header->publish.topic_name));
            /* note: 0x6 = 0b0110 */
            if ((fixed_header.flags & 0x6) > 0) {
                bytes_written += write_packet_identifier(fd, &(var_header->publish.packet_id));
            }
            bytes_written += write_var_int(fd, &(var_header->publish.props_len));
            bytes_written += write_properties(fd, &(var_header->publish.props), var_header->publish.props_len);
            break;
        case PUBACK:
            bytes_written += write_packet_identifier(fd, &(var_header->puback.packet_id));
            bytes_written += write_uint8(fd, &(var_header->puback.reason_code));
            if (var_header->puback.props_len > 0) {
                bytes_written += write_var_int(fd, &(var_header->puback.props_len));
                bytes_written += write_properties(fd, &(var_header->puback.props), var_header->puback.props_len);
            }
            break;
        case PUBREC:
            bytes_written += write_packet_identifier(fd, &(var_header->pubrec.packet_id));
            bytes_written += write_uint8(fd, &(var_header->pubrec.reason_code));
            if (var_header->pubrec.props_len > 0) {
                bytes_written += write_var_int(fd, &(var_header->pubrec.props_len));
                bytes_written += write_properties(fd, &(var_header->pubrec.props), var_header->pubrec.props_len);
            }
            break;
        case PUBREL:
            bytes_written += write_packet_identifier(fd, &(var_header->pubrel.packet_id));
            bytes_written += write_uint8(fd, &(var_header->pubrel.reason_code));
            if (var_header->pubrel.props_len > 0) {
                bytes_written += write_var_int(fd, &(var_header->pubrel.props_len));
                bytes_written += write_properties(fd, &(var_header->pubrel.props), var_header->pubrel.props_len);
            }
            break;
        case PUBCOMP:
            bytes_written += write_packet_identifier(fd, &(var_header->pubcomp.packet_id));
            bytes_written += write_uint8(fd, &(var_header->pubcomp.reason_code));
            if (var_header->pubcomp.props_len > 0) {
                bytes_written += write_var_int(fd, &(var_header->pubcomp.props_len));
                bytes_written += write_properties(fd, &(var_header->pubcomp.props), var_header->pubcomp.props_len);
            }
            break;
        case SUBSCRIBE:
            bytes_written += write_packet_identifier(fd, &(var_header->subscribe.packet_id));
            bytes_written += write_var_int(fd, &(var_header->subscribe.props_len));
            bytes_written += write_properties(fd, &(var_header->subscribe.props), var_header->subscribe.props_len);
            break;
        case SUBACK:
            bytes_written += write_packet_identifier(fd, &(var_header->suback.packet_id));
            bytes_written += write_var_int(fd, &(var_header->suback.props_len));
            bytes_written += write_properties(fd, &(var_header->suback.props), var_header->suback.props_len);
            break;
        case UNSUBSCRIBE:
            bytes_written += write_packet_identifier(fd, &(var_header->unsubscribe.packet_id));
            bytes_written += write_var_int(fd, &(var_header->unsubscribe.props_len));
            bytes_written += write_properties(fd, &(var_header->unsubscribe.props), var_header->unsubscribe.props_len);
            break;
        case UNSUBACK:
            bytes_written += write_packet_identifier(fd, &(var_header->unsuback.packet_id));
            bytes_written += write_var_int(fd, &(var_header->unsuback.props_len));
            bytes_written += write_properties(fd, &(var_header->unsuback.props), var_header->unsuback.props_len);
            break;
        case PINGREQ:
            /* empty */
            break;
        case PINGRESP:
            /* empty */
            break;
        case DISCONNECT:
            bytes_written += write_uint8(fd, &(var_header->disconnect.reason_code));
            if (var_header->disconnect.props_len > 0) {
                bytes_written += write_var_int(fd, &(var_header->disconnect.props_len));
                bytes_written += write_properties(fd, &(var_header->disconnect.props), var_header->disconnect.props_len);
            }
            break;
        case AUTH:
            bytes_written += write_uint8(fd, &(var_header->auth.reason_code));
            bytes_written += write_var_int(fd, &(var_header->auth.props_len));
            bytes_written += write_properties(fd, &(var_header->auth.props), var_header->auth.props_len);
            break;
        default:
            /* This case should not be reached if the packet is well-formed. */
            fprintf(stderr, "[Attempted to write invalid var header type %d]\n", fixed_header.type);
            exit(ERROR_CLIENT);
            break;
    }

    return bytes_written;
}

void destroy_var_header(MqttVarHeader var_header, MqttFixedHeader fixed_header) {
    switch ((MqttControlType)fixed_header.type) {
        case CONNECT:
            destroy_string(var_header.connect.protocol_name);
            destroy_properties(var_header.connect.props, var_header.connect.props_len);
            break;
        case CONNACK:
            destroy_properties(var_header.connack.props, var_header.connack.props_len);
            break;
        case PUBLISH:
            destroy_string(var_header.publish.topic_name);
            destroy_properties(var_header.publish.props, var_header.publish.props_len);
            break;
        case PUBACK:
            destroy_properties(var_header.puback.props, var_header.puback.props_len);
            break;
        case PUBREC:
            destroy_properties(var_header.pubrec.props, var_header.pubrec.props_len);
            break;
        case PUBREL:
            destroy_properties(var_header.pubrel.props, var_header.pubrel.props_len);
            break;
        case PUBCOMP:
            destroy_properties(var_header.pubcomp.props, var_header.pubcomp.props_len);
            break;
        case SUBSCRIBE:
            destroy_properties(var_header.subscribe.props, var_header.subscribe.props_len);
            break;
        case SUBACK:
            destroy_properties(var_header.suback.props, var_header.suback.props_len);
            break;
        case UNSUBSCRIBE:
            destroy_properties(var_header.unsubscribe.props, var_header.unsubscribe.props_len);
            break;
        case UNSUBACK:
            destroy_properties(var_header.unsuback.props, var_header.unsuback.props_len);
            break;
        case PINGREQ:
            /* empty */
            break;
        case PINGRESP:
            /* empty */
            break;
        case DISCONNECT:
            destroy_properties(var_header.disconnect.props, var_header.disconnect.props_len);
            break;
        case AUTH:
            destroy_properties(var_header.auth.props, var_header.auth.props_len);
            break;
    }
}

ssize_t read_payload(int fd, MqttPayload *payload, MqttFixedHeader fixed_header) {
    ssize_t bytes_read = 0;

    /* kind of a hack... */
    ssize_t byte_len = payload->other.len;
    switch (fixed_header.type) {
        case SUBSCRIBE:
            payload->subscribe.topic_amount = 0;
            payload->subscribe.topics = NULL;
            
            while (bytes_read < byte_len) {
                payload->subscribe.topic_amount++;
                payload->subscribe.topics = (struct StringWithOptions*)realloc(
                    payload->subscribe.topics,
                    payload->subscribe.topic_amount * sizeof(struct StringWithOptions)
                );
                if (!payload->subscribe.topics) {
                    perror("[Couldn't reallocate memory for topics]\n");
                    exit(ERROR_SERVER);
                }

                size_t i = payload->subscribe.topic_amount - 1;
                bytes_read += read_string(fd, &(payload->subscribe.topics[i].str));
                bytes_read += read_uint8(fd, &(payload->subscribe.topics[i].options));
            }
            break;
        case UNSUBSCRIBE:
            payload->unsubscribe.topic_amount = 0;
            payload->unsubscribe.topics = NULL;

            while (bytes_read < byte_len) {
                payload->unsubscribe.topic_amount++;
                payload->unsubscribe.topics = (String*)realloc(
                    payload->unsubscribe.topics,
                    payload->unsubscribe.topic_amount * sizeof(String)
                );
                if (!payload->unsubscribe.topics) {
                    perror("[Couldn't reallocate memory for topics]\n");
                    exit(ERROR_SERVER);
                }

                size_t i = payload->unsubscribe.topic_amount - 1;
                bytes_read += read_string(fd, &(payload->unsubscribe.topics[i]));
            }
            break;
        default:
            payload->other.content = (uint8_t*)malloc(byte_len * sizeof(uint8_t));
            bytes_read += read_many(fd, payload->other.content, byte_len);
    }

    return bytes_read;
}

ssize_t write_payload(int fd, MqttPayload *payload, MqttFixedHeader fixed_header) {
    ssize_t bytes_written = 0;

    switch (fixed_header.type) {
        case SUBSCRIBE:
            fprintf(stderr, "[Writing SUBSCRIBE payloads not implemented.]\n");
            exit(ERROR_SERVER);
            break;
        case UNSUBSCRIBE:
            fprintf(stderr, "[Writing UNSUBSCRIBE payloads not implemented.]\n");
            exit(ERROR_SERVER);
            break;
        default:
            if (payload->other.len > 0) {
                bytes_written += write_many(fd, payload->other.content, payload->other.len);
            }
    }

    return bytes_written;
}

void destroy_payload(MqttPayload payload, MqttFixedHeader fixed_header) {
    switch (fixed_header.type) {
        case SUBSCRIBE:
            for (ssize_t i = 0; i < payload.subscribe.topic_amount; i++) {
                destroy_string(payload.subscribe.topics[i].str);
            }
            free(payload.subscribe.topics);
            break;
        case UNSUBSCRIBE:
            for (ssize_t i = 0; i < payload.unsubscribe.topic_amount; i++) {
                destroy_string(payload.unsubscribe.topics[i]);
            }
            free(payload.unsubscribe.topics);
            break;
        default:
            free(payload.other.content);
    }
}

ssize_t read_control_packet(int fd, MqttControlPacket *packet) {
    ssize_t bytes_read = 0;

    // === MQTT Control Packet Fixed Header

    MqttFixedHeader header = { 0 };
    uint8_t byte;
    bytes_read += read_uint8(fd, &byte);
    header.flags = byte & 0x0F;
    header.type  = byte >> 4;
    bytes_read += read_var_int(fd, &header.len);

    // === MQTT Control Packet Variable Header

    MqttVarHeader var_header = { 0 };
    ssize_t remaining_read = 0;
    remaining_read += read_var_header(fd, &var_header, header);
    bytes_read += remaining_read;

    // === MQTT Control Packet Payload

    MqttPayload payload = { 0 };
    /* This is kind of a hack. It would be better not to do this. */
    payload.other.len = (ssize_t)header.len - remaining_read;

    bytes_read += read_payload(fd, &payload, header);

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
        perror("[Pipe creation for length calculation failed]\n");
        exit(ERROR_SERVER);
    }

    // Write the variable header to the write-end of the pipe.
    remaining_length += write_var_header(pipe_fds[1], &packet->var_header, packet->fixed_header);
    remaining_length += write_payload(pipe_fds[1], &packet->payload, packet->fixed_header);

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
    total_bytes_written += write_var_int(fd, &packet->fixed_header.len);

    // === Variable Header

    total_bytes_written += write_var_header(fd, &packet->var_header, packet->fixed_header);

    // === Payload

    total_bytes_written += write_payload(fd, &packet->payload, packet->fixed_header);

    return total_bytes_written;
}

void destroy_control_packet(MqttControlPacket packet) {
    destroy_var_header(packet.var_header, packet.fixed_header);
    destroy_payload(packet.payload, packet.fixed_header);
}

MqttControlPacket create_connack(void) {
    MqttFixedHeader fixed_header = {
        .type  = CONNACK,
        .flags = MQTT_FLG_CONNACK,
        .len   = 0 /* updated by the send function */
    };

    MqttVarHeader var_header = { .connack = {
        .ack_flags   = 0, /* fixed to 0 */
        .reason_code = 0, /* fixed to 0 */
        .props_len   = 0,
        .props       = NULL
    }};

    MqttPayload payload = { .other = {
        .len     = 0,
        .content = NULL
    }};

    MqttControlPacket packet = {
        .fixed_header = fixed_header,
        .var_header = var_header,
        .payload = payload
    };

    return packet;
}

/* NOTE: probably shouldn't call `destroy_control_packet` on this */
MqttControlPacket create_publish(String topic_name, char *msg, size_t msg_len) {
    MqttFixedHeader fixed_header = {
        .type  = PUBLISH,
        .flags = MQTT_FLG_PUBLISH,
        .len   = 0 /* updated by the send function */
    };

    MqttVarHeader var_header = { .publish = {
        .packet_id = 0,
        .topic_name = topic_name,
        .props_len = 0,
        .props = NULL
    }};

    MqttPayload payload = { .other = {
        .content = (uint8_t*)msg,
        .len     = msg_len
    }};

    MqttControlPacket packet = {
        .fixed_header = fixed_header,
        .var_header = var_header,
        .payload = payload
    };

    return packet;
}

MqttControlPacket create_suback(MqttControlPacket subscribe) {
    MqttFixedHeader fixed_header = {
        .type  = SUBACK,
        .flags = MQTT_FLG_SUBACK,
        .len   = 0 /* updated by the send function */
    };

    MqttVarHeader var_header = { .suback = {
        .packet_id = subscribe.var_header.subscribe.packet_id,
        .props_len = 0,
        .props     = NULL
    }};

    /* Payload contains a Reason Code for each topic.
     * Send 0x0 (Granted QoS 0) to all.
     */
    size_t content_len = sizeof(uint8_t) * subscribe.payload.subscribe.topic_amount;
    uint8_t *content = (uint8_t*)malloc(content_len);

    for (size_t i = 0; i < content_len; i++) {
        /* Granted QoS 0 */
        content[i] = 0x0;
    }

    MqttPayload payload = { .other = {
        .content = content,
        .len = content_len
    }};

    MqttControlPacket packet = {
        .fixed_header = fixed_header,
        .var_header = var_header,
        .payload = payload
    };

    return packet;
}

MqttControlPacket create_unsuback(MqttControlPacket unsubscribe) {
    MqttFixedHeader fixed_header = {
        .type  = UNSUBACK,
        .flags = MQTT_FLG_UNSUBACK,
        .len   = 0 /* updated by the send function */
    };

    MqttVarHeader var_header = { .unsuback = {
        .packet_id = unsubscribe.var_header.unsubscribe.packet_id,
        .props_len = 0,
        .props     = NULL
    }};

    /* Payload contains a Reason Code for each topic.
     * Send 0x0 (Success) to all.
     */
    size_t content_len = sizeof(uint8_t) * unsubscribe.payload.unsubscribe.topic_amount;
    uint8_t *content = (uint8_t*)malloc(content_len);

    for (size_t i = 0; i < content_len; i++) {
        /* Sucess */
        content[i] = 0x0;
    }
    
    MqttPayload payload = { .other = {
        .content = content,
        .len = content_len
    }};

    MqttControlPacket packet = {
        .fixed_header = fixed_header,
        .var_header = var_header,
        .payload = payload
    };

    return packet;
}

MqttControlPacket create_pingresp(void) {
    MqttFixedHeader fixed_header = {
        .type = PINGRESP,
        .flags = MQTT_FLG_PINGRESP,
        .len = 0 /* updated by the send function */
    };

    MqttVarHeader var_header = { .pingreq = {} };

    MqttPayload payload = { .other = {
        .content = NULL,
        .len = 0
    }};

    MqttControlPacket packet = {
        .fixed_header = fixed_header,
        .var_header = var_header,
        .payload = payload
    };

    return packet;
}
