#include <libfixed/fixed.h>
#include <libmat/mat.h>
#include <libmspbuiltins/builtins.h>
#include <libmsp/mem.h>
#include <stdio.h>
#include <libio/console.h>
#include "common/mat.h"

void dwt_init() {
	MAT_RESHAPE(b1, MAT_SIZE, MAT_SIZE);
	MAT_RESHAPE(b2, MAT_SIZE, MAT_SIZE);
	MAT_RESHAPE(b3, MAT_SIZE, MAT_SIZE);
#ifndef USE_SENSORS
	mat_t *mat_input_ptr = &mat_a_dense;
#endif
	for(uint16_t i = 0; i < MAT_SIZE; i++) {
		for(uint16_t j = 0; j < MAT_SIZE; j++) {
#ifndef USE_SENSORS
			MAT_SET(b1, MAT_GET(mat_input_ptr, i, j), i, j);
#endif
			MAT_SET(b2, 0, i, j);
			MAT_SET(b3, 0, i, j);
		}
	}
}


#define SQRT2 F_LIT(0.707106)
void dwt1d(fixed *src, fixed *dest,fixed *inter,
uint8_t copy,uint16_t stride ) {
	fixed *src_ptr = src;
	fixed *dest_ptr = dest;
	fixed *inter_ptr = inter;
	// Do a copy
	if(copy) {
		for(uint16_t i = 0; i < MAT_SIZE * stride; i += stride)
			*(dest_ptr + i) = *(src_ptr + i);
	}

	uint16_t k = 1;
	while((k << 1) <= MAT_SIZE) k <<= 1;

	while(1 < k) {
		k >>= 1;
		inter_ptr = inter;
		dest_ptr = dest;
		fixed *inter_k_ptr = inter_ptr + k * stride;
		fixed *even_dest_ptr = dest_ptr;
		fixed *odd_dest_ptr = dest_ptr + stride;
		for(uint16_t i = 0; i < k; i++) {
			*inter_ptr = F_MUL(F_ADD(*even_dest_ptr, *odd_dest_ptr), SQRT2);
			*inter_k_ptr = F_MUL(F_SUB(*even_dest_ptr, *odd_dest_ptr), SQRT2);
			inter_ptr += stride;
			inter_k_ptr += stride;
			even_dest_ptr += 2 * stride;
			odd_dest_ptr += 2 * stride;
		}

		inter_ptr = inter;
		// for(uint16_t j = 0; j < MAT_SIZE; j++)
		// 	printf("%d ", inter_ptr[j]);
		// printf("\n");
		for(uint16_t i = 0; i < (k << 1); i++) {
			*dest_ptr = *inter_ptr;
			dest_ptr += stride;
			inter_ptr += stride;
		}
	}
}

// run with inputs b1,b2,b3
void dwt(int mat_dim, mat_t *src,mat_t *dest, mat_t *inter){
	// PLACE CODE HERE
	for(uint16_t i =0; i < MAT_SIZE; i++) {
		dwt1d(MAT_PTR(src,i,0),MAT_PTR(dest,i,0),MAT_PTR(inter,i,0),1,1);
	}

	for(uint16_t j = 0; j < MAT_SIZE; j++) {
		dwt1d(MAT_PTR(src,0,j),MAT_PTR(dest,0,j),MAT_PTR(inter,0,j),0,MAT_SIZE);
	}
}

void dwt_check() {
#ifdef CONSOLE
	uint16_t rows = MAT_GET_DIM(b2, 0);
	uint16_t cols = MAT_GET_DIM(b2, 1);
	MAT_RESHAPE(b2, 1, rows, cols);
	MAT_DUMP(b2, 0);
#endif
}

// Takes a sensor function and pulls data into the right matrix
void dwt_data(float (*sense)()) {
	for (int i = 0; i < MAT_SIZE; i++) {
    if (i % 4 == 0) {
      UNSAFE_ATOMIC_START;
    }
		for (int j = 0; j < MAT_SIZE; j++) {
			float float_val = sense();
			fixed fixed_val = F_LIT(float_val);
			// Write into matrix
			MAT_SET(b1, fixed_val, i, j);
#ifndef HPVLP
			printf("%i ",MAT_GET(b1,i,j));
		}
		printf("\r\n");
    if ((i +1) % 4 == 0) {
      UNSAFE_ATOMIC_END;
    }
	}
	printf("=========done\r\n");
#else
		}// apologies for putting a paren in an ifdef block...
    if ((i +1) % 4 == 0) {
      UNSAFE_ATOMIC_END;
    }
	}
#endif
}

// Takes a sensor function and pulls data into the right matrix
void dwt_short_data(float (*sense)(),uint16_t sample_len) {
	static uint16_t i = 0, j = 0;
  UNSAFE_ATOMIC_START;
	for (int k = 0; k < MAT_SIZE; k++) {
		float float_val = sense();
		fixed fixed_val = F_LIT(float_val);
		// Write into matrix
		MAT_SET(b1, fixed_val, i, j);
		j++;
		// If we're at the end of a row, zero out col, increment row
		if (j >= MAT_SIZE) {
			j = 0;
			i++;
			// If we're at the end of the matrix, zero out row
			if (i >= MAT_SIZE) {
				i = 0;
			}
		}
#ifndef HPVLP
		printf("%i ",MAT_GET(b1,i,j));
		printf("\r\n");
	}
	printf("=========done\r\n");
  UNSAFE_ATOMIC_END;
#else
	}// apologies for putting a paren in an ifdef block...
  UNSAFE_ATOMIC_END;
#endif
}



