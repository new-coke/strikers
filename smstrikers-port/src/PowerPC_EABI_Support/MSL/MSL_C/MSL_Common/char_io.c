#include "char_io.h"
#include "critical_regions.h"
#include "misc_io.h"
#include "buffer_io.h"
#include "wchar_io.h"

/**
 * Offset/Address/Size: 0x28C | 0x8022FEBC | size: 0x214
 */
char* fgets(char* s, int n, FILE* file)
{
    char* p;
    int c;

    p = s;

    if (--n < 0)
    {
        return NULL;
    }

    __begin_critical_region(stdin_access);

    if (n != 0)
    {
        do
        {
            if (fwide(file, -1) >= 0)
            {
                c = EOF;
            }
            else
            {
                unsigned long length;

                length = file->buffer_length;
                file->buffer_length = length - 1;

                if (length != 0)
                {
                    unsigned char* ptr;

                    ptr = file->buffer_ptr;
                    file->buffer_ptr = ptr + 1;
                    c = *ptr;
                }
                else
                {
                    file->buffer_length = 0;

                    if (file->file_state.error != 0 || file->file_mode.file_kind == __closed_file)
                    {
                        c = EOF;
                    }
                    else
                    {
                        int io_state;

                        io_state = file->file_state.io_state;

                        if (io_state == __writing || !(file->file_mode.io_mode & __read))
                        {
                            file->file_state.error = 1;
                            file->buffer_length = 0;
                            c = EOF;
                        }
                        else if (io_state >= __rereading)
                        {
                            file->file_state.io_state = io_state - 1;

                            if (io_state == __rereading)
                            {
                                file->buffer_length = file->save_buffer_length;
                            }

                            c = ((unsigned char*)file)[io_state + 0xC];
                        }
                        else
                        {
                            int load_result;

                            file->file_state.io_state = __reading;
                            load_result = __load_buffer(file, NULL, __align_buffer);

                            if (load_result != __no_io_error || file->buffer_length == 0)
                            {
                                if (load_result == __io_error)
                                {
                                    file->file_state.error = 1;
                                    file->buffer_length = 0;
                                }
                                else
                                {
                                    file->file_state.io_state = __neutral;
                                    file->file_state.eof = 1;
                                    file->buffer_length = 0;
                                }

                                c = EOF;
                            }
                            else
                            {
                                unsigned long length2;
                                unsigned char* ptr2;

                                length2 = file->buffer_length;
                                file->buffer_length = length2 - 1;
                                ptr2 = file->buffer_ptr;
                                file->buffer_ptr = ptr2 + 1;
                                c = *ptr2;
                            }
                        }
                    }
                }
            }

            if (c == EOF)
            {
                if (file->file_state.eof != 0 && p != s)
                {
                    break;
                }

                __end_critical_region(stdin_access);
                return NULL;
            }

            *p++ = c;
        } while (c != '\n' && --n);
    }

    __end_critical_region(stdin_access);
    *p = 0;

    return s;
}

/**
 * Offset/Address/Size: 0xD4 | 0x8022FD04 | size: 0x1B8
 */
int __put_char(int c, FILE* stream)
{
    int ret;

    int file_kind = stream->file_mode.file_kind;
    stream->buffer_length = 0;

    if (stream->file_state.error != 0 || file_kind == __closed_file)
    {
        return -1;
    }

    if (file_kind == __console_file)
    {
        __stdio_atexit();
    }

    if (stream->file_state.io_state == __neutral && (stream->file_mode.io_mode & __write))
    {
        if ((stream->file_mode.io_mode & __append) && fseek(stream, 0, 2) != 0)
        {
            return 0;
        }

        stream->file_state.io_state = __writing;
        __prep_buffer(stream);
    }

    if (stream->file_state.io_state != __writing)
    {
        stream->file_state.error = 1;
        ret = -1;
        stream->buffer_length = 0;
    }
    else if ((stream->file_mode.buffer_mode == 2 || stream->buffer_size == (unsigned int)stream->buffer_ptr - (unsigned int)stream->buffer) && __flush_buffer(stream, NULL) != 0)
    {
        stream->file_state.error = 1;
        ret = -1;
        stream->buffer_length = 0;
    }
    else
    {
        stream->buffer_length--;
        *stream->buffer_ptr++ = c;

        if (stream->file_mode.buffer_mode != 2)
        {
            if ((stream->file_mode.buffer_mode == 0 || c == 10) && __flush_buffer(stream, NULL) != 0)
            {
                stream->file_state.error = 1;
                ret = -1;
                stream->buffer_length = 0;
                goto exit;
            }
            stream->buffer_length = 0;
        }

        ret = c & 0xFF;
    }

exit:
    return ret;
}

/**
 * Offset/Address/Size: 0x0 | 0x8022FC30 | size: 0xD4
 */
int fputs(const char* s, FILE* stream)
{
    char c;
    int var_r3;
    unsigned long len;
    int ret = 0;

    __begin_critical_region(stdin_access);
    while (c = *s++, c != 0)
    {
        if (fwide(stream, -1) >= 0)
        {
            var_r3 = -1;
        }
        else
        {
            len = stream->buffer_length;
            stream->buffer_length = len - 1;

            if (len != 0)
            {
                char* buf = (char*)stream->buffer_ptr;
                stream->buffer_ptr++;

                *buf = var_r3 = c & 0xFF;
            }
            else
            {
                var_r3 = __put_char(c, stream);
            }
        }

        if (var_r3 == -1)
        {
            ret = -1;
            break;
        }
    }
    __end_critical_region(stdin_access);

    return ret;
}
