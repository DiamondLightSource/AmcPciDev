#ifndef UTILS_H
#define UTILS_H
#include <linux/types.h>

#define MIN(a, b) ((a)<(b) ? (a):(b))

u16 calc_checksum16(char *buff, size_t size);

#endif
