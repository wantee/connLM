/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Wang Jian
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef  _CONNLM_UTILS_H_
#define  _CONNLM_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <math.h>

#include <stutils/st_opt.h>

#include <connlm/config.h>

#if _CONNLM_MATH_ == 0 // fast math
#  include "fastexp.h"
#  define sigmoidr fastersigmoid
#  define expr fasterexp
#  define logr fasterlog
#  define tanhr fastertanh
#elif _CONNLM_MATH_ == 1 // single math
#  define sigmoidr sigmoidf
#  define expr expf
#  define logr logf
#  define tanhr tanhf
#else // double math
#  define sigmoidr sigmoidd
#  define expr exp
#  define logr log
#  define tanhr tanh
#endif // _CONNLM_MATH_

#define exp10(a) pow(10.0, a)

#define logn(loga, base) ((base) == 0) ? (loga) : ((loga) / log(base))
#define logn10(log10a, base) ((base) == 0) ? (log10a / M_LOG10E) : (log10a / log10(base))

void matXvec(real_t *dst, real_t *mat, real_t *vec,
        int mat_wid, int vec_size, real_t scale);

void vecXmat(real_t *dst, real_t *vec, real_t *mat,
        int mat_col, int vec_size, real_t scale);

/*
 * Computing C = alpha * A' * B + beta * C
 * A is [k X m]; B is [k X n]; C is [m X n]
 * A' is transpose of A.
 */
void matXmat(real_t *C, real_t *A, real_t *B, int m, int n, int k,
        real_t alpha, real_t beta);

real_t dot_product(real_t *v1, real_t *v2, int vec_size);

void sigmoid(real_t *vec, int vec_size);

void softmax(real_t *vec, int vec_size);

/*
 * compute multinomial logit.
 * can be seen as
 *  full = vec + [0.0]
 *  softmax(full)
 *  return vec
 */
void multi_logit(real_t *vec, int vec_size);

void tanH(real_t *vec, int vec_size);

void connlm_show_usage(const char *module_name, const char *header,
        const char *usage, const char *eg,
        st_opt_t *opt, const char *trailer);

typedef enum _model_filter_t_ {
    MF_NONE       = 0x0000,
    MF_VOCAB      = 0x0001,
    MF_OUTPUT     = 0x0002,
    MF_COMP_NEG   = 0x0004,
    MF_ALL        = 0x0007,
    MF_ERR        = 0x1000,
} model_filter_t;

/**
 * Parsing a model filter.
 * A model filter can be: mdl,[+-][ovc{comp1}c{comp2}]:file_name.
 * Any string not in such format will be treated as a model file name.
 *
 * @ingroup g_utils
 * @param[in] mdl_filter string for the model filter.
 * @param[out] mdl_file name of model file.
 * @param[in] mdl_file_len max len of model file buffer.
 * @param[out] comp_names names of components specified, every MAX_NAME_LEN is one name. Must be st_freed outside this function.
 * @param[out] num_comp number of components specified, -1 means all.
 * @return filter type. MF_ERR if error.
 */
model_filter_t parse_model_filter(const char *mdl_filter,
        char *mdl_file, size_t mdl_file_len, char **comp_names,
        int *num_comp);

const char* model_filter_help();

char* escape_dot(char *out, size_t len, const char *str);

/**
 * Concatable Matrix
 *
 * Matrix built by concating row by row
 *
 * @ingroup g_updater_wt
 */
typedef struct _concatable_matrix_t_ {
    real_t *val; /**< values of matrix. */
    int col; /**< number of col. */
    int n_row; /**< number of row. */
    int cap_row; /**< capacity of row. */
} concat_mat_t;

#define safe_concat_mat_destroy(ptr) do {\
    if((ptr) != NULL) {\
        concat_mat_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

void concat_mat_destroy(concat_mat_t *mat);

/*
 * Add one row to concat_mat
 */
int concat_mat_add_row(concat_mat_t *mat, real_t *vec, int vec_size);

/*
 * Add all rows from src to dst
 */
int concat_mat_add_mat(concat_mat_t *dst, concat_mat_t *src);

/**
 * Storage Format for connLM model.
 * @ingroup g_connlm
 */
typedef enum _connlm_format_t_ {
    CONN_FMT_UNKNOWN             = 0x0000, /**< Unknown format. */
    CONN_FMT_TXT                 = 0x0001, /**< Text format. */
    CONN_FMT_BIN                 = 0x0002, /**< (flat) Binary format. */
    CONN_FMT_ZEROS_COMPRESSED    = 0x0004, /**< zeros-compressed (binary) format. */
    CONN_FMT_SHORT_QUANTIZATION  = 0x0008, /**< quantify to short (binary) format. */
} connlm_fmt_t;

#define connlm_fmt_is_bin(fmt) ((fmt) > 1)

/**
 * Parse the string representation to a connlm_fmt_t
 * @ingroup g_connlm
 * @param[in] str string to be parsed.
 * @return the format, CONN_FMT_UNKNOWN if string not valid or any other error.
 */
connlm_fmt_t connlm_format_parse(const char *str);

/**
 * quantify a float number to sint16
 * @ingroup g_connlm
 * @param[in] r the float number.
 * @param[in] multiple multiple to enlarge the number before quantify.
 * @return quantified value.
 */
int16_t quantify_int16(real_t r, real_t multiple);

/**
 * Load a short-quantization and zero-compressed vector from a stream.
 * @ingroup g_connlm
 * @param[out] a real-valued vector.
 * @param[in] sz size of the vector.
 * @param[in] fp file stream.
 * @return non-zero if any error.
 */
int load_sq_zc(real_t *a, size_t sz, FILE *fp);

/**
 * Load a short-quantization vector from a stream.
 * @ingroup g_connlm
 * @param[out] a real-valued vector.
 * @param[in] sz size of the vector.
 * @param[in] fp file stream.
 * @return non-zero if any error.
 */
int load_sq(real_t *a, size_t sz, FILE *fp);

/**
 * Load a zero-compressed vector from a stream.
 * @ingroup g_connlm
 * @param[out] a real-valued vector.
 * @param[in] sz size of the vector.
 * @param[in] fp file stream.
 * @return non-zero if any error.
 */
int load_zc(real_t *a, size_t sz, FILE *fp);

/**
 * Save a short-quantization and zero-compressed vector to a stream.
 * @ingroup g_connlm
 * @param[in] a real-valued vector.
 * @param[in] sz size of the vector.
 * @param[out] fp file stream.
 * @return non-zero if any error.
 */
int save_sq_zc(real_t *a, size_t sz, FILE *fp);

/**
 * Save a short-quantization vector to a stream.
 * @ingroup g_connlm
 * @param[in] a real-valued vector.
 * @param[in] sz size of the vector.
 * @param[out] fp file stream.
 * @return non-zero if any error.
 */
int save_sq(real_t *a, size_t sz, FILE *fp);

/**
 * Save a zero-compressed vector to a stream.
 * @ingroup g_connlm
 * @param[in] a real-valued vector.
 * @param[in] sz size of the vector.
 * @param[out] fp file stream.
 * @return non-zero if any error.
 */
int save_zc(real_t *a, size_t sz, FILE *fp);

/**
 * parse a real-valued vector from a string.
 * @ingroup g_connlm
 * @param[in] line input string.
 * @param[out] vec the vector. Must be st_free outside.
 * @param[out] name name of the vector, will be ignored if NULL.
 * @param[in] name_len length of name buffer,
 * @return size of vector, -1 if any error.
 */
int parse_vec(const char *line, real_t **vec, char *name, size_t name_len);

#ifdef __cplusplus
}
#endif

#endif
