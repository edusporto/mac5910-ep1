#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "errors.h"
#include "management.h"

const char *base_folder = "/tmp/temp.mac5910.1.11796510/";

int directory_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

void create_base_folder(void) {
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
