#include "sockets.h"

ssize_t read_many(int fd, uint8_t *byte, size_t len) {
    ssize_t bytes_read = read(fd, byte, len);
    if (bytes_read < 0 || (size_t)bytes_read < len) {
        perror("[Socket reading failed]");
        exit(ERROR_READ_FAILED);
    }
    return bytes_read;
}

ssize_t write_many(int fd, uint8_t *byte, size_t len) {
    ssize_t bytes_written = write(fd, byte, len);
    if (bytes_written < 0 || (size_t)bytes_written < len) {
        perror("[Socket writing failed]");
        exit(ERROR_READ_FAILED);
    }
    return bytes_written;
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
    return bytes_written;
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
