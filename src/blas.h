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

#ifndef  _CONNLM_BLAS_H_
#define  _CONNLM_BLAS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

#ifdef _USE_BLAS_
#  ifdef _HAVE_MKL_
#    include <mkl.h>
#  elif defined(_HAVE_ATLAS_)
#    include <cblas.h>
#  elif defined(_HAVE_ACCELERATE_)
#    include <Accelerate/Accelerate.h>
#  else
#    warn "No MKL or ATLAS included, fallback to Non-Blas"
#    undef _USE_BLAS_
#  endif
#endif

#if REAL_TYPE == double
#    define  cblas_gemm cblas_dgemm
#    define  cblas_gemv cblas_dgemv
#else
#    define  cblas_gemm cblas_sgemm
#    define  cblas_gemv cblas_sgemv
#endif

#ifdef __cplusplus
}
#endif

#endif
