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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_opt.h>
#include <stutils/st_string.h>
#include <stutils/st_io.h>

#include "connlm.h"

static const int CONNLM_MAGIC_NUM = 626140498;

const char* connlm_revision()
{
    return CONNLM_GIT_COMMIT;
}

int connlm_load_train_opt(connlm_t *connlm, st_opt_t *opt, const char *sec_name)
{
    param_t param;
    bptt_opt_t bptt_opt;
    int c;

    ST_CHECK_PARAM(connlm == NULL || opt == NULL, -1);

    if (param_load(&param, opt, sec_name, NULL) < 0) {
        ST_WARNING("Failed to param_load.");
        goto ST_OPT_ERR;
    }
    if (bptt_opt_load(&bptt_opt, opt, sec_name, NULL) < 0) {
        ST_WARNING("Failed to bptt_opt_load.");
        goto ST_OPT_ERR;
    }

    for (c = 0; c < connlm->num_comp; c++) {
        if (comp_load_train_opt(connlm->comps[c], opt, sec_name,
                    &param, &bptt_opt) < 0) {
            ST_WARNING("Failed to comp_load_train_opt.");
            goto ST_OPT_ERR;
        }
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

void connlm_destroy(connlm_t *connlm)
{
    int c;

    if (connlm == NULL) {
        return;
    }

    safe_vocab_destroy(connlm->vocab);
    safe_output_destroy(connlm->output);

    for (c = 0; c < connlm->num_comp; c++) {
        safe_comp_destroy(connlm->comps[c]);
    }
    safe_free(connlm->comps);
    connlm->num_comp = 0;
}

int connlm_init(connlm_t *connlm, FILE *topo_fp)
{
    char *content = NULL;
    size_t content_sz = 0;
    size_t content_cap = 0;

    char *line = NULL;
    size_t line_sz = 0;

    int c, d;
    bool err;

    int part;
    bool has_comp;

    ST_CHECK_PARAM(connlm == NULL || topo_fp == NULL, -1);

    safe_free(connlm->comps);
    connlm->num_comp = 0;
    part = 0;
    has_comp = false;
    while (st_fgets(&line, &line_sz, topo_fp, &err)) {
        trim(line);

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        if (part == 0) {
            if (strcasecmp("<component>", line) == 0) {
                has_comp = true;
                part = 1;

                /* Set default output normalization. */
                if (connlm->output->norm == ON_UNDEFINED) {
                    connlm->output->norm = ON_SOFTMAX;
                }
            } else if (strcasecmp("<output>", line) == 0) {
                if (has_comp) {
                    ST_WARNING("<output> must be placed before "
                            "all <component>s");
                    goto ERR;
                }
                part = 2;
            }
            continue;
        }

        if (strcasecmp("</component>", line) == 0) {
            if (part != 1) {
                ST_WARNING("Miss placed </component>");
                goto ERR;
            }
            connlm->comps = (component_t **)realloc(connlm->comps,
                    sizeof(component_t *) * (connlm->num_comp + 1));
            if (connlm->comps == NULL) {
                ST_WARNING("Failed to alloc comps.");
                goto ERR;
            }
            connlm->comps[connlm->num_comp] = comp_init_from_topo(content,
                    connlm->output, connlm->vocab->vocab_size);
            if (connlm->comps[connlm->num_comp] == NULL) {
                ST_WARNING("Failed to comp_init_from_topo.");
                goto ERR;
            }
            connlm->num_comp++;

            content_sz = 0;
            part = 0;
            continue;
        } else if (strcasecmp("</output>", line) == 0) {
            if (part != 2) {
                ST_WARNING("Miss placed </output>.");
                goto ERR;
            }
            if (output_parse_topo(connlm->output, content) < 0) {
                ST_WARNING("Failed to output_parse_topo.");
                goto ERR;
            }

            content_sz = 0;
            part = 0;
            continue;
        }

        if (strlen(line) + 2 + content_sz > content_cap) {
            content_cap += (strlen(line) + 2);
            content = (char *)realloc(content, content_cap);
            if (content == NULL) {
                ST_WARNING("Failed to realloc content.");
                goto ERR;
            }
        }

        strcpy(content + content_sz, line);
        content_sz += strlen(line);
        content[content_sz] = '\n';
        content_sz++;
        content[content_sz] = '\0';
    }

    if (err) {
        ST_WARNING("st_fgets error.");
        goto ERR;
    }

    if (connlm->comps == NULL) {
        ST_WARNING("No componet found.");
        goto ERR;
    }

    safe_free(line);
    safe_free(content);

    for (c = 0; c < connlm->num_comp - 1; c++) {
        for (d = c+1; d < connlm->num_comp; d++) {
            if (strcmp(connlm->comps[c]->name,
                        connlm->comps[d]->name) == 0) {
                ST_WARNING("Duplicated component name[%s]",
                        connlm->comps[c]->name);
                goto ERR;
            }
        }
    }

    return 0;

ERR:
    safe_free(line);
    safe_free(content);

    if (connlm->comps != NULL) {
        for (c = 0; c < connlm->num_comp; c++) {
            safe_comp_destroy(connlm->comps[c]);
        }
        safe_free(connlm->comps);
        connlm->num_comp = 0;
    }

    return -1;
}

connlm_t *connlm_new(vocab_t *vocab, output_t *output,
        component_t **comps, int n_comp)
{
    connlm_t *connlm = NULL;

    int c;

    ST_CHECK_PARAM(vocab == NULL && output == NULL
            && comps == NULL, NULL);

    connlm = (connlm_t *)malloc(sizeof(connlm_t));
    if (connlm == NULL) {
        ST_WARNING("Failed to malloc connlm_t");
        goto ERR;
    }
    memset(connlm, 0, sizeof(connlm_t));

    if (vocab != NULL) {
        connlm->vocab = vocab_dup(vocab);
        if (connlm->vocab == NULL) {
            ST_WARNING("Failed to vocab_dup.");
            goto ERR;
        }
    }

    if (output != NULL) {
        connlm->output = output_dup(output);
        if (connlm->output == NULL) {
            ST_WARNING("Failed to output_dup.");
            goto ERR;
        }
    }

    if (comps != NULL && n_comp > 0) {
        connlm->comps = (component_t **)malloc(sizeof(component_t *)
                * n_comp);
        if (connlm->comps == NULL) {
            ST_WARNING("Failed to malloc comps.");
            goto ERR;
        }

        for (c = 0; c < n_comp; c++) {
            connlm->comps[c] = comp_dup(comps[c]);
            if (connlm->comps[c] == NULL) {
                ST_WARNING("Failed to comp_dup. c[%d]", c);
                goto ERR;
            }
        }
        connlm->num_comp = n_comp;
    }

    return connlm;

ERR:
    safe_connlm_destroy(connlm);
    return NULL;
}

static int connlm_load_header(connlm_t *connlm, FILE *fp,
        bool *binary, FILE *fo_info)
{
    char line[MAX_LINE_LEN];
    size_t sz;
    bool b;

    union {
        char str[4];
        int magic_num;
    } flag;

    int version;
    int real_size;
    int c;
    int num_comp;

    ST_CHECK_PARAM((connlm == NULL && fo_info == NULL) || fp == NULL
            || binary == NULL, -1);

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    if (strncmp(flag.str, "    ", 4) == 0) {
        *binary = false;
    } else if (CONNLM_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (*binary) {
        if (fread(&version, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read version.");
            return -1;
        }

        if (version > CONNLM_FILE_VERSION) {
            ST_WARNING("Too high file versoin[%d], "
                    "please upgrade connlm toolkit", version);
            return -1;
        }

        if (version < 3) {
            ST_WARNING("File versoin[%d] less than 3 is not supported, "
                    "please downgrade connlm toolkit", version);
            return -1;
        }

        if (fread(&real_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read real size.");
            return -1;
        }

        if (real_size != sizeof(real_t)) {
            ST_WARNING("Real type not match. Please recompile toolkit");
            return -1;
        }

        if (fread(&num_comp, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read num_comp.");
            return -1;
        }
    } else {
        if (st_readline(fp, "    ") != 0) {
            ST_WARNING("Failed to read tag.");
            return -1;
        }

        if (st_readline(fp, "<CONNLM>") != 0) {
            ST_WARNING("tag error");
            return -1;
        }

        if (st_readline(fp, "Version: %d", &version) != 1) {
            ST_WARNING("Failed to read version.");
            return -1;
        }

        if (version > CONNLM_FILE_VERSION) {
            ST_WARNING("Too high file versoin[%d], "
                    "please update connlm toolkit");
            return -1;
        }

        if (st_readline(fp, "Real type: %"xSTR(MAX_LINE_LEN)"s",
                    line) != 1) {
            ST_WARNING("Failed to read real type.");
            return -1;
        }
        line[MAX_LINE_LEN - 1] = '\0';

        if (strcasecmp(line, "double") == 0) {
            real_size = sizeof(double);
        } else {
            real_size = sizeof(float);
        }

        if (real_size != sizeof(real_t)) {
            ST_WARNING("Real type not match. Please recompile toolkit");
            return -1;
        }

        if (st_readline(fp, "Num comp: %d", &num_comp) != 1) {
            ST_WARNING("Failed to read num_comp.");
            return -1;
        }
    }

    if (connlm != NULL) {
        connlm->num_comp = num_comp;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "<CONNLM>\n");
        fprintf(fo_info, "Version: %d\n", version);
        fprintf(fo_info, "Real type: %s\n",
                (real_size == sizeof(double)) ? "double" : "float");
        fprintf(fo_info, "Binary: %s\n", bool2str(*binary));
        fprintf(fo_info, "Num comp: %d\n", num_comp);
    }

    b = *binary;

    if (vocab_load_header((connlm == NULL) ? NULL : &(connlm->vocab),
                version, fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to vocab_load_header.");
        return -1;
    }
    if (*binary != b) {
        ST_WARNING("Both binary and text format in one file.");
        return -1;
    }

    if (output_load_header((connlm == NULL) ? NULL : &connlm->output,
                version, fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to output_load_header.");
        return -1;
    }
    if (*binary != b) {
        ST_WARNING("Both binary and text format in one file.");
        return -1;
    }

    if (connlm != NULL ) {
        sz = sizeof(component_t *) * (connlm->num_comp);
        connlm->comps = (component_t **)malloc(sz);
        if (connlm->comps == NULL) {
            ST_WARNING("Failed to alloc comps.");
            goto ERR;
        }
        memset(connlm->comps, 0, sz);
    }
    for (c = 0; c < num_comp; c++) {
        if (comp_load_header((connlm == NULL) ? NULL : connlm->comps + c,
                    version, fp, binary, fo_info) < 0) {
            ST_WARNING("Failed to comp_load_header.");
            goto ERR;
        }
        if (*binary != b) {
            ST_WARNING("Both binary and text format in one file.");
            goto ERR;
        }
    }

    if (fo_info != NULL) {
        fflush(fo_info);
    }

    return version;

ERR:
    if (connlm != NULL) {
        safe_free(connlm->comps);
        connlm->num_comp = 0;
    }
    return -1;
}

static int connlm_load_body(connlm_t *connlm, int version,
        FILE *fp, bool binary)
{
    int c;

    ST_CHECK_PARAM(connlm == NULL || fp == NULL, -1);

    if (vocab_load_body(connlm->vocab, version, fp, binary) < 0) {
        ST_WARNING("Failed to vocab_load_body.");
        return -1;
    }

    if (connlm->output != NULL) {
        if (output_load_body(connlm->output, version, fp, binary) < 0) {
            ST_WARNING("Failed to output_load_body.");
            return -1;
        }
    }

    for (c = 0; c < connlm->num_comp; c++) {
        if (comp_load_body(connlm->comps[c], version, fp, binary) < 0) {
            ST_WARNING("Failed to comp_load_body[%d].", c);
            return -1;
        }
    }

    return 0;
}

connlm_t* connlm_load(FILE *fp)
{
    connlm_t *connlm = NULL;
    int version;
    bool binary;

    ST_CHECK_PARAM(fp == NULL, NULL);

    connlm = (connlm_t *)malloc(sizeof(connlm_t));
    if (connlm == NULL) {
        ST_WARNING("Failed to malloc connlm_t");
        goto ERR;
    }
    memset(connlm, 0, sizeof(connlm_t));

    version = connlm_load_header(connlm, fp, &binary, NULL);
    if (version < 0) {
        ST_WARNING("Failed to connlm_load_header.");
        goto ERR;
    }

    if (connlm_load_body(connlm, version, fp, binary) < 0) {
        ST_WARNING("Failed to connlm_load_body.");
        goto ERR;
    }

    return connlm;

ERR:
    safe_connlm_destroy(connlm);
    return NULL;
}

int connlm_print_info(FILE *fp, FILE *fo_info)
{
    bool binary;

    ST_CHECK_PARAM(fp == NULL || fo_info == NULL, -1);

    if (connlm_load_header(NULL, fp, &binary, fo_info) < 0) {
        ST_WARNING("Failed to connlm_load_header.");
        return -1;
    }

    return 0;
}

static int connlm_save_header(connlm_t *connlm, FILE *fp, bool binary)
{
    int n;
    int c;

    ST_CHECK_PARAM(connlm == NULL || fp == NULL, -1);

    if (binary) {
        if (fwrite(&CONNLM_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        n = CONNLM_FILE_VERSION;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write version.");
            return -1;
        }

        n = sizeof(real_t);
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write real size.");
            return -1;
        }

        if (fwrite(&connlm->num_comp, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write num_comp.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<CONNLM>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }
        if (fprintf(fp, "Version: %d\n", CONNLM_FILE_VERSION) < 0) {
            ST_WARNING("Failed to fprintf version.");
            return -1;
        }
        if (fprintf(fp, "Real type: %s\n",
                (sizeof(double) == sizeof(real_t))
                    ? "double" : "float") < 0) {
            ST_WARNING("Failed to fprintf real type.");
            return -1;
        }
        if (fprintf(fp, "Num comp: %d\n",
                    connlm->num_comp) < 0) {
            ST_WARNING("Failed to fprintf num comp.");
            return -1;
        }
    }

    if (vocab_save_header(connlm->vocab, fp, binary) < 0) {
        ST_WARNING("Failed to vocab_save_header.");
        return -1;
    }

    if (output_save_header(connlm->output, fp, binary) < 0) {
        ST_WARNING("Failed to output_save_header.");
        return -1;
    }

    for (c = 0; c < connlm->num_comp; c++) {
        if (comp_save_header(connlm->comps[c], fp, binary) < 0) {
            ST_WARNING("Failed to comp_save_header.");
            return -1;
        }
    }

    return 0;
}

static int connlm_save_body(connlm_t *connlm, FILE *fp, bool binary)
{
    int c;

    ST_CHECK_PARAM(connlm == NULL || fp == NULL, -1);

    if (vocab_save_body(connlm->vocab, fp, binary) < 0) {
        ST_WARNING("Failed to vocab_save_body.");
        return -1;
    }

    if (output_save_body(connlm->output, fp, binary) < 0) {
        ST_WARNING("Failed to output_save_body.");
        return -1;
    }

    for (c = 0; c < connlm->num_comp; c++) {
        if (comp_save_body(connlm->comps[c], fp, binary) < 0) {
            ST_WARNING("Failed to comp_save_body.");
            return -1;
        }
    }

    return 0;
}

int connlm_save(connlm_t *connlm, FILE *fp, bool binary)
{
    ST_CHECK_PARAM(connlm == NULL || fp == NULL, -1);

    if (connlm_save_header(connlm, fp, binary) < 0) {
        ST_WARNING("Failed to connlm_save_header.");
        return -1;
    }

    if (connlm_save_body(connlm, fp, binary) < 0) {
        ST_WARNING("Failed to connlm_save_body.");
        return -1;
    }

    return 0;
}

static int connlm_draw_network(connlm_t *connlm, FILE *fp, bool verbose)
{
    char nodename[MAX_NAME_LEN];
    int c, i, n, g, glue_id;

    ST_CHECK_PARAM(connlm == NULL || fp == NULL, -1);

    fprintf(fp, "digraph network {\n");
    fprintf(fp, "  rankdir=BT;\n");
    fprintf(fp, "  labelloc=t;\n");
    fprintf(fp, "  label=\"Network\";\n\n");

    if (verbose) {
        fprintf(fp, "  subgraph cluster_overview {\n");
        fprintf(fp, "    label=\"Overview\";\n");
        fprintf(fp, "    node [shape=plaintext, style=solid];\n");
        fprintf(fp, "    edge [style=invis];\n\n");

        fprintf(fp, "    legend [label=\"# Compnoent: %d\\l"
                         "Vocab size: %d\\l",
                    connlm->num_comp,
                    connlm->vocab->vocab_size);
        i = 0;
        for (c = 0; c < connlm->num_comp; c++) {
            for (n = 0; n < connlm->comps[c]->num_glue_cycle; n++) {
                fprintf(fp, "Cycle #%d: ", i);
                for (g = 1; g <= connlm->comps[c]->glue_cycles[n][0] - 1; g++) {
                    glue_id = connlm->comps[c]->glue_cycles[n][g];
                    fprintf(fp, "%s -> ",
                            connlm->comps[c]->glues[glue_id]->name);
                }
                glue_id = connlm->comps[c]->glue_cycles[n][g];
                fprintf(fp, "%s\\l", connlm->comps[c]->glues[glue_id]->name);
                i++;
            }
        }
        fprintf(fp, "\"];\n");

        fprintf(fp, "  }\n\n");

        fprintf(fp, "  subgraph cluster_structure {\n");
    } else {
        fprintf(fp, "  subgraph structure {\n");
    }

    fprintf(fp, "    label=\"\";\n");
    fprintf(fp, "\n");
    fprintf(fp, "    %s [shape=triangle, orientation=180];\n",
            OUTPUT_LAYER_NAME);
    for (c = 0; c < connlm->num_comp; c++) {
        if (comp_draw(connlm->comps[c], fp, verbose) < 0) {
            ST_WARNING("Failed to comp_draw.");
            return -1;
        }
    }
    fprintf(fp, "\n");

    fprintf(fp, "    { rank=same; ");
    for (c = 0; c < connlm->num_comp; c++) {
        fprintf(fp, "%s; ", comp_input_nodename(connlm->comps[c],
                    nodename, MAX_NAME_LEN));
    }
    fprintf(fp, "}\n");
    fprintf(fp, "  }\n");

    fprintf(fp, "}\n");

    return 0;
}

int connlm_draw(connlm_t *connlm, FILE *fp, bool verbose)
{
    ST_CHECK_PARAM(connlm == NULL || fp == NULL, -1);

    if (connlm_draw_network(connlm, fp, verbose) < 0) {
        ST_WARNING("Failed to connlm_draw_network.");
        return -1;
    }

    if (connlm->output != NULL) {
        if (output_draw(connlm->output, fp,
            connlm->vocab != NULL ? connlm->vocab->cnts : NULL,
            connlm->vocab != NULL ? connlm->vocab->alphabet : NULL) < 0) {
            ST_WARNING("Failed to output_draw.");
            return -1;
        }
    }

    return 0;
}

int connlm_filter(connlm_t *connlm, model_filter_t mf,
        const char *comp_names, int num_comp)
{
    int c;
    int i;
    bool found;

    ST_CHECK_PARAM(connlm == NULL, -1);

    if (!(mf & MF_OUTPUT)) {
        safe_output_destroy(connlm->output);
    }
    if (!(mf & MF_VOCAB)) {
        safe_vocab_destroy(connlm->vocab);
    }

    if (num_comp < 0) {
        if (mf & MF_COMP_NEG) {
            for (c = 0; c < connlm->num_comp; c++) {
                safe_comp_destroy(connlm->comps[c]);
            }
            safe_free(connlm->comps);
            connlm->num_comp = 0;
        }

        return 0;
    }

    for (i = 0; i < num_comp; i++) { // check comp_names
        found = false;
        for (c = 0; c < connlm->num_comp; c++) {
            if (strcasecmp(connlm->comps[c]->name,
                        comp_names + MAX_NAME_LEN * i) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            ST_WARNING("No component named [%s]",
                    comp_names + MAX_NAME_LEN * i);
            return -1;
        }
    }

    if (mf & MF_COMP_NEG) {
        for (i = 0; i < num_comp; i++) {
            for (c = 0; c < connlm->num_comp; c++) {
                if (strcasecmp(connlm->comps[c]->name,
                            comp_names + MAX_NAME_LEN * i) == 0) {
                    safe_comp_destroy(connlm->comps[c]);
                    if (connlm->num_comp - c - 1 > 0) {
                        memmove(connlm->comps + c, connlm->comps + c + 1,
                            sizeof(component_t *) * (connlm->num_comp - c - 1));
                    }
                    connlm->num_comp--;
                    break;
                }
            }
        }
    } else {
        c = 0;
        while (c < connlm->num_comp) {
            found = false;
            for (i = 0; i < num_comp; i++) {
                if (strcasecmp(connlm->comps[c]->name,
                            comp_names + MAX_NAME_LEN * i) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                safe_comp_destroy(connlm->comps[c]);
                if (connlm->num_comp - c - 1 > 0) {
                    memmove(connlm->comps + c, connlm->comps + c + 1,
                            sizeof(component_t *) * (connlm->num_comp - c - 1));
                }
                connlm->num_comp--;
            } else {
                c++;
            }
        }
    }

    return 0;
}

int connlm_setup(connlm_t *connlm)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (connlm->output != NULL) {
        if (output_setup(connlm->output) < 0) {
            ST_WARNING("Failed to output_setup.");
            return -1;
        }
    }

    return 0;
}

int connlm_add_comp(connlm_t *connlm, component_t *comp)
{
    char name[MAX_NAME_LEN];
    int id, c;
    bool collision;

    ST_CHECK_PARAM(connlm == NULL || comp == NULL, -1);

    strncpy(name, comp->name, MAX_NAME_LEN);
    name[MAX_NAME_LEN - 1] = '\0';

    collision = false;
    for (c = 0; c < connlm->num_comp; c++) {
        if (strcasecmp(connlm->comps[c]->name, name) == 0) {
            collision = true;
        }
    }
    if (collision) {
        id = 0;
        while(collision && id <= connlm->num_comp) {
            snprintf(name, MAX_NAME_LEN, "%s_%d", comp->name, id);
            collision = false;
            for (c = 0; c < connlm->num_comp; c++) {
                if (strcasecmp(connlm->comps[c]->name, name) == 0) {
                    collision = true;
                }
            }
            id++;
        }
        if (id > connlm->num_comp) {
            ST_WARNING("Can not find a name without collision for comp[%s]",
                    comp->name);
            return -1;
        }

        ST_NOTICE("Rename comp[%s] to [%s].", comp->name, name);
    }

    connlm->comps = (component_t **)realloc(connlm->comps,
            sizeof(component_t *) * (connlm->num_comp + 1));
    if (connlm->comps == NULL) {
        ST_WARNING("Failed to alloc comps.");
        return -1;
    }
    connlm->comps[connlm->num_comp] = comp_dup(comp);
    if (connlm->comps[connlm->num_comp] == NULL) {
        ST_WARNING("Failed to comp_dup.");
        return -1;
    }
    strncpy(connlm->comps[connlm->num_comp]->name, name, MAX_NAME_LEN);
    connlm->comps[connlm->num_comp]->name[MAX_NAME_LEN - 1] = '\0';
    connlm->num_comp++;

    return 0;
}

int connlm_generate_wildcard_wt(connlm_t *connlm)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    return 0;
}
