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
#include <stdarg.h>
#include <assert.h>

#include <st_utils.h>

#include "connlm.h"

static FILE* mk_topo_file(const char *fmt, ...)
{
    va_list args;
    FILE *fp;

    fp = tmpfile();
    assert(fp != NULL);
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
    rewind(fp);

#if 0
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
#endif

    return fp;
}

static int check_comp(component_t *comp, char *name, int id)
{
    if (strncmp(comp->name, name, strlen(name)) != 0
            || comp->name[strlen(name)] != '0' + id
            || comp->name[strlen(name) + 1] != '\0') {
        fprintf(stderr, "component name not match.\n");
        return -1;
    }

    return 0;
}

static int check_layer(layer_t *layer, char *name, int id,
        char *type, int size)
{ 
    if (strncmp(layer->name, name, strlen(name)) != 0
            || layer->name[strlen(name)] != '0' + id
            || layer->name[strlen(name) + 1] != '\0') {
        fprintf(stderr, "layer name not match.\n");
        return -1;
    }

    if (strcmp(layer->type, type) != 0) {
        fprintf(stderr, "layer type not match.\n");
        return -1;
    }

    if (layer->size != size) {
        fprintf(stderr, "layer size not match.\n");
        return -1;
    }

    return 0;
}

#define MAX_REF_NUM 16
typedef struct _ref_t_ {
    char comp_name[MAX_NAME_LEN];
    char layer_name[MAX_NAME_LEN];
    char layer_type[MAX_REF_NUM][MAX_NAME_LEN];
    int layer_size[MAX_REF_NUM];
    int num_layer[MAX_REF_NUM];
    int num_comp;
} ref_t;

static int check_connlm(connlm_t *connlm, ref_t *ref)
{
    int c, l;

    if (connlm->num_comp != ref->num_comp) {
        fprintf(stderr, "num_comp not match\n");
        return -1;
    }
    for (c = 0; c < connlm->num_comp; c++) {
        if (check_comp(connlm->comps[c], ref->comp_name, c) != 0) {
            fprintf(stderr, "component not match\n");
            return -1;
        }

        if (connlm->comps[c]->num_layer != ref->num_layer[c]) {
            fprintf(stderr, "num_layer not match\n");
            return -1;
        }

        for (l = 0; l < connlm->comps[c]->num_layer; l++) {
            if (check_layer(connlm->comps[c]->layers[l], ref->layer_name, l,
                        ref->layer_type[l], ref->layer_size[l]) != 0) {
                fprintf(stderr, "layer not match\n");
                return -1;
            }
        }
    }

    return 0;
}

static int unit_test_connlm_read_topo()
{
    FILE *fp = NULL;
    int ncase = 0;
    connlm_t connlm;
    ref_t ref = {
        .comp_name = "comp",
        .layer_name = "layer",
        .layer_type = {"sigmoid", "tanh"},
        .layer_size = {16, 32},
    };

    fprintf(stderr, "  Testing Reading topology file...\n");
    fprintf(stderr, "    Case %d...", ncase++);
    fp = mk_topo_file("<component>\n"
            "</component>\n");
    memset(&connlm, 0, sizeof(connlm_t));
    if (connlm_init(&connlm, fp) >= 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    connlm_destroy(&connlm);
    safe_fclose(fp);
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    ref.num_comp = 1;
    ref.num_layer[0] = 0;
    fp = mk_topo_file("<component>\n"
            "# comments\n"
            "property name=%s%d\n"
            "</component>\n", ref.comp_name, 0);

    memset(&connlm, 0, sizeof(connlm_t));
    if (connlm_init(&connlm, fp) < 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_connlm(&connlm, &ref) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    connlm_destroy(&connlm);
    safe_fclose(fp);
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    ref.num_comp = 2;
    ref.num_layer[0] = 0;
    ref.num_layer[1] = 0;
    fp = mk_topo_file("<component>\n"
            "property name=%s%d\n"
            "</component>\n"
            "foo bar\n"
            "<component>\n"
            "property name=%s%d\n"
            "</component>\n",
            ref.comp_name, 0, ref.comp_name, 1);

    memset(&connlm, 0, sizeof(connlm_t));
    if (connlm_init(&connlm, fp) < 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_connlm(&connlm, &ref) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    connlm_destroy(&connlm);
    safe_fclose(fp);
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    ref.num_comp = 2;
    ref.num_layer[0] = 1;
    ref.num_layer[1] = 2;
    fp = mk_topo_file("<component>\n"
            "property name=%s%d\n"
            "layer name=%s%d type=%s size=%d\n"
            "</component>\n"
            "foo bar\n"
            "<component>\n"
            "property name=%s%d\n"
            "layer name=%s%d type=%s size=%d\n"
            "layer name=%s%d type=%s size=%d\n"
            "</component>\n",
            ref.comp_name, 0, ref.layer_name, 0,
            ref.layer_type[0], ref.layer_size[0],
            ref.comp_name, 1, ref.layer_name, 0,
            ref.layer_type[0], ref.layer_size[0],
            ref.layer_name, 1, ref.layer_type[1], ref.layer_size[1]);

    memset(&connlm, 0, sizeof(connlm_t));
    if (connlm_init(&connlm, fp) < 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_connlm(&connlm, &ref) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    connlm_destroy(&connlm);
    safe_fclose(fp);
    fprintf(stderr, "Success\n");

    return 0;

ERR:
    connlm_destroy(&connlm);
    safe_fclose(fp);
    return -1;
}

static int run_all_tests()
{
    int ret = 0;

    if (unit_test_connlm_read_topo() != 0) {
        ret = -1;
    }

    return ret;
}

int main(int argc, const char *argv[])
{
    int ret;

    fprintf(stderr, "Start testing...\n");
    ret = run_all_tests();
    if (ret != 0) {
        fprintf(stderr, "Tests failed.\n");
    } else {
        fprintf(stderr, "Tests succeeded.\n");
    }

    return ret;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
