#ifndef MANAGEMENT_H
#define MANAGEMENT_H

// Checks if a directory exists
int directory_exists(const char *path);

int fresh_dir(const char *path);
int ensure_dir(const char *path);
int remove_dir(const char *path);

int fresh_fifo(const char *path);
int ensure_fifo(const char *path);
int remove_fifo(const char *path);

#endif
