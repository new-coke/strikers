#ifndef STRIKERS_VITA_CONFIG_TEST_IO_H
#define STRIKERS_VITA_CONFIG_TEST_IO_H
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

enum ConfigTestOp { CT_OPEN, CT_READ, CT_CLOSE, CT_COPY, CT_WRITE, CT_FLUSH,
                    CT_REMOVE, CT_STAGE, CT_PUBLISH, CT_RESTORE };
enum ConfigTestTarget { CT_PRIMARY, CT_BACKUP, CT_OUTPUT };
struct ConfigTestFault {
    enum ConfigTestOp op;
    enum ConfigTestTarget target;
    int openNumber, after, used;
};
static struct ConfigTestFault config_faults[8];
static int config_fault_count, config_primary_opens, config_write_opens;
struct ConfigTestStream {
    FILE* file;
    enum ConfigTestTarget target;
    int openNumber, readError;
};
static struct ConfigTestStream config_streams[16];

static int config_ends_with(const char* text, const char* suffix)
{
    const size_t n = strlen(text), s = strlen(suffix);
    return n >= s && strcmp(text + n - s, suffix) == 0;
}

static enum ConfigTestTarget config_target(const char* path)
{
    if (config_ends_with(path, ".vita-defaults.bak"))
        return CT_BACKUP;
    return config_ends_with(path, ".tmp") ? CT_OUTPUT : CT_PRIMARY;
}

static int config_fail(enum ConfigTestOp op, enum ConfigTestTarget target, int openNumber)
{
    for (int i = 0; i < config_fault_count; ++i) {
        struct ConfigTestFault* f = &config_faults[i];
        if (!f->used && f->op == op && f->target == target &&
            (!f->openNumber || f->openNumber == openNumber)) {
            if (f->after) {
                --f->after;
                continue;
            }
            f->used = 1;
            errno = EACCES;
            return 1;
        }
    }
    return 0;
}

static struct ConfigTestStream* config_stream(FILE* file)
{
    for (size_t i = 0; i < sizeof config_streams / sizeof config_streams[0]; ++i)
        if (config_streams[i].file == file)
            return &config_streams[i];
    assert(!"untracked config stream");
    return NULL;
}

static void config_add_fault(enum ConfigTestOp op, enum ConfigTestTarget target,
                             int openNumber, int after)
{
    assert(config_fault_count < 8);
    config_faults[config_fault_count++] = (struct ConfigTestFault){op, target, openNumber, after, 0};
}

FILE* port_config_test_fopen(const char* path, const char* mode)
{
    const enum ConfigTestTarget target = config_target(path);
    const int number = target == CT_PRIMARY ? ++config_primary_opens : 0;
    if (mode[0] == 'w')
        ++config_write_opens;
    if (config_fail(CT_OPEN, target, number))
        return NULL;
    FILE* file = fopen(path, mode);
    if (file) {
        for (size_t i = 0; i < sizeof config_streams / sizeof config_streams[0]; ++i) {
            if (!config_streams[i].file) {
                config_streams[i] = (struct ConfigTestStream){file, target, number, 0};
                return file;
            }
        }
        assert(!"too many config streams");
    }
    return file;
}

char* port_config_test_fgets(char* line, int size, FILE* file)
{
    struct ConfigTestStream* s = config_stream(file);
    if (config_fail(CT_READ, s->target, s->openNumber)) {
        s->readError = 1;
        return NULL;
    }
    return fgets(line, size, file);
}

int port_config_test_fgetc(FILE* file)
{
    struct ConfigTestStream* s = config_stream(file);
    if (config_fail(CT_READ, s->target, s->openNumber)) {
        s->readError = 1;
        return EOF;
    }
    return fgetc(file);
}

int port_config_test_ferror(FILE* file)
{
    return config_stream(file)->readError || ferror(file);
}

int port_config_test_fputs(const char* text, FILE* file)
{
    struct ConfigTestStream* s = config_stream(file);
    return config_fail(CT_COPY, s->target, s->openNumber) ? EOF : fputs(text, file);
}

int port_config_test_fprintf(FILE* file, const char* format, ...)
{
    struct ConfigTestStream* s = config_stream(file);
    if (config_fail(CT_WRITE, s->target, s->openNumber))
        return -1;
    va_list args;
    va_start(args, format);
    const int result = vfprintf(file, format, args);
    va_end(args);
    return result;
}

int port_config_test_fflush(FILE* file)
{
    struct ConfigTestStream* s = config_stream(file);
    const int failed = config_fail(CT_FLUSH, s->target, s->openNumber);
    const int result = fflush(file);
    return failed ? EOF : result;
}

int port_config_test_fclose(FILE* file)
{
    struct ConfigTestStream* s = config_stream(file);
    const int failed = config_fail(CT_CLOSE, s->target, s->openNumber);
    s->file = NULL;
    const int result = fclose(file);
    return failed ? EOF : result;
}

int port_config_test_remove(const char* path)
{
    return config_fail(CT_REMOVE, config_target(path), 0) ? -1 : remove(path);
}

int port_config_test_rename(const char* from, const char* to)
{
    const enum ConfigTestTarget target = config_target(from);
    const enum ConfigTestOp op = target == CT_OUTPUT ? CT_PUBLISH :
                                 target == CT_BACKUP ? CT_RESTORE : CT_STAGE;
    if (config_fail(op, target, 0))
        return -1;
    struct stat st;
    if (!stat(to, &st)) {
        errno = EEXIST;
        return -1; // Native Vita rename never overwrites an existing destination.
    }
    return rename(from, to);
}

static void config_assert_closed(void)
{
    for (size_t i = 0; i < sizeof config_streams / sizeof config_streams[0]; ++i)
        assert(!config_streams[i].file);
}
#endif
