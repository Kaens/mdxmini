#ifndef MDX_LZX042_H
#define MDX_LZX042_H
#include <stddef.h>
/* 1 = decoded (caller frees *out), 0 = plain data, -1 = invalid LZX 0.42. */
int mdx_lzx042(const unsigned char *data, size_t size,
               unsigned char **out, size_t *out_size);
#endif
