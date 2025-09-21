#ifndef MQTT_H
#define MQTT_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "errors.h"

/* === MQTT Control Packet types === */
typedef enum MqttControlType {
    CONNECT     =  1,
    CONNACK     =  2,
    PUBLISH     =  3,
    PUBACK      =  4,
    PUBREC      =  5,
    PUBREL      =  6,
    PUBCOMP     =  7,
    SUBSCRIBE   =  8,
    SUBACK      =  9,
    UNSUBSCRIBE = 10,
    UNSUBACK    = 11,
    PINGREQ     = 12,
    PINGRESP    = 13,
    DISCONNECT  = 14,
    AUTH        = 15,
} MqttControlType;

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

typedef enum MqttPropType {
    BYTE      = 0,
    TWO_BYTE  = 1,
    FOUR_BYTE = 2,
    VAR_INT   = 3,
    BIN_DATA  = 4,
    STR       = 5,
    STR_PAIR  = 6,
} MqttPropType;

/* === Data structs === */

typedef uint32_t var_int;

/* Packet identifiers are actually 16-bit, but we encode it
 * as 32 bits to deal with packets that may optionally contain
 * a packet ID. */
typedef uint32_t PacketID;

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

typedef struct MqttVar_Connect {
    String protocol_name;
    uint8_t protocol_version;
    uint8_t connect_flags;
    var_int props_len;
    MqttProperty *props;
} MqttVar_Connect;

typedef struct MqttVar_Connack {
    uint8_t ack_flags;
    uint8_t reason_code;
    var_int props_len;
    MqttProperty *props;
} MqttVar_Connack;

typedef struct MqttVar_Publish {
    String topic_name;
    PacketID packet_id;
    var_int props_len;
    MqttProperty *props;
} MqttVar_Publish;

typedef struct MqttVar_Puback {
    PacketID packet_id;
    uint8_t reason_code;
    // CHECK REMAINING LENGTH
    var_int props_len;
    MqttProperty *props;
} MqttVar_Puback;

typedef struct MqttVar_Pubrec {
    PacketID packet_id;
    uint8_t reason_code;
    // CHECK REMAINING LENGTH
    var_int props_len;
    MqttProperty *props;
} MqttVar_Pubrec;

typedef struct MqttVar_Pubrel {
    PacketID packet_id;
    uint8_t reason_code;
    // CHECK REMAINING LENGTH
    var_int props_len;
    MqttProperty *props;
} MqttVar_Pubrel;

typedef struct MqttVar_Pubcomp {
    PacketID packet_id;
    uint8_t reason_code;
    // CHECK REMAINING LENGTH
    var_int props_len;
    MqttProperty *props;
} MqttVar_Pubcomp;

typedef struct MqttVar_Subscribe {
    PacketID packet_id;
    var_int props_len;
    MqttProperty *props;
} MqttVar_Subscribe;

typedef struct MqttVar_Suback {
    PacketID packet_id;
    var_int props_len;
    MqttProperty *props;
} MqttVar_Suback;

typedef struct MqttVar_Unsubscribe {
    PacketID packet_id;
    var_int props_len;
    MqttProperty *props;
} MqttVar_Unsubscribe;

typedef struct MqttVar_Unsuback {
    PacketID packet_id;
    var_int props_len;
    MqttProperty *props;
} MqttVar_Unsuback;

typedef struct MqttVar_Pingreq {
    /* empty */
} MqttVar_Pingreq;

typedef struct MqttVar_Pingresp {
    /* empty */
} MqttVar_Pingresp;

typedef struct MqttVar_Disconnect {
    uint8_t reason_code;
    // CHECK REMAINING_LENGTH
    var_int props_len;
    MqttProperty *props;
} MqttVar_Disconnect;

typedef struct MqttVar_Auth {
    uint8_t reason_code;
    var_int props_len;
    MqttProperty *props;
} MqttVar_Auth;

typedef union MqttVarHeader {
    MqttVar_Connect connect;
    MqttVar_Connack connack;
    MqttVar_Publish publish;
    MqttVar_Puback puback;
    MqttVar_Pubrec pubrec;
    MqttVar_Pubrel pubrel;
    MqttVar_Pubcomp pubcomp;
    MqttVar_Subscribe subscribe;
    MqttVar_Suback suback;
    MqttVar_Unsubscribe unsubscribe;
    MqttVar_Unsuback unsuback;
    MqttVar_Pingreq pingreq;
    MqttVar_Pingresp pingresp;
    MqttVar_Disconnect disconnect;
    MqttVar_Auth auth;
} MqttVarHeader;

/* = MQTT Payload */

// TODO: treat each payload correctly

// typedef MqttPay_Connect

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
ssize_t read_var_int(int fd, uint32_t *val);
ssize_t write_var_int(int fd, uint32_t *val);

ssize_t read_binary_data(int fd, BinaryData *data);
ssize_t write_binary_data(int fd, BinaryData *data);
void destroy_binary_data(BinaryData data);

ssize_t read_string(int fd, String *str);
ssize_t write_string(int fd, String *str);
void destroy_string(String str);

ssize_t read_string_pair(int fd, StringPair *pair);
ssize_t write_string_pair(int fd, StringPair *pair);
void destroy_string_pair(StringPair pair);

ssize_t read_packet_identifier(int fd, PacketID *id);
ssize_t write_packet_identifier(int fd, PacketID *id);

MqttPropType prop_id_to_type(uint16_t id);
ssize_t read_properties(int fd, MqttProperty **props, var_int len);
ssize_t write_properties(int fd, MqttProperty **props, var_int len);
void destroy_properties(MqttProperty *props, var_int len);

ssize_t read_var_header(int fd, MqttVarHeader *var_header, MqttFixedHeader fixed_header);
ssize_t write_var_header(int fd, MqttVarHeader *var_header, MqttFixedHeader fixed_header);
void destroy_var_header(MqttVarHeader var_header, MqttFixedHeader fixed_header);

ssize_t read_control_packet(int fd, MqttControlPacket *packet);
void update_remaining_length(MqttControlPacket *packet);
ssize_t write_control_packet(int fd, MqttControlPacket *packet);
void destroy_control_packet(MqttControlPacket packet);

MqttControlPacket create_connack();

#endif
