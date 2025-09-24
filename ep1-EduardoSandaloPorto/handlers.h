#ifndef HANDLERS_H
#define HANDLERS_H

#include "mqtt.h"

// Define buffer sizes used by the handlers
#define MAX_BASE_BUFFER 1024
#define MAX_MSG_SIZE 1024*1024

extern const char *BASE_FOLDER;

void catch_int(int dummy);

void treat_subscribe(int connfd, long long int user_id, MqttControlPacket packet);
void treat_unsubscribe(int connfd, long long int user_id, MqttControlPacket packet);
void treat_publish(long long int user_id, MqttControlPacket packet);
void treat_pingreq(int connfd);
void treat_disconnect(long long int user_id);

#endif
