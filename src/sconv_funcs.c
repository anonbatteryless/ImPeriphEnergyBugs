#include <libfixed/fixed.h>
#include <libmat/mat.h>
#include <libmspbuiltins/builtins.h>
#include <libmsp/mem.h>
#include <stdio.h>
#include <libio/console.h>
#include "common/mat.h"

void sconv_init() {
	uint16_t rows = MAT_GET_DIM((&mat_a_dense), 0);
	uint16_t cols = MAT_GET_DIM((&mat_a_dense), 1);
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
		}
	}
#endif
	for(uint16_t i = 0; i < frows; i++) {
		for(uint16_t j = 0; j < fcols; j++) {
			MAT_SET(b2, 0, 0, i, j);
			MAT_SET(b3, 0, 0, i, j);
		}
	}
}


// b1,b2,b3,\&mat_c_sparse
void sconv(mat_t *src, mat_t *dest, mat_t *inter, mat_t *filter) {
	// Added here because passing in mat_c_sparse_pseudo doesn't work, no idea why
	filter = &mat_c_sparse;
	uint16_t rows = MAT_GET_DIM(dest, 0);
	uint16_t cols = MAT_GET_DIM(dest, 1);
	MAT_RESHAPE(inter, rows, cols);
	uint16_t frows = filter->sparse.dims[2];
	uint16_t fcols = filter->sparse.dims[3];
	// Only a 2D convolution
	uint16_t total_elements = filter->sparse.sizes[0] + 1;

	uint16_t fsize = frows * fcols;

	fixed *dest_ptr = dest->data;
	for(uint16_t i = 0; i < rows; i++) {
		for(uint16_t j = 0; j < cols; j++) {
			fixed w = 0;
			uint16_t pos = 0;
			uint16_t idx = filter->sparse.offsets[pos];
			fixed *filter_ptr = filter->data;
			while(pos < total_elements) {
				uint16_t k = idx / fsize;
				uint16_t l = (idx % fsize) / fcols;
				uint16_t m = idx % fcols;
				w = F_ADD(w, F_MUL(MAT_GET(src, k, i + l, j + m), *filter_ptr));
				pos++;
				idx += filter->sparse.offsets[pos];
				filter_ptr++;
			}
			*dest_ptr++ = w;
		}
	}
}

void sconv_check() {
#ifdef CONSOLE
	uint16_t rows = MAT_GET_DIM((&mat_a_dense), 0);
	uint16_t cols = MAT_GET_DIM((&mat_a_dense), 1);
	uint16_t frows = MAT_GET_DIM((&mat_c_dense), 2);
	uint16_t fcols = MAT_GET_DIM((&mat_c_dense), 3);
	MAT_RESHAPE(b2, 1, (rows - frows + 1), (cols - fcols + 1));
	MAT_DUMP(b2, 0);
#endif
}

// Takes a sensor function and pulls data into the right matrix
void sconv_data(float (*sense)()) {
	for (int i = 0; i < MAT_SIZE; i++) {
		for (int j = 0; j < MAT_SIZE; j++) {
			float float_val = sense();
			fixed fixed_val = F_LIT(float_val);
			// Write into matrix
			MAT_SET(b1, fixed_val, i, j);
#ifndef HPVLP
			printf("%i ",MAT_GET(b1,i,j));
		}
		printf("\r\n");
	}
	printf("=========done\r\n");
#else
		}// apologies for putting a paren in an ifdef block...
	}
#endif
}


