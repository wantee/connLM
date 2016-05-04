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

#ifndef  _CONNLM_EMBEDDING_WEIGHT_H_
#define  _CONNLM_EMBEDDING_WEIGHT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

/** @defgroup g_wt_embedding NNet embedding weight.
 * @ingroup g_weight
 * Data structures and functions for NNet embedding weight.
 */

/**
 * Embedding weight.
 * @ingroup g_wt_embedding
 */
typedef struct _embedding_weight_t_ {
    /** forward function. */
    int (*forward)(struct _embedding_weight_t_ *wt);
    /** backpro function. */
    int (*backprop)(struct _embedding_weight_t_ *wt);
} emb_wt_t;

/**
 * Destroy a embedding weight and set the pointer to NULL.
 * @ingroup g_wt_embedding
 * @param[in] ptr pointer to emb_wt_t.
 */
#define safe_emb_wt_destroy(ptr) do {\
    if((ptr) != NULL) {\
        emb_wt_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a embedding weight.
 * @ingroup g_wt_embedding
 * @param[in] emb embedding weight to be destroyed.
 */
void emb_wt_destroy(emb_wt_t* emb);

/**
 * Duplicate a embedding weight.
 * @ingroup g_wt_embedding
 * @param[in] e embedding weight to be duplicated.
 * @return the duplicated embedding weight.
 */
emb_wt_t* emb_wt_dup(emb_wt_t *e);

/**
 * Load emb_wt header and initialise a new emb_wt.
 * @ingroup g_emb_wt
 * @param[out] emb_wt emb_wt initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo file stream used to print information, if it is not NULL.
 * @see emb_wt_load_body
 * @see emb_wt_save_header, emb_wt_save_body
 * @return non-zero value if any error.
 */
int emb_wt_load_header(emb_wt_t **emb_wt, int version,
        FILE *fp, bool *binary, FILE *fo_info);

/**
 * Load emb_wt body.
 * @ingroup g_emb_wt
 * @param[in] emb_wt emb_wt to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see emb_wt_load_header
 * @see emb_wt_save_header, emb_wt_save_body
 * @return non-zero value if any error.
 */
int emb_wt_load_body(emb_wt_t *emb_wt, int version, FILE *fp, bool binary);

/**
 * Save emb_wt header.
 * @ingroup g_emb_wt
 * @param[in] emb_wt emb_wt to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see emb_wt_save_body
 * @see emb_wt_load_header, emb_wt_load_body
 * @return non-zero value if any error.
 */
int emb_wt_save_header(emb_wt_t *emb_wt, FILE *fp, bool binary);

/**
 * Save emb_wt body.
 * @ingroup g_emb_wt
 * @param[in] emb_wt emb_wt to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see emb_wt_save_header
 * @see emb_wt_load_header, emb_wt_load_body
 * @return non-zero value if any error.
 */
int emb_wt_save_body(emb_wt_t *emb_wt, FILE *fp, bool binary);

#ifdef __cplusplus
}
#endif

#endif
