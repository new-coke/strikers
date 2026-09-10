#include "PowerPC_EABI_Support/Runtime/__va_arg.h"

#define ALIGN(addr, amount) (((unsigned int)(addr) + ((amount) - 1)) & ~((amount) - 1))

/**
 * Offset/Address/Size: 0x0 | 0x8023A08C | size: 0xC8
 */
void* __va_arg(va_list* ap, _va_arg_type type)
{
    char* addr;
    char* cur_gpr_ptr = &(ap->gpr);
    int cur_gpr = ap->gpr;
    int max = 8;
    int size = 4;
    int inc = 1;
    int even = 0;
    int fpr_offset = 0;
    int reg_size = 4;

    if (type == REAL)
    {
        cur_gpr_ptr = &(ap->fpr);
        cur_gpr = ap->fpr;
        size = 8;
        fpr_offset = 32; // 8 * 4
        reg_size = 8;
    }
    if (type == DOUBLEWORD)
    {
        size = 8;
        max = max - 1;
        if (cur_gpr & 1)
            even = 1;
        inc = 2;
    }

    if (cur_gpr < max)
    {
        cur_gpr += even;
        addr = ap->reg_save_area + fpr_offset + (cur_gpr * reg_size);
        *cur_gpr_ptr = cur_gpr + inc;
    }
    else
    {
        *cur_gpr_ptr = 8;
        addr = ap->input_arg_area;
        addr = (char*)ALIGN(addr, size);
        ap->input_arg_area = addr + size;
    }

    if (type == ARGPOINTER)
    {
        addr = *((char**)addr);
    }

    return addr;
}
