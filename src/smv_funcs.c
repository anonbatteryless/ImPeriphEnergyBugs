#include <libfixed/fixed.h>
#include <libmat/mat.h>
#include <libmspbuiltins/builtins.h>
#include <libmsp/mem.h>
#include <stdio.h>
#include <libio/console.h>
#include "common/fft.h"
#include "common/mat.h"


void smv_init() {
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

// Call with b1,b2,\&mat_a_sparse
// Note, for the non-blocked version, you really don't need bfilter
void smv(int mat_dim, mat_t *src, mat_t *dest, mat_t *filter) {
	uint16_t rows = MAT_GET_DIM(dest, 0);

#if 1
	fixed *dest_ptr = dest->data;
	for(uint16_t i = 0; i < rows; i++) {
		fixed w = 0;
		uint16_t j = filter->sparse.sizes[i];
		for(; j < filter->sparse.sizes[i + 1]; j++) {
			fixed *src_ptr = src->data + filter->sparse.offsets[j];
			w = F_ADD(w, F_MUL(filter->data[j], *src_ptr));
		}
		*dest_ptr++ = w;
	}
#else
	uint16_t block_offset = 0;
	uint16_t offset = 0;
	for(uint16_t i = 0; i < rows; i += BLOCK_SIZE) {
		PRINTF("Alive i/rows: %i/%i\r\n",i,rows);
		for(uint16_t j = 0; j < cols; j += BLOCK_SIZE) {
			PRINTF("Alive j/cols: %i/%i\r\n",j,cols);
			fixed *dest_ptr = dest->data + i;
			for(uint16_t k = block_offset; k < block_offset + BLOCK_SIZE; ++k) {
				PRINTF("Alive k/blkoffset: %i/%i\r\n",k,block_offset + BLOCK_SIZE);
				fixed w = 0;
				uint16_t l = bfilter->sparse.sizes[k] + offset;
				for(; l < bfilter->sparse.sizes[k + 1] + offset; ++l) {
					PRINTF("Alive l/k sizes: %i/%i\r\n",l, bfilter->sparse.sizes[k+1]);
					fixed *src_ptr = src->data + bfilter->sparse.offsets[l];
					w = F_ADD(w, F_MUL(bfilter->data[l], *src_ptr));
				}
				*dest_ptr++ += w;
			}
			offset += bfilter->sparse.sizes[block_offset + BLOCK_SIZE];
			block_offset += BLOCK_SIZE + 1;
		}
	}
#endif
}

void smv_check() {
	printf("Done!\r\n");
#ifdef CONSOLE
	MAT_RESHAPE(b2, 1, MAT_GET_DIM((&mat_a_dense), 0), 1);
	MAT_DUMP(b2, 0);
#endif
}

void smv_data(float (*sense)()) {
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
  UNSAFE_ATOMIC_END;
}

void smv_short_data(float (*sense)(),uint16_t sample_len) {
	static uint16_t i = 0;
  UNSAFE_ATOMIC_START;
	for(uint16_t k = 0; k < sample_len; k++) {
			float float_val = sense();
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
#else
	}// apologies for putting a paren in an ifdef block...
#endif
  UNSAFE_ATOMIC_END;
}



