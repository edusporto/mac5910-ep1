#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "errors.h"

ssize_t read_many(int fd, uint8_t *byte, size_t len);
ssize_t write_many(int fd, uint8_t *byte, size_t len);
ssize_t read_uint8(int fd, uint8_t *byte);
ssize_t write_uint8(int fd, uint8_t *byte);
ssize_t read_uint16(int fd, uint16_t *val);
ssize_t write_uint16(int fd, uint16_t *val);
ssize_t read_uint32(int fd, uint32_t *val);
ssize_t write_uint32(int fd, uint32_t *val);

#endif
