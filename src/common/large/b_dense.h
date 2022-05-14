#ifndef B_DENSE_H
#define B_DENSE_H

#include <libfixed/fixed.h>

#if  MAT_SIZE == 256
__nv fixed b_dense[256] = {
	F_LIT(-3), F_LIT(0),  F_LIT(0),  F_LIT(-3), F_LIT(-5), F_LIT(-3), F_LIT(3),
	F_LIT(4),  F_LIT(-4), F_LIT(4),  F_LIT(0),  F_LIT(-5), F_LIT(0),  F_LIT(-5),
	F_LIT(0),  F_LIT(3),  F_LIT(4),  F_LIT(4),  F_LIT(4),  F_LIT(-5), F_LIT(-5),
	F_LIT(0),  F_LIT(-5), F_LIT(3),  F_LIT(2),  F_LIT(2),  F_LIT(0),  F_LIT(-2),
	F_LIT(0),  F_LIT(0),  F_LIT(-4), F_LIT(4),  F_LIT(3),  F_LIT(-3), F_LIT(-4),
	F_LIT(-4), F_LIT(4),  F_LIT(0),  F_LIT(0),  F_LIT(-4), F_LIT(-5), F_LIT(3),
	F_LIT(0),  F_LIT(0),  F_LIT(-4), F_LIT(-3), F_LIT(3),  F_LIT(-4), F_LIT(-5),
	F_LIT(2),  F_LIT(-2), F_LIT(3),  F_LIT(0),  F_LIT(-2), F_LIT(-4), F_LIT(3),
	F_LIT(3),  F_LIT(0),  F_LIT(2),  F_LIT(0),  F_LIT(3),  F_LIT(2),  F_LIT(0),
	F_LIT(0),  F_LIT(4),  F_LIT(0),  F_LIT(-5), F_LIT(0),  F_LIT(4),  F_LIT(-2),
	F_LIT(-5), F_LIT(3),  F_LIT(-2), F_LIT(4),  F_LIT(-2), F_LIT(2),  F_LIT(-5),
	F_LIT(2),  F_LIT(3),  F_LIT(-3), F_LIT(2),  F_LIT(-2), F_LIT(4),  F_LIT(-5),
	F_LIT(-5), F_LIT(-5), F_LIT(-2), F_LIT(0),  F_LIT(-2), F_LIT(3),  F_LIT(-5),
	F_LIT(2),  F_LIT(-3), F_LIT(4),  F_LIT(-2), F_LIT(0),  F_LIT(0),  F_LIT(0),
	F_LIT(3),  F_LIT(-4), F_LIT(3),  F_LIT(-5), F_LIT(0),  F_LIT(0),  F_LIT(-2),
	F_LIT(3),  F_LIT(0),  F_LIT(-5), F_LIT(0),  F_LIT(0),  F_LIT(-4), F_LIT(0),
	F_LIT(2),  F_LIT(0),  F_LIT(-4), F_LIT(-4), F_LIT(0),  F_LIT(4),  F_LIT(-3),
	F_LIT(0),  F_LIT(-2), F_LIT(4),  F_LIT(0),  F_LIT(-5), F_LIT(4),  F_LIT(4),
	F_LIT(2),  F_LIT(-5), F_LIT(-2), F_LIT(-4), F_LIT(-3), F_LIT(0),  F_LIT(3),
	F_LIT(3),  F_LIT(0),  F_LIT(-2), F_LIT(-2), F_LIT(-5), F_LIT(3),  F_LIT(0),
	F_LIT(0),  F_LIT(2),  F_LIT(2),  F_LIT(-3), F_LIT(0),  F_LIT(-5), F_LIT(2),
	F_LIT(-5), F_LIT(0),  F_LIT(-2), F_LIT(0),  F_LIT(0),  F_LIT(2),  F_LIT(3),
	F_LIT(0),  F_LIT(-4), F_LIT(-5), F_LIT(-3), F_LIT(0),  F_LIT(0),  F_LIT(0),
	F_LIT(2),  F_LIT(0),  F_LIT(0),  F_LIT(-5), F_LIT(-3), F_LIT(-3), F_LIT(3),
	F_LIT(0),  F_LIT(3),  F_LIT(2),  F_LIT(-2), F_LIT(-5), F_LIT(3),  F_LIT(2),
	F_LIT(0),  F_LIT(0),  F_LIT(-2), F_LIT(-4), F_LIT(0),  F_LIT(-4), F_LIT(-3),
	F_LIT(2),  F_LIT(-2), F_LIT(0),  F_LIT(2),  F_LIT(4),  F_LIT(3),  F_LIT(2),
	F_LIT(-4), F_LIT(-2), F_LIT(3),  F_LIT(-3), F_LIT(0),  F_LIT(4),  F_LIT(0),
	F_LIT(-4), F_LIT(-5), F_LIT(4),  F_LIT(-2), F_LIT(3),  F_LIT(-4), F_LIT(0),
	F_LIT(2),  F_LIT(-2), F_LIT(0),  F_LIT(0),  F_LIT(-2), F_LIT(-4), F_LIT(0),
	F_LIT(3),  F_LIT(3),  F_LIT(3),  F_LIT(-2), F_LIT(-5), F_LIT(-4), F_LIT(2),
	F_LIT(4),  F_LIT(-5), F_LIT(-2), F_LIT(-4), F_LIT(-2), F_LIT(0),  F_LIT(2),
	F_LIT(3),  F_LIT(-4), F_LIT(3),  F_LIT(-2), F_LIT(3),  F_LIT(-3), F_LIT(2),
	F_LIT(-3), F_LIT(3),  F_LIT(0),  F_LIT(-4), F_LIT(4),  F_LIT(0),  F_LIT(4),
	F_LIT(0),  F_LIT(3),  F_LIT(-5), F_LIT(-4), F_LIT(-5), F_LIT(-3), F_LIT(-2),
	F_LIT(-3), F_LIT(0),  F_LIT(-2), F_LIT(-3), F_LIT(0),  F_LIT(0),  F_LIT(-2),
	F_LIT(3),  F_LIT(3),  F_LIT(0),  F_LIT(0)};

#else
__nv fixed b_dense[128] = {
	F_LIT(-3), F_LIT(0),  F_LIT(0),  F_LIT(-3), F_LIT(-5), F_LIT(-3), F_LIT(3),
	F_LIT(4),  F_LIT(-4), F_LIT(4),  F_LIT(0),  F_LIT(-5), F_LIT(0),  F_LIT(-5),
	F_LIT(0),  F_LIT(3),  F_LIT(4),  F_LIT(4),  F_LIT(4),  F_LIT(-5), F_LIT(-5),
	F_LIT(0),  F_LIT(-5), F_LIT(3),  F_LIT(2),  F_LIT(2),  F_LIT(0),  F_LIT(-2),
	F_LIT(0),  F_LIT(0),  F_LIT(-4), F_LIT(4),  F_LIT(3),  F_LIT(-3), F_LIT(-4),
	F_LIT(-4), F_LIT(4),  F_LIT(0),  F_LIT(0),  F_LIT(-4), F_LIT(-5), F_LIT(3),
	F_LIT(0),  F_LIT(0),  F_LIT(-4), F_LIT(-3), F_LIT(3),  F_LIT(-4), F_LIT(-5),
	F_LIT(2),  F_LIT(-2), F_LIT(3),  F_LIT(0),  F_LIT(-2), F_LIT(-4), F_LIT(3),
	F_LIT(3),  F_LIT(0),  F_LIT(2),  F_LIT(0),  F_LIT(3),  F_LIT(2),  F_LIT(0),
	F_LIT(0),  F_LIT(4),  F_LIT(0),  F_LIT(-5), F_LIT(0),  F_LIT(4),  F_LIT(-2),
	F_LIT(-5), F_LIT(3),  F_LIT(-2), F_LIT(4),  F_LIT(-2), F_LIT(2),  F_LIT(-5),
	F_LIT(2),  F_LIT(3),  F_LIT(-3), F_LIT(2),  F_LIT(-2), F_LIT(4),  F_LIT(-5),
	F_LIT(-5), F_LIT(-5), F_LIT(-2), F_LIT(0),  F_LIT(-2), F_LIT(3),  F_LIT(-5),
	F_LIT(2),  F_LIT(-3), F_LIT(4),  F_LIT(-2), F_LIT(0),  F_LIT(0),  F_LIT(0),
	F_LIT(3),  F_LIT(-4), F_LIT(3),  F_LIT(-5), F_LIT(0),  F_LIT(0),  F_LIT(-2),
	F_LIT(3),  F_LIT(0),  F_LIT(-5), F_LIT(0),  F_LIT(0),  F_LIT(-4), F_LIT(0),
	F_LIT(2),  F_LIT(0),  F_LIT(-4), F_LIT(-4), F_LIT(0),  F_LIT(4),  F_LIT(-3),
	F_LIT(0),  F_LIT(-2), F_LIT(4),  F_LIT(0),  F_LIT(-5), F_LIT(4),  F_LIT(4),
	F_LIT(3),  F_LIT(3)};
#endif

#endif
