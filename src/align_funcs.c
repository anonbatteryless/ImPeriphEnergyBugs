#include <libfixed/fixed.h>
#include <libmat/mat.h>
#include <libmspbuiltins/builtins.h>
#include <libmsp/mem.h>
#include <stdio.h>
#include <libio/console.h>
#include "common/mat.h"

#define LEFT_ARROW 1
#define UP_ARROW 2
#define DIAG_ARROW 4

__nv uint16_t test1[16] = {'G','A','A','C',
						'G','A','A','C',
						'G','A','A','C',
						'G','A','A','C'};
__nv uint16_t test2[16] = {'G','C','A','C',
						'G','C','A','C',
						'G','C','A','C',
						'G','C','A','C'};

void align_init(void) {
	uint16_t count = MAT_GET_DIM((&mat_a_dense), 0);
	MAT_RESHAPE(b1, count);
	MAT_RESHAPE(b2, count);
	MAT_RESHAPE(b3, count, count);
	MAT_RESHAPE(b4, count, count);
	#ifndef USE_SENSORS
	mat_t *mat_input_ptr = &mat_a_dense;
  // No longer safe for intermittent execution
	for(uint16_t i = 0; i < count; i++) {
		MAT_SET(b1, MAT_GET(mat_input_ptr, i, 0), i);
		MAT_SET(b2, MAT_GET(mat_input_ptr, (i + 4) % count, 0), i);
	}
	#endif
}

// Call with inputs b1,b2,b3,b4
void align(int mat_dim,mat_t *src1, mat_t *src2, mat_t *dest, mat_t *inter){
	int16_t *src1_ptr = src1->data;
	int16_t *src2_ptr = src2->data;
	int16_t *dest_ptr = dest->data;
	int16_t *inter_ptr = inter->data;

	uint16_t count = MAT_GET_DIM(src1, 0);

	// Zero out the penalty matrix
	uint16_t *row_ptr = inter_ptr;
	uint16_t *col_ptr = inter_ptr;
	for(uint16_t i = 0; i < count; i++) {
		*col_ptr++ = -i;
		*row_ptr = -i;
		row_ptr += count;
	}

	for(uint16_t i = 1; i < count; i++) {
		// k is row index, j is column index
		int16_t *inter_topLeft = inter_ptr + i - 1;
		int16_t *inter_top = inter_ptr + i;
		int16_t *inter_left = inter_ptr + count + i - 1;
		int16_t *dest_diag = dest_ptr + i + count;
		int16_t *inter_diag = inter_ptr + i + count;
		for(uint16_t j = i, k = 1; j > 0; j--, k++) {
			int16_t topleft = *inter_topLeft;
			inter_topLeft += count - 1;
			int16_t top = *inter_top;
			inter_top += count - 1;
			int16_t left = *inter_left;
			inter_left += count - 1;
			int16_t char1 = src1_ptr[k - 1];
			int16_t char2 = src2_ptr[j - 1];
			if(char1 == char2) topleft += 1;
			else topleft -= 1;
			top -= 1;
			left -= 1;
			uint16_t dxn = 0;
			int16_t max = (top >= left) ? top : left;
			if(topleft >= max) max = topleft;

			if(topleft == max) dxn += DIAG_ARROW;
			if(top == max) dxn += UP_ARROW;
			if(left == max) dxn += LEFT_ARROW;

			*dest_diag = dxn;
			dest_diag += count - 1;
			*inter_diag = max;
			inter_diag += count - 1;
		}
	}

	for(uint16_t i = 1; i < count; i++) {
		// k is row index, j is column index
		int16_t *inter_topLeft = MAT_PTR(inter, i - 1, count - 2);
		int16_t *inter_top = MAT_PTR(inter, i - 1, count - 1);
		int16_t *inter_left = MAT_PTR(inter, i, count - 2); 
		int16_t *dest_diag = MAT_PTR(dest, i, count - 1);
		int16_t *inter_diag = MAT_PTR(inter, i, count - 1);
		for(uint16_t j = count, k = i; j > i; j--, k++) {
			int16_t topleft = *inter_topLeft;
			inter_topLeft += count - 1;
			int16_t top = *inter_top;
			inter_top += count - 1;
			int16_t left = *inter_left;
			inter_left += count - 1;
			int16_t char1 = src1_ptr[k - 1];
			int16_t char2 = src2_ptr[j - 2];
			if(char1 == char2) topleft += 1;
			else topleft -= 1;
			top -= 1;
			left -= 1;
			uint16_t dxn = 0;
			int16_t max = (top >= left) ? top : left;
			if(topleft >= max) max = topleft;

			if(topleft == max) dxn += DIAG_ARROW;
			if(top == max) dxn += UP_ARROW;
			if(left == max) dxn += LEFT_ARROW;

			*dest_diag = dxn;
			dest_diag += count - 1;
			*inter_diag = max;
			inter_diag += count - 1;
		}
	}
}

void align_check(void) {
#ifdef CONSOLE
	uint16_t count = MAT_GET_DIM((&mat_b_dense), 0);
	//TODO double check that reshape doesn't screw up everything
	MAT_RESHAPE(b4, 1, count, count);
  UNSAFE_ATOMIC_START;
	printf("\nDistance Matrix:");
	MAT_DUMP(b4, 0);
  UNSAFE_ATOMIC_END
	MAT_RESHAPE(b3, 1, count, count);
  UNSAFE_ATOMIC_START;
	printf("\nDirection Matrix:");
	MAT_DUMP(b3, 0);
  UNSAFE_ATOMIC_END
#endif
}

void align_data(float (*sense)()) {
	uint16_t count = MAT_GET_DIM((&mat_a_dense), 0);
  UNSAFE_ATOMIC_START;
	for(uint16_t i = 0; i < count; i++) {
		float float_val = sense();
		fixed fixed_val = F_LIT(float_val);
		MAT_SET(b1, fixed_val, i);
#ifndef HPVLP
			printf("%i ,",MAT_GET(b1,i));
		}
		printf("=========done\r\n");
#else
		}// apologies for putting a paren in an ifdef block...
#endif
  UNSAFE_ATOMIC_END;
	for(uint16_t i = 0; i < count; i++) {
		MAT_SET(b2, MAT_GET(b1,(i+4) % count), i);
	}
}

void align_short_data(float (*sense)(),uint16_t sample_len) {
	uint16_t count = MAT_GET_DIM((&mat_a_dense), 0);
	static uint16_t i = 0;
  UNSAFE_ATOMIC_START;
	for(uint16_t k = 0; k < sample_len; k++) {
		float float_val = sense();
		fixed fixed_val = F_LIT(float_val);
		MAT_SET(b1, fixed_val, i);
#ifndef HPVLP
			printf("%i: %i \r\n",i,MAT_GET(b1,i));
#endif
		i++;
		if (i >= count) {
			i = 0;
		}
	}
  UNSAFE_ATOMIC_END;
	for(uint16_t i = 0; i < count; i++) {
		MAT_SET(b2, MAT_GET(b1,(i+4) % count), i);
	}
}
