#include <libfixed/fixed.h>
#include <libmat/mat.h>
#include <libmspbuiltins/builtins.h>
#include <libmsp/mem.h>
#include <stdio.h>
#include <libio/console.h>
#include "common/mat.h"

void dconv_mega_init(int mat_dim) {
	uint16_t rows = mat_dim;
	uint16_t cols = mat_dim;
	uint16_t frows = MAT_GET_DIM((&mat_c_dense), 2);
	uint16_t fcols = MAT_GET_DIM((&mat_c_dense), 3);
	MAT_RESHAPE(b1, 1, rows, cols);
	MAT_RESHAPE(b2, (rows - frows + 1), (cols - fcols + 1));
	MAT_RESHAPE(b3, (rows - frows + 1), (cols - fcols + 1));
	#ifndef USE_SENSORS
	mat_t *mat_input_ptr = &mat_a_dense;
	for(uint16_t i = 0; i < rows; i++) {
		for(uint16_t j = 0; j < cols; j++) {
			MAT_SET(b1, MAT_GET(mat_input_ptr, i, j), 0, i, j);
			//printf("%i ", (*MAT_PTR(b1,0,i,j))/32);
		}
		//printf("\r\n");
	}
	#endif
}

// Call with b1,b2,b3,&mat_c_dense
void dconv_mega(int mat_dim, mat_t *src, mat_t *dest, mat_t *inter, mat_t *filter) {
	// Adding here to eliminate some pointer nonsense
	filter = &mat_c_dense;
	uint16_t rows = mat_dim;
	uint16_t cols = mat_dim;
	MAT_RESHAPE(inter, rows, cols);

	uint16_t flayers = MAT_GET_DIM(filter, 1);
	uint16_t frows = MAT_GET_DIM(filter, 2);
	uint16_t fcols = MAT_GET_DIM(filter, 3);
	//printf("Filter layers: %i, filter rows: %i, filter cols:%i\r\n",
	//	flayers,frows,fcols);
  for(uint16_t i = 0; i < flayers; i++) {
    fixed *dest_ptr = dest->data;
    for(uint16_t j = 0; j < rows; j++) {
      for(uint16_t k = 0; k < cols; k++) {
        fixed w = 0;
        fixed *filter_ptr = MAT_PTR(filter, i, 0, 0);
        fixed *src_ptr = MAT_PTR(src, i, j, k);
        for(uint16_t l = 0; l < frows; l++) {
          for(uint16_t m = 0; m < fcols; m++) {
						//printf("ptr is: %i, src is: %i, w is: %i\r\n",*filter_ptr/32,*src_ptr/32,w/32);
            w = F_ADD(w, F_MUL(*src_ptr, *filter_ptr));
            filter_ptr++;
            src_ptr++;
          }
        }
        *dest_ptr++ = w;
      }
    }
  }
}

void dconv_mega_check(int mat_dim) {
	uint16_t rows = mat_dim;
	uint16_t cols = mat_dim;
	uint16_t frows = MAT_GET_DIM((&mat_c_dense), 2);
	uint16_t fcols = MAT_GET_DIM((&mat_c_dense), 3);
#ifdef CONSOLE
	printf("Before reshape:");
	uint16_t rows2 = mat_dim;
	uint16_t cols2 = mat_dim;
	for (int i = 0; i <  rows2; i++) {
		for (int j = 0; j < cols2; j++) {
			printf("%i ", (*MAT_PTR(b2,i,j))/32);
		}
		printf("\r\n");
	}
	printf("After reshape:\r\n");
#endif
	MAT_RESHAPE(b2, 1, (rows - frows + 1), (cols - fcols + 1));
#ifdef CONSOLE
	MAT_DUMP(b2, 0);
#endif
}

void dconv_mega_data(float(*sense)(), int mat_dim) {
	uint16_t rows = mat_dim;
	uint16_t cols = mat_dim;
#ifndef HPVLP
	printf("Starting:\r\n");
#endif
	for(uint16_t i = 0; i < rows; i++) {
    if (i % 2 == 0) {
      UNSAFE_ATOMIC_START;
      printf("i: %i\r\n",i);
    }
		for(uint16_t j = 0; j < cols; j++) {
			float float_val = sense();
			fixed fixed_val = F_LIT(float_val);
			// Write into matrix
			MAT_SET(b1, fixed_val, 0, i, j);
#ifndef HPVLP
			printf("%i ",MAT_GET(b1,0,i,j));
		}
		printf("\r\n");
    if ((i +1) % 2 == 0) {
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

void dconv_mega_short_data(float(*sense)(),uint16_t sample_len, int mat_dim) {
	uint16_t rows = mat_dim;
	uint16_t cols = mat_dim;
#ifndef HPVLP
	printf("Starting:\r\n");
#endif
	static uint16_t i = 0, j= 0;
  //UNSAFE_ATOMIC_START;
	for(uint16_t k = 0; k < sample_len; k++) {
		float float_val = sense();
		fixed fixed_val = F_LIT(float_val);
		// Write into matrix
		MAT_SET(b1, fixed_val, 0, i, j);
		j++;
		// If we're at the end of a row, zero out col, increment row
		if (j >= cols) {
			j = 0;
			i++;
			// If we're at the end of the matrix, zero out row
			if (i >= rows) {
				i = 0;
			}
		}
#ifndef HPVLP
		printf("%i ",MAT_GET(b1,0,i,j));
		printf("\r\n");
	}
	printf("=========done\r\n");
#else
	}// apologies for putting a paren in an ifdef block...
#endif
  //UNSAFE_ATOMIC_END;
}

