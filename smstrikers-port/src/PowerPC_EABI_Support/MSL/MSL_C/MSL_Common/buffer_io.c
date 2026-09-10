#include "types.h"

#include "buffer_io.h"
#include "critical_regions.h"

void __convert_from_newlines(unsigned char* buf, size_t* n)
{
}

void __convert_to_newlines(unsigned char* buf, size_t* n)
{
}

/**
 * Offset/Address/Size: 0x338 | 0x8022FBFC | size: 0x34
 */
void __prep_buffer(FILE* file)
{
    file->buffer_ptr = file->buffer;
    file->buffer_length = file->buffer_size;
    file->buffer_length -= file->position & file->buffer_alignment;
    file->buffer_position = file->position;
}

/**
 * Offset/Address/Size: 0x228 | 0x8022FAEC | size: 0x110
 */
int __load_buffer(FILE* file, size_t* bytes_loaded, int alignment)
{
    FILE* f;
    size_t* loaded;
    int ioresult;
    long n;
    unsigned char* p;

    f = file;
    loaded = bytes_loaded;

    __prep_buffer(f);

    if (alignment == __dont_align_buffer)
    {
        f->buffer_length = f->buffer_size;
    }

    ioresult = (*f->read_fn)(f->handle, f->buffer, (size_t*)&f->buffer_length, f->idle_fn);

    if (ioresult == __io_EOF)
    {
        f->buffer_length = 0;
    }

    if (loaded != NULL)
    {
        *loaded = f->buffer_length;
    }

    if (ioresult != __no_io_error)
    {
        return ioresult;
    }

    f->position += f->buffer_length;

    if (!f->file_mode.binary_io)
    {
        n = f->buffer_length;
        p = f->buffer;

        while (n--)
        {
            if (*p++ == '\n')
            {
                f->position++;
            }
        }
    }

    return __no_io_error;
}

/**
 * Offset/Address/Size: 0x164 | 0x8022FA28 | size: 0xC4
 */
int __flush_buffer(FILE* file, size_t* bytes_flushed)
{
    size_t buffer_len;
    int ioresult;

    buffer_len = file->buffer_ptr - file->buffer;

    if (buffer_len)
    {
        file->buffer_length = buffer_len;

        if (!file->file_mode.binary_io)
            __convert_from_newlines(file->buffer, (size_t*)&file->buffer_length);

        ioresult = (*file->write_fn)(file->handle, file->buffer, (size_t*)&file->buffer_length, file->idle_fn);

        if (bytes_flushed)
            *bytes_flushed = file->buffer_length;

        if (ioresult)
            return (ioresult);

        file->position += file->buffer_length;
    }

    __prep_buffer(file);

    return 0;
}

/**
 * Offset/Address/Size: 0x0 | 0x8022F8C4 | size: 0x164
 */
int setvbuf(FILE* file, char* buff, int mode, size_t size)
{
    int kind;

    kind = file->file_mode.file_kind;

    if (mode == _IONBF)
        fflush(file);

    if (file->file_state.io_state != __neutral || kind == __closed_file) /*970318 bkoz*/
        return (-1);

    if (mode != _IONBF && mode != _IOLBF && mode != _IOFBF)
        return (-1);

    if (file->buffer && file->file_state.free_buffer)
    {
        free(file->buffer);
    }
    __begin_critical_region(stdin_access);

    file->file_mode.buffer_mode = mode;
    file->file_state.free_buffer = 0;
    file->buffer = (unsigned char*)&file->char_buffer;
    file->buffer_ptr = (unsigned char*)&file->char_buffer;
    file->buffer_size = 1;
    file->buffer_length = 0;
    file->buffer_alignment = 0;

    if (mode == _IONBF || size < 1)
    {
        *(file->buffer_ptr) = '\0'; /*mm 970306*/
        __end_critical_region(stdin_access);
        return (0);
    }

    if (!buff)
    {
        if (!(buff = (char*)malloc(size)))
        {
            __end_critical_region(stdin_access);
            return (-1);
        }

        file->file_state.free_buffer = 1;
    }

    file->buffer = (unsigned char*)buff;
    file->buffer_ptr = file->buffer;
    file->buffer_size = size;
    file->buffer_alignment = 0;

    __end_critical_region(stdin_access);

    return (0);
}
