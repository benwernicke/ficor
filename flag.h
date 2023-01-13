#ifndef FLAG_H
#define FLAG_H

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    FLAG_BOOL,
    FLAG_STR,
} flag_type_t;

typedef enum {
    FLAG_ERROR_SUCCESS = 0,
    FLAG_ERROR_FLAG_UNKNOWN,
    FLAG_ERROR_ARG_UNKNOWN
} flag_error_t;

typedef struct flag_t flag_t;
struct flag_t {
    char short_identifier;
    char* long_identifier;
    char* description;
    void* target;
    flag_type_t type;
};

int flag_parse(const int argc, char** argv, flag_t* flags, uint32_t flags_len,
    int* dest_argc, char*** dest_argv);
void flag_print_usage(
    FILE* stream, char* general_usage, flag_t* flags, uint32_t flags_len);

char** flag_error_position(void);
const char* flag_error_format(int error);

#endif
