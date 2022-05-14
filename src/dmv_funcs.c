#include <libfixed/fixed.h>
#include <libmat/mat.h>
#include <libmspbuiltins/builtins.h>
#include <libmsp/mem.h>
#include <stdio.h>
#include <libio/console.h>
#include "common/mat.h"



void dmv_init() {
	MAT_RESHAPE(b1, MAT_GET_DIM((&mat_b_dense), 0), 1);
	MAT_RESHAPE(b2, MAT_GET_DIM((&mat_a_dense), 0), 1);
	MAT_RESHAPE(b3, MAT_GET_DIM((&mat_a_dense), 0), 1);
	#ifndef USE_SENSORS
	mat_t *mat_input_ptr = &mat_b_dense;
	for(uint16_t i = 0; i < MAT_GET_DIM((&mat_b_dense), 0); i++) {
		MAT_SET(b1, MAT_GET(mat_input_ptr, i, 0), i, 0);
	}
	#endif
}
// b1,b2,b3,\&mat_a_dense
void dmv(int mat_dim, mat_t *src, mat_t *dest, mat_t *inter,mat_t *filter) {
	uint16_t rows = MAT_GET_DIM(filter, 0);
	uint16_t cols = MAT_GET_DIM(filter, 1);
	uint16_t dcols = MAT_GET_DIM(dest, 1);
	MAT_RESHAPE(inter, rows, dcols);

#define min(a, b) (((a) < (b)) ? (a) : (b))

  for(uint16_t i = 0; i < rows; i += BLOCK_SIZE) {
    //LOG("i/rows: %i / %i\r\n",i,rows);
    for(uint16_t j = 0; j < cols; j += BLOCK_SIZE) {
      //LOG("j/cols: %i / %i\r\n",j,cols);
      fixed *dest_ptr = dest->data + i;
      for(uint16_t k = i; k < min(i + BLOCK_SIZE, rows); k++) {
        //LOG("k/block: %i / %i\r\n",k,min(i + BLOCK_SIZE, rows));
        fixed *filter_ptr = filter->data + k * cols + j;
        fixed *src_ptr = src->data + j;
        fixed w = 0;
        for(uint16_t l = j; l < min(j + BLOCK_SIZE, cols); l++) {
          //LOG("l/block: %i / %i\r\n",l, min(j + BLOCK_SIZE, cols));
          w = F_ADD(w, F_MUL(*filter_ptr, *src_ptr));
          filter_ptr++;
          src_ptr++;
        }
        *dest_ptr++ += w;
      }
    }
  }
}

void dmv_check() {
#ifdef CONSOLE
	MAT_RESHAPE(b2, 1, MAT_GET_DIM((&mat_a_dense), 0), 1);
	MAT_DUMP(b2, 0);
#endif
}

void dmv_data(float (*sense)()) {
  UNSAFE_ATOMIC_START;
	for(uint16_t i = 0; i < MAT_GET_DIM((&mat_b_dense), 0); i++) {
			float float_val = sense();
			fixed fixed_val = F_LIT(float_val);
			// Write into matrix
			MAT_SET(b1, fixed_val, i, 0);
#ifndef HPVLP
			printf("%i ",MAT_GET(b1,i,0));
	}
	printf("=========done\r\n");
#else
	}// apologies for putting a paren in an ifdef block...
#endif
  volatile int _loc0 = 0;
  UNSAFE_ATOMIC_END;
  volatile int _loc1 = 0;
}

void dmv_short_data(float (*sense)(),uint16_t sample_len) {
	static uint16_t i = 0;
  UNSAFE_ATOMIC_START;
	for(uint16_t k = 0; k < sample_len; k++) {
			float float_val =  sense();
			fixed fixed_val = F_LIT(float_val);
			// Write into matrix
			MAT_SET(b1, fixed_val, i, 0);
			i++;
			if (i >= MAT_GET_DIM((&mat_b_dense), 0)) {
				i = 0;
			}
#ifndef HPVLP
			printf("%i ",MAT_GET(b1,i,0));
	}
	printf("=========done\r\n");
  UNSAFE_ATOMIC_END;
#else
	}// apologies for putting a paren in an ifdef block...
  //UNSAFE_ATOMIC_END;
#endif
}
