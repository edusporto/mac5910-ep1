#ifndef MQTT_H
#define MQTT_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "errors.h"
#include "utils.h"

/* === MQTT Control Packet types === */
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

/* === MQTT Control Packet flags === */
#define MQTT_FLG_CONNECT 0b0000
#define MQTT_FLG_CONNACK 0b0000
typedef struct MqttFlgPublish {
    uint8_t retain : 1;
    uint8_t qos    : 2;
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

/* === Data structs === */

typedef uint32_t var_int;

typedef struct BinaryData {
    uint16_t len;
    uint8_t *bytes;
} BinaryData;

typedef struct String {
    uint16_t len;
    char *val;
} String;

typedef struct StringPair {
    String str1;
    String str2;
} StringPair;

/* === MQTT control packet representation == */

/* = MQTT fixed header */

typedef struct MqttFixedHeader {
    uint8_t flags : 4;  // bits 0-3
    uint8_t type  : 4;  // bits 4-7
    uint32_t len;
} MqttFixedHeader;

/* = MQTT variable header */

union MqttPropertyContent {
    uint8_t byte;
    uint16_t two_byte;
    uint32_t four_byte;
    uint32_t var_int;
    BinaryData data;
    String string;
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

/* = MQTT Payload */

typedef struct MqttPayload {
    ssize_t len;
    uint8_t *content;
} MqttPayload;

/* Full MQTT control packet */
typedef struct MqttControlPacket {
    MqttFixedHeader fixed_header;
    MqttVarHeader var_header;
    MqttPayload payload;
} MqttControlPacket;

/* === Function declarations === */
ssize_t read_variable_int(int fd, uint32_t *val);
ssize_t write_variable_int(int fd, uint32_t *val);

ssize_t read_binary_data(int fd, BinaryData *data);
ssize_t write_binary_data(int fd, BinaryData *data);
void destroy_binary_data(BinaryData data);

ssize_t read_string(int fd, String *str);
ssize_t write_string(int fd, String *str);
void destroy_string(String str);

ssize_t read_string_pair(int fd, StringPair *pair);
ssize_t write_string_pair(int fd, StringPair *pair);
void destroy_string_pair(StringPair pair);

ssize_t read_packet_identifier(int fd, uint16_t *id, MqttFixedHeader header);
ssize_t write_packet_identifier(int fd, uint16_t *id, MqttFixedHeader header);

int prop_id_to_type(uint16_t id);
ssize_t read_var_header(int fd, MqttVarHeader *props, MqttFixedHeader header);
ssize_t write_var_header(int fd, MqttVarHeader *props, MqttFixedHeader header);
void destroy_var_header(MqttVarHeader props);

ssize_t read_control_packet(int fd, MqttControlPacket *packet);
void update_remaining_length(MqttControlPacket *packet);
ssize_t write_control_packet(int fd, MqttControlPacket *packet);
void destroy_control_packet(MqttControlPacket packet);

MqttControlPacket create_connack();

#endif
