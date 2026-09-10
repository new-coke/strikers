#ifndef PORT_MSL_WCHAR_T_H
#define PORT_MSL_WCHAR_T_H
// MSL typedefs wchar_t as unsigned short and the host makes it int, or a builtin in C++, so defer to the host.
#include <stddef.h>
#include <wchar.h>
#endif
