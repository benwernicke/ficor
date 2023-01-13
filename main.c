#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "flag.h" // @source: flag.c

static const uint64_t SIGNATURE = 0xF1C0F1C0F1C0F1C0UL;

// ficor file spec
//                 8: signature
//                 4: ficor_sz
//      for ficor_sz:
//                     4: ficor.file_sz
//         ficor.file_sz: ficor.file
//                     4: ficor.info_sz
//         ficor.info_sz: ficor.info
//                     4: ficor.flag_buf_sz
//      ficorflag_buf_sz: ficor.flag_buf
//                     4: ficor.flag_sz

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

typedef struct ficor_t ficor_t;
struct ficor_t {
    char*    info;
    char*    file;
    char*    flag_buf;
    char**   flag;
    uint16_t file_sz;
    uint16_t flag_buf_sz;
    uint16_t flag_sz;
    uint16_t info_sz;
};

static ficor_t* ficor    = NULL;
static uint32_t ficor_sz = 0;

static char*    ficor_file = ".ficor";

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

        if (ficor[i].flag_buf_sz) {

            ficor[i].flag_buf = malloc(ficor[i].flag_buf_sz);
            ERR_IF(!ficor[i].flag_buf_sz, ERR_BAD_MALLOC);
            fread(&ficor[i].flag_buf_sz, 1, sizeof(ficor[i].flag_buf_sz), f);

            fread(&ficor[i].flag_sz, 1, sizeof(ficor[i].flag_sz), f);

            ficor[i].flag = malloc(ficor[i].flag_sz * sizeof(*ficor[i].flag));
            ERR_IF(!ficor[i].flag, ERR_BAD_MALLOC);

            // parse flags
            {
                char* s = ficor[i].flag_buf;
                uint32_t j = 0;
                for (; j < ficor[i].flag_buf_sz; ++j, ++s) {
                    ficor[i].flag[j] = s;
                    for (; *s; ++s) {  }
                }
            }
        } else {
            ficor[i].flag_buf = NULL;
            ficor[i].flag     = NULL;
            ficor[i].flag_sz  = 0;
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

        fwrite(&i->flag_buf_sz, 1, sizeof(i->flag_buf_sz), f);
        if (i->flag_buf_sz) {
            fwrite(i->flag_buf, 1, i->flag_buf_sz, f);
            free(i->flag_buf);
            fwrite(&i->flag_sz, 1, sizeof(i->flag_sz), f);
            free(i->flag);
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

static bool flag_help = 0;
static bool flag_init = 0;

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
    }
};
static const uint32_t flags_len = sizeof(flags) / sizeof(*flags);

void init(void)
{
    FILE* f = fopen(ficor_file, "wb");
    ERR_IF_MSG(!f, ERR_FILE, "could not open file '%s': %s", ficor_file, strerror(errno));

    fwrite(&SIGNATURE, 1, sizeof(SIGNATURE), f);
    uint16_t zero = 0;
    fwrite(&zero, 1, sizeof(zero), f);

    fclose(f);
    return;

error:
    if (f) fclose(f);
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

    unload_ficor();
    ERR_FORWARD_MSG("could not unload file additional output above");

error:
    return 1;
}
