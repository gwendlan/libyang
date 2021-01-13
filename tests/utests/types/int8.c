/**
 * @file int8.c
 * @author Radek Iša <isa@cesnet.cz>
 * @brief test for int8 values
 *
 * Copyright (c) 2020 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#define  _UTEST_MAIN_
#include "../utests.h"

#include "libyang.h"
#include "path.h"


#include <ctype.h>
static int string_cmp(const char * str1, const char * str2){
    int unsigned it1 = 0;
    int unsigned it2 = 0;

    if (str1 == NULL || str2 == NULL){
        if (str1 == NULL && str2 == NULL) {
            return 1;
        }
        else{
            return 0;
        }
    }

    while(isspace(str1[it1])) {
        it1++;
    }
    while(isspace(str2[it2])) {
        it2++;
    }

    while(str1[it1] != 0 && str2[it2] != 0 && str1[it1] == str2[it2]) {
         it1++;
         it2++;

         while(isspace(str1[it1])) {
             it1++;
         }
         while(isspace(str2[it2])) {
             it2++;
         }
   };

   return str1[it1] == str2[it2];
}


#define LYD_TREE_CREATE(INPUT, MODEL) \
    CHECK_PARSE_LYD_PARAM(INPUT, LYD_XML, 0, LYD_VALIDATE_PRESENT, LY_SUCCESS, MODEL)


#define MODULE_CREATE_YIN(MOD_NAME, NODES) \
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" \
    "<module name=\"" MOD_NAME "\"\n" \
    "        xmlns=\"urn:ietf:params:xml:ns:yang:yin:1\"\n" \
    "        xmlns:pref=\"urn:tests:" MOD_NAME "\">\n" \
    "    <yang-version value = \"1.1\" />\n" \
    "    <namespace uri=\"urn:tests:" MOD_NAME "\"/>\n" \
    "    <prefix value=\"pref\"/> \n" \
    NODES \
    "\n</module>\n"



#define MODULE_CREATE_YANG(MOD_NAME, NODES)\
    "module " MOD_NAME " {"\
        "yang-version 1.1;"\
        "namespace \"urn:tests:" MOD_NAME "\";"\
        "prefix pref;"\
        "description    \"desc\";"\
        NODES \
    "}"

#define TEST_SUCCESS_XML(MOD_NAME, DATA, TYPE, ...)\
    {\
        struct lyd_node *tree;\
        const char *data = "<port xmlns=\"urn:tests:" MOD_NAME "\">" DATA "</port>";\
        CHECK_PARSE_LYD_PARAM(data, LYD_XML, 0, LYD_VALIDATE_PRESENT, LY_SUCCESS, tree);\
        CHECK_LYSC_NODE(tree->schema, NULL, 0, 0x5, 1, "port", 0, LYS_LEAF, 0, 0, 0, 0);\
        CHECK_LYD_NODE_TERM((struct lyd_node_term *) tree, 0, 0, 0, 0, 1, TYPE, ## __VA_ARGS__);\
        lyd_free_all(tree);\
    }

#define TEST_SUCCESS_JSON(MOD_NAME, DATA, TYPE, ...)\
    {\
        struct lyd_node *tree;\
        const char *data = "{\"" MOD_NAME ":port\":" DATA "}";\
        CHECK_PARSE_LYD_PARAM(data, LYD_JSON, 0, LYD_VALIDATE_PRESENT, LY_SUCCESS, tree);\
        CHECK_LYSC_NODE(tree->schema, NULL, 0, 0x5, 1, "port", 0, LYS_LEAF, 0, 0, 0, 0);\
        CHECK_LYD_NODE_TERM((struct lyd_node_term *) tree, 0, 0, 0, 0, 1, TYPE, ## __VA_ARGS__);\
        lyd_free_all(tree);\
    }


#define TEST_ERROR_XML(MOD_NAME, DATA)\
    {\
        struct lyd_node *tree;\
        const char *data = "<port xmlns=\"urn:tests:" MOD_NAME "\">" DATA "</port>";\
        CHECK_PARSE_LYD_PARAM(data, LYD_XML, 0, LYD_VALIDATE_PRESENT, LY_EVALID, tree);\
        assert_null(tree);\
    }


#define TEST_ERROR_JSON(MOD_NAME, DATA)\
    {\
        struct lyd_node *tree;\
        const char *data = "{\"" MOD_NAME ":port\":" DATA "}";\
        CHECK_PARSE_LYD_PARAM(data, LYD_XML, 0, LYD_VALIDATE_PRESENT, LY_EVALID, tree);\
        assert_null(tree);\
    }



///////////////////////////
//TEST PARSED AND COMPILED
static void
test_schema_yang(void **state)
{
    (void) state;
    const char *schema;
    const struct lys_module *mod;
    struct lysc_node_leaf * lysc_leaf;
    struct lysp_node_leaf * lysp_leaf;
    struct lysc_range * range;

    schema = MODULE_CREATE_YANG("defs", "leaf port {type int8 {range \"0 .. 50 | 127\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 2, NULL);
    assert_int_equal(range->parts[0].min_64, 0);
    assert_int_equal(range->parts[0].max_64, 50);
    assert_int_equal(range->parts[1].min_64, 127);
    assert_int_equal(range->parts[1].max_64, 127);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "int8", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "0 .. 50 | 127", NULL, NULL, NULL, 0, NULL);


    //TEST MODULE T0
    schema = MODULE_CREATE_YANG("T0", "leaf port {type int8;}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 0);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x0, 0, 0, "int8", 0, 0, 1, 0, 0, 0);

    //TEST MODULE T1
    schema = MODULE_CREATE_YANG("T1", "leaf port {type int8 {range \"0 .. 50 |51 .. 60\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 2, NULL);
    assert_int_equal(range->parts[0].min_64, 0);
    assert_int_equal(range->parts[0].max_64, 50);
    assert_int_equal(range->parts[1].min_64, 51);
    assert_int_equal(range->parts[1].max_64, 60);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "int8", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "0 .. 50 |51 .. 60", NULL, NULL, NULL, 0, NULL);

    //TEST MODULE T1
    schema = MODULE_CREATE_YANG("T2", "leaf port {type int8 {range \"20\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 1, NULL);
    assert_int_equal(range->parts[0].min_64, 20);
    assert_int_equal(range->parts[0].max_64, 20);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "int8", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "20", NULL, NULL, NULL, 0, NULL);

    //TEST MODULE T3
    schema = MODULE_CREATE_YANG("T3", "leaf port {type int8 {range \"-128 .. -60 | -1 .. 1 |  60 .. 127\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 3, NULL);
    assert_int_equal(range->parts[0].min_64, -128);
    assert_int_equal(range->parts[0].max_64, -60);
    assert_int_equal(range->parts[1].min_64, -1);
    assert_int_equal(range->parts[1].max_64,  1);
    assert_int_equal(range->parts[2].min_64, 60);
    assert_int_equal(range->parts[2].max_64, 127);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "int8", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "-128 .. -60 | -1 .. 1 |  60 .. 127", NULL, NULL, NULL, 0, NULL);

    //TEST MODULE T4
    schema = MODULE_CREATE_YANG("T4", "leaf port {type int8 {range \"1 .. 1\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 1, NULL);
    assert_int_equal(range->parts[0].min_64, 1);
    assert_int_equal(range->parts[0].max_64, 1);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "int8", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "1 .. 1", NULL, NULL, NULL, 0, NULL);

    //TEST MODULE T4
    schema = MODULE_CREATE_YANG("T5", "leaf port {type int8 {range \"7\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 1, NULL);
    assert_int_equal(range->parts[0].min_64, 7);
    assert_int_equal(range->parts[0].max_64, 7);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "int8", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "7", NULL, NULL, NULL, 0, NULL);

    //TEST MODULE T4
    schema = MODULE_CREATE_YANG("T6", "leaf port {type int8 {range \"min .. max\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 1, NULL);
    assert_int_equal(range->parts[0].min_64, -128);
    assert_int_equal(range->parts[0].max_64, 127);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "int8", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "min .. max", NULL, NULL, NULL, 0, NULL);


    //TEST ERROR -60 .. 0 | 0 .. 127
    schema = MODULE_CREATE_YANG("ERR0", "leaf port {type int8 {range \"-60 .. 0 | 0 .. 127\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid range restriction - values are not in ascending order (0).",
          "/ERR0:port");

    //TEST ERROR 0 .. 128
    schema = MODULE_CREATE_YANG("ERR1", "leaf port {type int8 {range \"0 .. 128\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid range restriction - value \"128\" does not fit the type limitations.",
          "/ERR1:port");

    //TEST ERROR -129 .. 126
    schema = MODULE_CREATE_YANG("ERR2", "leaf port {type int8 {range \"-129 .. 0\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid range restriction - value \"-129\" does not fit the type limitations.",
          "/ERR2:port");

    //TEST ERROR 0
    schema = MODULE_CREATE_YANG("ERR3", "leaf port {type int8 {range \"-129\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid range restriction - value \"-129\" does not fit the type limitations.",
          "/ERR3:port");


    //TEST MODULE SUBTYPE
    schema = MODULE_CREATE_YANG("TS0",
            "typedef my_int_type {"
            "    type int8 {range \"-128 .. -60 | -1 .. 1 |  60 .. 127\";}"
            "}"
            "leaf my_leaf {type my_int_type; }");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "my_leaf", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 3, NULL);
    assert_int_equal(range->parts[0].min_64, -128);
    assert_int_equal(range->parts[0].max_64, -60);
    assert_int_equal(range->parts[1].min_64, -1);
    assert_int_equal(range->parts[1].max_64,  1);
    assert_int_equal(range->parts[2].min_64, 60);
    assert_int_equal(range->parts[2].max_64, 127);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "my_leaf", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x0, 0, 0, "my_int_type", 0, 0, 1, 0, 0, 0);


    schema = MODULE_CREATE_YANG("TS1",
                "typedef my_int_type {"
                "    type int8 {range \"-100 .. -60 | -1 .. 1 |  60 .. 127\";}"
                "}"
                "leaf my_leaf {type my_int_type {range \"min .. -60\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "my_leaf", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 1, NULL);
    assert_int_equal(range->parts[0].min_64, -100);
    assert_int_equal(range->parts[0].max_64, -60);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "my_leaf", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "my_int_type", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "min .. -60", NULL, NULL, NULL, 0, NULL);


    schema = MODULE_CREATE_YANG("TS2",
                "typedef my_int_type {"
                "    type int8 {range \"-100 .. -60 | -1 .. 1 |  60 .. 120\";}"
                "}"
                "leaf my_leaf {type my_int_type {range \"70 .. max\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "my_leaf", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 1, NULL);
    assert_int_equal(range->parts[0].min_64, 70);
    assert_int_equal(range->parts[0].max_64, 120);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "my_leaf", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "my_int_type", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "70 .. max", NULL, NULL, NULL, 0, NULL);


    schema = MODULE_CREATE_YANG("TS3",
                "typedef my_int_type {"
                "   type int8 {range \"-100 .. -60 | -1 .. 1 |  60 .. 127\";}"
                "}"
                "leaf my_leaf {type my_int_type {range \"-1 .. 1\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "my_leaf", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 1, NULL);
    assert_int_equal(range->parts[0].min_64, -1);
    assert_int_equal(range->parts[0].max_64, 1);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "my_leaf", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "my_int_type", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "-1 .. 1", NULL, NULL, NULL, 0, NULL);


    schema = MODULE_CREATE_YANG("TS4",
                "typedef my_int_type {"
                "   type int8 {range \"-128 .. -60 | -1 .. 1 |  60 .. 127\";}"
                "}"
                "leaf my_leaf {type my_int_type { "
                "   range \"min .. -60 | -1 .. 1 |  60 .. max\";}"
                "}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "my_leaf", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 3, NULL);
    assert_int_equal(range->parts[0].min_64, -128);
    assert_int_equal(range->parts[0].max_64, -60);
    assert_int_equal(range->parts[1].min_64, -1);
    assert_int_equal(range->parts[1].max_64,  1);
    assert_int_equal(range->parts[2].min_64, 60);
    assert_int_equal(range->parts[2].max_64, 127);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "my_leaf", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "my_int_type", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "min .. -60 | -1 .. 1 |  60 .. max", NULL, NULL, NULL, 0, NULL);



    //TEST SUBTYPE ERROR min .. max
    schema = MODULE_CREATE_YANG("TS_ERR0",
     "typedef my_int_type { type int8 {range \"-128 .. -60 | -1 .. 1 |  60 .. 127\";}}"
     "leaf my_leaf {type my_int_type {range \"min .. max\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid range restriction - the derived restriction (min .. max) is not equally or more limiting.",
          "/TS_ERR0:my_leaf");


    //TEST SUBTYPE ERROR -80 .. 80
    schema = MODULE_CREATE_YANG("TS_ERR1",
                "typedef my_int_type { type int8 {range \"-128 .. -60 | -1 .. 1 |  60 .. 127\";}}"
                " leaf my_leaf {type my_int_type {range \"-80 .. 80\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid range restriction - the derived restriction (-80 .. 80) is not equally or more limiting.",
          "/TS_ERR1:my_leaf");

    //TEST SUBTYPE ERROR 0 .. max
    schema = MODULE_CREATE_YANG("TS_ERR2",
                "typedef my_int_type { type int8 {range \"-128 .. -60 | -1 .. 1 |  60 .. 127\";}}"
                "leaf my_leaf {type my_int_type {range \"0 .. max\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid range restriction - the derived restriction (0 .. max) is not equally or more limiting.",
          "/TS_ERR2:my_leaf");

    //TEST SUBTYPE ERROR -2 .. 2
    schema = MODULE_CREATE_YANG("TS_ERR3",
                "typedef my_int_type { type int8 {range \"-128 .. -60 | -1 .. 1 |  60 .. 127\";}}"
                "leaf my_leaf {type my_int_type {range \"-2 .. 2\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid range restriction - the derived restriction (-2 .. 2) is not equally or more limiting.",
          "/TS_ERR3:my_leaf");

    schema = MODULE_CREATE_YANG("TS_ERR4",
                "typedef my_int_type { type int8 {range \"-128 .. -60 | -1 .. 1 |  60 .. 127\";}}"
                "leaf my_leaf {type my_int_type {range \"-100 .. -90 | 100 .. 128\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid range restriction - value \"128\" does not fit the type limitations.",
          "/TS_ERR4:my_leaf");


    //TEST DEFAULT VALUE
    schema = MODULE_CREATE_YANG("DF0",
            "leaf port {"
            "    type int8 {range \"0 .. 50 | 127\";}"
            "    default \"20\";"
            "}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x205, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, 1);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    CHECK_LYD_VALUE(*(lysc_leaf->dflt), INT8, "20", 20);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 2, NULL);
    assert_int_equal(range->parts[0].min_64, 0);
    assert_int_equal(range->parts[0].max_64, 50);
    assert_int_equal(range->parts[1].min_64, 127);
    assert_int_equal(range->parts[1].max_64, 127);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, "20");
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "int8", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "0 .. 50 | 127", NULL, NULL, NULL, 0, NULL);

    schema = MODULE_CREATE_YANG("DF1", "leaf port {type int8 {range \"0 .. 50 | 127\";}"
            "default \"127\"; }");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x205, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, 1);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    CHECK_LYD_VALUE(*(lysc_leaf->dflt), INT8, "127", 127);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 2, NULL);
    assert_int_equal(range->parts[0].min_64, 0);
    assert_int_equal(range->parts[0].max_64, 50);
    assert_int_equal(range->parts[1].min_64, 127);
    assert_int_equal(range->parts[1].max_64, 127);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, "127");
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "int8", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "0 .. 50 | 127", NULL, NULL, NULL, 0, NULL);


    //TEST ERROR TD0
    schema = MODULE_CREATE_YANG("TD_ERR0",
            "leaf port {"
            "   type int8 {range \"0 .. 50 | 127\";}"
            "   default \"128\";"
            "}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid default - value does not fit the type (Value is out of int8's min/max bounds.).",
          "Schema location /TD_ERR0:port.");

    //TEST ERROR TD1
    schema = MODULE_CREATE_YANG("TD_ERR1",
            "leaf port {"
            "    type int8 {range \"0 .. 50 | 127\";}"
            "    default \"-1\";"
            "}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid default - value does not fit the type (Value \"-1\" does not satisfy the range constraint.).",
          "Schema location /TD_ERR1:port.");

    //TEST ERROR TD2
    schema = MODULE_CREATE_YANG("TD_ERR2",
            "leaf port {"
            "    type int8 {range \"0 .. 50 | 127\";}"
            "    default \"60\";"
            "}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid default - value does not fit the type (Value \"60\" does not satisfy the range constraint.).",
          "Schema location /TD_ERR2:port.");


    schema = MODULE_CREATE_YANG("TD_ERR3",
                "typedef my_int_type { type int8 {range \"60 .. 127\";} default \"127\";}"
                "leaf my_leaf {type my_int_type {range \"70 .. 80\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid default - value does not fit the type (Value \"127\" does not satisfy the range constraint.).",
          "Schema location /TD_ERR3:my_leaf.");

}


static void
test_schema_yin(void **state){



    (void) state;
    const char *schema;
    const struct lys_module *mod;
    struct lysc_node_leaf * lysc_leaf;
    struct lysp_node_leaf * lysp_leaf;
    struct lysc_range * range;

    // TEST T0
    schema = MODULE_CREATE_YIN("T0", "<leaf name=\"port\"> <type name=\"int8\"/> </leaf>");
    UTEST_ADD_MODULE(schema, LYS_IN_YIN, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 0);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x0, 0, 0, "int8", 0, 0, 1, 0, 0, 0);

    // TEST T1
    schema = MODULE_CREATE_YIN("T1",
           "<leaf name=\"port\"> "
           "    <type name=\"int8\"> <range value = \"0 .. 10\"/>  </type>"
           "</leaf>");
    UTEST_ADD_MODULE(schema, LYS_IN_YIN, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 1, NULL);
    assert_int_equal(range->parts[0].min_64, 0);
    assert_int_equal(range->parts[0].max_64, 10);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "int8", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "0 .. 10", NULL, NULL, NULL, 0, NULL);

    // TEST T1
    schema = MODULE_CREATE_YIN("T2",
            "<leaf name=\"port\"> "
            "    <type name=\"int8\"> <range value = \"-127 .. 10 | max\"/>  </type>"
            "</leaf>");
    UTEST_ADD_MODULE(schema, LYS_IN_YIN, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 2, NULL);
    assert_int_equal(range->parts[0].min_64, -127);
    assert_int_equal(range->parts[0].max_64, 10);
    assert_int_equal(range->parts[1].min_64, 127);
    assert_int_equal(range->parts[1].max_64, 127);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "int8", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "-127 .. 10 | max", NULL, NULL, NULL, 0, NULL);

    // TEST T2
    schema = MODULE_CREATE_YIN("T3",
            "<leaf name=\"port\"> "
            "    <type name=\"int8\"> <range value =\"min .. 10 | 11 .. 12 | 30\"/> </type>"
            "</leaf>");
    UTEST_ADD_MODULE(schema, LYS_IN_YIN, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 3, NULL);
    assert_int_equal(range->parts[0].min_64, -128);
    assert_int_equal(range->parts[0].max_64, 10);
    assert_int_equal(range->parts[1].min_64, 11);
    assert_int_equal(range->parts[1].max_64, 12);
    assert_int_equal(range->parts[2].min_64, 30);
    assert_int_equal(range->parts[2].max_64, 30);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "int8", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "min .. 10 | 11 .. 12 | 30", NULL, NULL, NULL, 0, NULL);

    //TEST ERROR -60 .. 0 | 0 .. 127
    schema = MODULE_CREATE_YIN("TE0",
           "<leaf name=\"port\"> "
           "   <type name=\"int8\"> <range value = \"min .. 0 | 0 .. 12\"/>  </type>"
           "</leaf>");
    UTEST_ADD_MODULE(schema, LYS_IN_YIN, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid range restriction - values are not in ascending order (0).",
          "/TE0:port");

    //TEST ERROR 0 .. 128
    schema = MODULE_CREATE_YIN("TE1",
           "<leaf name=\"port\">"
           "   <type name=\"int8\"> <range value = \"0 .. 128\"/>  </type>"
           "</leaf>");
    UTEST_ADD_MODULE(schema, LYS_IN_YIN, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid range restriction - value \"128\" does not fit the type limitations.",
          "/TE1:port");

    //TEST ERROR -129 .. 126
    schema = MODULE_CREATE_YIN("TE2",
           "<leaf name=\"port\"> "
           "   <type name=\"int8\"> <range value =\"-129 .. 126\"/>  </type>"
           "</leaf>");
    UTEST_ADD_MODULE(schema, LYS_IN_YIN, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid range restriction - value \"-129\" does not fit the type limitations.",
          "/TE2:port");

    schema = MODULE_CREATE_YIN("TS0",
           "<typedef name= \"my_int_type\">"
           "   <type name=\"int8\"> <range value = \"-127 .. 10 | max\"/>  </type>"
           "</typedef>"
           "<leaf name=\"my_leaf\"> <type name=\"my_int_type\"/> </leaf>");
    UTEST_ADD_MODULE(schema, LYS_IN_YIN, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "my_leaf", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 2, NULL);
    assert_int_equal(range->parts[0].min_64, -127);
    assert_int_equal(range->parts[0].max_64, 10);
    assert_int_equal(range->parts[1].min_64, 127);
    assert_int_equal(range->parts[1].max_64, 127);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "my_leaf", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x0, 0, 0, "my_int_type", 0, 0, 1, 0, 0, 0);


    schema = MODULE_CREATE_YIN("TS1",
           "<typedef name= \"my_int_type\">"
           "    <type name=\"int8\"> <range value = \"-127 .. 10 | 90 .. 100\"/>  </type>"
           "</typedef>"
           "<leaf name=\"port\"> <type name=\"my_int_type\"> <range value ="
           " \"min .. -30 | 100 .. max\"/>  </type> </leaf>");
    UTEST_ADD_MODULE(schema, LYS_IN_YIN, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x5, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 2, NULL);
    assert_int_equal(range->parts[0].min_64, -127);
    assert_int_equal(range->parts[0].max_64, -30);
    assert_int_equal(range->parts[1].min_64, 100);
    assert_int_equal(range->parts[1].max_64, 100);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, NULL);
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "my_int_type", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "min .. -30 | 100 .. max", NULL, NULL, NULL, 0, NULL);

    //TEST ERROR
    schema = MODULE_CREATE_YIN("TS_ERR1",
           "<typedef name= \"my_int_type\">"
           "    <type name=\"int8\"> <range value = \"-127 .. 10 | 90 .. 100\"/>  </type>"
           "</typedef>"
           "<leaf name=\"port\">"
           "    <type name=\"my_int_type\"> <range value = \"min .. max\"/>  </type>"
           "</leaf>");
    UTEST_ADD_MODULE(schema, LYS_IN_YIN, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid range restriction - the derived restriction (min .. max) is not equally or more limiting.",
          "/TS_ERR1:port");

    schema = MODULE_CREATE_YIN("TS_ERR2",
           "<typedef name= \"my_int_type\">"
           "    <type name=\"int8\"> <range value = \"-127 .. 10 | 90 .. 100\"/>  </type>"
           "</typedef>"
           "<leaf name=\"port\">"
           "    <type name=\"my_int_type\"> <range value = \"5 .. 11\"/>  </type>"
           "</leaf>");
    UTEST_ADD_MODULE(schema, LYS_IN_YIN, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid range restriction - the derived restriction (5 .. 11) is not equally or more limiting.",
          "/TS_ERR2:port");

    //TEST DEFAULT VALUE
    schema = MODULE_CREATE_YIN("DF0",
            "<leaf name=\"port\">"
            "    <default value=\"12\" />"
            "    <type name=\"int8\"> <range value = \"min .. 0 | 1 .. 12\"/>  </type>"
            "</leaf>" );
    UTEST_ADD_MODULE(schema, LYS_IN_YIN, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    lysc_leaf = (void *) mod->compiled->data;
    CHECK_LYSC_NODE_LEAF(lysc_leaf, NULL, 0, 0x205, 1, "port", 0, 0, 0, NULL, 0, 0, NULL, 1);
    CHECK_LYSC_TYPE_NUM((struct lysc_type_num *)lysc_leaf->type, LY_TYPE_INT8, 0, 1);
    CHECK_LYD_VALUE(*(lysc_leaf->dflt), INT8, "12", 12);
    range = ((struct lysc_type_num *)lysc_leaf->type)->range;
    CHECK_LYSC_RANGE(range, NULL, NULL, NULL, 0, 2, NULL);
    assert_int_equal(range->parts[0].min_64, -128);
    assert_int_equal(range->parts[0].max_64, 0);
    assert_int_equal(range->parts[1].min_64, 1);
    assert_int_equal(range->parts[1].max_64, 12);
    lysp_leaf = (void *) mod->parsed->data;
    CHECK_LYSP_NODE_LEAF(lysp_leaf, NULL, 0, 0x0, 0, "port", 0, 0, NULL, 0, 0, NULL, "12");
    CHECK_LYSP_TYPE(&(lysp_leaf->type), 0, 0, 0, 0, 0, 0x80, 0, 0, "int8", 0, 0, 1, 1, 0, 0);
    CHECK_LYSP_RESTR(lysp_leaf->type.range, "min .. 0 | 1 .. 12", NULL, NULL, NULL, 0, NULL);


    //TEST ERROR TD0
    schema = MODULE_CREATE_YIN("TD_ERR0",
            "<leaf name=\"port\">"
            "    <default value=\"128\" />"
            "    <type name=\"int8\"> <range value = \"min .. 0 | 1 .. 12\"/>  </type>"
            "</leaf>");
    UTEST_ADD_MODULE(schema, LYS_IN_YIN, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid default - value does not fit the type (Value is out of int8's min/max bounds.).",
          "Schema location /TD_ERR0:port.");

    //TEST ERROR TD1
    schema = MODULE_CREATE_YIN("TD_ERR1",
            "<leaf name=\"port\">"
            "     <default value=\"13\" />"
            "     <type name=\"int8\"> <range value = \"min .. 0 | 1 .. 12\"/>  </type>"
            "</leaf>");
    UTEST_ADD_MODULE(schema, LYS_IN_YIN, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid default - value does not fit the type (Value \"13\" does not satisfy the range constraint.).",
          "Schema location /TD_ERR1:port.");


    schema = MODULE_CREATE_YIN("TD_ERR3",
           "<typedef name= \"my_int_type\">"
           "    <default value=\"10\" />"
           "    <type name=\"int8\"> <range value = \"-127 .. 10 | max\"/> </type>"
           "</typedef>"
           "<leaf name=\"my_leaf\">"
           "     <type name=\"my_int_type\">"
           "     <range value = \"-127 .. -80\"/>  </type>"
           "</leaf>");
    UTEST_ADD_MODULE(schema, LYS_IN_YIN, NULL, LY_EVALID, &mod);
    CHECK_LOG_CTX("Invalid default - value does not fit the type (Value \"10\" does not satisfy the range constraint.).",
          "Schema location /TD_ERR3:my_leaf.");
}


static void
test_schema_print(void **state)
{
    (void) state;
    const char *schema_yang, *schema_yin;
    char *printed;
    const struct lys_module *mod;

    schema_yang = MODULE_CREATE_YANG("PRINT0",
            "leaf port {type int8 {range \"0 .. 50 | 127\";}  default \"20\";}");
    schema_yin  = MODULE_CREATE_YIN("PRINT0",
              "<description>"
              "    <text>desc</text>"
              "</description>"
              "<leaf name=\"port\">"
              "    <type name=\"int8\">"
              "        <range value=\"0 .. 50 | 127\"/>"
              "    </type>"
              "<default value=\"20\"/>"
              "</leaf>");

    UTEST_ADD_MODULE(schema_yang, LYS_IN_YANG, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    assert_int_equal(LY_SUCCESS, lys_print_mem(&printed, mod, LYS_OUT_YIN, 0));
    assert_int_equal(1, string_cmp(printed, schema_yin));
    free(printed);


    schema_yang = MODULE_CREATE_YANG("PRINT1",
            "leaf port {type int8 {range \"0 .. 50 | 127\";}  default \"20\";}");
    schema_yin  = MODULE_CREATE_YIN("PRINT1",
              "<description>"
              "    <text>desc</text>"
              "</description>"
              "<leaf name=\"port\">"
              "    <type name=\"int8\">"
              "        <range value=\"0 .. 50 | 127\"/>"
              "    </type>"
              "<default value=\"20\"/>"
              "</leaf>");

    UTEST_ADD_MODULE(schema_yin, LYS_IN_YIN, NULL, LY_SUCCESS, &mod);
    assert_non_null(mod);
    assert_int_equal(LY_SUCCESS, lys_print_mem(&printed, mod, LYS_OUT_YANG, 0));
    assert_int_equal(1, string_cmp(printed, schema_yang));
    free(printed);
}


static void
test_data_xml(void **state){
    (void) state;
    const char *schema;
    struct lyd_node *tree;
    const char *data;

    schema = MODULE_CREATE_YANG("defs", "leaf port {type int8 {range \"0 .. 50 | 105\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, NULL);

    TEST_SUCCESS_XML("defs", "50", INT8, "50", 50);
    TEST_SUCCESS_XML("defs", "105", INT8, "105", 105);
    TEST_SUCCESS_XML("defs", "0", INT8, "0", 0);
    TEST_SUCCESS_XML("defs", "-0", INT8, "0", 0);
    TEST_ERROR_XML("defs", "-1");
    TEST_ERROR_XML("defs", "51");
    TEST_ERROR_XML("defs", "106");
    TEST_ERROR_XML("defs", "104");
    TEST_ERROR_XML("defs", "60");

    schema = MODULE_CREATE_YANG("T0", "leaf port {type int8; }");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, NULL);
    TEST_SUCCESS_XML("T0", "-128", INT8, "-128", -128);
    TEST_SUCCESS_XML("T0", "-100", INT8, "-100", -100);
    TEST_SUCCESS_XML("T0", "0", INT8, "0", 0);
    TEST_SUCCESS_XML("T0", "10", INT8, "10", 10);
    TEST_SUCCESS_XML("T0", "50", INT8, "50", 50);
    TEST_SUCCESS_XML("T0", "127", INT8, "127", 127);
    TEST_ERROR_XML("T0", "-129");
    TEST_ERROR_XML("T0", "128");
    TEST_ERROR_XML("T0", "256");
    TEST_ERROR_XML("T0", "1024");


    //check if there isnt default value then crash
    //default value
    schema = MODULE_CREATE_YANG("T1",
            "container cont {\n"
            "    leaf port {type int8 {range \"0 .. 50 | 105\";} default \"20\";}"
            "}");

    //check using default value
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, NULL);
    data = "<cont xmlns=\"urn:tests:" "T1" "\">"  "</cont>";
    CHECK_PARSE_LYD_PARAM(data, LYD_XML, 0, LYD_VALIDATE_PRESENT, LY_SUCCESS, tree);
    struct lysc_node_container * lysc_root = (void *)tree->schema;
    CHECK_LYSC_NODE(lysc_root->child, NULL, 0, 0x205, 1, "port", 0, LYS_LEAF, 1, 0, 0, 0);
    struct lyd_node_inner * lyd_root = ((struct lyd_node_inner *) tree);
    CHECK_LYD_NODE_TERM((struct lyd_node_term *) lyd_root->child, 1, 0, 0, 1, 1,
        INT8, "20", 20);\

    char * test;
    int dynamic;
    //struct lyd_node_term * term = (void *) lyd_root->child;
    //ly_type_print_clb lyd_value_print = ly_builtin_type_plugins[LY_TYPE_INT8].print;
    //test = lyd_value_print(&(term->value), LYD_XML, NULL,  &dynamic);
    //printf("TEXT %s\n", test);

    lyd_print_mem(&test, tree, LYD_JSON, LYD_PRINT_WITHSIBLINGS | LYD_PRINT_SHRINK);
    assert_non_null(test);
    printf("JSON : %s\n", test);
    free(test);
    lyd_print_mem(&test, tree, LYD_XML, LYD_PRINT_WITHSIBLINGS | LYD_PRINT_SHRINK);
    assert_non_null(test);
    printf("XML : %s\n", test);
    free(test);
    lyd_free_all(tree);

//    printf("%s\n", test);
//    free(test);
//    //CHECK_LYD_STRING_PARAM(tree, "", LYD_XML, 0);
//    //CHECK_LYD_NODE_TERM((struct lyd_node_term *) tree, 0, 0, 0, 0, 1, TYPE, ## __VA_ARGS__);

//
//    //check seting value if default value is set
//    data = "<cont xmlns=\"urn:tests:" "T1" "\">" "<port>" "30" "</port>"  "</cont>";
//    CHECK_PARSE_LYD_PARAM(data, LYD_XML, 0, LYD_VALIDATE_PRESENT, LY_SUCCESS, tree);
//    assert_non_null(tree);
//    CHECK_LYSC_NODE(tree->schema, NULL, 0, 0x5, 1, "cont", 0, LYS_CONTAINER, 0, 0, 0, 0);
//    lyd_print_mem(&test, tree, LYD_XML, LYD_PRINT_WITHSIBLINGS | LYD_PRINT_SHRINK);
//    printf("%s\n", test);
}


static void
test_data_json(void **state){
    (void) state;
    const char *schema;
//    struct lyd_node *tree;
//    const char *data;

    schema = MODULE_CREATE_YANG("defs", "leaf port {type int8 {range \"0 .. 50 | 105\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, NULL);

    TEST_SUCCESS_JSON("defs", "50", INT8, "50", 50);
    TEST_SUCCESS_JSON("defs", "50", INT8, "50", 50);
    TEST_SUCCESS_JSON("defs", "105", INT8, "105", 105);
    TEST_SUCCESS_JSON("defs", "0", INT8, "0", 0);
    TEST_SUCCESS_JSON("defs", "-0", INT8, "0", 0);
    TEST_ERROR_JSON("defs", "-1");
    TEST_ERROR_JSON("defs", "51");
    TEST_ERROR_JSON("defs", "106");
    TEST_ERROR_JSON("defs", "104");
    TEST_ERROR_JSON("defs", "60");

    schema = MODULE_CREATE_YANG("T0", "leaf port {type int8; }");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, NULL);
    TEST_SUCCESS_JSON("T0", "-128", INT8, "-128", -128);
    TEST_SUCCESS_JSON("T0", "-100", INT8, "-100", -100);
    TEST_SUCCESS_JSON("T0", "0", INT8, "0", 0);
    TEST_SUCCESS_JSON("T0", "10", INT8, "10", 10);
    TEST_SUCCESS_JSON("T0", "50", INT8, "50", 50);
    TEST_SUCCESS_JSON("T0", "127", INT8, "127", 127);
    TEST_ERROR_JSON("T0", "-129");
    TEST_ERROR_JSON("T0", "128");
    TEST_ERROR_JSON("T0", "256");
    TEST_ERROR_JSON("T0", "1024");

//    //check if there isnt default value then crash
//    //default value
//    schema = MODULE_CREATE_YANG("T1",
//            "container cont {\n"
//            "    leaf port {type int8 {range \"0 .. 50 | 105\";} default \"20\";}"
//            "}");
//    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, NULL);
//    //TEST_SUCCESS("T0", "", INT8, "20", 20);
//
//    struct lyd_node *tree = NULL;
//    const char *data = "<cont xmlns=\"urn:tests:" "T1" "\">" "<port>" "30" "</port>"  "</cont>";
//    CHECK_PARSE_LYD_PARAM(data, LYD_XML, 0, LYD_VALIDATE_PRESENT, LY_SUCCESS, tree);
//    assert_non_null(tree);
//    CHECK_LYSC_NODE(tree->schema, NULL, 0, 0x5, 1, "cont", 0, LYS_CONTAINER, 0, 0, 0, 0);
//    const char * test;
//    lyd_print_mem(&test, tree, LYD_XML, LYD_PRINT_WITHSIBLINGS | LYD_PRINT_SHRINK);
//    printf("%s\n", test);
//    free(test);
//    //CHECK_LYD_STRING_PARAM(tree, "", LYD_XML, 0);
//    //CHECK_LYD_NODE_TERM((struct lyd_node_term *) tree, 0, 0, 0, 0, 1, TYPE, ## __VA_ARGS__);
//    lyd_free_all(tree);
//
//    //check seting value if default value is set
//    struct lyd_node *tree = NULL;
//    const char *data = "<cont xmlns=\"urn:tests:" "T1" "\">" "<port>" "30" "</port>"  "</cont>";
//    CHECK_PARSE_LYD_PARAM(data, LYD_XML, 0, LYD_VALIDATE_PRESENT, LY_SUCCESS, tree);
//    assert_non_null(tree);
//    CHECK_LYSC_NODE(tree->schema, NULL, 0, 0x5, 1, "cont", 0, LYS_CONTAINER, 0, 0, 0, 0);
//    const char * test;
//    lyd_print_mem(&test, tree, LYD_XML, LYD_PRINT_WITHSIBLINGS | LYD_PRINT_SHRINK);
//    printf("%s\n", test);
}


static void
test_diff(void **state){
    (void) state;
    const char *schema;

    schema = MODULE_CREATE_YANG("defs", "leaf port {type int8 {range \"0 .. 50\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, NULL);

    struct lyd_node *model_1, *model_2;
    const char *data_1 = "<port xmlns=\"urn:tests:defs\"> 5 </port>";
    const char *data_2 = "<port xmlns=\"urn:tests:defs\"> 6 </port>";
    const char *diff_expected = "<port xmlns=\"urn:tests:defs\" "
                   "xmlns:yang=\"urn:ietf:params:xml:ns:yang:1\" "
                   "yang:operation=\"replace\" yang:orig-default=\"false\" yang:orig-value=\"5\">"
                   "6</port>";

    LYD_TREE_CREATE(data_1, model_1);
    LYD_TREE_CREATE(data_2, model_2);

    struct lyd_node *diff;
    assert_int_equal(LY_SUCCESS, lyd_diff_siblings(model_1, model_2, 0, &diff));
    assert_non_null(diff);

    char * test;
    lyd_print_mem(&test, diff, LYD_XML, LYD_PRINT_WITHSIBLINGS | LYD_PRINT_SHRINK);
    CHECK_LYD_STRING_PARAM(diff, diff_expected, LYD_XML, LYD_PRINT_WITHSIBLINGS | LYD_PRINT_SHRINK);
    free(test);

    lyd_free_all(model_1);
    lyd_free_all(model_2);
    lyd_free_all(diff);
}


static void
test_compare(void **state){
    (void) state;

    const char *schema = MODULE_CREATE_YANG("defs", "leaf port {type int8 {range \"0 .. 50\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, NULL);

    struct lyd_node *model_1, *model_2;
    const char *data_1 = "<port xmlns=\"urn:tests:defs\"> 5 </port>";
    const char *data_2 = "<port xmlns=\"urn:tests:defs\"> 5 </port>";

    LYD_TREE_CREATE(data_1, model_1);
    LYD_TREE_CREATE(data_2, model_2);

    assert_int_equal(LY_SUCCESS, lyd_compare_single(model_1, model_2, 0));

    lyd_free_all(model_1);\
    lyd_free_all(model_2);\

}

static void
test_duplicate(void **state){
    (void) state;
}


static void
test_new(void **state){
    (void) state;

    //test lyd_new_term
}


static void
test_merge(void **state){
    (void) state;
}


static void
test_print_xml(void **state){
    (void) state;

    const char *schema = MODULE_CREATE_YANG("defs", "leaf port {type int8 {range \"0 .. 50\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, NULL);

    struct lyd_node *model_1;
    const char *data_1 = "<port xmlns=\"urn:tests:defs\"> 50 </port>";
    LYD_TREE_CREATE(data_1, model_1);

    const char *expected = "<port xmlns=\"urn:tests:defs\">50</port>";
    CHECK_LYD_STRING_PARAM(model_1, expected, LYD_XML, LYD_PRINT_SHRINK | LYD_PRINT_WITHSIBLINGS | LYD_PRINT_SHRINK);

    lyd_free_all(model_1);

}


static void
test_plugin(void **state){
    (void) state;

    // plugin zebrat z proměných
    const char *schema = MODULE_CREATE_YANG("defs", "leaf port {type int8 {range \"0 .. 50\";}}");
    UTEST_ADD_MODULE(schema, LYS_IN_YANG, NULL, LY_SUCCESS, NULL);

    struct ly_err_item* err = NULL;
    struct lys_module * mod;
    struct lyd_node *model;
    struct lyd_node_term *term;
    struct lyd_value *value_orig = {0};\
    struct lyd_value value = {0};\

    const char *data = "<port xmlns=\"urn:tests:defs\"> 5 </port>";
    LYD_TREE_CREATE(data, model);
    term = (struct lyd_node_term *) model;
    CHECK_LYD_NODE_TERM(term, 0, 0, 0, 0, 1, INT8, "5", 5);

    value_orig = &term->value;
    assert_int_equal(LY_SUCCESS, value_orig->realtype->plugin->duplicate(UTEST_LYCTX, value_orig, &value));
    CHECK_LYD_VALUE(value, INT8, "5", 5);
    assert_ptr_equal(value_orig->realtype, value.realtype);

    //compare
    assert_int_equal(LY_SUCCESS, value_orig->realtype->plugin->compare(value_orig, &value));

    //store
    data = "10";
    mod  = ly_ctx_get_module(UTEST_LYCTX, "defs", NULL);
    value.realtype->plugin->free(UTEST_LYCTX, value_orig);
    assert_int_equal(LY_SUCCESS, value_orig->realtype->plugin->store(UTEST_LYCTX, value_orig->realtype,
    data, strlen(data), LY_TYPE_STORE_IMPLEMENT, LY_PREF_XML, NULL, LYD_VALHINT_DECNUM,
    mod->compiled->data, value_orig, NULL, &err));

    //compare
    assert_int_equal(LY_ENOT, value_orig->realtype->plugin->compare(&value, value_orig));
    value.realtype->plugin->free(UTEST_LYCTX, &value);

    lyd_free_all(model);
}


int
main(void)
{
    const struct CMUnitTest tests[] = {
        UTEST(test_schema_yang),
        UTEST(test_schema_yin),
        UTEST(test_schema_print),
        UTEST(test_data_xml),
        UTEST(test_data_json),
        UTEST(test_diff),
        UTEST(test_compare),
        UTEST(test_duplicate),
        UTEST(test_new),
        UTEST(test_merge),
        UTEST(test_print_xml),
        UTEST(test_plugin),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

