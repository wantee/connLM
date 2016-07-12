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

#include <stutils/st_macro.h>
#include <stutils/st_log.h>

#include "../../glues/out_wt_glue.h"
#include "out_wt_glue_updater.h"

typedef struct _out_wt_glue_updater_data_t_ {
} out_wt_glue_updater_data_t;

#define safe_out_wt_glue_updater_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        out_wt_glue_updater_data_destroy((out_wt_glue_updater_data_t *)ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

void out_wt_glue_updater_data_destroy(out_wt_glue_updater_data_t *data)
{
    if (data == NULL) {
        return;
    }
}

out_wt_glue_updater_data_t* out_wt_glue_updater_data_init()
{
    out_wt_glue_updater_data_t *data = NULL;

    data = (out_wt_glue_updater_data_t *)malloc(sizeof(out_wt_glue_updater_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to malloc out_wt_glue_updater_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(out_wt_glue_updater_data_t));

    return data;
ERR:
    safe_out_wt_glue_updater_data_destroy(data);
    return NULL;
}

void out_wt_glue_updater_destroy(glue_updater_t *glue_updater)
{
    if (glue_updater == NULL) {
        return;
    }

    safe_out_wt_glue_updater_data_destroy(glue_updater->extra);
}

int out_wt_glue_updater_init(glue_updater_t *glue_updater)
{
    ST_CHECK_PARAM(glue_updater == NULL, -1);

    if (strcasecmp(glue_updater->glue->type, OUT_WT_GLUE_NAME) != 0) {
        ST_WARNING("Not a direct glue_updater. [%s]",
                glue_updater->glue->type);
        return -1;
    }

    glue_updater->extra = (void *)out_wt_glue_updater_data_init();
    if (glue_updater->extra == NULL) {
        ST_WARNING("Failed to out_wt_glue_updater_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_out_wt_glue_updater_data_destroy(glue_updater->extra);
    return -1;
}

int out_wt_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater)
{
//    out_wt_glue_data_t *data = NULL;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL, -1);

//    data = (out_wt_glue_data_t *)glue_updater->glue->extra;

    return 0;
}
