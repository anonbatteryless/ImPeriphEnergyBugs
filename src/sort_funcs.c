#include <libfixed/fixed.h>
#include <libmat/mat.h>
#include <libmspbuiltins/builtins.h>
#include <libmsp/mem.h>
#include <stdio.h>
#include <libio/console.h>
#include "common/fft.h"
#include "common/mat.h"

void sort_init() {
	uint16_t count = MAT_GET_DIM((&mat_b_dense), 0);
	MAT_RESHAPE(b1, count);
	MAT_RESHAPE(b2, count);
	MAT_RESHAPE(b3, count);
#ifndef USE_SENSORS
	mat_t *mat_input_ptr = &mat_a_dense;
#endif
	for(uint16_t i = 0; i < count; i++) {
#ifndef USE_SENSORS
		MAT_SET(b1, MAT_GET(mat_input_ptr, i, 0), i);
#endif
		MAT_SET(b2, 0, i);
		MAT_SET(b3, 0, i);
	}
}

int16_t partition(fixed *data, int16_t l, int16_t h) {
	int16_t x = data[h];
	int16_t i = (l - 1);

	for(int16_t j = l; j <= h - 1; j++) {
		if (data[j] <= x) {
			i++;
			fixed tmp = data[i];
			data[i] = data[j];
			data[j] = tmp;
		}
	}
	fixed tmp = data[i + 1];
	data[i + 1] = data[h];
	data[h] = tmp;
	return (i + 1);
}

//pass in b1,b2,b3
void sort(int mat_dim, mat_t *src, mat_t *dest, mat_t *inter) {
	uint16_t count = MAT_GET_DIM(dest, 0);
	fixed *src_ptr = src->data;
	fixed *dest_ptr = dest->data;
	for(uint16_t i = 0; i < count; i++)
		dest_ptr[i] = src_ptr[i];

	int16_t l = 0, h = count - 1;
	// TODO: confirm that this substitution is correct
	uint16_t stack[count];
	//uint16_t stack[MAT_SIZE];

	int16_t top = -1;
	stack[++top] = 0;
	stack[++top] = h;
	while(top >= 0) {
		h = stack[top--];
		l = stack[top--];
		int16_t p = partition(dest_ptr, l, h);
		if(p - 1 > l) {
			stack[++top] = l;
			stack[++top] = p - 1;
		}

		if(p + 1 < h) {
			stack[++top] = p + 1;
			stack[++top] = h;
		}
	}
}

void sort_check() {
	//printf("Here\r\n");
#ifdef CONSOLE
	uint16_t count = MAT_GET_DIM((&mat_b_dense), 0);
	MAT_RESHAPE(b2, 1, 1, count);
	MAT_DUMP(b2, 0);
#endif
}

void sort_data(float (*sense)()) {
	for(uint16_t i = 0; i < MAT_GET_DIM((&mat_b_dense), 0); i++) {
			float float_val = sense();
			fixed fixed_val = F_LIT(float_val);
			// Write into matrix
			MAT_SET(b1, fixed_val, i, 0);
#ifndef HPVLP
			printf("%i ",MAT_GET(b1,i,0));
		}
	printf("======data inr\n");
#else
		}// apologies for putting a paren in an ifdef block...
#endif
}

void sort_short_data(float (*sense)(),uint16_t sample_len) {
	static uint16_t i = 0;
	for(uint16_t k = 0; k < sample_len; k++) {
			float float_val = sense();
			fixed fixed_val = F_LIT(float_val);
			// Write into matrix
			MAT_SET(b1, fixed_val, i, 0);
			i++;
			if( i >= MAT_GET_DIM((&mat_b_dense), 0)) {
				i = 0;
			}
#ifndef HPVLP
			printf("%i ",MAT_GET(b1,i,0));
		}
	printf("======data inr\n");
#else
		}// apologies for putting a paren in an ifdef block...
#endif
}


