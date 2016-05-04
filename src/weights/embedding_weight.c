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

#include <string.h>

#include <stutils/st_log.h>
#include <stutils/st_utils.h>

#include "embedding_weight.h"

void emb_wt_destroy(emb_wt_t *emb_wt)
{
    if (emb_wt == NULL) {
        return;
    }
}

emb_wt_t* emb_wt_dup(emb_wt_t *e)
{
    emb_wt_t *emb_wt = NULL;

    ST_CHECK_PARAM(e == NULL, NULL);

    emb_wt = (emb_wt_t *) malloc(sizeof(emb_wt_t));
    if (emb_wt == NULL) {
        ST_WARNING("Falied to malloc emb_wt_t.");
        goto ERR;
    }
    memset(emb_wt, 0, sizeof(emb_wt_t));

    emb_wt->forward = e->forward;
    emb_wt->backprop = e->backprop;

    return emb_wt;

ERR:
    safe_emb_wt_destroy(emb_wt);
    return NULL;
}

int emb_wt_load_header(emb_wt_t **emb_wt, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    return 0;
}

int emb_wt_load_body(emb_wt_t *emb_wt, int version, FILE *fp, bool binary)
{
    return 0;
}

int emb_wt_save_header(emb_wt_t *emb_wt, FILE *fp, bool binary)
{
    return 0;
}

int emb_wt_save_body(emb_wt_t *emb_wt, FILE *fp, bool binary)
{
    return 0;
}
