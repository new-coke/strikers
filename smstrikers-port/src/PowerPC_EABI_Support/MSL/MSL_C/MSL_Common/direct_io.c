#include "direct_io.h"
#include "critical_regions.h"
#include "wchar_io.h"
#include "buffer_io.h"
#include "misc_io.h"
#include "string.h"
#include "FILE_POS.h"

/**
 * Offset/Address/Size: 0x6D8 | 0x802307D8 | size: 0x7C
 */
size_t fread(const void* buffer, size_t size, size_t count, FILE* stream)
{
    size_t retval;

    __begin_critical_region(stdin_access);
    retval = __fread(buffer, size, count, stream);
    __end_critical_region(stdin_access);

    return retval;
}

/**
 * Offset/Address/Size: 0x388 | 0x80230488 | size: 0x350
 */
size_t __fread(const void* buffer, size_t size, size_t count, FILE* stream)
{
    int always_buffer, ioresult;
    unsigned char* read_ptr;
    size_t num_bytes, bytes_to_go, bytes_read;

#ifndef __NO_WIDE_CHAR
    if (fwide(stream, 0) == 0)
        fwide(stream, -1);
#endif

    bytes_to_go = size * count;

    if (!bytes_to_go || stream->file_state.error || stream->file_mode.file_kind == __closed_file)
        return 0;

    always_buffer = 1;
    if (stream->file_mode.binary_io)
    {
        if (stream->file_mode.buffer_mode != _IOFBF)
            always_buffer = 0;
    }

    if (stream->file_state.io_state == __neutral)
    {
        if (stream->file_mode.io_mode & __read)
        {
            stream->file_state.io_state = __reading;
            stream->buffer_length = 0;
        }
    }

    if (stream->file_state.io_state < __reading)
    {
        set_error(stream);
        return 0;
    }

    if (stream->file_mode.buffer_mode & _IOLBF)
    {
        if (__flush_line_buffered_output_files())
        {
            set_error(stream);
            return 0;
        }
    }

    read_ptr = (unsigned char*)buffer;
    bytes_read = 0;

    if (bytes_to_go && stream->file_state.io_state >= __rereading)
    {
        do
        {
            if (fwide(stream, 0) == 1)
            {
                bytes_read += 2;
                bytes_to_go -= 2;
                *(wchar_t*)read_ptr = stream->ungetc_wide_buffer[stream->file_state.io_state - __rereading];
                read_ptr += 2;
            }
            else
            {
                bytes_read += 1;
                bytes_to_go -= 1;
                *read_ptr = stream->ungetc_buffer[stream->file_state.io_state - __rereading];
                read_ptr += 1;
            }

            stream->file_state.io_state = stream->file_state.io_state - 1;

            if (!bytes_to_go)
                break;
        } while (stream->file_state.io_state >= __rereading);

        if (stream->file_state.io_state == __reading)
        {
            stream->buffer_length = stream->save_buffer_length;
        }
    }

    if (bytes_to_go)
    {
        if (stream->buffer_length || always_buffer)
        {
            do
            {
                if (!stream->buffer_length)
                {
                    ioresult = __load_buffer(stream, 0, 0);
                    if (ioresult)
                    {
                        if (ioresult == __io_error)
                        {
                            stream->file_state.error = 1;
                            stream->buffer_length = 0;
                        }
                        else
                        {
                            stream->file_state.io_state = __neutral;
                            stream->file_state.eof = 1;
                            stream->buffer_length = 0;
                        }
                        bytes_to_go = 0;
                        break;
                    }
                }

                num_bytes = stream->buffer_length;
                if (num_bytes > bytes_to_go)
                    num_bytes = bytes_to_go;

                memcpy(read_ptr, stream->buffer_ptr, num_bytes);

                bytes_to_go -= num_bytes;
                read_ptr += num_bytes;
                bytes_read += num_bytes;
                stream->buffer_ptr += num_bytes;
                stream->buffer_length -= num_bytes;

                if (!bytes_to_go)
                    break;
            } while (always_buffer);
        }
    }

    if (bytes_to_go && !always_buffer)
    {
        unsigned char* save_buffer = stream->buffer;
        unsigned long save_size = stream->buffer_size;

        stream->buffer = read_ptr;
        stream->buffer_size = bytes_to_go;

        ioresult = __load_buffer(stream, &num_bytes, 1);
        if (ioresult)
        {
            if (ioresult == __io_error)
            {
                stream->file_state.error = 1;
                stream->buffer_length = 0;
            }
            else
            {
                stream->file_state.io_state = __neutral;
                stream->file_state.eof = 1;
                stream->buffer_length = 0;
            }
        }

        bytes_read += num_bytes;

        stream->buffer = save_buffer;
        stream->buffer_size = save_size;

        __prep_buffer(stream);
        stream->buffer_length = 0;
    }

    return (bytes_read / size);
}

/**
 * Offset/Address/Size: 0x30C | 0x8023040C | size: 0x7C
 */
size_t fwrite(const void* buffer, size_t size, size_t count, FILE* stream)
{
    size_t retval;

    __begin_critical_region(stdin_access);
    retval = __fwrite(buffer, size, count, stream);
    __end_critical_region(stdin_access);

    return (retval);
}

/**
 * Offset/Address/Size: 0x0 | 0x80230100 | size: 0x30C
 */
size_t __fwrite(const void* buffer, size_t size, size_t count, FILE* stream)
{
    unsigned char* write_ptr;
    size_t num_bytes, bytes_to_go, bytes_written;
    int ioresult, always_buffer;

#ifndef __NO_WIDE_CHAR
    if (fwide(stream, 0) == 0)
        fwide(stream, -1);
#endif

    bytes_to_go = size * count;

    if (!bytes_to_go || stream->file_state.error || stream->file_mode.file_kind == __closed_file)
        return 0;

    if (stream->file_mode.file_kind == __console_file)
        __stdio_atexit();

    always_buffer = !stream->file_mode.binary_io || stream->file_mode.buffer_mode == _IOFBF || stream->file_mode.buffer_mode == _IOLBF;

    if (stream->file_state.io_state == __neutral)
    {
        if (stream->file_mode.io_mode & __write)
        {
            if (stream->file_mode.io_mode & __append)
            {
                if (fseek(stream, 0, SEEK_END))
                    return 0;
            }
            stream->file_state.io_state = __writing;

            __prep_buffer(stream);
        }
    }

    if (stream->file_state.io_state != __writing)
    {
        set_error(stream);
        return 0;
    }

    write_ptr = (unsigned char*)buffer;
    bytes_written = 0;

    if (bytes_to_go && (stream->buffer_ptr != stream->buffer || always_buffer))
    {
        stream->buffer_length = stream->buffer_size - (stream->buffer_ptr - stream->buffer);

        do
        {
            unsigned char* newline = NULL;

            num_bytes = stream->buffer_length;

            if (num_bytes > bytes_to_go)
                num_bytes = bytes_to_go;
            if (stream->file_mode.buffer_mode == _IOLBF && num_bytes)
                if ((newline = (unsigned char*)__memrchr(write_ptr, '\n', num_bytes)) != NULL)
                    num_bytes = newline + 1 - write_ptr;

            if (num_bytes)
            {
                memcpy(stream->buffer_ptr, write_ptr, num_bytes);

                write_ptr += num_bytes;
                bytes_written += num_bytes;
                bytes_to_go -= num_bytes;

                stream->buffer_ptr += num_bytes;
                stream->buffer_length -= num_bytes;
            }
            if (!stream->buffer_length || newline != NULL || (stream->file_mode.buffer_mode == _IONBF))
            {
                ioresult = __flush_buffer(stream, NULL);

                if (ioresult)
                {
                    set_error(stream);
                    bytes_to_go = 0;
                    break;
                }
            }
        } while (bytes_to_go && always_buffer);
    }

    if (bytes_to_go && !always_buffer)
    {
        unsigned char* save_buffer = stream->buffer;
        size_t save_size = stream->buffer_size;

        stream->buffer = write_ptr;
        stream->buffer_size = bytes_to_go;
        stream->buffer_ptr = write_ptr + bytes_to_go;

        if (__flush_buffer(stream, &num_bytes) != __no_io_error)
            set_error(stream);

        bytes_written += num_bytes;

        stream->buffer = save_buffer;
        stream->buffer_size = save_size;

        __prep_buffer(stream);

        stream->buffer_length = 0;
    }

    if (stream->file_mode.buffer_mode != _IOFBF)
        stream->buffer_length = 0;

    return ((bytes_written + size - 1) / size);
}
