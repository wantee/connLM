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

#ifndef  _CONNLM_CONFIG_H_
#define  _CONNLM_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CONNLM_VERSION   "0.1"

#define CONNLM_FILE_VERSION   2

#ifndef _USE_DOUBLE_
#define _USE_DOUBLE_ 0
#endif

#if _USE_DOUBLE_ == 1
   typedef double real_t;
#  define REAL_FMT "%lf"
#else
   typedef float real_t;
#  define REAL_FMT "%f"
#endif

typedef unsigned long count_t;
#define COUNT_FMT "%lu"
#define COUNT_MAX ((count_t)-1)

typedef unsigned long long hash_t;
typedef long long hash_size_t;
#define HASH_SIZE_FMT "%lld"

#define ALIGN_SIZE 128

#ifdef _USE_BLAS_
#  define _MINI_UPDATE_
#endif

#define SENT_END "</s>"
#define SENT_END_ID 0
#define UNK "<unk>"
#define UNK_ID 1

#ifdef __cplusplus
}
#endif

#endif

