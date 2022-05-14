#ifndef C_DENSE_H
#define C_DENSE_H

#include <libfixed/fixed.h>

__nv fixed c_dense[3][3] = {{F_LIT(-4), F_LIT(0), F_LIT(3)},
							   {F_LIT(2), F_LIT(-4), F_LIT(2)},
							   {F_LIT(-5), F_LIT(4), F_LIT(0)}};

#endif


/*
[-4, 0, 3],
[2, -4, 2],
[-5, 4, 0]

*/
