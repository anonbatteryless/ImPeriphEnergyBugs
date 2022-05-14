#ifndef C_SPARSE_H
#define C_SPARSE_H

#include <libfixed/fixed.h>

#define C_SPARSE_LEN 7

__nv fixed c_sparse[7] = {F_LIT(-4), F_LIT(3),  F_LIT(2), F_LIT(-4),
							 F_LIT(2),  F_LIT(-5), F_LIT(4)};

__nv uint16_t c_sparse_offsets[7] = {0, 2, 1, 1, 1, 1, 1};

__nv uint16_t c_sparse_sizes[1] = {7};

#endif