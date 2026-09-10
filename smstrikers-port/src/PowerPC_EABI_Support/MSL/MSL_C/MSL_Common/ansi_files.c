#include "ansi_files.h"
#include "critical_regions.h"

static unsigned char stdin_buff[0x100];

static unsigned char stdout_buff[0x100];

static unsigned char stderr_buff[0x100];

extern files __files = {
    {
        0,
        0,
        1,
        1,
        2,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        { 0, 0 },
        { 0, 0 },
        0,
        stdin_buff,
        0x00000100,
        stdin_buff,
        0,
        0,
        0,
        0,
        NULL,
        __read_console,
        __write_console,
        __close_console,
        NULL,
        &__files._stdout,
    },
    {
        1,
        0,
        2,
        1,
        2,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        { 0, 0 },
        { 0, 0 },
        0,
        stdout_buff,
        0x00000100,
        stdout_buff,
        0,
        0,
        0,
        0,
        NULL,
        __read_console,
        __write_console,
        __close_console,
        NULL,
        &__files._stderr,
    },
    {
        2,
        0,
        2,
        0,
        2,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        { 0, 0 },
        { 0, 0 },
        0,
        stderr_buff,
        0x00000100,
        stderr_buff,
        0,
        0,
        0,
        0,
        NULL,
        __read_console,
        __write_console,
        __close_console,
        NULL,
        &__files.empty,
    },
    {
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        { 0, 0 },
        { 0, 0 },
        0,
        NULL,
        0x00000000,
        NULL,
        0,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    },
};

/**
 * Offset/Address/Size: 0x280 | 0x8022C248 | size: 0x90
 */
FILE* __find_unopened_file(void)
{
    FILE* file = __files._stderr.next_file;
    FILE* last_file;

    while (file != NULL)
    {
        if (file->file_mode.file_kind == __closed_file)
        {
            return (file);
        }
        else
        {
            last_file = file;
            file = file->next_file;
        }
    }

    if ((file = (FILE*)malloc(sizeof(FILE))) != NULL) /* mm 981007 */
    {                                                 /* mm 981007 */
        memset((void*)file, 0, sizeof(FILE));         /* mm 981007 */
        file->is_dynamically_allocated = 1;           /* mm 981007 */
        last_file->next_file = file;                  /* mm 981007 */
        return (file);                                /* mm 981007 */
    }
    return (NULL);
}

/**
 * Offset/Address/Size: 0x1A4 | 0x8022C16C | size: 0xDC
 */
void __init_file(FILE* file, file_modes mode, char* buff, size_t size)
{
    file->handle = 0;
    file->file_mode = mode;
    file->file_state.io_state = __neutral;
    file->file_state.free_buffer = 0;
    file->file_state.eof = 0;
    file->file_state.error = 0;
    file->position = 0;

    if (size)
        setvbuf(file, buff, _IOFBF, size);
    else
        setvbuf(file, 0, _IONBF, 0);

    file->buffer_ptr = file->buffer;
    file->buffer_length = 0;

    if (file->file_mode.file_kind == __disk_file)
    {
        file->position_fn = __position_file;
        file->read_fn = __read_file;
        file->write_fn = __write_file;
        file->close_fn = __close_file;
    }

    file->idle_fn = 0;
}

/**
 * Offset/Address/Size: 0xFC | 0x8022C0C4 | size: 0xA8
 */
void __close_all(void)
{
    FILE* file = &__files._stdin;
    FILE* last_file;

    __begin_critical_region(stdin_access);

    while (file != NULL)
    {
        if (file->file_mode.file_kind != __closed_file)
        {
            fclose(file);
        }

        last_file = file;
        file = file->next_file;

        if (last_file->is_dynamically_allocated)
        {
            free(last_file);
        }
        else
        {
            last_file->file_mode.file_kind = __unavailable_file;
            if (file != NULL && file->is_dynamically_allocated)
            {
                last_file->next_file = NULL;
            }
        }
    }

    __end_critical_region(stdin_access);
}

/**
 * Offset/Address/Size: 0x8C | 0x8022C054 | size: 0x70
 */
unsigned int __flush_all(void)
{
    unsigned int ret = 0;
    FILE* file = &__files._stdin;

    while (file)
    {
        if (file->file_mode.file_kind != __closed_file && fflush(file))
        {
            ret = -1;
        }
        file = file->next_file;
    }

    return ret;
}

/**
 * Offset/Address/Size: 0x0 | 0x8022BFC8 | size: 0x8C
 */
int __flush_line_buffered_output_files(void)
{
    int result = 0;
    FILE* file = &__files._stdin;

    while (file != NULL)
    {
        if (file->file_mode.file_kind != __closed_file && (file->file_mode.buffer_mode & _IOLBF) && file->file_state.io_state == __writing)
        {
            if (fflush(file))
            {
                result = EOF;
            }
        }
        file = file->next_file;
    }

    return (result);
}
