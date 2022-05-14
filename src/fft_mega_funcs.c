#include <libfixed/fixed.h>
#include <libmat/mat.h>
#include <libmspbuiltins/builtins.h>
#include <libmsp/mem.h>
#include <stdio.h>
#include <libio/console.h>
#include <librustic/checkpoint.h>
#include "common/fft.h"
#include "common/mat.h"
#include "harness.h"
//#include "common/mat_externs.h"

// Runs 1d fft on a single row or column
// Args:
// dest_real/imag: matrix where result will go
// inter_real/imag: input matrix
// row,col: you get the idea
// num_elements: number of elements in just the row/col being evaluated
// stride: set to 1 for going across a row, set to number of cols if going down
// a column
uint16_t flag = 1;

void fft1d(mat_t *dest_real, mat_t *dest_imag,
			mat_t *inter_real, mat_t *inter_imag,
			uint16_t row_idx, uint16_t col_idx, uint16_t num_elements, uint16_t stride) {
	mat_t *dest_real_ptr = dest_real;
	mat_t *dest_imag_ptr = dest_imag;
	mat_t *src_real_ptr = inter_real;
	mat_t *src_imag_ptr = inter_imag;
	fixed *src_real_ptr_base = MAT_PTR(src_real_ptr, row_idx, col_idx);
	fixed *src_imag_ptr_base = MAT_PTR(src_imag_ptr, row_idx, col_idx);
	fixed *dest_real_ptr_base = MAT_PTR(dest_real_ptr, row_idx, col_idx);
	fixed *dest_imag_ptr_base = MAT_PTR(dest_imag_ptr, row_idx, col_idx);
	for(uint16_t i = 0; i < num_elements * stride; i += stride) {
		*(dest_real_ptr_base + i) = *(src_real_ptr_base + i);
		*(dest_imag_ptr_base + i) = *(src_imag_ptr_base + i);
	}
	// Bit reverse
	for(uint16_t i = 0; i < num_elements; i++) {
		uint16_t j = 0;
		uint16_t m = i;
		uint16_t p = 1;
		while(p < num_elements) {
			j = (j << 1) + (m & 1);
			m >>= 1;
			p <<= 1;
		}
		if(j > i) { // Do a swap
			fixed tmp = *(dest_real_ptr_base + (j * stride));
			*(dest_real_ptr_base + (j * stride)) = *(dest_real_ptr_base + (i * stride));
			*(dest_real_ptr_base + (i * stride)) = tmp;
			tmp = *(dest_imag_ptr_base + (j * stride));
			*(dest_imag_ptr_base + (j * stride)) = *(dest_imag_ptr_base + (i * stride));
			*(dest_imag_ptr_base + (i * stride)) = tmp;
		}
	}

	uint16_t Ls = 1;
	while(Ls < num_elements) {
		uint16_t step = (Ls << 1);
		for(uint16_t j = 0; j < Ls; j++) {
			fixed theta = F_DIV(F_LIT(j), F_LIT(Ls));
			fixed wr = wrs[theta];
			fixed wi = F_MUL(F_LIT(-flag), wis[theta]);
			fixed *dest_real_ptr1 = dest_real_ptr_base + ((j + Ls) * stride);
			fixed *dest_imag_ptr1 = dest_imag_ptr_base + ((j + Ls) * stride);
			fixed *dest_real_ptr2 = dest_real_ptr_base + ((j * stride));
			fixed *dest_imag_ptr2 = dest_imag_ptr_base + ((j * stride));
			for(uint16_t k = j; k < num_elements; k += step) {
				fixed tr = F_SUB(F_MUL(wr, *dest_real_ptr1),
					F_MUL(wi, *dest_imag_ptr1));
				fixed ti = F_ADD(F_MUL(wr, *dest_imag_ptr1),
					F_MUL(wi, *dest_real_ptr1));
				*dest_real_ptr1 = F_SUB(*dest_real_ptr2, tr);
				*dest_imag_ptr1 = F_SUB(*dest_imag_ptr2, ti);
				*dest_real_ptr2 = F_ADD(*dest_real_ptr2, tr);
				*dest_imag_ptr2 = F_ADD(*dest_imag_ptr2, ti);
				dest_real_ptr1 += stride * step;
				dest_imag_ptr1 += stride * step;
				dest_real_ptr2 += stride * step;
				dest_imag_ptr2 += stride * step;
			}
		}
		Ls <<= 1;
	}
}

void fft1d_row(uint16_t num_elements, mat_t *dest_real, mat_t *dest_imag, mat_t *inter_real,
  uint16_t row_idx) {
	mat_t *dest_real_ptr = dest_real;
	mat_t *dest_imag_ptr = dest_imag;
	mat_t *src_real_ptr = inter_real;
	fixed *src_real_ptr_base = MAT_PTR(src_real_ptr, row_idx, 0);
	fixed *dest_real_ptr_base = MAT_PTR(dest_real_ptr, row_idx, 0);
	fixed *dest_imag_ptr_base = MAT_PTR(dest_imag_ptr, row_idx, 0);
	for(uint16_t i = 0; i < num_elements; i++) {
		*(dest_real_ptr_base + i) = *(src_real_ptr_base + i);
		*(dest_imag_ptr_base + i) = 0;
	}
	// Bit reverse
	for(uint16_t i = 0; i < num_elements; i++) {
		uint16_t j = 0;
		uint16_t m = i;
		uint16_t p = 1;
		while(p < num_elements) {
			j = (j << 1) + (m & 1);
			m >>= 1;
			p <<= 1;
		}
		if(j > i) { // Do a swap
			fixed tmp = *(dest_real_ptr_base + (j));
			*(dest_real_ptr_base + (j)) = *(dest_real_ptr_base + (i));
			*(dest_real_ptr_base + (i)) = tmp;
		}
	}

	uint16_t Ls = 1;
	while(Ls < num_elements) {
		uint16_t step = (Ls << 1);
		for(uint16_t j = 0; j < Ls; j++) {
			fixed theta = F_DIV(F_LIT(j), F_LIT(Ls));
			fixed wr = wrs[theta];
			fixed wi = F_MUL(F_LIT(-flag), wis[theta]);
			fixed *dest_real_ptr1 = dest_real_ptr_base + ((j + Ls));
			fixed *dest_imag_ptr1 = dest_imag_ptr_base + ((j + Ls));
			fixed *dest_real_ptr2 = dest_real_ptr_base + ((j));
			fixed *dest_imag_ptr2 = dest_imag_ptr_base + ((j));
			for(uint16_t k = j; k < num_elements; k += step) {
				fixed tr = F_SUB(F_MUL(wr, *dest_real_ptr1),
					F_MUL(wi, *dest_imag_ptr1));
				fixed ti = F_ADD(F_MUL(wr, *dest_imag_ptr1),
					F_MUL(wi, *dest_real_ptr1));
				*dest_real_ptr1 = F_SUB(*dest_real_ptr2, tr);
				*dest_imag_ptr1 = F_SUB(*dest_imag_ptr2, ti);
				*dest_real_ptr2 = F_ADD(*dest_real_ptr2, tr);
				*dest_imag_ptr2 = F_ADD(*dest_imag_ptr2, ti);
				dest_real_ptr1 += step;
				dest_imag_ptr1 += step;
				dest_real_ptr2 += step;
				dest_imag_ptr2 += step;
			}
		}
		Ls <<= 1;
	}
}


// Initializes matrices
void fft_mega_init(int mat_dim) {
	MAT_RESHAPE(b1, mat_dim, mat_dim);
	MAT_RESHAPE(b2, mat_dim, mat_dim);
	MAT_RESHAPE(b3, mat_dim, mat_dim);
	MAT_RESHAPE(b4, mat_dim, mat_dim);
	//MAT_RESHAPE(b5, mat_dim, mat_dim);
	//MAT_RESHAPE(b6, mat_dim, mat_dim);
#ifndef USE_SENSORS
	mat_t *mat_input_ptr = &mat_a_dense;
#endif
	//mat_t *mat_input_ptr2 = &mat_a_imag_dense; // in case we want to initialize
	//the imaginary matrix for testing
	for(uint16_t i = 0; i < mat_dim; i++) {
		for(uint16_t j = 0; j < mat_dim; j++) {
			#ifndef USE_SENSORS
			MAT_SET(b1, MAT_GET(mat_input_ptr, i, j), i, j);
			#endif
			//MAT_SET(b4, MAT_GET(mat_input_ptr2, i, j), i, j);
			// The following are not necessary for the first go round, but setting
			// these to zero provides piece of mind for repeated trials
			MAT_SET(b2, 0, i, j);
			MAT_SET(b3, 0, i, j);
			MAT_SET(b4, 0, i, j);
		}
	}
	return;
}

// Tests how fixed point is working
void fixed_test(void) {
	for (int16_t i = -50; i < 50; i += 1) {
		for (int16_t j = -50; j < 50; j+= 1) {
			fixed tmp = F_MUL(F_LIT(i),F_LIT(j));
			printf("%i * %i = %i, should be %i\r\n",i,j,tmp/32, i*j);
			if (tmp < 0 && i*j > 0 || tmp > 0 && i*j < 0) {
				printf("Sign error!\r\n");
			}
		}
	}
	return;
}


// Prints out the (integer) value of the destination matrices (b2 and b3)
//TODO: make this take matrix pointers as input
void fft_mega_check(int mat_dim) {
	uint16_t rows = mat_dim;
	uint16_t cols = mat_dim;
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < cols; j++) {
			printf("%i ",(*MAT_PTR(b2,i,j))/32);
			printf("%ij,",(*MAT_PTR(b3,i,j))/32);
		}
		printf("\r\n");
	}
	printf("[END]\r\n\r\n");
	return;
}

// Runs a 2d fft by performing 1d ffts along the 0th and 1st dimensions of the
// matrix --> previously fft2d
void fft_mega(int mat_dim, mat_t *dest_real, mat_t *dest_imag, mat_t *inter_real,
			mat_t *inter_imag) {
  int16_t num_elements =  mat_dim;
	for (int i = 0; i < mat_dim; i++) {
		fft1d(dest_real,dest_imag,inter_real,inter_imag,i,0,num_elements,1);
	}
	fixed *inter_real_ptr = MAT_PTR(inter_real, 0, 0) + num_elements;
	fixed *inter_imag_ptr = MAT_PTR(inter_imag, 0, 0) + num_elements;
	fixed *dest_real_ptr = MAT_PTR(dest_real, 0, 0) + num_elements;
	fixed *dest_imag_ptr = MAT_PTR(dest_imag, 0, 0) + num_elements;
	for(int i = 0; i < num_elements * num_elements; i++) {
		*inter_real_ptr++ = *dest_real_ptr++;
		*inter_imag_ptr++ = *dest_imag_ptr++;
	}

	for (int j = 0; j < mat_dim; j++) {
		fft1d(dest_real,dest_imag,inter_real,inter_imag,0,j,num_elements,num_elements);
	}
	return;
}

#define UNROLL_FACTOR 4
// Takes a sensor function and pulls data into the right matrix
void fft_mega_data(float (*sense)(),int mat_dim) {
	for (int i = 0; i < mat_dim; i++) {
      if (i % 2 == 0) {
        UNSAFE_ATOMIC_START;
        printf("I: %i\r\n",i);
      }
      for (int j = 0; j < mat_dim; j++) {
        float float_val = sense();
        fixed fixed_val = F_LIT(float_val);
        // Write into matrix
        MAT_SET(b1, fixed_val, i, j);
#ifndef HPVLP
        PRINTF("%i ",MAT_GET(b1,i,j));
      }
      PRINTF("\r\n");
      if ((i +1) % 4 == 0) {
        UNSAFE_ATOMIC_END;
      }
	}
  UNSAFE_ATOMIC_START;
	PRINTF("=========done\r\n");
  UNSAFE_ATOMIC_END;
#else
		  }// apologies for putting a paren in an ifdef block...
    //
    if ((i +1) % 2 == 0) {
      UNSAFE_ATOMIC_END;
    }
	}
#endif
}


// Takes a sensor function and pulls data into the right matrix
void fft_mega_short_data(float (*sense)(),uint16_t sample_len, int mat_dim) {
	static int i=0,j=0;
	for (int k = 0; k < sample_len; k++) {
		float float_val = sense();
		fixed fixed_val = F_LIT(float_val);
		// Write into matrix
		MAT_SET(b1, fixed_val, i, j);
#ifndef HPVLP
		printf("%i %i: %i \r\n",i,j,MAT_GET(b1,i,j));
#endif
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
	}
}

__nv fixed wrs[32] = {
F_LIT(1.0),
F_LIT(0.995184726672),
F_LIT(0.980785280403),
F_LIT(0.956940335732),
F_LIT(0.923879532511),
F_LIT(0.881921264348),
F_LIT(0.831469612303),
F_LIT(0.773010453363),
F_LIT(0.707106781187),
F_LIT(0.634393284164),
F_LIT(0.55557023302),
F_LIT(0.471396736826),
F_LIT(0.382683432365),
F_LIT(0.290284677254),
F_LIT(0.195090322016),
F_LIT(0.0980171403296),
F_LIT(6.12323399574e-17),
F_LIT(-0.0980171403296),
F_LIT(-0.195090322016),
F_LIT(-0.290284677254),
F_LIT(-0.382683432365),
F_LIT(-0.471396736826),
F_LIT(-0.55557023302),
F_LIT(-0.634393284164),
F_LIT(-0.707106781187),
F_LIT(-0.773010453363),
F_LIT(-0.831469612303),
F_LIT(-0.881921264348),
F_LIT(-0.923879532511),
F_LIT(-0.956940335732),
F_LIT(-0.980785280403),
F_LIT(-0.995184726672)};

__nv fixed wis[32] = {
F_LIT(0.0),
F_LIT(0.0980171403296),
F_LIT(0.195090322016),
F_LIT(0.290284677254),
F_LIT(0.382683432365),
F_LIT(0.471396736826),
F_LIT(0.55557023302),
F_LIT(0.634393284164),
F_LIT(0.707106781187),
F_LIT(0.773010453363),
F_LIT(0.831469612303),
F_LIT(0.881921264348),
F_LIT(0.923879532511),
F_LIT(0.956940335732),
F_LIT(0.980785280403),
F_LIT(0.995184726672),
F_LIT(1.0),
F_LIT(0.995184726672),
F_LIT(0.980785280403),
F_LIT(0.956940335732),
F_LIT(0.923879532511),
F_LIT(0.881921264348),
F_LIT(0.831469612303),
F_LIT(0.773010453363),
F_LIT(0.707106781187),
F_LIT(0.634393284164),
F_LIT(0.55557023302),
F_LIT(0.471396736826),
F_LIT(0.382683432365),
F_LIT(0.290284677254),
F_LIT(0.195090322016),
F_LIT(0.0980171403296)};



