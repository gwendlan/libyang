/**
 * @file parser_yin.c
 * @author David Sedlák <xsedla1d@stud.fit.vutbr.cz>
 * @brief YIN parser.
 *
 * Copyright (c) 2015 - 2019 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */
#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>

#include "context.h"
#include "dict.h"
#include "xml.h"
#include "tree.h"
#include "tree_schema.h"
#include "tree_schema_internal.h"
#include "parser_yin.h"

/**
 * @brief check if given string is URI of yin namespace.
 *
 * @param ns Namespace URI to check.
 *
 * @return true if ns equals YIN_NS_URI false otherwise.
 */
#define IS_YIN_NS(ns) (strcmp(ns, YIN_NS_URI) == 0)

const char *const yin_attr_list[] = {
    [YIN_ARG_NAME] = "name",
    [YIN_ARG_TARGET_NODE] = "target-node",
    [YIN_ARG_MODULE] = "module",
    [YIN_ARG_VALUE] = "value",
    [YIN_ARG_TEXT] = "text",
    [YIN_ARG_CONDITION] = "condition",
    [YIN_ARG_URI] = "uri",
    [YIN_ARG_DATE] = "date",
    [YIN_ARG_TAG] = "tag",
};

enum yang_keyword
yin_match_keyword(struct yin_parser_ctx *ctx, const char *name, size_t name_len,
                  const char *prefix, size_t prefix_len, enum yang_keyword parrent)
{
    const char *start = NULL;
    enum yang_keyword kw = YANG_NONE;
    const struct lyxml_ns *ns = NULL;

    if (!name || name_len == 0) {
        return YANG_NONE;
    }

    ns = lyxml_ns_get(&ctx->xml_ctx, prefix, prefix_len);
    if (ns) {
        if (!IS_YIN_NS(ns->uri)) {
            return YANG_CUSTOM;
        }
    } else {
        /* elements without namespace are automatically unknown */
        return YANG_NONE;
    }

    start = name;
    kw = lysp_match_kw(NULL, &name);

    if (name - start == (long int)name_len) {
        /* this is done because of collision in yang statement value and yang argument mapped to yin element value */
        if (kw == YANG_VALUE && parrent == YANG_ERROR_MESSAGE) {
            return YIN_VALUE;
        }
        return kw;
    } else {
        if (strncmp(start, "text", name_len) == 0) {
            return YIN_TEXT;
        } else {
            return YANG_NONE;
        }
    }
}

enum yin_argument
yin_match_argument_name(const char *name, size_t len)
{
    enum yin_argument arg = YIN_ARG_UNKNOWN;
    size_t already_read = 0;
    LY_CHECK_RET(len == 0, YIN_ARG_NONE);

#define IF_ARG(STR, LEN, STMT) if (!strncmp((name) + already_read, STR, LEN)) {already_read+=LEN;arg=STMT;}
#define IF_ARG_PREFIX(STR, LEN) if (!strncmp((name) + already_read, STR, LEN)) {already_read+=LEN;
#define IF_ARG_PREFIX_END }

    switch (*name) {
    case 'c':
        already_read += 1;
        IF_ARG("ondition", 8, YIN_ARG_CONDITION);
        break;

    case 'd':
        already_read += 1;
        IF_ARG("ate", 3, YIN_ARG_DATE);
        break;

    case 'm':
        already_read += 1;
        IF_ARG("odule", 5, YIN_ARG_MODULE);
        break;

    case 'n':
        already_read += 1;
        IF_ARG("ame", 3, YIN_ARG_NAME);
        break;

    case 't':
        already_read += 1;
        IF_ARG_PREFIX("a", 1)
            IF_ARG("g", 1, YIN_ARG_TAG)
            else IF_ARG("rget-node", 9, YIN_ARG_TARGET_NODE)
        IF_ARG_PREFIX_END
        else IF_ARG("ext", 3, YIN_ARG_TEXT)
        break;

    case 'u':
        already_read += 1;
        IF_ARG("ri", 2, YIN_ARG_URI)
        break;

    case 'v':
        already_read += 1;
        IF_ARG("alue", 4, YIN_ARG_VALUE);
        break;
    }

    /* whole argument must be matched */
    if (already_read != len) {
        arg = YIN_ARG_UNKNOWN;
    }

#undef IF_ARG
#undef IF_ARG_PREFIX
#undef IF_ARG_PREFIX_END

    return arg;
}

void free_arg_rec(struct yin_parser_ctx *ctx, struct yin_arg_record *record) {
    (void)ctx; /* unused */
    if (record && record->dynamic_content) {
        free(record->content);
    }
}

#define IS_NODE_ELEM(kw) (kw == YANG_ANYXML || kw == YANG_ANYDATA || kw == YANG_LEAF || kw == YANG_LEAF_LIST || \
                          kw == YANG_TYPEDEF || kw == YANG_USES || kw == YANG_LIST || kw == YANG_NOTIFICATION || \
                          kw == YANG_GROUPING || kw == YANG_CONTAINER || kw == YANG_CASE || kw == YANG_CHOICE || \
                          kw == YANG_ACTION || kw == YANG_RPC || kw == YANG_AUGMENT)

#define HAS_META(kw) (IS_NODE_ELEM(kw) || kw == YANG_ARGUMENT || kw == YANG_IMPORT || kw == YANG_INCLUDE || kw == YANG_INPUT || kw == YANG_OUTPUT)

/**
 * @brief Free subelems information allocated on heap.
 *
 * @param[in] count Size of subelems array.
 * @param[in] subelems Subelems array to free.
 */
static void
subelems_deallocator(size_t count, struct yin_subelement *subelems)
{
    for(size_t i = 0; i < count; ++i) {
        if (HAS_META(subelems[i].type)) {
            free(subelems[i].dest);
        }
    }

    free(subelems);
}

/**
 * @brief Allocate subelems information on heap.
 *
 * @param[in] ctx Yin parser context, used for logging.
 * @param[in] count Number of subelements.
 * @param[in] parent Parent node if any.
 * @param[out] result Allocated subelems array.
 *
 * @return LY_SUCCESS on success LY_EMEM on memmory allocation failure.
 */
static LY_ERR
subelems_allocator(struct yin_parser_ctx *ctx, size_t count, struct lysp_node *parent,
                   struct yin_subelement **result, ...)
{
    va_list ap;

    *result = calloc(count, sizeof **result);
    LY_CHECK_GOTO(!(*result), MEM_ERR);

    va_start(ap, result);
    for (size_t i = 0; i < count; ++i) {
        /* TYPE */
        (*result)[i].type = va_arg(ap, enum yang_keyword);
        /* DEST */
        if (IS_NODE_ELEM((*result)[i].type)) {
            struct tree_node_meta *node_meta = NULL;
            node_meta = calloc(1, sizeof *node_meta);
            LY_CHECK_GOTO(!node_meta, MEM_ERR);
            node_meta->parent = parent;
            node_meta->siblings = va_arg(ap, void *);
            (*result)[i].dest = node_meta;
        } else if ((*result)[i].type == YANG_ARGUMENT) {
            struct yin_argument_meta *arg_meta = NULL;
            arg_meta = calloc(1, sizeof *arg_meta);
            LY_CHECK_GOTO(!arg_meta, MEM_ERR);
            arg_meta->argument = va_arg(ap, const char **);
            arg_meta->flags = va_arg(ap, uint16_t *);
            (*result)[i].dest = arg_meta;
        } else if ((*result)[i].type == YANG_IMPORT) {
            struct import_meta *imp_meta = NULL;
            imp_meta = calloc(1, sizeof *imp_meta);
            LY_CHECK_GOTO(!imp_meta, MEM_ERR);
            imp_meta->prefix = va_arg(ap, const char *);
            imp_meta->imports = va_arg(ap, struct lysp_import **);
            (*result)[i].dest = imp_meta;
        } else if ((*result)[i].type == YANG_INCLUDE) {
            struct include_meta *inc_meta = NULL;
            inc_meta = calloc(1, sizeof *inc_meta);
            LY_CHECK_GOTO(!inc_meta, MEM_ERR);
            inc_meta->name = va_arg(ap, const char *);
            inc_meta->includes = va_arg(ap, struct lysp_include **);
            (*result)[i].dest = inc_meta;
        } else if ((*result)[i].type == YANG_INPUT || (*result)[i].type == YANG_OUTPUT) {
            struct inout_meta *inout_meta = NULL;
            inout_meta = calloc(1, sizeof *inout_meta);
            LY_CHECK_GOTO(!inout_meta, MEM_ERR);
            inout_meta->parent = parent;
            inout_meta->inout_p = va_arg(ap, struct lysp_action_inout *);
            (*result)[i].dest = inout_meta;
        } else {
            (*result)[i].dest = va_arg(ap, void *);
        }
        /* FLAGS */
        (*result)[i].flags = va_arg(ap, int);
    }
    va_end(ap);

    return LY_SUCCESS;

MEM_ERR:
    subelems_deallocator(count, *result);
    LOGMEM(ctx->xml_ctx.ctx);
    return LY_EMEM;
}

LY_ERR
yin_load_attributes(struct yin_parser_ctx *ctx, const char **data, struct yin_arg_record **attrs)
{
    LY_ERR ret = LY_SUCCESS;
    struct yin_arg_record *argument_record = NULL;
    const char *prefix, *name;
    size_t prefix_len, name_len;

    /* load all attributes */
    while (ctx->xml_ctx.status == LYXML_ATTRIBUTE) {
        ret = lyxml_get_attribute(&ctx->xml_ctx, data, &prefix, &prefix_len, &name, &name_len);
        LY_CHECK_GOTO(ret, cleanup);

        if (ctx->xml_ctx.status == LYXML_ATTR_CONTENT) {
            LY_ARRAY_NEW_GOTO(ctx->xml_ctx.ctx, *attrs, argument_record, ret, cleanup);
            argument_record->name = name;
            argument_record->name_len = name_len;
            argument_record->prefix = prefix;
            argument_record->prefix_len = prefix_len;
            ret = lyxml_get_string(&ctx->xml_ctx, data, &argument_record->content, &argument_record->content_len,
                                   &argument_record->content, &argument_record->content_len, &argument_record->dynamic_content);
            LY_CHECK_GOTO(ret, cleanup);
        }
    }

cleanup:
    if (ret != LY_SUCCESS) {
        FREE_ARRAY(ctx, *attrs, free_arg_rec);
        *attrs = NULL;
    }
    return ret;
}

LY_ERR
yin_validate_value(struct yin_parser_ctx *ctx, enum yang_arg val_type, char *val, size_t len)
{
    int prefix = 0;
    unsigned int c;
    size_t utf8_char_len;
    size_t already_read = 0;
    while (already_read < len) {
        LY_CHECK_ERR_RET(ly_getutf8((const char **)&val, &c, &utf8_char_len),
                         LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INCHAR, (val)[-utf8_char_len]), LY_EVALID);
        already_read += utf8_char_len;
        LY_CHECK_ERR_RET(already_read > len, LOGINT(ctx->xml_ctx.ctx), LY_EINT);

        switch (val_type) {
        case Y_IDENTIF_ARG:
            LY_CHECK_RET(lysp_check_identifierchar((struct lys_parser_ctx *)ctx, c, !already_read, NULL));
            break;
        case Y_PREF_IDENTIF_ARG:
            LY_CHECK_RET(lysp_check_identifierchar((struct lys_parser_ctx *)ctx, c, !already_read, &prefix));
            break;
        case Y_STR_ARG:
        case Y_MAYBE_STR_ARG:
            LY_CHECK_RET(lysp_check_stringchar((struct lys_parser_ctx *)ctx, c));
            break;
        }
    }

    return LY_SUCCESS;
}

/**
 * @brief Parse yin argument.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs ([Sized array](@ref sizedarrays)) of attributes.
 * @param[in,out] data Data to read from.
 * @param[in] arg_type Type of argument that is expected in parsed element (use YIN_ARG_NONE for elements without
 *            special argument).
 * @param[out] arg_val Where value of argument should be stored. Can be NULL if arg_type is specified as YIN_ARG_NONE.
 * @param[in] val_type Type of expected value of attribute.
 * @param[in] current_element Identification of current element, used for logging.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_attribute(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, enum yin_argument arg_type,
                    const char **arg_val, enum yang_arg val_type, enum yang_keyword current_element)
{
    enum yin_argument arg = YIN_ARG_UNKNOWN;
    struct yin_arg_record *iter = NULL;
    bool found = false;

    /* validation of attributes */
    LY_ARRAY_FOR(attrs, struct yin_arg_record, iter) {
        /* yin arguments represented as attributes have no namespace, which in this case means no prefix */
        if (!iter->prefix) {
            arg = yin_match_argument_name(iter->name, iter->name_len);
            if (arg == YIN_ARG_NONE) {
                continue;
            } else if (arg == arg_type) {
                LY_CHECK_ERR_RET(found, LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_DUP_ATTR,
                                 yin_attr2str(arg), ly_stmt2str(current_element)), LY_EVALID);
                found = true;
                LY_CHECK_RET(yin_validate_value(ctx, val_type, iter->content, iter->content_len));
                if (iter->dynamic_content) {
                    *arg_val = lydict_insert_zc(ctx->xml_ctx.ctx, iter->content);
                    LY_CHECK_RET(!(*arg_val), LY_EMEM);
                    /* string is no longer supposed to be freed when the sized array is freed */
                    iter->dynamic_content = 0;
                } else {
                    if (iter->content_len == 0) {
                        *arg_val = lydict_insert(ctx->xml_ctx.ctx, "", 0);
                    } else {
                        *arg_val = lydict_insert(ctx->xml_ctx.ctx, iter->content, iter->content_len);
                    }
                    LY_CHECK_RET(!(*arg_val), LY_EMEM);
                }
            } else {
                LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_UNEXP_ATTR, iter->name_len, iter->name, ly_stmt2str(current_element));
                return LY_EVALID;
            }
        }
    }

    /* anything else than Y_MAYBE_STR_ARG is mandatory */
    if (val_type != Y_MAYBE_STR_ARG && !found) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LYVE_SYNTAX_YIN, "Missing mandatory attribute %s of %s element.", yin_attr2str(arg_type), ly_stmt2str(current_element));
        return LY_EVALID;
    }

    return LY_SUCCESS;
}

/**
 * @brief Get record with given type. Array must be sorted in ascending order by array[n].type.
 *
 * @param[in] type Type of wanted record.
 * @param[in] array_size Size of array.
 * @param[in] array Searched array.
 *
 * @return Pointer to desired record on success, NULL if element is not in the array.
 */
static struct yin_subelement *
get_record(enum yang_keyword type, signed char array_size, struct yin_subelement *array)
{
    signed char left = 0, right = array_size - 1, middle;

    while (left <= right) {
        middle = left + (right - left) / 2;

        if (array[middle].type == type) {
            return &array[middle];
        }

        if (array[middle].type < type) {
            left = middle + 1;
        } else {
            right = middle - 1;
        }
    }

    return NULL;
}

/**
 * @brief Helper function to check mandatory constraint of subelement.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] subelem_info Array of information about subelements.
 * @param[in] subelem_info_size Size of subelem_info array.
 * @param[in] current_element Identification of element that is currently being parsed, used for logging.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_check_subelem_mandatory_constraint(struct yin_parser_ctx *ctx, struct yin_subelement *subelem_info,
                                       signed char subelem_info_size, enum yang_keyword current_element)
{
    for (signed char i = 0; i < subelem_info_size; ++i) {
        /* if there is element that is mandatory and isn't parsed log error and return LY_EVALID */
        if (subelem_info[i].flags & YIN_SUBELEM_MANDATORY && !(subelem_info[i].flags & YIN_SUBELEM_PARSED)) {
            LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_MAND_SUBELEM,
                          ly_stmt2str(subelem_info[i].type), ly_stmt2str(current_element));
            return LY_EVALID;
        }
    }

    return LY_SUCCESS;
}

/**
 * @brief Helper function to check "first" constraint of subelement.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] subelem_info Array of information about subelements.
 * @param[in] subelem_info_size Size of subelem_info array.
 * @param[in] current_element Identification of element that is currently being parsed, used for logging.
 * @param[in] exp_first Record in subelem_info array that is expected to be defined as first subelement.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_check_subelem_first_constraint(struct yin_parser_ctx *ctx, struct yin_subelement *subelem_info,
                                   signed char subelem_info_size, enum yang_keyword current_element,
                                   struct yin_subelement *exp_first)
{
    for (signed char i = 0; i < subelem_info_size; ++i) {
        if (subelem_info[i].flags & YIN_SUBELEM_PARSED) {
            LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_FIRT_SUBELEM,
                           ly_stmt2str(exp_first->type), ly_stmt2str(current_element));
            return LY_EVALID;
        }
    }

    return LY_SUCCESS;
}

/**
 * @brief Helper function to check if array of information about subelements is in ascending order.
 *
 * @param[in] subelem_info Array of information about subelements.
 * @param[in] subelem_info_size Size of subelem_info array.
 *
 * @return True iff subelem_info array is in ascending order, False otherwise.
 */
#ifndef NDEBUG
static bool
is_ordered(struct yin_subelement *subelem_info, signed char subelem_info_size)
{
    enum yang_keyword current = YANG_NONE; /* 0 (minimal value) */

    for (signed char i = 0; i < subelem_info_size; ++i) {
        if (subelem_info[i].type <= current) {
            return false;
        }
        current = subelem_info[i].type;
    }

    return true;
}
#endif

/**
 * @brief Parse simple element without any special constraints and argument mapped to yin attribute,
 * for example prefix or namespace element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] kw Type of current element.
 * @param[out] value Where value of attribute should be stored.
 * @param[in] arg_type Expected type of attribute.
 * @param[in] arg_val_type Type of expected value of attribute.
 * @param[in,out] exts Extension instance to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_simple_element(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, enum yang_keyword kw,
                         const char **value, enum yin_argument arg_type, enum yang_arg arg_val_type, struct lysp_ext_instance **exts)
{
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, arg_type, value, arg_val_type, kw));
    struct yin_subelement subelems[1] = {
                                            {YANG_CUSTOM, NULL, 0}
                                        };

    return yin_parse_content(ctx, subelems, 1, data, kw, NULL, exts);
}

/**
 * @brief Parse path element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] kw Type of current element.
 * @param[out] type Type structure to store parsed value, flags and extension instances.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_path(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, enum yang_keyword kw,
               struct lysp_type *type)
{
    LY_CHECK_RET(yin_parse_simple_element(ctx, attrs, data, kw, &type->path,
                                          YIN_ARG_VALUE, Y_STR_ARG, &type->exts));
    type->flags |= LYS_SET_PATH;

    return LY_SUCCESS;
}

/**
 * @brief Parse pattern element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] patterns Restrictions to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_pattern(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                  struct lysp_type *type)
{
    const char *real_value = NULL;
    char *saved_value = NULL;
    struct lysp_restr *restr;

    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, type->patterns, restr, LY_EMEM);
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_VALUE, &real_value, Y_STR_ARG, YANG_PATTERN));
    size_t len = strlen(real_value);

    saved_value = malloc(len + 2);
    LY_CHECK_ERR_RET(!saved_value, LOGMEM(ctx->xml_ctx.ctx), LY_EMEM);
    memmove(saved_value + 1, real_value, len);
    FREE_STRING(ctx->xml_ctx.ctx, real_value);
    saved_value[0] = 0x06;
    saved_value[len + 1] = '\0';
    restr->arg = lydict_insert_zc(ctx->xml_ctx.ctx, saved_value);
    LY_CHECK_ERR_RET(!restr->arg, LOGMEM(ctx->xml_ctx.ctx), LY_EMEM);
    type->flags |= LYS_SET_PATTERN;

    struct yin_subelement subelems[6] = {
                                            {YANG_DESCRIPTION, &restr->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_ERROR_APP_TAG, &restr->eapptag, YIN_SUBELEM_UNIQUE},
                                            {YANG_ERROR_MESSAGE, &restr->emsg, YIN_SUBELEM_UNIQUE},
                                            {YANG_MODIFIER, &restr->arg, YIN_SUBELEM_UNIQUE},
                                            {YANG_REFERENCE, &restr->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0}
                                        };
    return yin_parse_content(ctx, subelems, 6, data, YANG_PATTERN, NULL, &restr->exts);
}

/**
 * @brief Parse fraction-digits element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] type Type structure to store value, flags and extension instances.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_fracdigits(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                     struct lysp_type *type)
{
    const char *temp_val = NULL;
    char *ptr;
    unsigned long int num;

    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_VALUE, &temp_val, Y_STR_ARG, YANG_FRACTION_DIGITS));

    if (temp_val[0] == '\0' || (temp_val[0] == '0') || !isdigit(temp_val[0])) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN, temp_val, "value", "fraction-digits");
        FREE_STRING(ctx->xml_ctx.ctx, temp_val);
        return LY_EVALID;
    }

    errno = 0;
    num = strtoul(temp_val, &ptr, 10);
    if (*ptr != '\0') {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN, temp_val, "value", "fraction-digits");
        FREE_STRING(ctx->xml_ctx.ctx, temp_val);
        return LY_EVALID;
    }
    if ((errno == ERANGE) || (num > 18)) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN, temp_val, "value", "fraction-digits");
        FREE_STRING(ctx->xml_ctx.ctx, temp_val);
        return LY_EVALID;
    }
    FREE_STRING(ctx->xml_ctx.ctx, temp_val);
    type->fraction_digits = num;
    type->flags |= LYS_SET_FRDIGITS;
    struct yin_subelement subelems[1] = {
                                            {YANG_CUSTOM, NULL, 0}
                                        };
    return yin_parse_content(ctx, subelems, 1, data, YANG_FRACTION_DIGITS, NULL, &type->exts);
}

/**
 * @brief Parse enum element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] type Type structure to store enum value, flags and extension instances.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_enum(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, struct lysp_type *type)
{
    struct lysp_type_enum *en;

    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, type->enums, en, LY_EMEM);
    type->flags |= LYS_SET_ENUM;
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &en->name, Y_IDENTIF_ARG, YANG_ENUM));
    LY_CHECK_RET(lysp_check_enum_name((struct lys_parser_ctx *)ctx, en->name, strlen(en->name)));
    YANG_CHECK_NONEMPTY((struct lys_parser_ctx *)ctx, strlen(en->name), "enum");
    CHECK_UNIQUENESS((struct lys_parser_ctx *)ctx, type->enums, name, "enum", en->name);

    struct yin_subelement subelems[6] = {
                                            {YANG_DESCRIPTION, &en->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_IF_FEATURE, &en->iffeatures, 0},
                                            {YANG_REFERENCE, &en->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_STATUS, &en->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_VALUE, en, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0}
                                        };
    return yin_parse_content(ctx, subelems, 6, data, YANG_ENUM, NULL, &en->exts);
}

/**
 * @brief Parse bit element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] enum_kw Identification of actual keyword, can be set to YANG_BIT or YANG_ENUM.
 * @param[in,out] type Type structure to store bit value, flags and extension instances.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_bit(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                   struct lysp_type *type)
{
    struct lysp_type_enum *en;

    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, type->bits, en, LY_EMEM);
    type->flags |= LYS_SET_BIT;
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &en->name, Y_IDENTIF_ARG, YANG_BIT));
    CHECK_UNIQUENESS((struct lys_parser_ctx *)ctx, type->enums, name, "bit", en->name);

    struct yin_subelement subelems[6] = {
                                            {YANG_DESCRIPTION, &en->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_IF_FEATURE, &en->iffeatures, 0},
                                            {YANG_POSITION, en, YIN_SUBELEM_UNIQUE},
                                            {YANG_REFERENCE, &en->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_STATUS, &en->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0}
                                        };
    return yin_parse_content(ctx, subelems, 6, data, YANG_BIT, NULL, &en->exts);
}

/**
 * @brief Parse simple element without any special constraints and argument mapped to yin attribute, that can have
 * more instances, such as base or if-feature.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] kw Type of current element.
 * @param[out] values Parsed values to add to.
 * @param[in] arg_type Expected type of attribute.
 * @param[in] arg_val_type Type of expected value of attribute.
 * @param[in,out] exts Extension instance to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_simple_elements(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, enum yang_keyword kw,
                          const char ***values, enum yin_argument arg_type, enum yang_arg arg_val_type, struct lysp_ext_instance **exts)
{
    const char **value;
    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *values, value, LY_EMEM);
    uint32_t index = LY_ARRAY_SIZE(*values) - 1;
    struct yin_subelement subelems[1] = {
                                            {YANG_CUSTOM, &index, 0}
                                        };

    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, arg_type, value, arg_val_type, kw));

    return yin_parse_content(ctx, subelems, 1, data, kw, NULL, exts);
}

/**
 * @brief Parse simple element without any special constraints and argument mapped to yin attribute.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] kw Type of current element.
 * @param[out] values Parsed values to add to.
 * @param[in] arg_type Expected type of attribute.
 * @param[in] arg_val_type Type of expected value of attribute.
 * @param[in,out] exts Extension instance to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_simple_elem(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, enum yang_keyword kw,
                      struct yin_subelement *subinfo, enum yin_argument arg_type, enum yang_arg arg_val_type, struct lysp_ext_instance **exts)
{
    if (subinfo->flags & YIN_SUBELEM_UNIQUE) {
        LY_CHECK_RET(yin_parse_simple_element(ctx, attrs, data, kw, (const char **)subinfo->dest,
                                              arg_type, arg_val_type, exts));
    } else {
        LY_CHECK_RET(yin_parse_simple_elements(ctx, attrs, data, kw, (const char ***)subinfo->dest,
                                               arg_type, arg_val_type, exts));
    }

    return LY_SUCCESS;
}

/**
 * @brief Parse base element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] parent Identification of parent element.
 * @param[out] dest Where parsed values should be stored.
 * @param[in,out] exts Extension instance to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_base(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, enum yang_keyword parent,
               void *dest, struct lysp_ext_instance **exts)
{
    struct lysp_type *type = NULL;

    if (parent == YANG_TYPE) {
        type = (struct lysp_type *)dest;
        LY_CHECK_RET(yin_parse_simple_elements(ctx, attrs, data, YANG_BASE, &type->bases, YIN_ARG_NAME,
                                               Y_PREF_IDENTIF_ARG, exts));
        type->flags |= LYS_SET_BASE;
    } else if (parent == YANG_IDENTITY) {
        LY_CHECK_RET(yin_parse_simple_elements(ctx, attrs, data, YANG_BASE, (const char ***)dest,
                                               YIN_ARG_NAME, Y_PREF_IDENTIF_ARG, exts));
    } else {
        LOGINT(ctx->xml_ctx.ctx);
        return LY_EINT;
    }

    return LY_SUCCESS;
}

/**
 * @brief Parse require instance element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @prama[out] type Type structure to store value, flag and extensions.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_pasrse_reqinstance(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs,
                       const char **data,  struct lysp_type *type)
{
    const char *temp_val = NULL;
    struct yin_subelement subelems[1] = {
                                            {YANG_CUSTOM, NULL, 0}
                                        };

    type->flags |= LYS_SET_REQINST;
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_VALUE, &temp_val, Y_STR_ARG, YANG_REQUIRE_INSTANCE));
    if (strcmp(temp_val, "true") == 0) {
        type->require_instance = 1;
    } else if (strcmp(temp_val, "false") != 0) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN VALID_VALS2, temp_val, "value",
                       "require-instance", "true", "false");
        FREE_STRING(ctx->xml_ctx.ctx, temp_val);
        return LY_EVALID;
    }
    FREE_STRING(ctx->xml_ctx.ctx, temp_val);

    return yin_parse_content(ctx, subelems, 1, data, YANG_REQUIRE_INSTANCE, NULL, &type->exts);
}

/**
 * @brief Parse modifier element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] pat Value to write to.
 * @param[in,out] exts Extension instances to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_modifier(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                   const char **pat, struct lysp_ext_instance **exts)
{
    assert(**pat == 0x06);
    const char *temp_val;
    char *modified_val;
    struct yin_subelement subelems[1] = {
                                            {YANG_CUSTOM, NULL, 0}
                                        };

    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_VALUE, &temp_val, Y_STR_ARG, YANG_MODIFIER));
    if (strcmp(temp_val, "invert-match") != 0) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN VALID_VALS1, temp_val, "value",
                       "modifier", "invert-match");
        FREE_STRING(ctx->xml_ctx.ctx, temp_val);
        return LY_EVALID;
    }
    lydict_remove(ctx->xml_ctx.ctx, temp_val);

    /* allocate new value */
    modified_val = malloc(strlen(*pat) + 1);
    LY_CHECK_ERR_RET(!modified_val, LOGMEM(ctx->xml_ctx.ctx), LY_EMEM);
    strcpy(modified_val, *pat);
    lydict_remove(ctx->xml_ctx.ctx, *pat);

    /* modify the new value */
    modified_val[0] = 0x15;
    *pat = lydict_insert_zc(ctx->xml_ctx.ctx, modified_val);

    return yin_parse_content(ctx, subelems, 1, data, YANG_MODIFIER, NULL, exts);
}

/**
 * @brief Parse a restriction element (length, range or one instance of must).
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] restr_kw Identificaton of element that is being parsed, can be set to YANG_MUST, YANG_LENGTH or YANG_RANGE.
 * @param[in] restr Value to write to.
 */
static LY_ERR
yin_parse_restriction(struct yin_parser_ctx *ctx,  struct yin_arg_record *attrs, const char **data,
                      enum yang_keyword restr_kw, struct lysp_restr *restr)
{
    assert(restr_kw == YANG_MUST || restr_kw == YANG_LENGTH || restr_kw == YANG_RANGE);
    struct yin_subelement subelems[5] = {
                                            {YANG_DESCRIPTION, &restr->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_ERROR_APP_TAG, &restr->eapptag, YIN_SUBELEM_UNIQUE},
                                            {YANG_ERROR_MESSAGE, &restr->emsg, YIN_SUBELEM_UNIQUE},
                                            {YANG_REFERENCE, &restr->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0}
                                        };
    /* argument of must is called condition, but argument of length and range is called value */
    enum yin_argument arg_type = (restr_kw == YANG_MUST) ? YIN_ARG_CONDITION : YIN_ARG_VALUE;
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, arg_type, &restr->arg, Y_STR_ARG, restr_kw));

    return yin_parse_content(ctx, subelems, 5, data, restr_kw, NULL, &restr->exts);
}

/**
 * @brief Parse range element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[out] type Type structure to store parsed value and flags.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_range(struct yin_parser_ctx *ctx,  struct yin_arg_record *attrs,
                const char **data, struct lysp_type *type)
{
    type->range = calloc(1, sizeof *type->range);
    LY_CHECK_ERR_RET(!type->range, LOGMEM(ctx->xml_ctx.ctx), LY_EMEM);
    LY_CHECK_RET(yin_parse_restriction(ctx, attrs, data, YANG_RANGE, type->range));
    type->flags |=  LYS_SET_RANGE;

    return LY_SUCCESS;
}

/**
 * @brief Parse length element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[out] type Type structure to store parsed value and flags.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_length(struct yin_parser_ctx *ctx,  struct yin_arg_record *attrs,
                const char **data, struct lysp_type *type)
{
    type->length = calloc(1, sizeof *type->length);
    LY_CHECK_ERR_RET(!type->length, LOGMEM(ctx->xml_ctx.ctx), LY_EMEM);
    LY_CHECK_RET(yin_parse_restriction(ctx, attrs, data, YANG_LENGTH, type->length));
    type->flags |= LYS_SET_LENGTH;

    return LY_SUCCESS;
}

/**
 * @brief Parse must element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] restrs Restrictions to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_must(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, struct lysp_restr **restrs)
{
    struct lysp_restr *restr;

    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *restrs, restr, LY_EMEM);
    return yin_parse_restriction(ctx, attrs, data, YANG_MUST, restr);
}

/**
 * @brief Parse position or value element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] kw Type of current element, can be set to YANG_POSITION or YANG_VALUE.
 * @param[out] enm Enum structure to save value, flags and extensions.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_value_pos_element(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                            enum yang_keyword kw, struct lysp_type_enum *enm)
{
    assert(kw == YANG_POSITION || kw == YANG_VALUE);
    const char *temp_val = NULL;
    char *ptr;
    long int num;
    unsigned long int unum;

    /* set value flag */
    enm->flags |= LYS_SET_VALUE;

    /* get attribute value */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_VALUE, &temp_val, Y_STR_ARG, kw));
    if (!temp_val || temp_val[0] == '\0' || (temp_val[0] == '+') ||
        ((temp_val[0] == '0') && (temp_val[1] != '\0')) || ((kw == YANG_POSITION) && !strcmp(temp_val, "-0"))) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN, temp_val, "value", ly_stmt2str(kw));
        goto error;
    }

    /* convert value */
    errno = 0;
    if (kw == YANG_VALUE) {
        num = strtol(temp_val, &ptr, 10);
        if (num < INT64_C(-2147483648) || num > INT64_C(2147483647)) {
            LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN, temp_val, "value", ly_stmt2str(kw));
            goto error;
        }
    } else {
        unum = strtoul(temp_val, &ptr, 10);
        if (unum > UINT64_C(4294967295)) {
            LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN, temp_val, "value", ly_stmt2str(kw));
            goto error;
        }
    }
    /* check if whole argument value was converted */
    if (*ptr != '\0') {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN, temp_val, "value", ly_stmt2str(kw));
        goto error;
    }
    if (errno == ERANGE) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_OOB_YIN, temp_val, "value", ly_stmt2str(kw));
        goto error;
    }
    /* save correctly ternary operator can't be used because num and unum have different signes */
    if (kw == YANG_VALUE) {
        enm->value = num;
    } else {
        enm->value = unum;
    }
    FREE_STRING(ctx->xml_ctx.ctx, temp_val);

    /* parse subelements */
    struct yin_subelement subelems[1] = {
                                            {YANG_CUSTOM, NULL, 0}
                                        };
    return yin_parse_content(ctx, subelems, 1, data, kw, NULL, &enm->exts);

    error:
        FREE_STRING(ctx->xml_ctx.ctx, temp_val);
        return LY_EVALID;
}


/**
 * @brief Parse belongs-to element.
 *
 * @param[in] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[out] submod Structure of submodule that is being parsed.
 * @param[in,out] exts Extension instances to add to.
 *
 * @return LY_ERR values
 */
static LY_ERR
yin_parse_belongs_to(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                     struct lysp_submodule *submod, struct lysp_ext_instance **exts)
{
    struct yin_subelement subelems[2] = {
                                            {YANG_PREFIX, &submod->prefix, YIN_SUBELEM_MANDATORY | YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0}
                                        };
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_MODULE, &submod->belongsto, Y_IDENTIF_ARG, YANG_BELONGS_TO));

    return yin_parse_content(ctx, subelems, 2, data, YANG_BELONGS_TO, NULL, exts);
}

/**
 * @brief Function to parse meta tags (description, contact, ...) eg. elements with
 * text element as child
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] Type of element can be set to YANG_ORGANIZATION or YANG_CONTACT or YANG_DESCRIPTION or YANG_REFERENCE.
 * @param[out] value Where the content of meta element should be stored.
 * @param[in,out] exts Extension instance to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_meta_element(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                       enum yang_keyword elem_type, const char **value, struct lysp_ext_instance **exts)
{
    assert(elem_type == YANG_ORGANIZATION || elem_type == YANG_CONTACT || elem_type == YANG_DESCRIPTION || elem_type == YANG_REFERENCE);

    struct yin_subelement subelems[2] = {
                                            {YANG_CUSTOM, NULL, 0},
                                            {YIN_TEXT, value, YIN_SUBELEM_MANDATORY | YIN_SUBELEM_UNIQUE | YIN_SUBELEM_FIRST}
                                        };
    /* check attributes */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NONE, NULL, Y_MAYBE_STR_ARG, elem_type));

    /* parse content */
    return yin_parse_content(ctx, subelems, 2, data, elem_type, NULL, exts);
}

/**
 * @brief Parse error-message element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from.
 * @param[out] value Where the content of error-message element should be stored.
 * @param[in,out] exts Extension instance to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_err_msg_element(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                          const char **value, struct lysp_ext_instance **exts)
{
    struct yin_subelement subelems[2] = {
                                            {YANG_CUSTOM, NULL, 0},
                                            {YIN_VALUE, value, YIN_SUBELEM_MANDATORY | YIN_SUBELEM_UNIQUE | YIN_SUBELEM_FIRST}
                                        };

    /* check attributes */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NONE, NULL, Y_MAYBE_STR_ARG, YANG_ERROR_MESSAGE));

    return yin_parse_content(ctx, subelems, 2, data, YANG_ERROR_MESSAGE, NULL, exts);
}

/**
 * @brief parse type element.
 *
 * @brief Parse type element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] parent Identification of parent element.
 * @param[in,out] type Type to wrote to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_type(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
               enum yang_keyword parent, struct yin_subelement *subinfo)
{
    struct lysp_type *type = NULL;
    if (parent == YANG_DEVIATE) {
        *(struct lysp_type **)subinfo->dest = calloc(1, sizeof **(struct lysp_type **)subinfo->dest);
        LY_CHECK_ERR_RET(!(*(struct lysp_type **)subinfo->dest), LOGMEM(ctx->xml_ctx.ctx), LY_EMEM);
        type = *((struct lysp_type **)subinfo->dest);
    } else  {
        type = (struct lysp_type *)subinfo->dest;
    }
    /* type as child of another type */
    if (parent == YANG_TYPE) {
        struct lysp_type *nested_type = NULL;
        LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, type->types, nested_type, LY_EMEM);
        type->flags |= LYS_SET_TYPE;
        type = nested_type;
    }
    struct yin_subelement subelems[11] = {
                                            {YANG_BASE, type, 0},
                                            {YANG_BIT, type, 0},
                                            {YANG_ENUM, type, 0},
                                            {YANG_FRACTION_DIGITS, type, YIN_SUBELEM_UNIQUE},
                                            {YANG_LENGTH, type, YIN_SUBELEM_UNIQUE},
                                            {YANG_PATH, type, YIN_SUBELEM_UNIQUE},
                                            {YANG_PATTERN, type, 0},
                                            {YANG_RANGE, type, YIN_SUBELEM_UNIQUE},
                                            {YANG_REQUIRE_INSTANCE, type, YIN_SUBELEM_UNIQUE},
                                            {YANG_TYPE, type},
                                            {YANG_CUSTOM, NULL, 0},
                                        };
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &type->name, Y_PREF_IDENTIF_ARG, YANG_TYPE));
    return yin_parse_content(ctx, subelems, 11, data, YANG_TYPE, NULL, &type->exts);
}

/**
 * @brief Parse max-elements element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] max Value to write to.
 * @param[in] flags Flags to write to.
 * @param[in,out] exts Extension instances to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_maxelements(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, uint32_t *max,
                      uint16_t *flags, struct lysp_ext_instance **exts)
{
    const char *temp_val = NULL;
    char *ptr;
    unsigned long int num;
    struct yin_subelement subelems[1] = {
                                            {YANG_CUSTOM, NULL, 0},
                                        };

    *flags |= LYS_SET_MAX;
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_VALUE, &temp_val, Y_STR_ARG, YANG_MAX_ELEMENTS));
    if (!temp_val || temp_val[0] == '\0' || temp_val[0] == '0' || (temp_val[0] != 'u' && !isdigit(temp_val[0]))) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN, temp_val, "value", "max-elements");
        FREE_STRING(ctx->xml_ctx.ctx, temp_val);
        return LY_EVALID;
    }

    if (strcmp(temp_val, "unbounded")) {
        errno = 0;
        num = strtoul(temp_val, &ptr, 10);
        if (*ptr != '\0') {
            LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN, temp_val, "value", "max-elements");
            FREE_STRING(ctx->xml_ctx.ctx, temp_val);
            return LY_EVALID;
        }
        if ((errno == ERANGE) || (num > UINT32_MAX)) {
            LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_OOB_YIN, temp_val, "value", "max-elements");
            FREE_STRING(ctx->xml_ctx.ctx, temp_val);
            return LY_EVALID;
        }
        *max = num;
    }
    FREE_STRING(ctx->xml_ctx.ctx, temp_val);
    return yin_parse_content(ctx, subelems, 1, data, YANG_MAX_ELEMENTS, NULL, exts);
}

/**
 * @brief Parse max-elements element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] min Value to write to.
 * @param[in] flags Flags to write to.
 * @param[in,out] exts Extension instances to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_minelements(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, uint32_t *min,
                      uint16_t *flags, struct lysp_ext_instance **exts)
{
    const char *temp_val = NULL;
    char *ptr;
    unsigned long int num;
    struct yin_subelement subelems[1] = {
                                            {YANG_CUSTOM, NULL, 0},
                                        };

    *flags |= LYS_SET_MIN;
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_VALUE, &temp_val, Y_STR_ARG, YANG_MIN_ELEMENTS));

    if (!temp_val || temp_val[0] == '\0' || (temp_val[0] == '0' && temp_val[1] != '\0')) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN, temp_val, "value", "min-elements");
        FREE_STRING(ctx->xml_ctx.ctx, temp_val);
        return LY_EVALID;
    }

    errno = 0;
    num = strtoul(temp_val, &ptr, 10);
    if (ptr[0] != 0) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN, temp_val, "value", "min-elements");
        FREE_STRING(ctx->xml_ctx.ctx, temp_val);
        return LY_EVALID;
    }
    if (errno == ERANGE || num > UINT32_MAX) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_OOB_YIN, temp_val, "value", "min-elements");
        FREE_STRING(ctx->xml_ctx.ctx, temp_val);
        return LY_EVALID;
    }
    *min = num;
    FREE_STRING(ctx->xml_ctx.ctx, temp_val);
    return yin_parse_content(ctx, subelems, 1, data, YANG_MIN_ELEMENTS, NULL, exts);
}

/**
 * @brief Parse min-elements or max-elements element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] parent Identification of parent element.
 * @param[in] current Identification of current element.
 * @param[in] dest Where the parsed value and flags should be stored.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_minmax(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                 enum yang_keyword parent, enum yang_keyword current, void *dest)
{
    assert(current == YANG_MAX_ELEMENTS || current == YANG_MIN_ELEMENTS);
    assert(parent == YANG_LEAF_LIST || parent == YANG_REFINE || parent == YANG_LIST || parent == YANG_DEVIATE);
    uint32_t *lim;
    uint16_t *flags;
    struct lysp_ext_instance **exts;

    if (parent == YANG_LEAF_LIST) {
        lim = (current == YANG_MAX_ELEMENTS) ? &((struct lysp_node_leaflist *)dest)->max : &((struct lysp_node_leaflist *)dest)->min;
        flags = &((struct lysp_node_leaflist *)dest)->flags;
        exts = &((struct lysp_node_leaflist *)dest)->exts;
    } else if (parent == YANG_REFINE) {
        lim = (current == YANG_MAX_ELEMENTS) ? &((struct lysp_refine *)dest)->max : &((struct lysp_refine *)dest)->min;
        flags = &((struct lysp_refine *)dest)->flags;
        exts = &((struct lysp_refine *)dest)->exts;
    } else if (parent == YANG_LIST) {
        lim = (current == YANG_MAX_ELEMENTS) ? &((struct lysp_node_list *)dest)->max : &((struct lysp_node_list *)dest)->min;
        flags = &((struct lysp_node_list *)dest)->flags;
        exts = &((struct lysp_node_list *)dest)->exts;
    } else {
        lim = ((struct minmax_dev_meta *)dest)->lim;
        flags = ((struct minmax_dev_meta *)dest)->flags;
        exts = ((struct minmax_dev_meta *)dest)->exts;
    }

    if (current == YANG_MAX_ELEMENTS) {
        LY_CHECK_RET(yin_parse_maxelements(ctx, attrs, data, lim, flags, exts));
    } else {
        LY_CHECK_RET(yin_parse_minelements(ctx, attrs, data, lim, flags, exts));
    }

    return LY_SUCCESS;
}

/**
 * @brief Parser ordered-by element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[out] flags Flags to write to.
 * @param[in,out] exts Extension instance to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_orderedby(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                    uint16_t *flags, struct lysp_ext_instance **exts)
{
    const char *temp_val;
    struct yin_subelement subelems[1] = {
                                            {YANG_CUSTOM, NULL, 0},
                                        };

    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_VALUE, &temp_val, Y_STR_ARG, YANG_ORDERED_BY));
    if (strcmp(temp_val, "system") == 0) {
        *flags |= LYS_ORDBY_SYSTEM;
    } else if (strcmp(temp_val, "user") == 0) {
        *flags |= LYS_ORDBY_USER;
    } else {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN VALID_VALS2, temp_val, "value",
                       "ordered-by", "system", "user");
        FREE_STRING(ctx->xml_ctx.ctx, temp_val);
        return LY_EVALID;
    }
    FREE_STRING(ctx->xml_ctx.ctx, temp_val);

    return yin_parse_content(ctx, subelems, 1, data, YANG_ORDERED_BY, NULL, exts);
}

/**
 * @brief parse any-data or any-xml element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] any_kw Identification of current element, can be set to YANG_ANY_DATA or YANG_ANY_XML
 * @param[in] node_meta Meta information about parent node and siblings to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_any(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
              enum yang_keyword any_kw, struct tree_node_meta *node_meta)
{
    struct lysp_node_anydata *any;

    /* create new sibling */
    LY_LIST_NEW_RET(ctx->xml_ctx.ctx, node_meta->siblings, any, next);
    any->nodetype = (any_kw == YANG_ANYDATA) ? LYS_ANYDATA : LYS_ANYXML;
    any->parent = node_meta->parent;

    /* parser argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &any->name, Y_IDENTIF_ARG, any_kw));

    struct yin_subelement subelems[9] = {
                                            {YANG_CONFIG, &any->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_DESCRIPTION, &any->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_IF_FEATURE, &any->iffeatures, 0},
                                            {YANG_MANDATORY, &any->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_MUST, &any->musts, 0},
                                            {YANG_REFERENCE, &any->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_STATUS, &any->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_WHEN, &any->when, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0},
                                        };
    return yin_parse_content(ctx, subelems, 9, data, any_kw, NULL, &any->exts);
}

/**
 * @brief parse leaf element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] node_meta Meta information about parent node and siblings to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_leaf(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
               struct tree_node_meta *node_meta)
{
    struct lysp_node_leaf *leaf;

    /* create structure new leaf */
    LY_LIST_NEW_RET(ctx->xml_ctx.ctx, node_meta->siblings, leaf, next);
    leaf->nodetype = LYS_LEAF;
    leaf->parent = node_meta->parent;

    /* parser argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &leaf->name, Y_IDENTIF_ARG, YANG_LEAF));

    /* parse content */
    struct yin_subelement subelems[12] = {
                                            {YANG_CONFIG, &leaf->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_DEFAULT, &leaf->dflt, YIN_SUBELEM_UNIQUE},
                                            {YANG_DESCRIPTION, &leaf->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_IF_FEATURE, &leaf->iffeatures, 0},
                                            {YANG_MANDATORY, &leaf->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_MUST, &leaf->musts, 0},
                                            {YANG_REFERENCE, &leaf->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_STATUS, &leaf->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_TYPE, &leaf->type, YIN_SUBELEM_UNIQUE | YIN_SUBELEM_MANDATORY},
                                            {YANG_UNITS, &leaf->units, YIN_SUBELEM_UNIQUE},
                                            {YANG_WHEN, &leaf->when, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0},
                                         };
    return yin_parse_content(ctx, subelems, 12, data, YANG_LEAF, NULL, &leaf->exts);
}

/**
 * @brief Parse leaf-list element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] node_meta Meta information about parent node and siblings to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_leaflist(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                   struct tree_node_meta *node_meta)
{
    struct lysp_node_leaflist *llist;

    LY_LIST_NEW_RET(ctx->xml_ctx.ctx, node_meta->siblings, llist, next);

    llist->nodetype = LYS_LEAFLIST;
    llist->parent = node_meta->parent;

    /* parse argument */
    yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &llist->name, Y_IDENTIF_ARG, YANG_LEAF_LIST);

    /* parse content */
    struct yin_subelement subelems[14] = {
                                            {YANG_CONFIG, &llist->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_DEFAULT, &llist->dflts, 0},
                                            {YANG_DESCRIPTION, &llist->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_IF_FEATURE, &llist->iffeatures, 0},
                                            {YANG_MAX_ELEMENTS, llist, YIN_SUBELEM_UNIQUE},
                                            {YANG_MIN_ELEMENTS, llist, YIN_SUBELEM_UNIQUE},
                                            {YANG_MUST, &llist->musts, 0},
                                            {YANG_ORDERED_BY, &llist->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_REFERENCE, &llist->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_STATUS, &llist->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_TYPE, &llist->type, YIN_SUBELEM_UNIQUE | YIN_SUBELEM_MANDATORY},
                                            {YANG_UNITS, &llist->units, YIN_SUBELEM_UNIQUE},
                                            {YANG_WHEN, &llist->when, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0},
                                        };
    LY_CHECK_RET(yin_parse_content(ctx, subelems, 14, data, YANG_LEAF_LIST, NULL, &llist->exts));

    /* invalid combination of subelements */
    if ((llist->min) && (llist->dflts)) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INCHILDSTMSCOMB_YIN, "min-elements", "default", "leaf-list");
        return LY_EVALID;
    }
    if (llist->max && llist->min > llist->max) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_MINMAX, llist->min, llist->max);
        return LY_EVALID;
    }

    return LY_SUCCESS;
}

/**
 * @brief Parse typedef element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] typedef_meta Meta information about parent node and typedefs to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_typedef(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                  struct tree_node_meta *typedef_meta)
{
    struct lysp_tpdf *tpdf;
    struct lysp_tpdf **tpdfs = (struct lysp_tpdf **)typedef_meta->siblings;
    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *tpdfs, tpdf, LY_EMEM);

    /* parse argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &tpdf->name, Y_IDENTIF_ARG, YANG_TYPEDEF));

    /* parse content */
    struct yin_subelement subelems[7] = {
                                            {YANG_DEFAULT, &tpdf->dflt, YIN_SUBELEM_UNIQUE},
                                            {YANG_DESCRIPTION, &tpdf->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_REFERENCE, &tpdf->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_STATUS, &tpdf->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_TYPE, &tpdf->type, YIN_SUBELEM_UNIQUE | YIN_SUBELEM_MANDATORY},
                                            {YANG_UNITS, &tpdf->units, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0},
                                        };
    LY_CHECK_RET(yin_parse_content(ctx, subelems, 7, data, YANG_TYPEDEF, NULL, &tpdf->exts));

    /* store data for collision check */
    if (typedef_meta->parent && !(typedef_meta->parent->nodetype & (LYS_GROUPING | LYS_ACTION | LYS_INOUT | LYS_NOTIF))) {
        LY_CHECK_RET(ly_set_add(&ctx->tpdfs_nodes, typedef_meta->parent, 0) == -1, LY_EMEM);
    }

    return LY_SUCCESS;
}

/**
 * @brief Parse refine element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] refines Refines to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_refine(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                 struct lysp_refine **refines)
{
    struct lysp_refine *rf;

    /* allocate new refine */
    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *refines, rf, LY_EMEM);

    /* parse attribute */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_TARGET_NODE, &rf->nodeid, Y_STR_ARG, YANG_REFINE));
    YANG_CHECK_NONEMPTY((struct lys_parser_ctx *)ctx, strlen(rf->nodeid), "refine");

    /* parse content */
    struct yin_subelement subelems[11] = {
                                            {YANG_CONFIG, &rf->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_DEFAULT, &rf->dflts, 0},
                                            {YANG_DESCRIPTION, &rf->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_IF_FEATURE, &rf->iffeatures, 0},
                                            {YANG_MANDATORY, &rf->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_MAX_ELEMENTS, rf, YIN_SUBELEM_UNIQUE},
                                            {YANG_MIN_ELEMENTS, rf, YIN_SUBELEM_UNIQUE},
                                            {YANG_MUST, &rf->musts, 0},
                                            {YANG_PRESENCE, &rf->presence, YIN_SUBELEM_UNIQUE},
                                            {YANG_REFERENCE, &rf->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0},
                                        };
    return yin_parse_content(ctx, subelems, 11, data, YANG_REFINE, NULL, &rf->exts);
}

/**
 * @brief Parse uses element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] node_meta Meta information about parent node and siblings to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_uses(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
               struct tree_node_meta *node_meta)
{
    struct lysp_node_uses *uses;

    /* create new uses */
    LY_LIST_NEW_RET(ctx->xml_ctx.ctx, node_meta->siblings, uses, next);
    uses->nodetype = LYS_USES;
    uses->parent = node_meta->parent;

    /* parse argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &uses->name, Y_PREF_IDENTIF_ARG, YANG_USES));

    /* parse content */
    struct tree_node_meta augments = {(struct lysp_node *)uses, (struct lysp_node **)&uses->augments};
    struct yin_subelement subelems[8] = {
                                            {YANG_AUGMENT, &augments, 0},
                                            {YANG_DESCRIPTION, &uses->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_IF_FEATURE, &uses->iffeatures, 0},
                                            {YANG_REFERENCE, &uses->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_REFINE, &uses->refines, 0},
                                            {YANG_STATUS, &uses->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_WHEN, &uses->when, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0},
                                        };
    LY_CHECK_RET(yin_parse_content(ctx, subelems, 8, data, YANG_USES, NULL, &uses->exts));
    LY_CHECK_RET(lysp_parse_finalize_reallocated((struct lys_parser_ctx *)ctx, NULL, uses->augments, NULL, NULL));

    return LY_SUCCESS;
}

/**
 * @brief Parse revision element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] revs Parsed revisions to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_revision(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                   struct lysp_revision **revs)
{
    struct lysp_revision *rev;
    const char *temp_date = NULL;

    /* allocate new reivison */
    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *revs, rev, LY_EMEM);

    /* parse argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_DATE, &temp_date, Y_STR_ARG, YANG_REVISION));
    /* check value */
    if (lysp_check_date((struct lys_parser_ctx *)ctx, temp_date, strlen(temp_date), "revision")) {
        FREE_STRING(ctx->xml_ctx.ctx, temp_date);
        return LY_EVALID;
    }
    strcpy(rev->date, temp_date);
    FREE_STRING(ctx->xml_ctx.ctx, temp_date);

    /* parse content */
    struct yin_subelement subelems[3] = {
                                            {YANG_DESCRIPTION, &rev->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_REFERENCE, &rev->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0},
                                        };
    return yin_parse_content(ctx, subelems, 3, data, YANG_REVISION, NULL, &rev->exts);
}

/**
 * @brief Parse include element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] inc_meta Meta informatinou about module/submodule name and includes to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_include(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                  struct include_meta *inc_meta)
{
    struct lysp_include *inc;

    /* allocate new include */
    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *inc_meta->includes, inc, LY_EMEM);

    /* parse argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_MODULE, &inc->name, Y_IDENTIF_ARG, YANG_INCLUDE));

    /* submodules share the namespace with the module names, so there must not be
     * a module of the same name in the context, no need for revision matching */
    if (!strcmp(inc_meta->name, inc->name) || ly_ctx_get_module_latest(ctx->xml_ctx.ctx, inc->name)) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_NAME_COL, inc->name);
        return LY_EVALID;
    }

    /* parse content */
    struct yin_subelement subelems[4] = {
                                            {YANG_DESCRIPTION, &inc->dsc, YIN_SUBELEM_UNIQUE | YIN_SUBELEM_VER2},
                                            {YANG_REFERENCE, &inc->ref, YIN_SUBELEM_UNIQUE | YIN_SUBELEM_VER2},
                                            {YANG_REVISION_DATE, &inc->rev, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0},
                                        };
    return yin_parse_content(ctx, subelems, 4, data, YANG_INCLUDE, NULL, &inc->exts);
}

/**
 * @brief Parse revision date element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of revision-date element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] rev Array to store the parsed value in.
 * @param[in,out] exts Extension instances to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_revision_date(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, char *rev,
                        struct lysp_ext_instance **exts)
{
    const char *temp_rev;
    struct yin_subelement subelems[1] = {
                                            {YANG_CUSTOM, NULL, 0}
                                        };

    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_DATE, &temp_rev, Y_STR_ARG, YANG_REVISION_DATE));
    LY_CHECK_ERR_RET(lysp_check_date((struct lys_parser_ctx *)ctx, temp_rev, strlen(temp_rev), "revision-date") != LY_SUCCESS,
                     FREE_STRING(ctx->xml_ctx.ctx, temp_rev), LY_EVALID);

    strcpy(rev, temp_rev);
    FREE_STRING(ctx->xml_ctx.ctx, temp_rev);

    return yin_parse_content(ctx, subelems, 1, data, YANG_REVISION_DATE, NULL, exts);
}

/**
 * @brief Parse config element.
 *
 * @param[in] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of import element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] flags Flags to add to.
 * @param[in,out] exts Extension instances to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_config(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, uint16_t *flags,
                 struct lysp_ext_instance **exts)
{
    const char *temp_val = NULL;
    struct yin_subelement subelems[1] = {
                                            {YANG_CUSTOM, NULL, 0}
                                        };

    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_VALUE, &temp_val, Y_STR_ARG, YANG_CONFIG));
    if (strcmp(temp_val, "true") == 0) {
        *flags |= LYS_CONFIG_W;
    } else if (strcmp(temp_val, "false") == 0) {
        *flags |= LYS_CONFIG_R;
    } else {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN VALID_VALS2, temp_val, "value", "config",
                       "true", "false");
        FREE_STRING(ctx->xml_ctx.ctx, temp_val);
        return LY_EVALID;
    }
    FREE_STRING(ctx->xml_ctx.ctx, temp_val);

    return yin_parse_content(ctx, subelems, 1, data, YANG_CONFIG, NULL, exts);
}

/**
 * @brief Parse yang-version element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of yang-version element.
 * @param[in] data Data to read from, always moved to currently handled character.
 * @param[out] version Storage for the parsed information.
 * @param[in,out] exts Extension instance to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_yangversion(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, uint8_t *version,
                      struct lysp_ext_instance **exts)
{
    const char *temp_version = NULL;
    struct yin_subelement subelems[1] = {
                                            {YANG_CUSTOM, NULL, 0}
                                        };

    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_VALUE, &temp_version, Y_STR_ARG, YANG_YANG_VERSION));
    if (strcmp(temp_version, "1.0") == 0) {
        *version = LYS_VERSION_1_0;
    } else if (strcmp(temp_version, "1.1") == 0) {
        *version = LYS_VERSION_1_1;
    } else {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN VALID_VALS2, temp_version, "value",
                       "yang-version", "1.0", "1.1");
        FREE_STRING(ctx->xml_ctx.ctx, temp_version);
        return LY_EVALID;
    }
    FREE_STRING(ctx->xml_ctx.ctx, temp_version);
    ctx->mod_version = *version;

    return yin_parse_content(ctx, subelems, 1, data, YANG_YANG_VERSION, NULL, exts);
}

/**
 * @brief Parse import element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of import element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] imp_meta Meta information about prefix and imports to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_import(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, struct import_meta *imp_meta)
{
    struct lysp_import *imp;
    /* allocate new element in sized array for import */
    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *imp_meta->imports, imp, LY_EMEM);

    struct yin_subelement subelems[5] = {
                                            {YANG_DESCRIPTION, &imp->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_PREFIX, &imp->prefix, YIN_SUBELEM_MANDATORY | YIN_SUBELEM_UNIQUE},
                                            {YANG_REFERENCE, &imp->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_REVISION_DATE, imp->rev, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0}
                                        };

    /* parse import attributes */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_MODULE, &imp->name, Y_IDENTIF_ARG, YANG_IMPORT));
    LY_CHECK_RET(yin_parse_content(ctx, subelems, 5, data, YANG_IMPORT, NULL, &imp->exts));
    /* check prefix validity */
    LY_CHECK_RET(lysp_check_prefix((struct lys_parser_ctx *)ctx, *imp_meta->imports, imp_meta->prefix, &imp->prefix), LY_EVALID);

    return LY_SUCCESS;
}

/**
 * @brief Parse mandatory element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of status element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] flags Flags to add to.
 * @param[in,out] exts Extension instances to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_mandatory(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, uint16_t *flags,
                    struct lysp_ext_instance **exts)
{
    const char *temp_val = NULL;
    struct yin_subelement subelems[1] = {
                                            {YANG_CUSTOM, NULL, 0}
                                        };

    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_VALUE, &temp_val, Y_STR_ARG, YANG_MANDATORY));
    if (strcmp(temp_val, "true") == 0) {
        *flags |= LYS_MAND_TRUE;
    } else if (strcmp(temp_val, "false") == 0) {
        *flags |= LYS_MAND_FALSE;
    } else {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN VALID_VALS2, temp_val, "value",
                       "mandatory", "true", "false");
        FREE_STRING(ctx->xml_ctx.ctx, temp_val);
        return LY_EVALID;
    }
    FREE_STRING(ctx->xml_ctx.ctx, temp_val);

    return yin_parse_content(ctx, subelems, 1, data, YANG_MANDATORY, NULL, exts);
}

/**
 * @brief Parse status element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of status element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] flags Flags to add to.
 * @param[in,out] exts Extension instances to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_status(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, uint16_t *flags,
                 struct lysp_ext_instance **exts)
{
    const char *value = NULL;
    struct yin_subelement subelems[1] = {
                                            {YANG_CUSTOM, NULL, 0}
                                        };

    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_VALUE, &value, Y_STR_ARG, YANG_STATUS));
    if (strcmp(value, "current") == 0) {
        *flags |= LYS_STATUS_CURR;
    } else if (strcmp(value, "deprecated") == 0) {
        *flags |= LYS_STATUS_DEPRC;
    } else if (strcmp(value, "obsolete") == 0) {
        *flags |= LYS_STATUS_OBSLT;
    } else {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN VALID_VALS3, value, "value",
                       "status", "current", "deprecated", "obsolete");
        FREE_STRING(ctx->xml_ctx.ctx, value);
        return LY_EVALID;
    }
    FREE_STRING(ctx->xml_ctx.ctx, value);

    return yin_parse_content(ctx, subelems, 1, data, YANG_STATUS, NULL, exts);
}

/**
 * @brief Parse when element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of when element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[out] when_p When pointer to parse to.
 */
static LY_ERR
yin_parse_when(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, struct lysp_when **when_p)
{
    struct lysp_when *when;
    when = calloc(1, sizeof *when);
    LY_CHECK_ERR_RET(!when, LOGMEM(ctx->xml_ctx.ctx), LY_EMEM);
    yin_parse_attribute(ctx, attrs, YIN_ARG_CONDITION, &when->cond, Y_STR_ARG, YANG_WHEN);
    *when_p = when;
    struct yin_subelement subelems[3] = {
                                            {YANG_DESCRIPTION, &when->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_REFERENCE, &when->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0}
                                        };

    return yin_parse_content(ctx, subelems, 3, data, YANG_WHEN, NULL, &when->exts);
}

/**
 * @brief Parse yin-elemenet element.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of yin-element element.
 * @param[in,out] data Data to read from, always moved to currently handled position.
 * @param[in,out] flags Flags to add to.
 * @prama[in,out] exts Extension instance to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_yin_element_element(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                              uint16_t *flags, struct lysp_ext_instance **exts)
{
    const char *temp_val = NULL;
    struct yin_subelement subelems[1] = {
                                            {YANG_CUSTOM, NULL, 0}
                                        };

    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_VALUE, &temp_val, Y_STR_ARG, YANG_YIN_ELEMENT));
    if (strcmp(temp_val, "true") == 0) {
        *flags |= LYS_YINELEM_TRUE;
    } else if (strcmp(temp_val, "false") == 0) {
        *flags |= LYS_YINELEM_FALSE;
    } else {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN VALID_VALS2, temp_val, "value",
                       "yin-element", "true", "false");
        FREE_STRING(ctx->xml_ctx.ctx, temp_val);
        return LY_EVALID;
    }
    FREE_STRING(ctx->xml_ctx.ctx, temp_val);

    return yin_parse_content(ctx, subelems, 1, data, YANG_YIN_ELEMENT, NULL, exts);
}

/**
 * @brief Parse argument element.
 *
 * @param[in,out] xml_ctx Xml context.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of argument element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] arg_meta Meta information about destionation af prased data.
 * @param[in,out] exts Extension instance to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_argument_element(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                           struct yin_argument_meta *arg_meta, struct lysp_ext_instance **exts)
{
    struct yin_subelement subelems[2] = {
                                            {YANG_YIN_ELEMENT, arg_meta->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0}
                                        };

    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, arg_meta->argument, Y_IDENTIF_ARG, YANG_ARGUMENT));

    return yin_parse_content(ctx, subelems, 2, data, YANG_ARGUMENT, NULL, exts);
}

/**
 * @brief Parse the extension statement.
 *
 * @param[in,out] ctx Yin parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of extension element.
 * @param[in,out] data Data to read from.
 * @param[in,out] extensions Extensions to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_extension(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, struct lysp_ext **extensions)
{
    struct lysp_ext *ex;
    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *extensions, ex, LY_EMEM);
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &ex->name, Y_IDENTIF_ARG, YANG_EXTENSION));

    struct yin_argument_meta arg_info = {&ex->flags, &ex->argument};
    struct yin_subelement subelems[5] = {
                                            {YANG_ARGUMENT, &arg_info, YIN_SUBELEM_UNIQUE},
                                            {YANG_DESCRIPTION, &ex->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_REFERENCE, &ex->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_STATUS, &ex->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0}
                                        };

    return yin_parse_content(ctx, subelems, 5, data, YANG_EXTENSION, NULL, &ex->exts);
}

/**
 * @brief Parse feature element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] features Features to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_feature(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                  struct lysp_feature **features)
{
    struct lysp_feature *feat;

    /* allocate new feature */
    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *features, feat, LY_EMEM);

    /* parse argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &feat->name, Y_IDENTIF_ARG, YANG_FEATURE));

    /* parse content */
    struct yin_subelement subelems[5] = {
                                            {YANG_DESCRIPTION, &feat->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_IF_FEATURE, &feat->iffeatures, 0},
                                            {YANG_REFERENCE, &feat->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_STATUS, &feat->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0},
                                        };
    return yin_parse_content(ctx, subelems, 5, data, YANG_FEATURE, NULL, &feat->exts);
}

/**
 * @brief Parse identity element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] identities Identities to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_identity(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                   struct lysp_ident **identities)
{
    struct lysp_ident *ident;

    /* allocate new identity */
    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *identities, ident, LY_EMEM);

    /* parse argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &ident->name, Y_IDENTIF_ARG, YANG_IDENTITY));

    /* parse content */
    struct yin_subelement subelems[6] = {
                                            {YANG_BASE, &ident->bases, 0},
                                            {YANG_DESCRIPTION, &ident->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_IF_FEATURE, &ident->iffeatures, YIN_SUBELEM_VER2},
                                            {YANG_REFERENCE, &ident->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_STATUS, &ident->flags, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0},
                                        };
    return yin_parse_content(ctx, subelems, 6, data, YANG_IDENTITY, NULL, &ident->exts);
}

/**
 * @brief Parse list element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] node_meta Meta information about parent node and siblings to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_list(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
               struct tree_node_meta *node_meta)
{
    struct lysp_node_list *list;
    LY_ERR ret = LY_SUCCESS;
    struct yin_subelement *subelems = NULL;

    LY_LIST_NEW_RET(ctx->xml_ctx.ctx, node_meta->siblings, list, next);
    list->nodetype = LYS_LIST;
    list->parent = node_meta->parent;

    /* parse argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &list->name, Y_IDENTIF_ARG, YANG_LIST));

    /* parse list content */
    LY_CHECK_RET(subelems_allocator(ctx, 25, (struct lysp_node *)list, &subelems,
                                        YANG_ACTION, &list->actions, 0,
                                        YANG_ANYDATA, &list->child, 0,
                                        YANG_ANYXML, &list->child, 0,
                                        YANG_CHOICE, &list->child, 0,
                                        YANG_CONFIG, &list->flags, YIN_SUBELEM_UNIQUE,
                                        YANG_CONTAINER, &list->child, 0,
                                        YANG_DESCRIPTION, &list->dsc, YIN_SUBELEM_UNIQUE,
                                        YANG_GROUPING, &list->groupings, 0,
                                        YANG_IF_FEATURE, &list->iffeatures, 0,
                                        YANG_KEY, &list->key, YIN_SUBELEM_UNIQUE,
                                        YANG_LEAF, &list->child, 0,
                                        YANG_LEAF_LIST, &list->child, 0,
                                        YANG_LIST, &list->child, 0,
                                        YANG_MAX_ELEMENTS, list, YIN_SUBELEM_UNIQUE,
                                        YANG_MIN_ELEMENTS, list, YIN_SUBELEM_UNIQUE,
                                        YANG_MUST, &list->musts, 0,
                                        YANG_NOTIFICATION, &list->notifs, 0,
                                        YANG_ORDERED_BY, &list->flags, YIN_SUBELEM_UNIQUE,
                                        YANG_REFERENCE, &list->ref, YIN_SUBELEM_UNIQUE,
                                        YANG_STATUS, &list->flags, YIN_SUBELEM_UNIQUE,
                                        YANG_TYPEDEF, &list->typedefs, 0,
                                        YANG_UNIQUE, &list->uniques, 0,
                                        YANG_USES, &list->child, 0,
                                        YANG_WHEN, &list->when, YIN_SUBELEM_UNIQUE,
                                        YANG_CUSTOM, NULL, 0
                                   ));
    ret = yin_parse_content(ctx, subelems, 25, data, YANG_LIST, NULL, &list->exts);
    subelems_deallocator(25, subelems);
    LY_CHECK_RET(ret);

    /* finalize parent pointers to the reallocated items */
    LY_CHECK_RET(lysp_parse_finalize_reallocated((struct lys_parser_ctx *)ctx, list->groupings, NULL, list->actions, list->notifs));

    if (list->max && list->min > list->max) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_MINMAX, list->min, list->max);
        return LY_EVALID;
    }

    return LY_SUCCESS;
}

/**
 * @brief Parse notification element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] notif_meta Meta information about parent node and notifications to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_notification(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                       struct tree_node_meta *notif_meta)
{
    struct lysp_notif *notif;
    struct lysp_notif **notifs = (struct lysp_notif **)notif_meta->siblings;
    LY_ERR ret = LY_SUCCESS;
    struct yin_subelement *subelems = NULL;

    /* allocate new notification */
    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *notifs, notif, LY_EMEM);
    notif->nodetype = LYS_NOTIF;
    notif->parent = notif_meta->parent;

    /* parse argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &notif->name, Y_IDENTIF_ARG, YANG_NOTIFICATION));

    /* parse notification content */
    LY_CHECK_RET(subelems_allocator(ctx, 16, (struct lysp_node *)notif, &subelems,
                                        YANG_ANYDATA, &notif->data, 0,
                                        YANG_ANYXML, &notif->data, 0,
                                        YANG_CHOICE, &notif->data, 0,
                                        YANG_CONTAINER, &notif->data, 0,
                                        YANG_DESCRIPTION, &notif->dsc, YIN_SUBELEM_UNIQUE,
                                        YANG_GROUPING, &notif->groupings, 0,
                                        YANG_IF_FEATURE, &notif->iffeatures, 0,
                                        YANG_LEAF, &notif->data, 0,
                                        YANG_LEAF_LIST, &notif->data, 0,
                                        YANG_LIST, &notif->data, 0,
                                        YANG_MUST, &notif->musts, YIN_SUBELEM_VER2,
                                        YANG_REFERENCE, &notif->ref, YIN_SUBELEM_UNIQUE,
                                        YANG_STATUS, &notif->flags, YIN_SUBELEM_UNIQUE,
                                        YANG_TYPEDEF, &notif->typedefs, 0,
                                        YANG_USES, &notif->data, 0,
                                        YANG_CUSTOM, NULL, 0
                                   ));

    ret = yin_parse_content(ctx, subelems, 16, data, YANG_NOTIFICATION, NULL, &notif->exts);
    subelems_deallocator(16, subelems);
    LY_CHECK_RET(ret);

    /* finalize parent pointers to the reallocated items */
    LY_CHECK_RET(lysp_parse_finalize_reallocated((struct lys_parser_ctx *)ctx, notif->groupings, NULL, NULL, NULL));

    return LY_SUCCESS;
}

/**
 * @brief Parse notification element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in,out] notif_meta Meta information about parent node and notifications to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_grouping(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                   struct tree_node_meta *gr_meta)
{
    struct lysp_grp *grp;
    struct lysp_grp **grps = (struct lysp_grp **)gr_meta->siblings;
    LY_ERR ret = LY_SUCCESS;
    struct yin_subelement *subelems = NULL;

    /* create new grouping */
    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *grps, grp, LY_EMEM);
    grp->nodetype = LYS_GROUPING;
    grp->parent = gr_meta->parent;

    /* parse argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &grp->name, Y_IDENTIF_ARG, YANG_GROUPING));

    /* parse grouping content */
    LY_CHECK_RET(subelems_allocator(ctx, 16, (struct lysp_node *)grp, &subelems,
                                        YANG_ACTION, &grp->actions, 0,
                                        YANG_ANYDATA, &grp->data, 0,
                                        YANG_ANYXML, &grp->data, 0,
                                        YANG_CHOICE, &grp->data, 0,
                                        YANG_CONTAINER, &grp->data, 0,
                                        YANG_DESCRIPTION, &grp->dsc, YIN_SUBELEM_UNIQUE,
                                        YANG_GROUPING, &grp->groupings, 0,
                                        YANG_LEAF, &grp->data, 0,
                                        YANG_LEAF_LIST, &grp->data, 0,
                                        YANG_LIST, &grp->data, 0,
                                        YANG_NOTIFICATION, &grp->notifs, 0,
                                        YANG_REFERENCE, &grp->ref, YIN_SUBELEM_UNIQUE,
                                        YANG_STATUS, &grp->flags, YIN_SUBELEM_UNIQUE,
                                        YANG_TYPEDEF, &grp->typedefs, 0,
                                        YANG_USES, &grp->data, 0,
                                        YANG_CUSTOM, NULL, 0
                                   ));
    ret = yin_parse_content(ctx, subelems, 16, data, YANG_GROUPING, NULL, &grp->exts);
    subelems_deallocator(16, subelems);
    LY_CHECK_RET(ret);

    /* finalize parent pointers to the reallocated items */
    LY_CHECK_RET(lysp_parse_finalize_reallocated((struct lys_parser_ctx *)ctx, grp->groupings, NULL, grp->actions, grp->notifs));

    return LY_SUCCESS;
}

/**
 * @brief Parse list element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] node_meta Meta information about parent node and siblings to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_container(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                    struct tree_node_meta *node_meta)
{
    struct lysp_node_container *cont;
    LY_ERR ret = LY_SUCCESS;
    struct yin_subelement *subelems = NULL;

    /* create new container */
    LY_LIST_NEW_RET(ctx->xml_ctx.ctx, node_meta->siblings, cont, next);
    cont->nodetype = LYS_CONTAINER;
    cont->parent = node_meta->parent;

    /* parse aegument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME,  &cont->name, Y_IDENTIF_ARG, YANG_CONTAINER));

    /* parse container content */
    LY_CHECK_RET(subelems_allocator(ctx, 21, (struct lysp_node *)cont, &subelems,
                                        YANG_ACTION, &cont->actions, YIN_SUBELEM_VER2,
                                        YANG_ANYDATA, &cont->child, YIN_SUBELEM_VER2,
                                        YANG_ANYXML, &cont->child, 0,
                                        YANG_CHOICE, &cont->child, 0,
                                        YANG_CONFIG, &cont->flags, YIN_SUBELEM_UNIQUE,
                                        YANG_CONTAINER, &cont->child, 0,
                                        YANG_DESCRIPTION, &cont->dsc, YIN_SUBELEM_UNIQUE,
                                        YANG_GROUPING, &cont->groupings, 0,
                                        YANG_IF_FEATURE, &cont->iffeatures, 0,
                                        YANG_LEAF, &cont->child, 0,
                                        YANG_LEAF_LIST, &cont->child, 0,
                                        YANG_LIST, &cont->child, 0,
                                        YANG_MUST, &cont->musts, 0,
                                        YANG_NOTIFICATION, &cont->notifs, YIN_SUBELEM_VER2,
                                        YANG_PRESENCE, &cont->presence, YIN_SUBELEM_UNIQUE,
                                        YANG_REFERENCE, &cont->ref, YIN_SUBELEM_UNIQUE,
                                        YANG_STATUS, &cont->flags, YIN_SUBELEM_UNIQUE,
                                        YANG_TYPEDEF, &cont->typedefs, 0,
                                        YANG_USES, &cont->child, 0,
                                        YANG_WHEN, &cont->when, YIN_SUBELEM_UNIQUE,
                                        YANG_CUSTOM, NULL, 0
                                   ));
    ret = yin_parse_content(ctx, subelems, 21, data, YANG_CONTAINER, NULL, &cont->exts);
    subelems_deallocator(21, subelems);
    LY_CHECK_RET(ret);

    LY_CHECK_RET(lysp_parse_finalize_reallocated((struct lys_parser_ctx *)ctx, cont->groupings, NULL, cont->actions, cont->notifs));

    return LY_SUCCESS;
}

/**
 * @brief Parse case element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] node_meta Meta information about parent node and siblings to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_case(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
               struct tree_node_meta *node_meta)
{
    struct lysp_node_case *cas;
    LY_ERR ret = LY_SUCCESS;
    struct yin_subelement *subelems = NULL;;

    /* create new case */
    LY_LIST_NEW_RET(ctx->xml_ctx.ctx, node_meta->siblings, cas, next);
    cas->nodetype = LYS_CASE;
    cas->parent = node_meta->parent;

    /* parse argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &cas->name, Y_IDENTIF_ARG, YANG_CASE));

    /* parse case content */
    LY_CHECK_RET(subelems_allocator(ctx, 14, (struct lysp_node *)cas, &subelems,
                                        YANG_ANYDATA, &cas->child, YIN_SUBELEM_VER2,
                                        YANG_ANYXML, &cas->child, 0,
                                        YANG_CHOICE, &cas->child, 0,
                                        YANG_CONTAINER, &cas->child, 0,
                                        YANG_DESCRIPTION, &cas->dsc, YIN_SUBELEM_UNIQUE,
                                        YANG_IF_FEATURE, &cas->iffeatures, 0,
                                        YANG_LEAF, &cas->child, 0,
                                        YANG_LEAF_LIST, &cas->child, 0,
                                        YANG_LIST, &cas->child, 0,
                                        YANG_REFERENCE, &cas->ref, YIN_SUBELEM_UNIQUE,
                                        YANG_STATUS, &cas->flags, YIN_SUBELEM_UNIQUE,
                                        YANG_USES, &cas->child, 0,
                                        YANG_WHEN, &cas->when, YIN_SUBELEM_UNIQUE,
                                        YANG_CUSTOM, NULL, 0
                                   ));
    ret = yin_parse_content(ctx, subelems, 14, data, YANG_CASE, NULL, &cas->exts);
    subelems_deallocator(14, subelems);

    return ret;
}

/**
 * @brief Parse choice element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] node_meta Meta information about parent node and siblings to add to.
 *
 * @return LY_ERR values.
 */
LY_ERR
yin_parse_choice(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                 struct tree_node_meta *node_meta)
{
    LY_ERR ret = LY_SUCCESS;
    struct yin_subelement *subelems = NULL;
    struct lysp_node_choice *choice;

    /* create new choice */
    LY_LIST_NEW_RET(ctx->xml_ctx.ctx, node_meta->siblings, choice, next);

    choice->nodetype = LYS_CHOICE;
    choice->parent = node_meta->parent;

    /* parse argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &choice->name, Y_IDENTIF_ARG, YANG_CHOICE));

    /* parse choice content */
    LY_CHECK_RET(subelems_allocator(ctx, 17, (struct lysp_node *)choice, &subelems,
                                        YANG_ANYDATA, &choice->child, YIN_SUBELEM_VER2,
                                        YANG_ANYXML, &choice->child, 0,
                                        YANG_CASE, &choice->child, 0,
                                        YANG_CHOICE, &choice->child, YIN_SUBELEM_VER2,
                                        YANG_CONFIG, &choice->flags, YIN_SUBELEM_UNIQUE,
                                        YANG_CONTAINER, &choice->child, 0,
                                        YANG_DEFAULT, &choice->dflt, YIN_SUBELEM_UNIQUE,
                                        YANG_DESCRIPTION, &choice->dsc, YIN_SUBELEM_UNIQUE,
                                        YANG_IF_FEATURE, &choice->iffeatures, 0,
                                        YANG_LEAF, &choice->child, 0,
                                        YANG_LEAF_LIST, &choice->child, 0,
                                        YANG_LIST, &choice->child, 0,
                                        YANG_MANDATORY, &choice->flags, YIN_SUBELEM_UNIQUE,
                                        YANG_REFERENCE, &choice->ref, YIN_SUBELEM_UNIQUE,
                                        YANG_STATUS, &choice->flags, YIN_SUBELEM_UNIQUE,
                                        YANG_WHEN, &choice->when, YIN_SUBELEM_UNIQUE,
                                        YANG_CUSTOM, NULL, 0
                                   ));
    ret = yin_parse_content(ctx, subelems, 17, data, YANG_CHOICE, NULL, &choice->exts);
    subelems_deallocator(17, subelems);
    return ret;
}

/**
 * @brief Parse input or output element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] inout_meta Meta information about parent node and siblings and input/output pointer to write to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_inout(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, enum yang_keyword inout_kw,
                struct inout_meta *inout_meta)
{
    LY_ERR ret = LY_SUCCESS;
    struct yin_subelement *subelems = NULL;

    /* initiate structure */
    inout_meta->inout_p->nodetype = (inout_kw == YANG_INPUT) ? LYS_INPUT : LYS_OUTPUT;
    inout_meta->inout_p->parent = inout_meta->parent;

    /* check attributes */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NONE, NULL, Y_MAYBE_STR_ARG, inout_kw));

    /* parser input/output content */
    LY_CHECK_RET(subelems_allocator(ctx, 12, (struct lysp_node *)inout_meta->inout_p, &subelems,
                                        YANG_ANYDATA, &inout_meta->inout_p->data, YIN_SUBELEM_VER2,
                                        YANG_ANYXML, &inout_meta->inout_p->data, 0,
                                        YANG_CHOICE, &inout_meta->inout_p->data, 0,
                                        YANG_CONTAINER, &inout_meta->inout_p->data, 0,
                                        YANG_GROUPING, &inout_meta->inout_p->groupings, 0,
                                        YANG_LEAF, &inout_meta->inout_p->data, 0,
                                        YANG_LEAF_LIST, &inout_meta->inout_p->data, 0,
                                        YANG_LIST, &inout_meta->inout_p->data, 0,
                                        YANG_MUST, &inout_meta->inout_p->musts, YIN_SUBELEM_VER2,
                                        YANG_TYPEDEF, &inout_meta->inout_p->typedefs, 0,
                                        YANG_USES, &inout_meta->inout_p->data, 0,
                                        YANG_CUSTOM, NULL, 0
                                   ));
    ret = yin_parse_content(ctx, subelems, 12, data, inout_kw, NULL, &inout_meta->inout_p->exts);
    subelems_deallocator(12, subelems);
    LY_CHECK_RET(ret);

    /* finalize parent pointers to the reallocated items */
    LY_CHECK_RET(lysp_parse_finalize_reallocated((struct lys_parser_ctx *)ctx, inout_meta->inout_p->groupings, NULL, NULL, NULL));

    return LY_SUCCESS;
}

/**
 * @brief Parse action element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] act_meta Meta information about parent node and actions to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_action(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                 struct tree_node_meta *act_meta)
{
    struct lysp_action *act, **acts = (struct lysp_action **)act_meta->siblings;
    LY_ERR ret = LY_SUCCESS;
    struct yin_subelement *subelems = NULL;

    /* create new action */
    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *acts, act, LY_EMEM);
    act->nodetype = LYS_ACTION;
    act->parent = act_meta->parent;

    /* parse argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_NAME, &act->name, Y_IDENTIF_ARG, YANG_ACTION));

    /* parse content */
    LY_CHECK_RET(subelems_allocator(ctx, 9, (struct lysp_node *)act, &subelems,
                                        YANG_DESCRIPTION, &act->dsc, YIN_SUBELEM_UNIQUE,
                                        YANG_GROUPING, &act->groupings, 0,
                                        YANG_IF_FEATURE, &act->iffeatures, 0,
                                        YANG_INPUT, &act->input, YIN_SUBELEM_UNIQUE,
                                        YANG_OUTPUT, &act->output, YIN_SUBELEM_UNIQUE,
                                        YANG_REFERENCE, &act->ref, YIN_SUBELEM_UNIQUE,
                                        YANG_STATUS, &act->flags, YIN_SUBELEM_UNIQUE,
                                        YANG_TYPEDEF, &act->typedefs, 0,
                                        YANG_CUSTOM, NULL, 0
                                   ));
    ret = (yin_parse_content(ctx, subelems, 9, data, YANG_ACTION, NULL, &act->exts));
    subelems_deallocator(9, subelems);
    LY_CHECK_RET(ret);

    LY_CHECK_RET(lysp_parse_finalize_reallocated((struct lys_parser_ctx *)ctx, act->groupings, NULL, NULL, NULL));

    return LY_SUCCESS;
}

/**
 * @brief Parse augment element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] aug_meta Meta information about parent node and augments to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_augment(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                  struct tree_node_meta *aug_meta)
{
    struct lysp_augment *aug;
    struct lysp_augment **augs = (struct lysp_augment **)aug_meta->siblings;
    LY_ERR ret = LY_SUCCESS;
    struct yin_subelement *subelems = NULL;

    /* create new augment */
    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *augs, aug, LY_EMEM);
    aug->nodetype = LYS_AUGMENT;
    aug->parent = aug_meta->parent;

    /* parse argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_TARGET_NODE, &aug->nodeid, Y_STR_ARG, YANG_AUGMENT));
    YANG_CHECK_NONEMPTY((struct lys_parser_ctx *)ctx, strlen(aug->nodeid), "augment");

    /* parser augment content */
    LY_CHECK_RET(subelems_allocator(ctx, 17, (struct lysp_node *)aug, &subelems,
                                        YANG_ACTION, &aug->actions, YIN_SUBELEM_VER2,
                                        YANG_ANYDATA, &aug->child, YIN_SUBELEM_VER2,
                                        YANG_ANYXML, &aug->child, 0,
                                        YANG_CASE, &aug->child, 0,
                                        YANG_CHOICE, &aug->child, 0,
                                        YANG_CONTAINER, &aug->child, 0,
                                        YANG_DESCRIPTION, &aug->dsc, YIN_SUBELEM_UNIQUE,
                                        YANG_IF_FEATURE, &aug->iffeatures, 0,
                                        YANG_LEAF, &aug->child, 0,
                                        YANG_LEAF_LIST, &aug->child, 0,
                                        YANG_LIST, &aug->child, 0,
                                        YANG_NOTIFICATION, &aug->notifs, YIN_SUBELEM_VER2,
                                        YANG_REFERENCE, &aug->ref, YIN_SUBELEM_UNIQUE,
                                        YANG_STATUS, &aug->flags, YIN_SUBELEM_UNIQUE,
                                        YANG_USES, &aug->child, 0,
                                        YANG_WHEN, &aug->when, YIN_SUBELEM_UNIQUE,
                                        YANG_CUSTOM, NULL, 0
                                   ));
    ret = yin_parse_content(ctx, subelems, 17, data, YANG_AUGMENT, NULL, &aug->exts);
    subelems_deallocator(17, subelems);
    LY_CHECK_RET(ret);

    LY_CHECK_RET(lysp_parse_finalize_reallocated((struct lys_parser_ctx *)ctx, NULL, NULL, aug->actions, aug->notifs));

    return LY_SUCCESS;
}

/**
 * @brief Parse deviate element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] deviates Deviates to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_deviate(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                  struct lysp_deviate **deviates)
{
    LY_ERR ret = LY_SUCCESS;
    uint8_t dev_mod;
    const char *temp_val;
    struct lysp_deviate *d;
    struct lysp_deviate_add *d_add = NULL;
    struct lysp_deviate_rpl *d_rpl = NULL;
    struct lysp_deviate_del *d_del = NULL;

    /* parse argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_VALUE, &temp_val, Y_STR_ARG, YANG_DEVIATE));

    if (strcmp(temp_val, "not-supported") == 0) {
        dev_mod = LYS_DEV_NOT_SUPPORTED;
    } else if (strcmp(temp_val, "add") == 0) {
        dev_mod = LYS_DEV_ADD;
    } else if (strcmp(temp_val, "replace") == 0) {
        dev_mod = LYS_DEV_REPLACE;
    } else if (strcmp(temp_val, "delete") == 0) {
        dev_mod = LYS_DEV_DELETE;
    } else {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INVAL_YIN VALID_VALS4, temp_val, "value", "deviate",
                       "not-supported", "add", "replace", "delete");
        FREE_STRING(ctx->xml_ctx.ctx, temp_val);
        return LY_EVALID;
    }
    FREE_STRING(ctx->xml_ctx.ctx, temp_val);

    if (dev_mod == LYS_DEV_NOT_SUPPORTED) {
        d = calloc(1, sizeof *d);
        LY_CHECK_ERR_RET(!d, LOGMEM(ctx->xml_ctx.ctx), LY_EMEM);
        struct yin_subelement subelems[1] = {
                                                {YANG_CUSTOM, NULL, 0}
                                            };
        ret = yin_parse_content(ctx, subelems, 1, data, YANG_DEVIATE, NULL, &d->exts);

    } else if (dev_mod == LYS_DEV_ADD) {
        d_add = calloc(1, sizeof *d_add);
        LY_CHECK_ERR_RET(!d_add, LOGMEM(ctx->xml_ctx.ctx), LY_EMEM);
        d = (struct lysp_deviate *)d_add;
        struct minmax_dev_meta min = {&d_add->min, &d_add->flags, &d_add->exts};
        struct minmax_dev_meta max = {&d_add->max, &d_add->flags, &d_add->exts};
        struct yin_subelement subelems[9] = {
                                                {YANG_CONFIG, &d_add->flags, YIN_SUBELEM_UNIQUE},
                                                {YANG_DEFAULT, &d_add->dflts, 0},
                                                {YANG_MANDATORY, &d_add->flags, YIN_SUBELEM_UNIQUE},
                                                {YANG_MAX_ELEMENTS, &max, YIN_SUBELEM_UNIQUE},
                                                {YANG_MIN_ELEMENTS, &min, YIN_SUBELEM_UNIQUE},
                                                {YANG_MUST, &d_add->musts, 0},
                                                {YANG_UNIQUE, &d_add->uniques, 0},
                                                {YANG_UNITS, &d_add->units, YIN_SUBELEM_UNIQUE},
                                                {YANG_CUSTOM, NULL, 0},
                                            };
        ret = yin_parse_content(ctx, subelems, 9, data, YANG_DEVIATE, NULL, &d_add->exts);

    } else if (dev_mod == LYS_DEV_REPLACE) {
        d_rpl = calloc(1, sizeof *d_rpl);
        LY_CHECK_ERR_RET(!d_rpl, LOGMEM(ctx->xml_ctx.ctx), LY_EMEM);
        d = (struct lysp_deviate *)d_rpl;
        struct minmax_dev_meta min = {&d_rpl->min, &d_rpl->flags, &d_rpl->exts};
        struct minmax_dev_meta max = {&d_rpl->max, &d_rpl->flags, &d_rpl->exts};
        struct yin_subelement subelems[8] = {
                                                {YANG_CONFIG, &d_rpl->flags, YIN_SUBELEM_UNIQUE},
                                                {YANG_DEFAULT, &d_rpl->dflt, YIN_SUBELEM_UNIQUE},
                                                {YANG_MANDATORY, &d_rpl->flags, YIN_SUBELEM_UNIQUE},
                                                {YANG_MAX_ELEMENTS, &max, YIN_SUBELEM_UNIQUE},
                                                {YANG_MIN_ELEMENTS, &min, YIN_SUBELEM_UNIQUE},
                                                {YANG_TYPE, &d_rpl->type, YIN_SUBELEM_UNIQUE},
                                                {YANG_UNITS, &d_rpl->units, YIN_SUBELEM_UNIQUE},
                                                {YANG_CUSTOM, NULL, 0},
                                            };
        ret = yin_parse_content(ctx, subelems, 8, data, YANG_DEVIATE, NULL, &d_rpl->exts);

    } else {
        d_del = calloc(1, sizeof *d_del);
        LY_CHECK_ERR_RET(!d_del, LOGMEM(ctx->xml_ctx.ctx), LY_EMEM);
        d = (struct lysp_deviate *)d_del;
        struct yin_subelement subelems[5] = {
                                                {YANG_DEFAULT, &d_del->dflts, 0},
                                                {YANG_MUST, &d_del->musts, 0},
                                                {YANG_UNIQUE, &d_del->uniques, 0},
                                                {YANG_UNITS, &d_del->units, YIN_SUBELEM_UNIQUE},
                                                {YANG_CUSTOM, NULL, 0},
                                            };
        ret = yin_parse_content(ctx, subelems, 5, data, YANG_DEVIATE, NULL, &d_del->exts);
    }
    LY_CHECK_GOTO(ret, cleanup);

    d->mod = dev_mod;
    /* insert into siblings */
    LY_LIST_INSERT(deviates, d, next);

    return ret;

cleanup:
    free(d);
    return ret;
}

/**
 * @brief Parse deviation element.
 *
 * @param[in,out] ctx YIN parser context for logging and to store current state.
 * @param[in] attrs [Sized array](@ref sizedarrays) of attributes of current element.
 * @param[in,out] data Data to read from, always moved to currently handled character.
 * @param[in] deviations Deviations to add to.
 *
 * @return LY_ERR values.
 */
static LY_ERR
yin_parse_deviation(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data,
                    struct lysp_deviation **deviations)
{
    struct lysp_deviation *dev;

    /* create new deviation */
    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *deviations, dev, LY_EMEM);

    /* parse argument */
    LY_CHECK_RET(yin_parse_attribute(ctx, attrs, YIN_ARG_TARGET_NODE, &dev->nodeid, Y_STR_ARG, YANG_DEVIATION));
    YANG_CHECK_NONEMPTY((struct lys_parser_ctx *)ctx, strlen(dev->nodeid), "deviation");
    struct yin_subelement subelems[4] = {
                                            {YANG_DESCRIPTION, &dev->dsc, YIN_SUBELEM_UNIQUE},
                                            {YANG_DEVIATE, &dev->deviates, YIN_SUBELEM_MANDATORY},
                                            {YANG_REFERENCE, &dev->ref, YIN_SUBELEM_UNIQUE},
                                            {YANG_CUSTOM, NULL, 0},
                                        };
    return yin_parse_content(ctx, subelems, 4, data, YANG_DEVIATION, NULL, &dev->exts);
}

/**
 * @brief Map keyword type to substatement info.
 *
 * @param[in] kw Keyword type.
 *
 * @return correct LYEXT_SUBSTMT information.
 */
static LYEXT_SUBSTMT
kw2lyext_substmt(enum yang_keyword kw)
{
    switch (kw) {
    case YANG_ARGUMENT:
        return LYEXT_SUBSTMT_ARGUMENT;
    case YANG_BASE:
        return LYEXT_SUBSTMT_BASE;
    case YANG_BELONGS_TO:
        return LYEXT_SUBSTMT_BELONGSTO;
    case YANG_CONTACT:
        return LYEXT_SUBSTMT_CONTACT;
    case YANG_DEFAULT:
        return LYEXT_SUBSTMT_DEFAULT;
    case YANG_DESCRIPTION:
        return LYEXT_SUBSTMT_DESCRIPTION;
    case YANG_ERROR_APP_TAG:
        return LYEXT_SUBSTMT_ERRTAG;
    case YANG_ERROR_MESSAGE:
        return LYEXT_SUBSTMT_ERRMSG;
    case YANG_KEY:
        return LYEXT_SUBSTMT_KEY;
    case YANG_NAMESPACE:
        return LYEXT_SUBSTMT_NAMESPACE;
    case YANG_ORGANIZATION:
        return LYEXT_SUBSTMT_ORGANIZATION;
    case YANG_PATH:
        return LYEXT_SUBSTMT_PATH;
    case YANG_PREFIX:
        return LYEXT_SUBSTMT_PREFIX;
    case YANG_PRESENCE:
        return LYEXT_SUBSTMT_PRESENCE;
    case YANG_REFERENCE:
        return LYEXT_SUBSTMT_REFERENCE;
    case YANG_REVISION_DATE:
        return LYEXT_SUBSTMT_REVISIONDATE;
    case YANG_UNITS:
        return LYEXT_SUBSTMT_UNITS;
    case YANG_VALUE:
        return LYEXT_SUBSTMT_VALUE;
    case YANG_YANG_VERSION:
        return LYEXT_SUBSTMT_VERSION;
    case YANG_MODIFIER:
        return LYEXT_SUBSTMT_MODIFIER;
    case YANG_REQUIRE_INSTANCE:
        return LYEXT_SUBSTMT_REQINSTANCE;
    case YANG_YIN_ELEMENT:
        return LYEXT_SUBSTMT_YINELEM;
    case YANG_CONFIG:
        return LYEXT_SUBSTMT_CONFIG;
    case YANG_MANDATORY:
        return LYEXT_SUBSTMT_MANDATORY;
    case YANG_ORDERED_BY:
        return LYEXT_SUBSTMT_ORDEREDBY;
    case YANG_STATUS:
        return LYEXT_SUBSTMT_STATUS;
    case YANG_FRACTION_DIGITS:
        return LYEXT_SUBSTMT_FRACDIGITS;
    case YANG_MAX_ELEMENTS:
        return LYEXT_SUBSTMT_MAX;
    case YANG_MIN_ELEMENTS:
        return LYEXT_SUBSTMT_MIN;
    case YANG_POSITION:
        return LYEXT_SUBSTMT_POSITION;
    case YANG_UNIQUE:
        return LYEXT_SUBSTMT_UNIQUE;
    case YANG_IF_FEATURE:
        return LYEXT_SUBSTMT_IFFEATURE;
    default:
        return LYEXT_SUBSTMT_SELF;
    }
}

/**
 * @brief map keyword to keyword-group.
 *
 * @param[in] ctx YIN parser context used for logging.
 * @param[in] kw Keyword that is child of module or submodule.
 * @param[out] group Group of keyword.
 *
 * @return LY_SUCCESS on success LY_EINT if kw can't be mapped to kw_group, should not happen if called correctly.
 */
static LY_ERR
kw2kw_group(struct yin_parser_ctx *ctx, enum yang_keyword kw, enum yang_module_stmt *group)
{
    switch (kw) {
        /* module header */
        case YANG_NONE:
        case YANG_NAMESPACE:
        case YANG_PREFIX:
        case YANG_BELONGS_TO:
        case YANG_YANG_VERSION:
            *group = Y_MOD_MODULE_HEADER;
            break;
        /* linkage */
        case YANG_INCLUDE:
        case YANG_IMPORT:
            *group = Y_MOD_LINKAGE;
            break;
        /* meta */
        case YANG_ORGANIZATION:
        case YANG_CONTACT:
        case YANG_DESCRIPTION:
        case YANG_REFERENCE:
            *group = Y_MOD_META;
            break;
        /* revision */
        case YANG_REVISION:
            *group = Y_MOD_REVISION;
            break;
        /* body */
        case YANG_ANYDATA:
        case YANG_ANYXML:
        case YANG_AUGMENT:
        case YANG_CHOICE:
        case YANG_CONTAINER:
        case YANG_DEVIATION:
        case YANG_EXTENSION:
        case YANG_FEATURE:
        case YANG_GROUPING:
        case YANG_IDENTITY:
        case YANG_LEAF:
        case YANG_LEAF_LIST:
        case YANG_LIST:
        case YANG_NOTIFICATION:
        case YANG_RPC:
        case YANG_TYPEDEF:
        case YANG_USES:
        case YANG_CUSTOM:
            *group = Y_MOD_BODY;
            break;
        default:
            LOGINT(ctx->xml_ctx.ctx);
            return LY_EINT;
    }

    return LY_SUCCESS;
}

/**
 * @brief Check if relative order of two keywords is valid.
 *
 * @param[in] ctx YIN parser context used for logging.
 * @param[in] kw Current keyword.
 * @param[in] next_kw Next keyword.
 * @param[in] parrent Identification of parrent element, can be se to to YANG_MODULE of YANG_SUBMODULE,
 *            because relative order is required only in module and submodule sub-elements, used for logging.
 *
 * @return LY_SUCCESS on succes and LY_EVALID if relative order is invalid.
 */
static LY_ERR
yin_check_relative_order(struct yin_parser_ctx *ctx, enum yang_keyword kw, enum yang_keyword next_kw, enum yang_keyword parrent)
{
    assert(parrent == YANG_MODULE || parrent == YANG_SUBMODULE);
    enum yang_module_stmt gr, next_gr;

    LY_CHECK_RET(kw2kw_group(ctx, kw, &gr));
    LY_CHECK_RET(kw2kw_group(ctx, next_kw, &next_gr));

    if (gr > next_gr) {
        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INORDER_YIN, ly_stmt2str(parrent), ly_stmt2str(next_kw), ly_stmt2str(kw));
        return LY_EVALID;
    }

    return LY_SUCCESS;
}

LY_ERR
yin_parse_content(struct yin_parser_ctx *ctx, struct yin_subelement *subelem_info, signed char subelem_info_size,
                  const char **data, enum yang_keyword current_element, const char **text_content, struct lysp_ext_instance **exts)
{
    LY_ERR ret = LY_SUCCESS;
    char *out = NULL;
    const char *prefix, *name;
    size_t out_len = 0, prefix_len, name_len;
    int dynamic = 0;
    struct yin_arg_record *attrs = NULL;
    enum yang_keyword kw = YANG_NONE, last_kw = YANG_NONE;
    struct yin_subelement *subelem = NULL;

    assert(is_ordered(subelem_info, subelem_info_size));

    if (ctx->xml_ctx.status == LYXML_ELEM_CONTENT) {
        ret = lyxml_get_string(&ctx->xml_ctx, data, &out, &out_len, &out, &out_len, &dynamic);
        /* current element has subelements as content */
        if (ret == LY_EINVAL) {
            while (ctx->xml_ctx.status == LYXML_ELEMENT) {
                ret = lyxml_get_element(&ctx->xml_ctx, data, &prefix, &prefix_len, &name, &name_len);
                LY_CHECK_GOTO(ret, cleanup);
                if (!name) {
                    /* end of current element reached */
                    break;
                }
                ret = yin_load_attributes(ctx, data, &attrs);
                LY_CHECK_GOTO(ret, cleanup);
                last_kw = kw;
                kw = yin_match_keyword(ctx, name, name_len, prefix, prefix_len, current_element);

                /* check if this element can be child of current element */
                subelem = get_record(kw, subelem_info_size, subelem_info);
                if (!subelem) {
                    if (current_element == YANG_DEVIATE && isdevsub(kw)) {
                        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INDEV_YIN, ly_stmt2str(kw));
                    } else {
                        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_UNEXP_SUBELEM, name_len, name, ly_stmt2str(current_element));
                    }
                    ret = LY_EVALID;
                    goto cleanup;
                }

                /* relative order is required only in module and submodule sub-elements */
                if (current_element == YANG_MODULE || current_element == YANG_SUBMODULE) {
                    ret = yin_check_relative_order(ctx, last_kw, kw, current_element);
                    LY_CHECK_GOTO(ret, cleanup);
                }

                /* flag check */
                if ((subelem->flags & YIN_SUBELEM_UNIQUE) && (subelem->flags & YIN_SUBELEM_PARSED)) {
                    /* subelement uniquenes */
                    LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_SUBELEM_REDEF, ly_stmt2str(kw), ly_stmt2str(current_element));
                    return LY_EVALID;
                }
                if (subelem->flags & YIN_SUBELEM_FIRST) {
                    /* subelement is supposed to be defined as first subelement */
                    ret = yin_check_subelem_first_constraint(ctx, subelem_info, subelem_info_size, current_element, subelem);
                    LY_CHECK_GOTO(ret, cleanup);
                }
                if (subelem->flags & YIN_SUBELEM_VER2) {
                    /* subelement is supported only in version 1.1 or higher */
                    if (ctx->mod_version < 2) {
                        LOGVAL_PARSER((struct lys_parser_ctx *)ctx, LY_VCODE_INSUBELEM2, ly_stmt2str(kw), ly_stmt2str(current_element));
                        ret = LY_EVALID;
                        goto cleanup;
                    }
                }
                /* note that element was parsed for easy uniqueness check in next iterations */
                subelem->flags |= YIN_SUBELEM_PARSED;

                switch (kw) {
                /* call responsible function */
                case YANG_CUSTOM:
                    ret = yin_parse_extension_instance(ctx, attrs, data, name2fullname(name, prefix_len),
                                                      namelen2fulllen(name_len, prefix_len),
                                                      kw2lyext_substmt(current_element),
                                                      (subelem->dest) ? *((uint32_t*)subelem->dest) : 0, exts);
                    break;
                case YANG_ACTION:
                case YANG_RPC:
                    ret = yin_parse_action(ctx, attrs, data, (struct tree_node_meta *)subelem->dest);
                    break;
                case YANG_ANYDATA:
                case YANG_ANYXML:
                    ret = yin_parse_any(ctx, attrs, data, kw, (struct tree_node_meta *)subelem->dest);
                    break;
                case YANG_ARGUMENT:
                    ret = yin_parse_argument_element(ctx, attrs, data, (struct yin_argument_meta *)subelem->dest, exts);
                    break;
                case YANG_AUGMENT:
                    ret = yin_parse_augment(ctx, attrs, data, (struct tree_node_meta *)subelem->dest);
                    break;
                case YANG_BASE:
                    ret = yin_parse_base(ctx, attrs, data, current_element, subelem->dest, exts);
                    break;
                case YANG_BELONGS_TO:
                    ret = yin_parse_belongs_to(ctx, attrs, data, (struct lysp_submodule *)subelem->dest, exts);
                    break;
                case YANG_BIT:
                    ret = yin_parse_bit(ctx, attrs, data, (struct lysp_type *)subelem->dest);
                    break;
                case YANG_CASE:
                    ret = yin_parse_case(ctx, attrs, data, (struct tree_node_meta *)subelem->dest);
                    break;
                case YANG_CHOICE:
                    ret = yin_parse_choice(ctx, attrs, data, (struct tree_node_meta *)subelem->dest);
                    break;
                case YANG_CONFIG:
                    ret = yin_parse_config(ctx, attrs, data, (uint16_t *)subelem->dest, exts);
                    break;
                case YANG_CONTACT:
                case YANG_DESCRIPTION:
                case YANG_ORGANIZATION:
                case YANG_REFERENCE:
                    ret = yin_parse_meta_element(ctx, attrs, data, kw, (const char **)subelem->dest, exts);
                    break;
                case YANG_CONTAINER:
                    ret = yin_parse_container(ctx, attrs, data, (struct tree_node_meta *)subelem->dest);
                    break;
                case YANG_DEFAULT:
                case YANG_ERROR_APP_TAG:
                case YANG_KEY:
                case YANG_PRESENCE:
                    ret = yin_parse_simple_elem(ctx, attrs, data, kw, subelem, YIN_ARG_VALUE, Y_STR_ARG, exts);
                    break;
                case YANG_DEVIATE:
                    ret = yin_parse_deviate(ctx, attrs, data, (struct lysp_deviate **)subelem->dest);
                    break;
                case YANG_DEVIATION:
                    ret = yin_parse_deviation(ctx, attrs, data, (struct lysp_deviation **)subelem->dest);
                    break;
                case YANG_ENUM:
                    ret = yin_parse_enum(ctx, attrs, data, (struct lysp_type *)subelem->dest);
                    break;
                case YANG_ERROR_MESSAGE:
                    ret = yin_parse_err_msg_element(ctx, attrs, data, (const char **)subelem->dest, exts);
                    break;
                case YANG_EXTENSION:
                    ret = yin_parse_extension(ctx, attrs, data, (struct lysp_ext **)subelem->dest);
                    break;
                case YANG_FEATURE:
                    ret = yin_parse_feature(ctx, attrs, data, (struct lysp_feature **)subelem->dest);
                    break;
                case YANG_FRACTION_DIGITS:
                    ret = yin_parse_fracdigits(ctx, attrs, data, (struct lysp_type *)subelem->dest);
                    break;
                case YANG_GROUPING:
                    ret = yin_parse_grouping(ctx, attrs, data, (struct tree_node_meta *)subelem->dest);
                    break;
                case YANG_IDENTITY:
                    ret = yin_parse_identity(ctx, attrs, data, (struct lysp_ident **)subelem->dest);
                    break;
                case YANG_IF_FEATURE:
                case YANG_UNITS:
                    ret = yin_parse_simple_elem(ctx, attrs, data, kw, subelem, YIN_ARG_NAME, Y_STR_ARG, exts);
                    break;
                case YANG_IMPORT:
                    ret = yin_parse_import(ctx, attrs, data, (struct import_meta *)subelem->dest);
                    break;
                case YANG_INCLUDE:
                    ret = yin_parse_include(ctx, attrs, data, (struct include_meta *)subelem->dest);
                    break;
                case YANG_INPUT:
                case YANG_OUTPUT:
                    ret = yin_parse_inout(ctx, attrs, data, kw, (struct inout_meta *)subelem->dest);
                    break;
                case YANG_LEAF:
                    ret = yin_parse_leaf(ctx, attrs, data, (struct tree_node_meta *)subelem->dest);
                    break;
                case YANG_LEAF_LIST:
                    ret = yin_parse_leaflist(ctx, attrs, data, (struct tree_node_meta *)subelem->dest);
                    break;
                case YANG_LENGTH:
                    ret = yin_parse_length(ctx, attrs, data, (struct lysp_type *)subelem->dest);
                    break;
                case YANG_LIST:
                    ret = yin_parse_list(ctx, attrs, data, (struct tree_node_meta *)subelem->dest);
                    break;
                case YANG_MANDATORY:
                    ret = yin_parse_mandatory(ctx, attrs, data, (uint16_t *)subelem->dest, exts);
                    break;
                case YANG_MAX_ELEMENTS:
                case YANG_MIN_ELEMENTS:
                    ret = yin_parse_minmax(ctx, attrs, data, current_element, kw, subelem->dest);
                    break;
                case YANG_MODIFIER:
                    ret = yin_parse_modifier(ctx, attrs, data, (const char **)subelem->dest, exts);
                    break;
                case YANG_MUST:
                    ret = yin_parse_must(ctx, attrs, data, (struct lysp_restr **)subelem->dest);
                    break;
                case YANG_NAMESPACE:
                    ret = yin_parse_simple_elem(ctx, attrs, data, kw, subelem, YIN_ARG_URI, Y_STR_ARG, exts);
                    break;
                case YANG_NOTIFICATION:
                    ret = yin_parse_notification(ctx, attrs, data, (struct tree_node_meta *)subelem->dest);
                    break;
                case YANG_ORDERED_BY:
                    ret = yin_parse_orderedby(ctx, attrs, data, (uint16_t *)subelem->dest, exts);
                    break;
                case YANG_PATH:
                    ret = yin_parse_path(ctx, attrs, data, kw, (struct lysp_type *)subelem->dest);
                    break;
                case YANG_PATTERN:
                    ret = yin_parse_pattern(ctx, attrs, data, (struct lysp_type *)subelem->dest);
                    break;
                case YANG_VALUE:
                case YANG_POSITION:
                    ret = yin_parse_value_pos_element(ctx, attrs, data, kw, (struct lysp_type_enum *)subelem->dest);
                    break;
                case YANG_PREFIX:
                    ret = yin_parse_simple_elem(ctx, attrs, data, kw, subelem, YIN_ARG_VALUE, Y_IDENTIF_ARG, exts);
                    break;
                case YANG_RANGE:
                    ret = yin_parse_range(ctx, attrs, data, (struct lysp_type *)subelem->dest);
                    break;
                case YANG_REFINE:
                    ret = yin_parse_refine(ctx, attrs, data, (struct lysp_refine **)subelem->dest);
                    break;
                case YANG_REQUIRE_INSTANCE:
                    ret = yin_pasrse_reqinstance(ctx, attrs, data, (struct lysp_type *)subelem->dest);
                    break;
                case YANG_REVISION:
                    ret = yin_parse_revision(ctx, attrs, data, (struct lysp_revision **)subelem->dest);
                    break;
                case YANG_REVISION_DATE:
                    ret = yin_parse_revision_date(ctx, attrs, data, (char *)subelem->dest, exts);
                    break;
                case YANG_STATUS:
                    ret = yin_parse_status(ctx, attrs, data, (uint16_t *)subelem->dest, exts);
                    break;
                case YANG_TYPE:
                    ret = yin_parse_type(ctx, attrs, data, current_element, subelem);
                    break;
                case YANG_TYPEDEF:
                    ret = yin_parse_typedef(ctx, attrs, data, (struct tree_node_meta *)subelem->dest);
                    break;
                case YANG_UNIQUE:
                    ret = yin_parse_simple_elem(ctx, attrs, data, kw, subelem, YIN_ARG_TAG, Y_STR_ARG, exts);
                    break;
                case YANG_USES:
                    ret = yin_parse_uses(ctx, attrs, data, (struct tree_node_meta *)subelem->dest);
                    break;
                case YANG_WHEN:
                    ret = yin_parse_when(ctx, attrs, data, (struct lysp_when **)subelem->dest);
                    break;
                case YANG_YANG_VERSION:
                    ret = yin_parse_yangversion(ctx, attrs, data, (uint8_t *)subelem->dest, exts);
                    break;
                case YANG_YIN_ELEMENT:
                    ret = yin_parse_yin_element_element(ctx, attrs, data, (uint16_t *)subelem->dest, exts);
                    break;
                case YIN_TEXT:
                case YIN_VALUE:
                    ret = yin_parse_content(ctx, NULL, 0, data, kw, (const char **)subelem->dest, NULL);
                    break;
                default:
                    LOGINT(ctx->xml_ctx.ctx);
                    return LY_EINT;
                }
                LY_CHECK_GOTO(ret, cleanup);
                FREE_ARRAY(ctx, attrs, free_arg_rec);
                attrs = NULL;
                subelem = NULL;
            }
        } else {
            /* elements with text or none content */
            /* save text content, if text_content isn't set, it's just ignored */
            /* no resources are allocated in this branch, no need to use cleanup label */
            LY_CHECK_RET(yin_validate_value(ctx, Y_STR_ARG, out, out_len));
            if (text_content) {
                if (dynamic) {
                    *text_content = lydict_insert_zc(ctx->xml_ctx.ctx, out);
                    if (!*text_content) {
                        free(out);
                        return LY_EMEM;
                    }
                } else {
                    if (out_len == 0) {
                        *text_content = lydict_insert(ctx->xml_ctx.ctx, "", 0);
                    } else {
                        *text_content = lydict_insert(ctx->xml_ctx.ctx, out, out_len);
                    }
                }
            }
            /* load closing element */
            LY_CHECK_RET(lyxml_get_element(&ctx->xml_ctx, data, &prefix, &prefix_len, &name, &name_len));
        }
    }
    /* mandatory subelemnts are checked only after whole element was succesfully parsed */
    LY_CHECK_RET(yin_check_subelem_mandatory_constraint(ctx, subelem_info, subelem_info_size, current_element));

cleanup:
    FREE_ARRAY(ctx, attrs, free_arg_rec);
    return ret;
}

LY_ERR
yin_parse_extension_instance(struct yin_parser_ctx *ctx, struct yin_arg_record *attrs, const char **data, const char *ext_name,
                             int ext_name_len, LYEXT_SUBSTMT subelem, uint32_t subelem_index, struct lysp_ext_instance **exts)
{
    LY_ERR ret = LY_SUCCESS;
    char *out;
    const char *name, *prefix;
    size_t out_len, prefix_len, name_len;
    int dynamic;
    struct lysp_ext_instance *e;
    struct lysp_stmt *last_subelem = NULL, *new_subelem = NULL;
    struct yin_arg_record *iter;

    LY_ARRAY_NEW_RET(ctx->xml_ctx.ctx, *exts, e, LY_EMEM);

    e->yin = 0;
    /* store name and insubstmt info */
    e->name = lydict_insert(ctx->xml_ctx.ctx, ext_name, ext_name_len);
    e->insubstmt = subelem;
    e->insubstmt_index = subelem_index;
    e->yin |= LYS_YIN;

    /* store attributes as subelements */
    LY_ARRAY_FOR_ITER(attrs, struct yin_arg_record, iter) {
        if (!iter->prefix) {
            new_subelem = calloc(1, sizeof(*new_subelem));
            if (!e->child) {
                e->child = new_subelem;
            } else {
                last_subelem->next = new_subelem;
            }
            last_subelem = new_subelem;

            last_subelem->flags |= LYS_YIN_ATTR;
            last_subelem->stmt = lydict_insert(ctx->xml_ctx.ctx, iter->name, iter->name_len);
            LY_CHECK_ERR_RET(!last_subelem->stmt, LOGMEM(ctx->xml_ctx.ctx), LY_EMEM);
            if (iter->dynamic_content) {
                last_subelem->arg = lydict_insert_zc(ctx->xml_ctx.ctx, iter->content);
                LY_CHECK_ERR_RET(!last_subelem->arg, LOGMEM(ctx->xml_ctx.ctx), LY_EMEM);
            } else {
                last_subelem->arg = lydict_insert(ctx->xml_ctx.ctx, iter->content, iter->content_len);
                LY_CHECK_ERR_RET(!last_subelem->arg, LOGMEM(ctx->xml_ctx.ctx), LY_EMEM);
            }
        }
    }

    /* parse subelements */
    if (ctx->xml_ctx.status == LYXML_ELEM_CONTENT) {
        ret = lyxml_get_string(&ctx->xml_ctx, data, &out, &out_len, &out, &out_len, &dynamic);
        if (ret == LY_EINVAL) {
            while (ctx->xml_ctx.status == LYXML_ELEMENT) {
                LY_CHECK_RET(lyxml_get_element(&ctx->xml_ctx, data, &prefix, &prefix_len, &name, &name_len));
                if (!name) {
                    /* end of extension instance reached */
                    break;
                }
                LY_CHECK_RET(yin_parse_element_generic(ctx, name, name_len, data, &new_subelem));
                if (!e->child) {
                    e->child = new_subelem;
                } else {
                    last_subelem->next = new_subelem;
                }
                last_subelem = new_subelem;
            }
        } else {
            /* save text content */
            if (dynamic) {
                e->argument = lydict_insert_zc(ctx->xml_ctx.ctx, out);
                if (!e->argument) {
                    free(out);
                    return LY_EMEM;
                }
            } else {
                e->argument = lydict_insert(ctx->xml_ctx.ctx, out, out_len);
                LY_CHECK_RET(!e->argument, LY_EMEM);
            }
            LY_CHECK_RET(lyxml_get_element(&ctx->xml_ctx, data, &prefix, &prefix_len, &name, &name_len));
            LY_CHECK_RET(name, LY_EINT);
        }
    }

    return LY_SUCCESS;
}

LY_ERR
yin_parse_element_generic(struct yin_parser_ctx *ctx, const char *name, size_t name_len, const char **data,
                          struct lysp_stmt **element)
{
    LY_ERR ret = LY_SUCCESS;
    const char *temp_prefix, *temp_name;
    char *out = NULL;
    size_t out_len, temp_name_len, temp_prefix_len, prefix_len;
    int dynamic;
    struct yin_arg_record *subelem_args = NULL;
    struct lysp_stmt *last = NULL, *new = NULL;

    /* allocate new structure for element */
    *element = calloc(1, sizeof(**element));
    (*element)->stmt = lydict_insert(ctx->xml_ctx.ctx, name, name_len);
    LY_CHECK_ERR_RET(!(*element)->stmt, LOGMEM(ctx->xml_ctx.ctx), LY_EMEM);

    last = (*element)->child;
    /* load attributes */
    while(ctx->xml_ctx.status == LYXML_ATTRIBUTE) {
        /* add new element to linked-list */
        new = calloc(1, sizeof(*last));
        LY_CHECK_ERR_GOTO(ret, LOGMEM(ctx->xml_ctx.ctx), err);
        if (!(*element)->child) {
            /* save first */
            (*element)->child = new;
        } else {
            last->next = new;
        }
        last = new;

        last->flags |= LYS_YIN_ATTR;
        ret = lyxml_get_attribute(&ctx->xml_ctx, data, &temp_prefix, &prefix_len, &temp_name, &temp_name_len);
        LY_CHECK_GOTO(ret, err);
        ret = lyxml_get_string(&ctx->xml_ctx, data, &out, &out_len, &out, &out_len, &dynamic);
        LY_CHECK_GOTO(ret, err);
        last->stmt = lydict_insert(ctx->xml_ctx.ctx, temp_name, temp_name_len);
        LY_CHECK_ERR_GOTO(!last->stmt, LOGMEM(ctx->xml_ctx.ctx); ret = LY_EMEM, err);
        /* attributes with prefix are ignored */
        if (!temp_prefix) {
            if (dynamic) {
                last->arg = lydict_insert_zc(ctx->xml_ctx.ctx, out);
                if (!last->arg) {
                    free(out);
                    LOGMEM(ctx->xml_ctx.ctx);
                    ret = LY_EMEM;
                    goto err;
                }
            } else {
                last->arg = lydict_insert(ctx->xml_ctx.ctx, out, out_len);
                LY_CHECK_ERR_GOTO(!last->arg, LOGMEM(ctx->xml_ctx.ctx); ret = LY_EMEM, err);
            }
        }
    }

    /* parse content of element */
    ret = lyxml_get_string(&ctx->xml_ctx, data, &out, &out_len, &out, &out_len, &dynamic);
    if (ret == LY_EINVAL) {
        while (ctx->xml_ctx.status == LYXML_ELEMENT) {
            /* parse subelements */
            ret = lyxml_get_element(&ctx->xml_ctx, data, &temp_prefix, &temp_prefix_len, &temp_name, &temp_name_len);
            LY_CHECK_GOTO(ret, err);
            if (!name) {
                /* end of element reached */
                break;
            }
            ret = yin_parse_element_generic(ctx, temp_name, temp_name_len, data, &last->next);
            LY_CHECK_GOTO(ret, err);
            last = last->next;
        }
    } else {
        /* save element content */
        if (out_len != 0) {
            if (dynamic) {
                (*element)->arg = lydict_insert_zc(ctx->xml_ctx.ctx, out);
                if (!(*element)->arg) {
                    free(out);
                    LOGMEM(ctx->xml_ctx.ctx);
                    ret = LY_EMEM;
                    goto err;
                }
            } else {
                (*element)->arg = lydict_insert(ctx->xml_ctx.ctx, out, out_len);
                LY_CHECK_ERR_GOTO(!(*element)->arg, LOGMEM(ctx->xml_ctx.ctx); ret = LY_EMEM, err);
            }
        }
        /* read closing tag */
        ret = lyxml_get_element(&ctx->xml_ctx, data, &temp_prefix, &prefix_len, &temp_name, &temp_name_len);
        LY_CHECK_GOTO(ret, err);
    }

    FREE_ARRAY(ctx, subelem_args, free_arg_rec);
    return LY_SUCCESS;

err:
    FREE_ARRAY(ctx, subelem_args, free_arg_rec);
    return ret;
}

LY_ERR
yin_parse_mod(struct yin_parser_ctx *ctx, struct yin_arg_record *mod_attrs, const char **data, struct lysp_module *mod)
{
    LY_ERR ret = LY_SUCCESS;
    struct yin_subelement *subelems = NULL;

    LY_CHECK_RET(yin_parse_attribute(ctx, mod_attrs, YIN_ARG_NAME, &mod->mod->name, Y_IDENTIF_ARG, YANG_MODULE));
    LY_CHECK_RET(subelems_allocator(ctx, 28, NULL, &subelems,
                                            YANG_ANYDATA, &mod->data, YIN_SUBELEM_VER2,
                                            YANG_ANYXML, &mod->data, 0,
                                            YANG_AUGMENT, &mod->augments, 0,
                                            YANG_CHOICE, &mod->data, 0,
                                            YANG_CONTACT, &mod->mod->contact, YIN_SUBELEM_UNIQUE,
                                            YANG_CONTAINER, &mod->data, 0,
                                            YANG_DESCRIPTION, &mod->mod->dsc, YIN_SUBELEM_UNIQUE,
                                            YANG_DEVIATION, &mod->deviations, 0,
                                            YANG_EXTENSION, &mod->extensions, 0,
                                            YANG_FEATURE, &mod->features, 0,
                                            YANG_GROUPING, &mod->groupings, 0,
                                            YANG_IDENTITY, &mod->identities, 0,
                                            YANG_IMPORT, mod->mod->prefix, &mod->imports, 0,
                                            YANG_INCLUDE, mod->mod->name, &mod->includes, 0,
                                            YANG_LEAF, &mod->data, 0,
                                            YANG_LEAF_LIST, &mod->data, 0,
                                            YANG_LIST, &mod->data, 0,
                                            YANG_NAMESPACE, &mod->mod->ns, YIN_SUBELEM_MANDATORY | YIN_SUBELEM_UNIQUE,
                                            YANG_NOTIFICATION, &mod->notifs, 0,
                                            YANG_ORGANIZATION, &mod->mod->org, YIN_SUBELEM_UNIQUE,
                                            YANG_PREFIX, &mod->mod->prefix, YIN_SUBELEM_MANDATORY | YIN_SUBELEM_UNIQUE,
                                            YANG_REFERENCE, &mod->mod->ref, YIN_SUBELEM_UNIQUE,
                                            YANG_REVISION, &mod->revs, 0,
                                            YANG_RPC, &mod->rpcs, 0,
                                            YANG_TYPEDEF, &mod->typedefs, 0,
                                            YANG_USES, &mod->data, 0,
                                            YANG_YANG_VERSION, &mod->mod->version, YIN_SUBELEM_MANDATORY | YIN_SUBELEM_UNIQUE,
                                            YANG_CUSTOM, NULL, 0
                                   ));

    ret = yin_parse_content(ctx, subelems, 28, data, YANG_MODULE, NULL, &mod->exts);
    subelems_deallocator(28, subelems);

    return ret;
}

LY_ERR
yin_parse_submod(struct yin_parser_ctx *ctx, struct yin_arg_record *mod_attrs, const char **data, struct lysp_submodule *submod)
{
    LY_ERR ret = LY_SUCCESS;
    struct yin_subelement *subelems = NULL;

    LY_CHECK_RET(yin_parse_attribute(ctx, mod_attrs, YIN_ARG_NAME, &submod->name, Y_IDENTIF_ARG, YANG_SUBMODULE));
    LY_CHECK_RET(subelems_allocator(ctx, 27, NULL, &subelems,
                                        YANG_ANYDATA, &submod->data, YIN_SUBELEM_VER2,
                                        YANG_ANYXML, &submod->data, 0,
                                        YANG_AUGMENT, &submod->augments, 0,
                                        YANG_BELONGS_TO, submod, YIN_SUBELEM_MANDATORY | YIN_SUBELEM_UNIQUE,
                                        YANG_CHOICE, &submod->data, 0,
                                        YANG_CONTACT, &submod->contact, YIN_SUBELEM_UNIQUE,
                                        YANG_CONTAINER, &submod->data, 0,
                                        YANG_DESCRIPTION, &submod->dsc, YIN_SUBELEM_UNIQUE,
                                        YANG_DEVIATION, &submod->deviations, 0,
                                        YANG_EXTENSION, &submod->extensions, 0,
                                        YANG_FEATURE, &submod->features, 0,
                                        YANG_GROUPING, &submod->groupings, 0,
                                        YANG_IDENTITY, &submod->identities, 0,
                                        YANG_IMPORT, submod->prefix, &submod->imports, 0,
                                        YANG_INCLUDE, submod->name, &submod->includes, 0,
                                        YANG_LEAF, &submod->data, 0,
                                        YANG_LEAF_LIST, &submod->data, 0,
                                        YANG_LIST, &submod->data, 0,
                                        YANG_NOTIFICATION, &submod->notifs, 0,
                                        YANG_ORGANIZATION, &submod->org, YIN_SUBELEM_UNIQUE,
                                        YANG_REFERENCE, &submod->ref, YIN_SUBELEM_UNIQUE,
                                        YANG_REVISION, &submod->revs, 0,
                                        YANG_RPC, &submod->rpcs, 0,
                                        YANG_TYPEDEF, &submod->typedefs, 0,
                                        YANG_USES, &submod->data, 0,
                                        YANG_YANG_VERSION, &submod->version, YIN_SUBELEM_MANDATORY | YIN_SUBELEM_UNIQUE,
                                        YANG_CUSTOM, NULL, 0
                                   ));

    ret = yin_parse_content(ctx, subelems, 27, data, YANG_SUBMODULE, NULL, &submod->exts);
    subelems_deallocator(27, subelems);

    return ret;
}

LY_ERR
yin_parse_submodule(struct yin_parser_ctx **yin_ctx, struct ly_ctx *ctx, struct lys_parser_ctx *main_ctx, const char *data, struct lysp_submodule **submod)
{
    enum yang_keyword kw = YANG_NONE;
    LY_ERR ret = LY_SUCCESS;
    const char *prefix, *name;
    size_t prefix_len, name_len;
    struct yin_arg_record *attrs = NULL;
    struct lysp_submodule *mod_p = NULL;

    /* create context */
    *yin_ctx = calloc(1, sizeof **yin_ctx);
    LY_CHECK_ERR_RET(!(*yin_ctx), LOGMEM(ctx), LY_EMEM);
    (*yin_ctx)->xml_ctx.ctx = ctx;
    (*yin_ctx)->xml_ctx.line = 1;

    /* map the typedefs and groupings list from main context to the submodule's context */
    memcpy(&(*yin_ctx)->tpdfs_nodes, &main_ctx->tpdfs_nodes, sizeof main_ctx->tpdfs_nodes);
    memcpy(&(*yin_ctx)->grps_nodes, &main_ctx->grps_nodes, sizeof main_ctx->grps_nodes);

    /* check submodule */
    ret = lyxml_get_element(&(*yin_ctx)->xml_ctx, &data, &prefix, &prefix_len, &name, &name_len);
    LY_CHECK_GOTO(ret, cleanup);
    ret = yin_load_attributes(*yin_ctx, &data, &attrs);
    LY_CHECK_GOTO(ret, cleanup);
    kw = yin_match_keyword(*yin_ctx, name, name_len, prefix, prefix_len, YANG_NONE);

    if (kw == YANG_MODULE) {
        LOGERR(ctx, LY_EDENIED, "Input data contains module in situation when a submodule is expected.");
        ret = LY_EINVAL;
        goto cleanup;
    } else if (kw != YANG_SUBMODULE) {
        LOGVAL_PARSER((struct lys_parser_ctx *)*yin_ctx, LY_VCODE_MOD_SUBOMD, ly_stmt2str(kw));
        ret = LY_EVALID;
        goto cleanup;
    }

    mod_p = calloc(1, sizeof *mod_p);
    LY_CHECK_ERR_GOTO(!mod_p, LOGMEM(ctx), cleanup);
    mod_p->parsing = 1;

    ret = yin_parse_submod(*yin_ctx, attrs, &data, mod_p);
    LY_CHECK_GOTO(ret, cleanup);

    name = NULL;
    if ((*yin_ctx)->xml_ctx.status == LYXML_ELEMENT) {
        const char *temp_data = data;
        ret = lyxml_get_element(&(*yin_ctx)->xml_ctx, &data, &prefix, &prefix_len, &name, &name_len);
        data = temp_data;
    }
    if ((*yin_ctx)->xml_ctx.status != LYXML_END || name) {
        LOGVAL_PARSER((struct lys_parser_ctx *)*yin_ctx, LY_VCODE_TRAILING_SUBMOD, 15, data, strlen(data) > 15 ? "..." : "");
        ret = LY_EVALID;
        goto cleanup;
    }

    mod_p->parsing = 0;
    *submod = mod_p;

cleanup:
    if (ret) {
        lysp_submodule_free(ctx, mod_p);
        yin_parser_ctx_free(*yin_ctx);
        *yin_ctx = NULL;
    }

    FREE_ARRAY(*yin_ctx, attrs, free_arg_rec);
    return ret;
}

LY_ERR
yin_parse_module(struct yin_parser_ctx **yin_ctx, const char *data, struct lys_module *mod)
{
    LY_ERR ret = LY_SUCCESS;
    enum yang_keyword kw = YANG_NONE;
    struct lysp_module *mod_p = NULL;
    const char *prefix, *name;
    size_t prefix_len, name_len;
    struct yin_arg_record *attrs = NULL;

    /* create context */
    *yin_ctx = calloc(1, sizeof **yin_ctx);
    LY_CHECK_ERR_RET(!(*yin_ctx), LOGMEM(mod->ctx), LY_EMEM);
    (*yin_ctx)->xml_ctx.ctx = mod->ctx;
    (*yin_ctx)->xml_ctx.line = 1;

    /* check module */
    ret = lyxml_get_element(&(*yin_ctx)->xml_ctx, &data, &prefix, &prefix_len, &name, &name_len);
    LY_CHECK_GOTO(ret, cleanup);
    ret = yin_load_attributes(*yin_ctx, &data, &attrs);
    LY_CHECK_GOTO(ret, cleanup);
    kw = yin_match_keyword(*yin_ctx, name, name_len, prefix, prefix_len, YANG_NONE);
    if (kw == YANG_SUBMODULE) {
        LOGERR(mod->ctx, LY_EDENIED, "Input data contains submodule which cannot be parsed directly without its main module.");
        ret = LY_EINVAL;
        goto cleanup;
    } else if (kw != YANG_MODULE) {
        LOGVAL_PARSER((struct lys_parser_ctx *)*yin_ctx, LY_VCODE_MOD_SUBOMD, ly_stmt2str(kw));
        ret = LY_EVALID;
        goto cleanup;
    }

    /* allocate module */
    mod_p = calloc(1, sizeof *mod_p);
    LY_CHECK_ERR_GOTO(!mod_p, LOGMEM(mod->ctx), cleanup);
    mod_p->mod = mod;
    mod_p->parsing = 1;

    /* parse module substatements */
    ret = yin_parse_mod(*yin_ctx, attrs, &data, mod_p);
    LY_CHECK_GOTO(ret, cleanup);

    /* check trailing characters */
    if ((*yin_ctx)->xml_ctx.status == LYXML_ELEMENT) {
        ret = lyxml_get_element(&(*yin_ctx)->xml_ctx, &data, &prefix, &prefix_len, &name, &name_len);
    }
    if ((*yin_ctx)->xml_ctx.status != LYXML_END || name) {
        LOGVAL_PARSER((struct lys_parser_ctx *)*yin_ctx, LY_VCODE_TRAILING_MOD, 15, data, strlen(data) > 15 ? "..." : "");

        ret = LY_EVALID;
        goto cleanup;
    }

    mod_p->parsing = 0;
    mod->parsed = mod_p;

cleanup:
    if (ret != LY_SUCCESS) {
        lysp_module_free(mod_p);
        yin_parser_ctx_free(*yin_ctx);
        *yin_ctx = NULL;
    }
    FREE_ARRAY(*yin_ctx, attrs, free_arg_rec);
    return ret;
}
