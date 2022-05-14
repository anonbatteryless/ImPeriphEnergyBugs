#include <libfixed/fixed.h>
#include <libmat/mat.h>
#include <libmspbuiltins/builtins.h>
#include <libmsp/mem.h>
#include <stdio.h>
#include <libio/console.h>
#include "common/mat.h"



void dmv_mega_init(int mat_dim) {
	MAT_RESHAPE(b1, mat_dim, 1);
	MAT_RESHAPE(b2, mat_dim, 1);
	MAT_RESHAPE(b3, mat_dim, 1);
	#ifndef USE_SENSORS
	mat_t *mat_input_ptr = &mat_b_dense;
	for(uint16_t i = 0; i < MAT_GET_DIM((&mat_b_dense), 0); i++) {
		MAT_SET(b1, MAT_GET(mat_input_ptr, i, 0), i, 0);
	}
	#endif
}
// b1,b2,b3,\&mat_a_dense
void dmv_mega(int mat_dim, mat_t *src, mat_t *dest, mat_t *inter,mat_t *filter) {
	uint16_t rows = mat_dim;
	uint16_t cols = mat_dim;
	uint16_t dcols = mat_dim;
	MAT_RESHAPE(inter, rows, dcols);
  /*UNSAFE_ATOMIC_START;
  printf("Running: %u\r\n",mat_dim);
  UNSAFE_ATOMIC_END;*/
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

void dmv_mega_check(int mat_dim) {
#ifdef CONSOLE
	MAT_RESHAPE(b2, 1, mat_dim, 1);
	MAT_DUMP(b2, 0);
#endif
}

void dmv_mega_data(float (*sense)(), int mat_dim) {
  UNSAFE_ATOMIC_START;//TODO: will this finish?
	for(uint16_t i = 0; i < mat_dim; i++) {
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

void dmv_mega_short_data(float (*sense)(),uint16_t sample_len, int mat_dim) {
	static uint16_t i = 0;
  //UNSAFE_ATOMIC_START;//TODO: will this finish?
	for(uint16_t k = 0; k < sample_len; k++) {
			float float_val =  sense();
			fixed fixed_val = F_LIT(float_val);
			// Write into matrix
			MAT_SET(b1, fixed_val, i, 0);
			i++;
			if (i >= mat_dim) {
				i = 0;
			}
#ifndef HPVLP
			printf("%i ",MAT_GET(b1,i,0));
	}
	printf("=========done\r\n");
#else
	}// apologies for putting a paren in an ifdef block...
#endif
  //UNSAFE_ATOMIC_END;
}
