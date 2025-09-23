#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "errors.h"
#include "management.h"

int directory_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/* Helper function. Not in `management.h` */
int internal_remove_dir(const char *path) {
    char command[1024];
    snprintf(command, sizeof(command), "rm -rf \"%s\"", path);
    return system(command);
}

int fresh_dir(const char *path) {
    int existed = directory_exists(path);

    if (existed) {
        if (internal_remove_dir(path) != 0) {
            fprintf(stderr, "[ERROR: Could not delete existing directory '%s']\n", path);
            exit(ERROR_SERVER);
        }
    }
    if (mkdir(path, 0755) == -1) {
        fprintf(stderr, "[ERROR: Could not create directory '%s']\n", path);
        exit(ERROR_SERVER);
    }
    
    return existed;
}

int ensure_dir(const char *path) {
    int existed = directory_exists(path);

    if (!existed) {
        if (mkdir(path, 0755) == -1) {
            fprintf(stderr, "[ERROR: Could not create directory '%s']\n", path);
            exit(ERROR_SERVER);
        }
    }
    
    return existed;
}

int remove_dir(const char *path) {
    int existed = directory_exists(path);

    if (existed) {
        if (internal_remove_dir(path) != 0) {
            fprintf(stderr, "[ERROR: Could not remove directory '%s']\n", path);
            exit(ERROR_SERVER);
        }
    }

    return existed;
}

int fifo_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISFIFO(st.st_mode));
}

int fresh_fifo(const char *path) {
    int existed = fifo_exists(path);

    if (existed) {
        if (unlink(path) == -1) {
            fprintf(stderr, "[ERROR: Could not delete existing FIFO '%s']\n", path);
            exit(ERROR_SERVER);
        }
    }
    if (mkfifo(path, 0644) == -1) {
        fprintf(stderr, "[ERROR: Could not create FIFO '%s']\n", path);
        exit(ERROR_SERVER);
    }
    
    return existed;
}

int ensure_fifo(const char *path) {
    int existed = fifo_exists(path);

    if (!existed) {
        if (mkfifo(path, 0666) == -1) {
            fprintf(stderr, "[ERROR: Could not create FIFO '%s']\n", path);
            exit(ERROR_SERVER);
        }
    }
    
    return existed;
}

int remove_fifo(const char *path) {
    int existed = fifo_exists(path);

    if (existed) {
        if (unlink(path) == -1) {
            fprintf(stderr, "[ERROR: Could not remove FIFO '%s']\n", path);
            exit(ERROR_SERVER);
        }
    }

    return existed;
}
