#include "file_io.h"
#include "ctype.h"

FILE* __find_unopened_file(void);
void __begin_critical_region(int region);
void __end_critical_region(int region);

/**
 * Offset/Address/Size: 0x504 | 0x80230D58 | size: 0x1BC
 */
int fclose(FILE* file)
{
    int flush_result, close_result;

    if (file == NULL)
        return (-1);
    if (file->file_mode.file_kind == __closed_file)
        return (0);

    flush_result = fflush(file);

    close_result = (*file->close_fn)(file->handle);

    file->file_mode.file_kind = __closed_file;
    file->handle = 0;

    if (file->file_state.free_buffer)
        free((FILE*)file->buffer);
    return ((flush_result || close_result) ? -1 : 0);
}

/**
 * Offset/Address/Size: 0x3CC | 0x80230C20 | size: 0x138
 */
int fflush(FILE* file)
{
    int pos;

    if (file == NULL)
    {
        return __flush_all();
    }

    if (file->file_state.error != 0 || file->file_mode.file_kind == __closed_file)
    {
        return -1;
    }

    if (file->file_mode.io_mode == __read)
    {
        return 0;
    }

    if (file->file_state.io_state >= __rereading)
    {
        file->file_state.io_state = __reading;
    }

    if (file->file_state.io_state == __reading)
    {
        file->buffer_length = 0;
    }

    if (file->file_state.io_state != __writing)
    {
        file->file_state.io_state = __neutral;
        return 0;
    }

    if (file->file_mode.file_kind != __disk_file)
    {
        pos = 0;
    }
    else
    {
        pos = ftell(file);
    }

    if (__flush_buffer(file, 0) != 0)
    {
        file->file_state.error = 1;
        file->buffer_length = 0;
        return -1;
    }

    file->file_state.io_state = __neutral;
    file->position = pos;
    file->buffer_length = 0;
    return 0;
}

/*
 * fopen's open path is factored into this static-inline helper so MWCC
 * allocates 'modes' as an inline-expansion temporary at the lowest stack slot
 * (sp+0x8), reproducing the target's first-use slot order [modes, __init_file
 * copy, __open_file copy]. 'file' is threaded by FILE** so it stays in r31
 * across the inlined calls; 'inline' keeps the helper from emitting a symbol.
 */
static inline void __fopen_setup(const char* filename, const char* mode, FILE** pfile)
{
    file_modes modes;
    FILE* file = *pfile;

    if (!__get_file_modes(mode, &modes))
    {
        *pfile = NULL;
        return;
    }

    __init_file(file, modes, NULL, 0x400);

    if (__open_file(filename, modes, (__file_handle*)file))
    {
        file->file_mode.file_kind = __closed_file;
        if (file->file_state.free_buffer)
            free(file->buffer);
        *pfile = NULL;
        return;
    }

    if (modes.io_mode & __append)
        fseek(file, 0, SEEK_END);
}

/**
 * Offset/Address/Size: 0x17C | 0x802309D0 | size: 0x250
 */
FILE* fopen(const char* filename, const char* mode)
{
    FILE* file;

    __begin_critical_region(2);

    file = __find_unopened_file();
    __stdio_atexit();

    if (!file)
    {
        file = NULL;
    }
    else
    {
        if (file && file->file_mode.file_kind != __closed_file)
        {
            fflush(file);
            (*file->close_fn)(file->handle);
            file->file_mode.file_kind = __closed_file;
            file->handle = 0;
            if (file->file_state.free_buffer)
                free(file->buffer);
        }

        clearerr(file);

        __fopen_setup(filename, mode, &file);
    }

    __end_critical_region(2);
    return file;
}

/**
 * Offset/Address/Size: 0x0 | 0x80230854 | size: 0x17C
 */
int __get_file_modes(const char* mode, file_modes* modes)
{
    int mode_char;
    const char* mode_str;
    unsigned char open_mode;
    int io_mode;

    modes->file_kind = __disk_file;
    modes->file_orientation = 0;
    modes->binary_io = 0;

    mode_char = *mode++;

    switch (mode_char)
    {
    case 'r':
        open_mode = 0;
        break;
    case 'w':
        open_mode = 2;
        break;
    case 'a':
        open_mode = 1;
        break;
    default:
        return 0;
    }

    mode_str = mode + 1;
    modes->open_mode = open_mode;

    switch (*mode)
    {
    case 'b':
        modes->binary_io = 1;
        if (*mode_str == '+')
        {
            mode_char = (mode_char << 8) | '+';
        }
        break;
    case '+':
        mode_char = (mode_char << 8) | '+';
        if (*mode_str == 'b')
        {
            modes->binary_io = 1;
        }
        break;
    }

    switch (mode_char)
    {
    case 'r':
        io_mode = __read;
        break;
    case 'w':
        io_mode = __write;
        break;
    case 'a':
        io_mode = __write | __append;
        break;
    case ('r' << 8) | '+':
        io_mode = __read_write;
        break;
    case ('w' << 8) | '+':
        io_mode = __read_write;
        break;
    case ('a' << 8) | '+':
        io_mode = __read | __write | __append;
        break;
    }

    modes->io_mode = io_mode;
    return 1;
}
