#include "flag.h"

static char** arg_iter = NULL;
static char** arg_iter_end = NULL;

static inline flag_t* find_flag_short_identifier(
    char iden, flag_t* flags, uint32_t flags_len)
{
    flag_t* iter = flags;
    flag_t* const end = flags + flags_len;
    for (; iter != end; ++iter) {
        if (iter->short_identifier == iden) {
            return iter;
        }
    }
    return NULL;
}

static inline flag_t* find_flag_long_identifier(
    char* iden, flag_t* flags, uint32_t flags_len)
{
    flag_t* iter = flags;
    flag_t* const end = flags + flags_len;
    for (; iter != end; ++iter) {
        if (strcmp(iter->long_identifier, iden) == 0) {
            return iter;
        }
    }
    return NULL;
}

static inline char* get_eq_sign(char* s)
{
    for (; *s; ++s) {
        if (*s == '=') {
            return s;
        }
    }
    return NULL;
}

static inline flag_error_t parse_flag_long_identifier(
    flag_t* flags, uint32_t flags_len)
{
    char* eq_sign = get_eq_sign(*arg_iter + 2);

    if (eq_sign) {
        *eq_sign = 0;
    }

    flag_t* f = find_flag_long_identifier(*arg_iter + 2, flags, flags_len);

    if (eq_sign) {
        *eq_sign = '=';
    }

    switch (f->type) {
    case FLAG_BOOL: {
        if (eq_sign) {
            eq_sign += 1;
            if (strcmp(eq_sign, "true") == 0) {
                *(bool*)f->target = 1;
            } else if (strcmp(eq_sign, "1") == 0) {
                *(bool*)f->target = 1;
            } else if (strcmp(eq_sign, "false") == 0) {
                *(bool*)f->target = 0;
            } else if (strcmp(eq_sign, "0") == 0) {
                *(bool*)f->target = 0;
            } else {
                return FLAG_ERROR_ARG_UNKNOWN;
            }
        } else {
            *(bool*)f->target = 1;
        }
    } break;
    case FLAG_STR: {
        if (eq_sign) {
            *(char**)f->target = eq_sign + 1;
        } else if (*(arg_iter + 1)) {
            arg_iter += 1;
            *(char**)f->target = *arg_iter;
        } else {
            return FLAG_ERROR_ARG_UNKNOWN;
        }
    } break;
    }

    return FLAG_ERROR_SUCCESS;
}

static inline flag_error_t parse_flag_short_identifier(
    flag_t* flags, uint32_t flags_len)
{
    flag_error_t e = FLAG_ERROR_SUCCESS;
    char* eq_sign = get_eq_sign(*arg_iter + 1);
    if (eq_sign) {
        *eq_sign = 0;
    }

    {
        char* iter = *arg_iter + 1;
        for (; *iter; ++iter) {

            flag_t* f = find_flag_short_identifier(*iter, flags, flags_len);

            if (!f) {
                e = FLAG_ERROR_FLAG_UNKNOWN;
                goto end;
            }

            switch (f->type) {
            case FLAG_BOOL: {
                if (eq_sign) {
                    char* a = eq_sign + 1;
                    if (strcmp(a, "true") == 0) {
                        *(bool*)f->target = 1;
                    } else if (strcmp(a, "1") == 0) {
                        *(bool*)f->target = 1;
                    } else if (strcmp(a, "false") == 0) {
                        *(bool*)f->target = 0;
                    } else if (strcmp(a, "0") == 0) {
                        *(bool*)f->target = 0;
                    } else {
                        e = FLAG_ERROR_ARG_UNKNOWN;
                        goto end;
                    }
                } else {
                    *(bool*)f->target = 1;
                }
            } break;
            case FLAG_STR: {
                if (eq_sign) {
                    *(char**)f->target = eq_sign + 1;
                } else if (*(arg_iter + 1)) {
                    arg_iter += 1;
                    *(char**)f->target = *arg_iter;
                } else {
                    e = FLAG_ERROR_ARG_UNKNOWN;
                    goto end;
                }
            } break;
            }
        }
    }

end:
    if (eq_sign) {
        *eq_sign = '=';
    }

    return e;
}

int flag_parse(const int argc, char** argv, flag_t* flags, uint32_t flags_len,
    int* dest_argc, char*** dest_argv)
{
    assert(argc > 0);
    assert(argv);
    assert(flags);
    assert(flags_len);

    arg_iter = argv;
    arg_iter_end = argv + argc;

    if (dest_argc) {
        *dest_argc = 0;
    }

    flag_error_t e = FLAG_ERROR_SUCCESS;

    for (; arg_iter != arg_iter_end; ++arg_iter) {
        if ((*arg_iter)[0] == '-' && (*arg_iter)[1] == '-') {
            e = parse_flag_long_identifier(flags, flags_len);
        } else if ((*arg_iter)[0] == '-') {
            e = parse_flag_short_identifier(flags, flags_len);
        } else {
            if (dest_argc) {
                (*dest_argv)[*dest_argc] = *arg_iter;
                *dest_argc += 1;
            }
        }
        if (e) {
            return e;
        }
    }

    if (*dest_argc) {
        (*dest_argv)[*dest_argc] = NULL;
    }

    return e;
}

void flag_print_usage(
    FILE* stream, char* general_usage, flag_t* flags, uint32_t flags_len)
{
    fprintf(stream, "%s\n\n", general_usage);
    flag_t* iter = flags;
    flag_t* const end = flags + flags_len;
    for (; iter != end; ++iter) {
        if (iter->short_identifier && iter->long_identifier) {
            fprintf(stream, "-%c/--%s\t\t%s\n", iter->short_identifier,
                iter->long_identifier, iter->description);
        } else if (iter->short_identifier) {
            fprintf(stream, "-%c\t\t%s\n", iter->short_identifier,
                iter->description);
        } else {
            fprintf(stream, "--%s\t\t%s\n", iter->long_identifier,
                iter->description);
        }
    }
    fprintf(stream, "\n\n");
}

char** flag_error_position(void) { return arg_iter; }

const char* flag_error_format(int error)
{
    static const char* strings[] = {
        [FLAG_ERROR_SUCCESS] = "success",
        [FLAG_ERROR_FLAG_UNKNOWN] = "flag unknown",
        [FLAG_ERROR_ARG_UNKNOWN] = "argument unknown",
    };
    return strings[error];
}
