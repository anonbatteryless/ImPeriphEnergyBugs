#include <libfixed/fixed.h>
#include <libmat/mat.h>
#include <libmspbuiltins/builtins.h>
#include <libmsp/mem.h>
#include <stdio.h>
#include <libio/console.h>
#include "common/mat.h"
#include "harness.h"

void dmm_mega_init(int mat_dim) {
	// This assumes a square matrix
	uint16_t rows = mat_dim;
	uint16_t cols = mat_dim;
	MAT_RESHAPE(b1, rows, cols);
	MAT_RESHAPE(b2, rows, cols);
#ifndef USE_SENSORS
	mat_t *mat_input_ptr = &mat_a_dense;
#endif
	for(uint16_t i = 0; i < rows; i++) {
		for(uint16_t j = 0; j < cols; j++) {
#ifndef USE_SENSORS
			MAT_SET(b1, MAT_GET(mat_input_ptr, i, j), i, j);
#endif
			MAT_SET(b2, 0, i, j);
			MAT_SET(b3, 0, i, j);
		}
	}
}


void dmm_mega(int mat_dim, mat_t *src, mat_t* dest, mat_t* inter,
  mat_t *filter) {
	uint16_t rows = mat_dim;
	uint16_t cols = mat_dim;
	uint16_t dcols = mat_dim;
	MAT_RESHAPE(inter, rows, dcols);

#define min(a, b) (((a) < (b)) ? (a) : (b))

  for(uint16_t i = 0; i < rows; i += BLOCK_SIZE) {
    for(uint16_t j = 0; j < dcols; j += BLOCK_SIZE) {
      for(uint16_t k = 0; k < cols; k += BLOCK_SIZE) {
        fixed *filter_ptr = filter->data + i * cols + k;
        for(uint16_t l = i; l < min(i + BLOCK_SIZE, rows); l++) {
          fixed *dest_ptr = dest->data + l * dcols + j;
          for(uint16_t m = j; m < min(j + BLOCK_SIZE, dcols); m++) {
            fixed w = 0;
            fixed *src_ptr = src->data + dcols * k + m;
            fixed *cur_filter_ptr = filter_ptr;
            for(uint16_t n = k; n < min(k + BLOCK_SIZE, cols); n++) {
              w = F_ADD(w, F_MUL(*cur_filter_ptr, *src_ptr));
              src_ptr += dcols;
              cur_filter_ptr++;
            }
            *dest_ptr++ += w;
          }
          filter_ptr += cols;
        }
      }
    }
  }
}

void dmm_mega_check(int mat_dim) {
#ifdef CONSOLE
	uint16_t rows = mat_dim;
	uint16_t cols = mat_dim;
	MAT_RESHAPE(b2, 1, rows, cols);
  PRINTF("\r\nStart========\r\n");
  UNSAFE_ATOMIC_START;
	MAT_DUMP(b2, 0);
  UNSAFE_ATOMIC_END;
  PRINTF("\r\nEnd========\r\n");
#endif
}


// Takes a sensor function and pulls data into the right matrix
void dmm_mega_data(float (*sense)(), int mat_dim) {
	for (int i = 0; i < mat_dim; i++) {
    if (i % 2 == 0) {
      UNSAFE_ATOMIC_START;
      printf("i: %i\r\n",i);
    }
		for (int j = 0; j < mat_dim; j++) {
			float float_val = sense();
			fixed fixed_val = F_LIT(float_val);
			// Write into matrix
			MAT_SET(b1, fixed_val, i, j);
#ifndef HPVLP
			printf("%i ",MAT_GET(b1,i,j));
		}
		printf("| %i\r\n",i);
    if ((i +1) % 4 == 0) {
      UNSAFE_ATOMIC_END;
    }
	}
	printf("=========done\r\n");
#else
		}// apologies for putting a paren in an ifdef block...
    if ((i +1) % 2 == 0) {
      UNSAFE_ATOMIC_END;
    }
	}
#endif
}

// Takes a sensor function and pulls data into the right matrix
void dmm_mega_short_data(float (*sense)(),uint16_t sample_len,int mat_dim) {
	static uint16_t i=0,j=0;
  //UNSAFE_ATOMIC_START;
	for (int k = 0; k < sample_len; k++) {
    //UNSAFE_ATOMIC_START; //TODO figure out if this can be unrolled
		float float_val = sense();
    //UNSAFE_ATOMIC_END;
		fixed fixed_val = F_LIT(float_val);
		// Write into matrix
		MAT_SET(b1, fixed_val, i, j);
		j++;
		// If we're at the end of a row, zero out col, increment row
		if (j >= mat_dim) {
			j = 0;
			i++;
			// If we're at the end of the matrix, zero out row
			if (i >= mat_dim) {
				i = 0;
			}
		}
#ifndef HPVLP
		printf("%i ",MAT_GET(b1,i,j));
		printf("\r\n");
	}
	printf("=========done\r\n");
#else
		}// apologies for putting a paren in an ifdef block...
#endif
  //UNSAFE_ATOMIC_END; --> moved outside
}
