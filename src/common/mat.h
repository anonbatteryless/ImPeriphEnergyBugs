#ifndef _MAT_H_
#define _MAT_H_
#warning "Including mat.h now!"

#ifndef INPUT_SIZE
#define INPUT_SIZE 1
#endif

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 64
#endif
// List of needs:
// ALIGN: mat_a_dense, mat_b_dense b1-b4
// DCONV: mat_a_dense, mat_c_dense b1-b3
// DMM: mat_a_dense, b1-b3
// DMV: mat_a_dense, mat_b_dense, b1-b3
// DWT: mat_a_dense, b1-b3
// FFT: mat_a_dense b1-b4, FFT stuff
// SCONV: mat_a_dense, mat_c_dense, mat_c_sparse, b1-b3 --> ***
// SMM: mat_a_sparse, mat_a_bsparse,b2,b3 --> ***
// SMV: mat_a_dense,mat_b_dense,mat_a_sparse,mat_a_bsparse b1-b3
// SORT: mat_a_dense,mat_b_dense, b1-b3
//*** need to fix for mat size = 128


#define ALIGN 0
#define DCONV 1
#define DMM   2
#define DMV   3
#define DWT   4
#define FFT   5
#define SCONV 6
#define SMM   7
#define SMV   8
#define SORT  9


#if INPUT_SIZE == 1
#pragma message "Small"
#warning "Setting mat size to 16"
#define MAT_SIZE 16
#define TILE_SIZE 3
#define MAT_BLOCK_SIZE 16
#define MAT_BLOCK_COLS 1


#if FUNC_NUM != SMM
#include "small/a_dense.h"
#endif

#if FUNC_NUM==ALIGN || FUNC_NUM==DMV || FUNC_NUM==SMV || FUNC_NUM==SORT
#include "small/b_dense.h"
#endif

#if FUNC_NUM==DCONV || FUNC_NUM==SCONV
#include "small/c_dense.h"
#endif

#if FUNC_NUM==SMM || FUNC_NUM==SMV
#include "small/a_sparse.h"
#endif

//#include "small/b_sparse.h"
#if FUNC_NUM==SCONV
#include "small/c_sparse.h"
#endif

#if FUNC_NUM==SMM
#include "small/a_bsparse.h"
#endif
//#include "small/b_bsparse.h"

#elif INPUT_SIZE == 2
#pragma message "Medium"
#warning "Setting mat size to 64"
#define MAT_SIZE 64
#define TILE_SIZE 5
#define MAT_BLOCK_SIZE BLOCK_SIZE
#define MAT_BLOCK_COLS MAT_SIZE / BLOCK_SIZE

#if FUNC_NUM != SMM
#include "medium/a_dense.h"
#endif

#if FUNC_NUM==ALIGN || FUNC_NUM==DMV || FUNC_NUM==SMV || FUNC_NUM==SORT
#include "medium/b_dense.h"
#endif

#if FUNC_NUM==DCONV || FUNC_NUM==SCONV
#include "medium/c_dense.h"
#endif

#if FUNC_NUM==SMM || FUNC_NUM==SMV
#include "medium/a_sparse.h"
#endif

//#include "medium/b_sparse.h"
#if FUNC_NUM==SCONV
#include "medium/c_sparse.h"
#endif

#if FUNC_NUM==SMM
#include "medium/a_bsparse.h"
#endif
//#include "medium/b_bsparse.h"


#else

#pragma message "Large"
#define MAT_SIZE 128
#define TILE_SIZE 5
#define MAT_BLOCK_SIZE 64
#define MAT_BLOCK_COLS MAT_SIZE / BLOCK_SIZE
#endif

#if FUNC_NUM != SMM
#include "large/a_dense.h"
#endif

#if FUNC_NUM==ALIGN || FUNC_NUM==DMV || FUNC_NUM==SMV || FUNC_NUM==SORT
#include "large/b_dense.h"
#endif

#if FUNC_NUM==DCONV || FUNC_NUM==SCONV
#include "large/c_dense.h"
#endif

#if FUNC_NUM==SMM || FUNC_NUM==SMV
#include "large/a_sparse.h"
#endif

//#include "large/b_sparse.h"
#if FUNC_NUM==SCONV
#include "large/c_sparse.h"
#endif

#if FUNC_NUM==SMM
#include "large/a_bsparse.h"
#endif
//#include "small/b_bsparse.h"


#if FUNC_NUM != SMM
__nv mat_t mat_a_dense = {
		.dims = {MAT_SIZE, MAT_SIZE}, .strides = {MAT_SIZE, 1}, .len_dims = 2, .data = a_dense,
};
__nv mat_t *mat_a_dense_pseudo = &mat_a_dense;
#endif

#if 0
__nv mat_t mat_a_imag_dense = {
		.dims = {MAT_SIZE, MAT_SIZE}, .strides = {MAT_SIZE, 1}, .len_dims = 2, .data
		= a_imag_dense,
};
#endif

#if FUNC_NUM==SMM || FUNC_NUM==SMV
__nv mat_t mat_a_sparse = {.dims = {A_SPARSE_LEN},
								.strides = {1},
								.len_dims = 1,
								.data = a_sparse,
								.sparse = {.dims = {MAT_SIZE, MAT_SIZE},
											 .len_dims = 2,
											 .offsets = a_sparse_offsets,
											 .sizes = a_sparse_sizes}};
__nv mat_t *mat_a_sparse_pseudo = &mat_a_sparse;
#endif

#if FUNC_NUM==SMM
#warning "Defining smm"
__nv mat_t mat_a_bsparse = {.dims = {A_BSPARSE_LEN},
								.strides = {1},
								.len_dims = 1,
								.data = a_bsparse,
								.sparse = {.dims = {MAT_BLOCK_SIZE, MAT_BLOCK_COLS},
											 .len_dims = 2,
											 .offsets = a_bsparse_offsets,
											 .sizes = a_bsparse_sizes}};
__nv mat_t *mat_a_bsparse_pseudo = &mat_a_bsparse;
#endif

#if FUNC_NUM==ALIGN || FUNC_NUM==DMV || FUNC_NUM==SMV || FUNC_NUM==SORT
__nv mat_t mat_b_dense = {
		.dims = {MAT_SIZE, 1}, .strides = {1, 1}, .len_dims = 2, .data = b_dense,
};
__nv mat_t *mat_b_dense_pseudo = &mat_b_dense;
#endif

#if 0
__nv mat_t mat_b_sparse = {.dims = {B_SPARSE_LEN},
								.strides = {1},
								.len_dims = 1,
								.data = b_sparse,
								.sparse = {.dims = {MAT_SIZE, 1},
											 .len_dims = 2,
											 .offsets = b_sparse_offsets,
											 .sizes = b_sparse_sizes}};

__nv mat_t mat_b_bsparse = {.dims = {B_BSPARSE_LEN},
								.strides = {1},
								.len_dims = 1,
								.data = b_bsparse,
								.sparse = {.dims = {MAT_BLOCK_SIZE, MAT_BLOCK_COLS},
											 .len_dims = 2,
											 .offsets = b_bsparse_offsets,
											 .sizes = b_bsparse_sizes}};
#endif

#if FUNC_NUM==DCONV || FUNC_NUM==SCONV
#pragma warning "allocing c dense"
__nv mat_t mat_c_dense = {
		.dims = {1, 1, TILE_SIZE, TILE_SIZE},
		.len_dims = 4,
		.strides = {TILE_SIZE * TILE_SIZE, TILE_SIZE * TILE_SIZE, TILE_SIZE, 1},
		.data = c_dense,
};
__nv mat_t *mat_c_dense_pseudo = &mat_c_dense;
#endif

#if FUNC_NUM==SCONV
__nv mat_t mat_c_sparse = {.dims = {C_SPARSE_LEN},
								.len_dims = 1,
								.strides = {1},
								.data = c_sparse,
								.sparse = {.dims = {1, 1, TILE_SIZE, TILE_SIZE},
											 .len_dims = 4,
											 .sizes = c_sparse_sizes,
											 .offsets = c_sparse_offsets}};
__nv mat_t *mat_c_sparse_pseudo = &mat_c_sparse;
#endif

#if INPUT_SIZE == 1
#define BUF_OFFSET2 256
#define BUF_OFFSET3 512
#define BUF_OFFSET4 768
#define BUF_TOTAL   1024

#elif INPUT_SIZE == 2
#define BUF_OFFSET2 4096
#define BUF_OFFSET3 8192
#define BUF_OFFSET4 12288
#define BUF_TOTAL   16384

#elif INPUT_SIZE == 3
#define BUF_OFFSET2 16384
#define BUF_OFFSET3 32768
#define BUF_OFFSET4 49152
#define BUF_TOTAL   65536

#else
#error "Not a valid size option"
#endif

#if MAT_SIZE == 128
#pragma "running with 128"
#endif

__nv fixed buf[BUF_TOTAL];
__nv mat_t buf1 = {.data = buf};
__nv mat_t buf2 = {.data = buf + BUF_OFFSET2};
__nv mat_t buf3 = {.data = buf + BUF_OFFSET3};
__nv mat_t buf4 = {.data = buf + BUF_OFFSET4};

#if 0
_nv mat_t buf5 = {.data = buf + MAT_SIZE * MAT_SIZE * 4};
_nv mat_t buf6 = {.data = buf + MAT_SIZE * MAT_SIZE * 5};
#endif
__nv mat_t *b1 = &buf1;
__nv mat_t *b2 = &buf2;
__nv mat_t *b3 = &buf3;
__nv mat_t *b4 = &buf4;
#if 0
__nv mat_t *b5 = &buf5;
__nv mat_t *b6 = &buf6;
#endif

#endif
