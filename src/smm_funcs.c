#include <libfixed/fixed.h>
#include <libmat/mat.h>
#include <libmspbuiltins/builtins.h>
#include <libmsp/mem.h>
#include <stdio.h>
#include <libio/console.h>
#include "common/fft.h"
#include "common/mat.h"


void smm_init() {
	uint16_t rows = mat_a_sparse.sparse.dims[0];
	uint16_t cols = mat_a_sparse.sparse.dims[1];
	uint16_t dcols = mat_a_sparse.sparse.dims[1];
	MAT_RESHAPE(b2, rows, dcols);
	MAT_RESHAPE(b3, rows, dcols);
	for(uint16_t i = 0; i< MAT_SIZE;i++) {
		for(uint16_t j = 0; j < MAT_SIZE; j++) {
			MAT_SET(b2,0,i,j);
			MAT_SET(b3,0,i,j);
		}
	}
}


//Run with  \&mat_a_sparse,b2,b3,\&mat_a_bsparse
void smm(mat_t *src,mat_t *dest,mat_t *inter, mat_t *bsrc) {
	mat_t *filter = src;
	mat_t *bfilter = bsrc;
	fixed *dest_ptr = dest->data;
	for(uint16_t i = 0; i < src->sparse.dims[0]; i++) {
		for(uint16_t j = 0; j < src->sparse.dims[1]; j++) {
			*dest_ptr++ = 0;
		}
	}
#if 1
	for(uint16_t i = 0; i < filter->sparse.dims[0]; i++) {
		fixed *dest_ptr = MAT_PTR(dest, i, 0);
		uint16_t j = filter->sparse.sizes[i];
		for(; j < filter->sparse.sizes[i + 1]; j++) {
			uint16_t row = filter->sparse.offsets[j];
			uint16_t k = src->sparse.sizes[row];
			fixed f = filter->data[j];
			for(; k < src->sparse.sizes[row + 1]; k++) {
				fixed w = F_MUL(f, src->data[k]);
				uint16_t col = src->sparse.offsets[k];
				fixed *d = dest_ptr + col;
				*d = F_ADD(w, *d);
			}
		}
	}
#else

	uint16_t block_cols = bsrc->sparse.dims[1];
	uint16_t filter_offset = 0;
	uint16_t filter_block_offset = 0;
	for(uint16_t i = 0; i < rows; i += BLOCK_SIZE) {
		PRINTF("i = %i\r\n",i);
		uint16_t src_offset = 0;
		uint16_t src_block_offset = 0;
		for(uint16_t j = 0, crow = 0; 
			j < cols; j += BLOCK_SIZE, crow += (BLOCK_SIZE + 1) * block_cols) {
			PRINTF("j = %i\r\n",j);

			for(uint16_t k = 0, ccol = 0; k < dcols; 
				k += BLOCK_SIZE, ccol += (BLOCK_SIZE + 1)) {
				PRINTF("k = %i\r\n",k);

				uint16_t m = filter_block_offset;
				for(; m < filter_block_offset + BLOCK_SIZE; m++) {
					PRINTF("m = %i\r\n",m);
					uint16_t row = i + m - filter_block_offset;
					fixed *dest_ptr = MAT_PTR(dest, row, 0);
					uint16_t n = bfilter->sparse.sizes[m] + filter_offset;
					for(; n < bfilter->sparse.sizes[m + 1] + filter_offset; n++) {
						PRINTF("n = %i\r\n",n);
						fixed f = bfilter->data[n];
						uint16_t col = bfilter->sparse.offsets[n];

						uint16_t col_idx = crow + ccol + col % BLOCK_SIZE;
						uint16_t p = bsrc->sparse.sizes[col_idx] + src_offset;
						for(; p < bsrc->sparse.sizes[col_idx + 1] + src_offset; p++) {
							PRINTF("p = %x / %x\r\n",p,bsrc->sparse.sizes[col_idx + 1] + src_offset);
							fixed w = F_MUL(f, bsrc->data[p]);
							uint16_t dcol = bsrc->sparse.offsets[p];
							fixed *d = dest_ptr + dcol;
							*d = F_ADD(w, *d);
						}
					}
				}
				PRINTF("Another loop\r\n");
				src_offset += bsrc->sparse.sizes[src_block_offset + BLOCK_SIZE];
				src_block_offset += BLOCK_SIZE + 1;
			}

			filter_offset += bfilter->sparse.sizes[filter_block_offset + BLOCK_SIZE];
			filter_block_offset += BLOCK_SIZE + 1;
		}
	}
#endif
}

void smm_check() {
	uint16_t rows = MAT_GET_DIM(b2, 0);
	uint16_t cols = MAT_GET_DIM(b2, 1);
	MAT_RESHAPE(b2, 1, rows, cols);
#ifdef CONSOLE
	MAT_DUMP(b2, 0);
#endif
}

