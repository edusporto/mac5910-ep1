#ifndef MANAGEMENT_H
#define MANAGEMENT_H

// Base folder to store topics and messages
extern const char *base_folder;

// Checks if a directory exists
int directory_exists(const char *path);

// Creates base folder to store topics and messages.
void create_base_folder(void);

#endif
