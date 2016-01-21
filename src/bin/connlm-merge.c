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

#include <stutils/st_log.h>
#include <stutils/st_io.h>

#include <connlm/utils.h>
#include <connlm/connlm.h>

bool g_binary;

st_opt_t *g_cmd_opt;

int connlm_merge_parse_opt(int *argc, const char *argv[])
{
    st_log_opt_t log_opt;
    bool b;

    g_cmd_opt = st_opt_create();
    if (g_cmd_opt == NULL) {
        ST_WARNING("Failed to st_opt_create.");
        goto ST_OPT_ERR;
    }

    if (st_opt_parse(g_cmd_opt, argc, argv) < 0) {
        ST_WARNING("Failed to st_opt_parse.");
        goto ST_OPT_ERR;
    }

    if (st_log_load_opt(&log_opt, g_cmd_opt, NULL) < 0) {
        ST_WARNING("Failed to st_log_load_opt");
        goto ST_OPT_ERR;
    }

    if (st_log_open(&log_opt) != 0) {
        ST_WARNING("Failed to open log");
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_BOOL(g_cmd_opt, NULL, "BINARY", g_binary, true,
            "Save file as binary format");

    if (st_opt_get_bool(g_cmd_opt, NULL, "help", &b, false,
                "Print help") < 0) {
        ST_WARNING("Failed to st_opt_get_bool for help");
        goto ST_OPT_ERR;
    }

    return (b ? 1 : 0);

ST_OPT_ERR:
    return -1;
}

void show_usage(const char *module_name)
{
    connlm_show_usage(module_name,
            "Merge Models",
            "<model-in-filter1> <model-in-filter2> ... <model-out>",
            g_cmd_opt);
}

int main(int argc, const char *argv[])
{
    FILE *fp = NULL;
    connlm_t *connlm = NULL;
    connlm_t *connlm1 = NULL;
    model_filter_t *mfs = NULL;
    char *fnames = NULL;
    int ret;
    int i;

    int hs_size = -1;
    bool mdl_rnn = false;
    bool mdl_maxent = false;
    bool mdl_lbl = false;
    bool mdl_ffnn = false;

    ret = connlm_merge_parse_opt(&argc, argv);
    if (ret < 0) {
        goto ERR;
    } if (ret == 1) {
        show_usage(argv[0]);
        goto ERR;
    }

    if (strcmp(connlm_revision(), CONNLM_GIT_COMMIT) != 0) {
        ST_WARNING("Binary revision[%s] not match with library[%s].",
                CONNLM_GIT_COMMIT, connlm_revision());
    }

    if (argc < 4) {
        show_usage(argv[0]);
        goto ERR;
    }

    st_opt_show(g_cmd_opt, "connLM Merge Options");
    for (i = 1; i < argc - 1; i++) {
        ST_CLEAN("Model-in-%d: %s", i, argv[i]);
    }
    ST_CLEAN("Model-out: %s", argv[argc - 1]);

    mfs = (model_filter_t *)malloc(sizeof(model_filter_t)*(argc-2));
    if (mfs == NULL) {
        ST_WARNING("Failed to malloc mfs.");
        goto ERR;
    }

    fnames = (char *)malloc(sizeof(char)*MAX_DIR_LEN*(argc-2));
    if (fnames == NULL) {
        ST_WARNING("Failed to malloc fnames.");
        goto ERR;
    }

    for (i = 1; i < argc - 1; i++) {
        mfs[i - 1] = parse_model_filter(argv[i],
                fnames + (i-1) * MAX_DIR_LEN, MAX_DIR_LEN);
        if (mfs[i - 1] == MF_NONE) {
            ST_WARNING("Failed to parse_model_filter. [%s]", argv[i]);
            goto ERR;
        }
    }

    for (i = 0; i < argc - 2; i++) {
        if (mfs[i] & MF_RNN) {
            if (mdl_rnn) {
                ST_WARNING("Can't merge: Duplicate RNN model.");
                goto ERR;
            }
            mdl_rnn = true;
        }
        if (mfs[i] & MF_MAXENT) {
            if (mdl_maxent) {
                ST_WARNING("Can't merge: Duplicate MAXENT model.");
                goto ERR;
            }
            mdl_maxent = true;
        }
        if (mfs[i] & MF_LBL) {
            if (mdl_lbl) {
                ST_WARNING("Can't merge: Duplicate LBL model.");
                goto ERR;
            }
            mdl_lbl = true;
        }
        if (mfs[i] & MF_FFNN) {
            if (mdl_ffnn) {
                ST_WARNING("Can't merge: Duplicate FFNN model.");
                goto ERR;
            }
            mdl_ffnn = true;
        }
    }

    fp = st_fopen(fnames, "rb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen. [%s]", fnames);
        goto ERR;
    }

    connlm1 = connlm_load(fp);
    if (connlm1 == NULL) {
        ST_WARNING("Failed to connlm_load. [%s]", fnames);
        goto ERR;
    }
    safe_st_fclose(fp);

    if (connlm1->vocab == NULL || connlm1->output == NULL) {
        ST_WARNING("Can't merge: No vocab or output for model[%s].",
                fnames);
        goto ERR;
    }

    connlm = connlm_new(connlm1->vocab, connlm1->output, NULL, -1);
    if (connlm == NULL) {
        ST_WARNING("Failed to connlm_new.");
        goto ERR;
    }

    if (connlm->output->output_opt.hs) {
        hs_size = connlm_get_hs_size(connlm1);
        if (hs_size <= 0) {
            ST_WARNING("Failed to connlm_get_hs_size.");
            goto ERR;
        }
    }

    i = 0;
    ST_NOTICE("Merging Model %d(%s)...", i, argv[i + 1]);
    if (mfs[i] & MF_MAXENT) {
        if (connlm1->maxent == NULL) {
            ST_WARNING("No MaxEnt for model[%s]", argv[i+1]);
        } else {
            connlm->maxent = maxent_dup(connlm1->maxent);
            if (connlm->maxent == NULL) {
                ST_WARNING("Failed to dup maxent. [%s]",
                        fnames + i * MAX_DIR_LEN);
                goto ERR;
            }
        }
    }

    if (mfs[i] & MF_RNN) {
        if (connlm1->rnn == NULL) {
            ST_WARNING("No RNN for model[%s]", argv[i+1]);
        } else {
            connlm->rnn = rnn_dup(connlm1->rnn);
            if (connlm->rnn == NULL) {
                ST_WARNING("Failed to dup rnn. [%s]",
                        fnames + i * MAX_DIR_LEN);
                goto ERR;
            }
        }
    }

    if (mfs[i] & MF_LBL) {
        if (connlm1->lbl == NULL) {
            ST_WARNING("No LBL for model[%s]", argv[i+1]);
        } else {
            connlm->lbl = lbl_dup(connlm1->lbl);
            if (connlm->lbl == NULL) {
                ST_WARNING("Failed to dup lbl. [%s]",
                        fnames + i * MAX_DIR_LEN);
                goto ERR;
            }
        }
    }

    if (mfs[i] & MF_FFNN) {
        if (connlm1->ffnn == NULL) {
            ST_WARNING("No FFNN for model[%s]", argv[i+1]);
        } else {
            connlm->ffnn = ffnn_dup(connlm1->ffnn);
            if (connlm->ffnn == NULL) {
                ST_WARNING("Failed to dup ffnn. [%s]",
                        fnames + i * MAX_DIR_LEN);
                goto ERR;
            }
        }
    }
    safe_connlm_destroy(connlm1);

    for (i = 1; i < argc - 2; i++) {
        fp = st_fopen(fnames + i * MAX_DIR_LEN, "rb");
        if (fp == NULL) {
            ST_WARNING("Failed to st_fopen. [%s]", fnames + i*MAX_DIR_LEN);
            goto ERR;
        }

        connlm1 = connlm_load(fp);
        if (connlm1 == NULL) {
            ST_WARNING("Failed to connlm_load. [%s]", fnames+i*MAX_DIR_LEN);
            goto ERR;
        }
        safe_st_fclose(fp);
        
        if (connlm1->vocab == NULL || connlm1->output == NULL) {
            ST_WARNING("Can't merge: No vocab or output for model[%s].",
                    fnames + i * MAX_DIR_LEN);
            goto ERR;
        }

        ST_NOTICE("Merging Model %d(%s)...", i, argv[i + 1]);
        if (!vocab_equal(connlm->vocab, connlm1->vocab)) {
            ST_WARNING("Can't merge: Vocab not equal. [%s].",
                    fnames + i * MAX_DIR_LEN);
            goto ERR;
        }

        if (!output_equal(connlm->output, connlm1->output)) {
            ST_WARNING("Can't merge: Output not equal. [%s].",
                    fnames + i * MAX_DIR_LEN);
            goto ERR;
        }

        if (connlm->output->output_opt.hs) {
            if (hs_size != connlm_get_hs_size(connlm1)) {
                ST_WARNING("Can not merge: hs_size not match.");
                goto ERR;
            }
        }

        if (mfs[i] & MF_MAXENT) {
            if (connlm1->maxent == NULL) {
                ST_WARNING("No MaxEnt for model[%s]", argv[i+1]);
            } else {
                connlm->maxent = maxent_dup(connlm1->maxent);
                if (connlm->maxent == NULL) {
                    ST_WARNING("Failed to dup maxent. [%s]",
                            fnames + i * MAX_DIR_LEN);
                    goto ERR;
                }
            }
        }

        if (mfs[i] & MF_RNN) {
            if (connlm1->rnn == NULL) {
                ST_WARNING("No RNN for model[%s]", argv[i+1]);
            } else {
                connlm->rnn = rnn_dup(connlm1->rnn);
                if (connlm->rnn == NULL) {
                    ST_WARNING("Failed to dup rnn. [%s]",
                            fnames + i * MAX_DIR_LEN);
                    goto ERR;
                }
            }
        }

        if (mfs[i] & MF_LBL) {
            if (connlm1->lbl == NULL) {
                ST_WARNING("No LBL for model[%s]", argv[i+1]);
            } else {
                connlm->lbl = lbl_dup(connlm1->lbl);
                if (connlm->lbl == NULL) {
                    ST_WARNING("Failed to dup lbl. [%s]",
                            fnames + i * MAX_DIR_LEN);
                    goto ERR;
                }
            }
        }

        if (mfs[i] & MF_FFNN) {
            if (connlm1->ffnn == NULL) {
                ST_WARNING("No FFNN for model[%s]", argv[i+1]);
            } else {
                connlm->ffnn = ffnn_dup(connlm1->ffnn);
                if (connlm->ffnn == NULL) {
                    ST_WARNING("Failed to dup ffnn. [%s]",
                            fnames + i * MAX_DIR_LEN);
                    goto ERR;
                }
            }
        }

        safe_connlm_destroy(connlm1);
    }

    fp = st_fopen(argv[argc - 1], "wb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen. [%s]", argv[argc - 1]);
        goto ERR;
    }

    if (connlm_save(connlm, fp, g_binary) < 0) {
        ST_WARNING("Failed to connlm_save. [%s]", argv[argc - 1]);
        goto ERR;
    }

    safe_st_fclose(fp);
    safe_free(mfs);
    safe_free(fnames);
    safe_st_opt_destroy(g_cmd_opt);
    safe_connlm_destroy(connlm);

    st_log_close(0);
    return 0;

ERR:
    safe_st_fclose(fp);
    safe_free(mfs);
    safe_free(fnames);
    safe_st_opt_destroy(g_cmd_opt);
    safe_connlm_destroy(connlm);
    safe_connlm_destroy(connlm1);

    st_log_close(1);
    return -1;
}
