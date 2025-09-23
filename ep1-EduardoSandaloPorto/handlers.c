#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <dirent.h>

#include "handlers.h"
#include "errors.h"
#include "management.h"
#include "mqtt.h"

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

void treat_publish(MqttControlPacket packet) {
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
