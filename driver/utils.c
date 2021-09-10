#include "utils.h"

u16 calc_checksum16(char *buff, size_t size)
{
    u32 result = 0;
    u16 *ptr = (u16 *) buff;
    for (size_t i = 0; i < size/2; i++) {
        result += ptr[i];
    }
    if (size & 1)
        result += (*(u8 *) (buff + size - 1)) << 8;
    result = (result & 0xffff) + (result >> 16);
    result = (result & 0xffff) + (result >> 16);
    return (~result) & 0xffff;
}
