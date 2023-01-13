#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "flag.h" // @source: flag.c

static const uint64_t SIGNATURE = 0xF1C0F1C0F1C0F1C0UL;

// flag stuff

static bool  flag_help     = 0;
static bool  flag_init     = 0;
static bool  flag_dump     = 0;
static char* flag_add_file = NULL;
static char* flag_set_info = NULL;
static char* flag_set_tag  = NULL;
static char* flag_include  = NULL;
static char* flag_exclude  = NULL;
static bool  flag_info     = 0;
static char* ficor_file = ".ficor";

static flag_t flags[] = {
    {
        .short_identifier = 'h',
        .long_identifier  = "help",
        .description      = "show this page and exit",
        .target           = &flag_help,
        .type             = FLAG_BOOL,
    },
    {
        .short_identifier = 0,
        .long_identifier  = "init",
        .description      = "init current working directory as ficor directory",
        .target           = &flag_init,
        .type             = FLAG_BOOL,
    },
    {
        .short_identifier = 0,
        .long_identifier  = "dump",
        .description      = "dump all decorators",
        .target           = &flag_dump,
        .type             = FLAG_BOOL,
    },
    {
        .short_identifier = 0,
        .long_identifier  = "add-file",
        .description      = "add a file to decor: '--add-file <filename> [-t <tags>] [-set-info <info>]'",
        .target           = &flag_add_file,
        .type             = FLAG_STR,
    },
    {
        .short_identifier = 't',
        .long_identifier  = "set-tag",
        .description      = "set tag. Used by --add-file, --edit-file, --add-tag",
        .target           = &flag_set_tag,
        .type             = FLAG_STR,
    },
    {
        .short_identifier = 0,
        .long_identifier  = "set-info",
        .description      = "set info. Used by --add-file, --edit-file --add-info",
        .target           = &flag_set_info,
        .type             = FLAG_STR,
    },
    {
        .short_identifier = 'i',
        .long_identifier  = "include",
        .description      = "only include flags with given tags in output",
        .target           = &flag_include,
        .type             = FLAG_STR,
    },
    {
        .short_identifier = 'e',
        .long_identifier  = "exclude",
        .description      = "exclude flags with given tags from output",
        .target           = &flag_exclude,
        .type             = FLAG_STR,
    },
    {
        .short_identifier = 0,
        .long_identifier  = "info",
        .description      = "print info to output",
        .target           = &flag_info,
        .type             = FLAG_BOOL,
    },
    {
        .short_identifier = 0,
        .long_identifier  = "config",
        .description      = "use given file as config",
        .target           = &ficor_file,
        .type             = FLAG_STR,
    },
};

static const uint32_t flags_len = sizeof(flags) / sizeof(*flags);

// ficor file spec
//                 8: signature
//                 4: ficor_sz
//      for ficor_sz:
//                     4: ficor.file_sz
//         ficor.file_sz: ficor.file
//                     4: ficor.info_sz
//         ficor.info_sz: ficor.info
//                     4: ficor.tag_buf_sz
//      ficortag_buf_sz: ficor.tag_buf
//                     4: ficor.tag_sz

typedef enum {
    ERR_OK = 0,
    ERR_BAD_MALLOC,
    ERR_FILE,
    ERR_FLAG,
} err_t;

static err_t error = ERR_OK;

void format_error(void)
{
    switch (error) {
    case ERR_BAD_MALLOC:
        fprintf(stderr, "Error: could not allocate memory: %s", strerror(errno));
        break;
    case ERR_FILE:
    case ERR_FLAG:
    case ERR_OK:
    default:
        break;
    }
}

#define ERR(e) do { error = e; goto error; } while (0)
#define ERR_IF(b, e) do { if ( b ) { ERR(e); } } while (0)
#define ERR_IF_MSG(b, e, ...)               \
    do {                                    \
        if (b) {                            \
            fprintf(stderr, "Error: ");     \
            fprintf(stderr, __VA_ARGS__);   \
            fprintf(stderr, "\n");          \
            ERR(e);                         \
        }                                   \
    } while(0)

#define ERR_FORWARD_MSG(...)                \
    do {                                    \
        if (error != ERR_OK) {              \
            fprintf(stderr, "Error: ");     \
            fprintf(stderr, __VA_ARGS__);   \
            fprintf(stderr, "\n");          \
            goto error;                     \
        }                                   \
    } while (0)

#define ERR_FORWARD(...)                \
    do {                                    \
        if (error != ERR_OK) {              \
            goto error;                     \
        }                                   \
    } while (0)

typedef struct ficor_t ficor_t;
struct ficor_t {
    char*    info;
    char*    file;
    char*    tag_buf;
    char**   tag;
    uint32_t file_sz;
    uint32_t tag_buf_sz;
    uint32_t tag_sz;
    uint32_t info_sz;
};

static ficor_t* ficor    = NULL;
static uint32_t ficor_sz = 0;


static void load_ficor(void)
{
    FILE* f = fopen(ficor_file, "rb");
    ERR_IF_MSG(!f, ERR_FILE, "could not open file '%s': %s", 
               ficor_file, 
               strerror(errno));
    
    uint64_t sig;
    fread(&sig, 1, sizeof(sig), f);
    ERR_IF_MSG(sig != SIGNATURE, ERR_FILE, 
               "%s is not a valid ficor file",
               ficor_file);

    fread(&ficor_sz, 1, sizeof(ficor_sz), f);
    ficor = calloc(ficor_sz + 1, sizeof(*ficor));
    ERR_IF(!ficor, ERR_BAD_MALLOC);

    uint32_t i = 0;
    for (; i < ficor_sz; ++i) {
        fread(&ficor[i].file_sz, 1, sizeof(ficor[i].file_sz), f);
        ficor[i].file = malloc(ficor[i].file_sz);
        ERR_IF(!ficor[i].file, ERR_BAD_MALLOC);
        fread(ficor[i].file, 1, ficor[i].file_sz, f);

        fread(&ficor[i].info_sz, 1, sizeof(ficor[i].info_sz), f);
        if (ficor[i].info_sz) {
            ficor[i].info = malloc(ficor[i].info_sz);
            ERR_IF(!ficor[i].info, ERR_BAD_MALLOC);
            fread(ficor[i].info, 1, ficor[i].info_sz, f);
        } else {
            ficor[i].info = NULL;
        }

        fread(&ficor[i].tag_buf_sz, 1, sizeof(ficor[i].tag_buf_sz), f);
        if (ficor[i].tag_buf_sz) {

            ficor[i].tag_buf = malloc(ficor[i].tag_buf_sz);
            ERR_IF(!ficor[i].tag_buf_sz, ERR_BAD_MALLOC);
            fread(ficor[i].tag_buf, 1, ficor[i].tag_buf_sz, f);

            fread(&ficor[i].tag_sz, 1, sizeof(ficor[i].tag_sz), f);

            ficor[i].tag = malloc(ficor[i].tag_sz * sizeof(*ficor[i].tag));
            ERR_IF(!ficor[i].tag, ERR_BAD_MALLOC);

            // parse tags
            {
                char* s = ficor[i].tag_buf;
                uint32_t j = 0;
                for (; j < ficor[i].tag_sz; ++j, ++s) {
                    ficor[i].tag[j] = s;
                    for (; *s; ++s) {  }
                }
            }
        } else {
            ficor[i].tag_buf = NULL;
            ficor[i].tag     = NULL;
            ficor[i].tag_sz  = 0;
        }
    }

    fclose(f);

    return;

error:
    if (f) fclose(f);
    return;
}

void unload_ficor(void)
{
    FILE* f = fopen(ficor_file, "wb");
    ERR_IF_MSG(!f, ERR_FILE, "could not open file '%s': %s", ficor_file, strerror(errno));

    fwrite(&SIGNATURE, 1, sizeof(SIGNATURE), f);
    fwrite(&ficor_sz, 1, sizeof(ficor_sz), f);

    ficor_t* i = ficor;
    ficor_t* const e = ficor + ficor_sz;

    for (; i != e; ++i) {
        fwrite(&i->file_sz, 1, sizeof(i->file_sz), f);
        fwrite(i->file, 1, i->file_sz, f);
        free(i->file);

        fwrite(&i->info_sz, 1, sizeof(i->info_sz), f);
        if (i->info_sz) {
            fwrite(i->info, 1, i->info_sz, f);
            free(i->info);
        }

        fwrite(&i->tag_buf_sz, 1, sizeof(i->tag_buf_sz), f);
        if (i->tag_buf_sz) {
            fwrite(i->tag_buf, 1, i->tag_buf_sz, f);
            free(i->tag_buf);
            fwrite(&i->tag_sz, 1, sizeof(i->tag_sz), f);
            free(i->tag);
        }
    }

    free(ficor);
    ficor    = NULL;
    ficor_sz = 0;

    fclose(f);
    return;

error:
    if (f) fclose(f);
    return;
}



void init(void)
{
    FILE* f = fopen(ficor_file, "wb");
    ERR_IF_MSG(!f, ERR_FILE, "could not open file '%s': %s", ficor_file, strerror(errno));

    fwrite(&SIGNATURE, 1, sizeof(SIGNATURE), f);
    uint32_t zero = 0;
    fwrite(&zero, 1, sizeof(zero), f);

    fclose(f);
    return;

error:
    if (f) fclose(f);
    return;
}

void dump(void)
{
    ficor_t*       f = ficor;
    ficor_t* const e = ficor + ficor_sz;
    for (; f != e; ++f) {
        printf("Name:\n\t%s\nInfo:\n", f->file);
        if (f->info) {
            printf("\t%s\n", f->info);
        }
        printf("Tags:\n");

        char** t = f->tag;
        char** const te = f->tag + f->tag_sz;
        for (; t != te; ++t) {
            printf("\t%s\n", *t);
        }
        putc('\n', stdout);
    }
}

void add_file(void)
{
    ficor_t* f = &ficor[ficor_sz++];

    f->file_sz = strlen(flag_add_file) + 1;
    f->file = malloc(f->file_sz);
    ERR_IF(!f->file, ERR_BAD_MALLOC);
    strcpy(f->file, flag_add_file);

    if (flag_set_tag) {
        f->tag_buf_sz = strlen(flag_set_tag) + 1;
        f->tag_buf    = malloc(f->tag_buf_sz);
        ERR_IF(!f->tag_buf, ERR_BAD_MALLOC);
        strcpy(f->tag_buf, flag_set_tag);

        f->tag_sz = 1;
        char* s = f->tag_buf;
        for (; *s; ++s) {
            if (*s == ':') {
                *s++ = 0;
                f->tag_sz += 1;
            }
        }

        s = f->tag_buf;
        f->tag = malloc(f->tag_sz * sizeof(*f->tag));
        ERR_IF(!f->tag, ERR_BAD_MALLOC);

        char** t = f->tag;
        char** const te = f->tag + f->tag_sz;

        for (; t != te; ++t, ++s) {
            *t = s;
            for (; *s; ++s) {  }
        }

    } else {
        f->tag_buf_sz = 0;
    }

    if (flag_set_info) {
        f->info_sz = strlen(flag_set_info) + 1;
        f->info    = malloc(f->info_sz);
        strcpy(f->info, flag_set_info);
    } else {
        f->info_sz = 0;
    }
    
error:
    return;
}

static char** find_tag(char** buf, uint32_t sz, char* tag)
{
    char** iter = buf;
    char** end  = buf + sz;
    for (; iter != end; ++iter) {
        if (strcmp(*iter, tag) == 0) {
            return iter;
        }
    }
    return NULL;
}

static bool is_tag_disjunct(char** a, uint32_t a_sz, char** b, uint32_t b_sz)
{
    char** iter = b;
    char** end  = b + b_sz;
    for (; iter != end; ++iter) {
        if (find_tag(a, a_sz, *iter)) {
            return 0;
        }
    }
    return 1;
}

static bool is_tag_subset(char** a, uint32_t a_sz, char** b, uint32_t b_sz)
{
    char** iter = b;
    char** end  = b + b_sz;
    for (; iter != end; ++iter) {
        if (!find_tag(a, a_sz, *iter)) {
            return 0;
        }
    }
    return 1;
}

static void list(void)
{
    char**   include    = NULL;
    uint32_t include_sz = 0;
    char**   exclude    = NULL;
    uint32_t exclude_sz = 0;

    if (flag_include) {
        include_sz = 1;    
        char* s = flag_include;
        for (; *s; ++s) {
            if (*s == ':') {
                *s++ = 0;
                include_sz += 1;
            }
        }

        include = malloc(include_sz * sizeof(*include));
        ERR_IF(!include, ERR_BAD_MALLOC);

        char** t  = include;
        char** te = include + include_sz;
        s = flag_include;
        for (; t != te; ++t) {
            *t = s;
            for (; *s; ++s) {  }
        }
    }

    if (flag_exclude) {
        exclude_sz = 1;
        char* s = flag_exclude;
        for (; *s; ++s) {
            if (*s == ':') {
                *s++ = 0;
                exclude_sz += 1;
            }
        }

        exclude = malloc(exclude_sz * sizeof(*exclude));
        ERR_IF(!exclude, ERR_BAD_MALLOC);

        char** t  = exclude;
        char** te = exclude + exclude_sz;
        s = flag_exclude;
        for (; t != te; ++t, ++s) {
            *t = s;
            for (; *s; ++s) {  }
        }
    }

    ficor_t* f = ficor;
    ficor_t* fe = ficor + ficor_sz;

    for (; f != fe; ++f) {
        if (include && !is_tag_subset(f->tag, f->tag_sz, include, include_sz)) {
            continue;
        }
        if (exclude && !is_tag_disjunct(f->tag, f->tag_sz, exclude, include_sz)) {
            continue;
        }
        printf("%s", f->file);
        if (flag_info && f->info) {
            printf(" %s", f->info);
        }
        putc('\n', stdout);
    }

    free(include);
    free(exclude);

    return;

error:
    free(include);
    free(exclude);

    return;
}

int main(int argc, char** argv)
{
    // flag stuff
    {
        int e = flag_parse(argc, argv, flags, flags_len, &argc, &argv);
        ERR_IF_MSG(e, ERR_FLAG, "while parsing flags: %s: %s", flag_error_format(e), *flag_error_position());
    }

    if (flag_help) {
        flag_print_usage(stdout, "Simple file decorator tool", flags, flags_len);
        exit(0);
    }

    if (flag_init) {
        init();
        ERR_FORWARD_MSG("could not initialize direcoty additional output above");
        exit(0);
    }

    load_ficor();
    ERR_FORWARD_MSG("could not load file additional output above");

    if (flag_add_file) {
        add_file();
        ERR_FORWARD();
    } else {
        list();
    }

    if (flag_dump) {
        dump();
    }

    unload_ficor();
    ERR_FORWARD_MSG("could not unload file additional output above");

    return 0;

error:
    return 1;
}
