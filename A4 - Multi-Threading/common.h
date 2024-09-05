#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>

// Function Prototypes
bool is_empty(char* str);
bool is_number(const char* str);
bool check_rotate_arg(char* degrees);
bool check_flip_arg(char* direction);
bool check_scale_arg(char* widthStr, char* heightStr);

#endif
