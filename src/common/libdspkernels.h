#ifndef _LIBDSPKERNELS_H_
#define _LIBDSPKERNELS_H_

void fft1d_row(uint16_t num_elements, mat_t *dest_real, mat_t *dest_imag, mat_t *inter_real,
  uint16_t row_idx);

void fft1d(mat_t *dest_real, mat_t *dest_imag,
			mat_t *inter_real, mat_t *inter_imag,
			uint16_t row_idx, uint16_t col_idx, uint16_t num_elements, uint16_t
      stride);

#endif
