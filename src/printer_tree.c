
/**
 * @file printer_tree.c
 * @author Adam Piecek <piecek@cesnet.cz>
 * @brief RFC tree printer for libyang data structure
 *
 * Copyright (c) 2015 - 2020 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "common.h"
#include "compat.h"
#include "context.h"
#include "dict.h"
#include "log.h"
#include "parser_data.h"
#include "plugins_types.h"
#include "printer_internal.h"
#include "tree.h"
#include "tree_schema.h"
#include "tree_schema_internal.h"
#include "xpath.h"
#include "out_internal.h"


/* -######-- Declarations start -#######- */

/*
 *          +---------+    +---------+    +---------+
 *   output |   trp   |    |   trb   |    |   tro   |
 *      <---+  Print  +<---+  Browse +<-->+  Obtain |
 *          |         |    |         |    |         |
 *          +---------+    +----+----+    +---------+
 *                              ^
 *                              |
 *                         +----+----+
 *                         |   trm   |
 *                         | Manager |
 *                         |         |
 *                         +----+----+
 *                              ^
 *                              | input
 *                              +
 */   


/* Glossary:
 * trt - type
 * trp - functions for Printing
 * trb - functions for Browse the tree
 * tro - functions for Obtaining information from libyang 
 * trm - Main functions, Manager
 * trg - General functions
 * TRC - constant that is not configurable
 * tmp - functions that will be removed
 */


struct trt_tree_ctx;
struct trt_getters;
struct trt_fp_modify_ctx;
struct trt_fp_read;
struct trt_fp_crawl;
struct trt_fp_print;

/**
 * @brief Last layer for printing.
 *
 * Variadic arguments are expected to be of the type char*.
 *
 * @param[in,out] out struct ly_out* or other auxiliary structure for printing.
 * @param[in] arg_count number of arguments in va_list.
 * @param[in] ap variadic argument list from <stdarg.h>.
 */
typedef void (*trt_print_func)(void *out, int arg_count, va_list ap);


/**
 * @brief Customizable structure for printing on any output.
 *
 * Structures trt_printing causes printing and trt_injecting_strlen does not print but is used for character counting.
 */
typedef struct
{
    void* out;              /**< Pointer to output data. Typical ly_out* or any c++ container in the case of testing. */
    trt_print_func pf;      /**< Pointer to function which takes void* out and do typically printing actions. */
    uint32_t cnt_linebreak; /**< Counter of printed line breaks. */
} trt_printing,
  trt_injecting_strlen;

/** Set .cnt_linebreak to zero. */
static void trp_cnt_linebreak_reset(trt_printing*);
/** Increment .cnt_linebreak by one. */
static void trp_cnt_linebreak_increment(trt_printing*);

/**
 * @brief Print variadic number of char* pointers.
 *
 * @param[in] p struct ly_out* or other auxiliary structure for printing.
 * @param[in] arg_count number of arguments in va_list.
 */
static void trp_print(trt_printing* p, int arg_count, ...);

/**
 * @brief Callback functions that print themselves without printer overhead
 *
 * This includes strings that cannot be printed using the char * pointer alone.
 * Instead, pieces of string are distributed in multiple locations in memory, so a function is needed to find and print them.
 *
 * Structures trt_cf_print_keys is for print a list's keys and trt_cf_print_iffeatures for list of features.
 */
typedef struct 
{
    const struct trt_tree_ctx* ctx;                         /**< Context of libyang tree. */
    void (*pf)(const struct trt_tree_ctx *, trt_printing*);  /**< Pointing to definition of printing e.g. keys or features. */
} trt_cf_print_keys,
  trt_cf_print_iffeatures;

/**
 * @brief Used for counting characters. Used in trt_injecting_strlen as void* out.
 */
typedef struct
{
    uint32_t bytes;
} trt_counter;

/**
 * @brief Counts the characters to be printed instead of printing.
 *
 * Used in trt_injecting_strlen as trt_print_func.
 *
 * @param[in,out] out it is expected to be type of trt_counter.
 * @param[in] arg_count number of arguments in va_list.
 */
static void trp_injected_strlen(void *out, int arg_count, va_list ap); 

/* ======================================= */
/* ----------- <Print getters> ----------- */
/* ======================================= */

/**
 * @brief Functions that provide printing themselves
 *
 * Functions must print including spaces or delimiters between names.
 */
struct trt_fp_print
{
    void (*print_features_names)(const struct trt_tree_ctx*, trt_printing*);   /**< Print list of features without {}? wrapper. */
    void (*print_keys)(const struct trt_tree_ctx*, trt_printing*);            /**< Print list's keys without [] wrapper. */
};

/**
 * @brief Package which only groups getter function.
 */
typedef struct
{
    const struct trt_tree_ctx* tree_ctx;    /**< Context of libyang tree. */
    struct trt_fp_print fps;                /**< Print function. */
} trt_pck_print;

/* ================================ */
/* ----------- <indent> ----------- */
/* ================================ */

/**
 * @brief Constants which are defined in the RFC or are observable from the pyang tool.
 */
typedef enum
{
    trd_indent_empty = 0,               /**< If the node is a case node, there is no space before the <name>. */
    trd_indent_long_line_break = 2,     /**< The new line should be indented so that it starts below <name> with a whitespace offset of at least two characters. */
    trd_indent_line_begin = 2,          /**< Indent below the keyword (module, augment ...).  */
    trd_indent_btw_siblings = 2,        /**< Between | and | characters. */
    trd_indent_before_keys = 1,         /**< <x>___<keys>. */
    trd_indent_before_type = 4,         /**< <x>___<type>, but if mark is set then indent == 3. */
    trd_indent_before_iffeatures = 1,   /**< <x>___<iffeatures>. */
} trt_cnf_indent;


/**
 * @brief Type of indent in node.
 */
typedef enum
{
    trd_indent_in_node_normal = 0,  /**< Node fits on one line. */
    trd_indent_in_node_divided,     /**< The node must be split into multiple rows. */
    trd_indent_in_node_failed       /**< Cannot be crammed into one line. The condition for the maximum line length is violated. */
} trt_indent_in_node_type;

/** Recording the number of gaps. */
typedef int16_t trt_indent_btw;

/** Constant to indicate the need to break a line. */
const trt_indent_btw trd_linebreak = -1;

/**
 * @brief Records the alignment between the individual elements of the node.
 */
typedef struct
{
    trt_indent_in_node_type type;       /**< Type of indent in node. */
    trt_indent_btw btw_name_opts;       /**< Indent between node name and opts. */
    trt_indent_btw btw_opts_type;       /**< Indent between opts and type. */
    trt_indent_btw btw_type_iffeatures; /**< Indent between type and features. Ignored if <type> missing. */
} trt_indent_in_node;

/** Check that they can be considered equivalent. */
static ly_bool trp_indent_in_node_are_eq(trt_indent_in_node, trt_indent_in_node);

/**
 * @brief Writing a linebreak constant.
 *
 * The order where the linebreak tag can be placed is from the end.
 *
 * @param[in] item containing alignment lengths or already line break marks
 * @return with a newly placed linebreak tag.
 * @return .type = trd_indent_in_node_failed if it is not possible to place a more line breaks.
 */
static trt_indent_in_node trp_indent_in_node_place_break(trt_indent_in_node item);

/**
 * @brief Type of wrappers to be printed.
 */
typedef enum 
{
    trd_wrapper_top = 0,    /**< Related to the module. */
    trd_wrapper_body        /**< Related to e.g. Augmentations or Groupings */
} trd_wrapper_type;

/**
 * @brief For resolving sibling symbol placement.
 *
 * Bit indicates where the sibling symbol must be printed.
 * This place is in multiples of trd_indent_btw_siblings.
 */
typedef struct
{
    trd_wrapper_type type;
    uint64_t bit_marks1;
    uint32_t actual_pos;
} trt_wrapper;

/** Get wrapper related to the module. */
static trt_wrapper trp_init_wrapper_top();

/** Get wrapper related to e.g. Augmenations or Groupings. */
static trt_wrapper trp_init_wrapper_body();

/** Setting '|' symbol because node is divided or it is not last sibling. */
static trt_wrapper trp_wrapper_set_mark(trt_wrapper);

/** Setting ' ' symbol because node is last sibling. */
static trt_wrapper trp_wrapper_set_shift(trt_wrapper);

/** Setting ' ' symbol if node is last sibling otherwise set '|'. */
static trt_wrapper trp_wrapper_if_last_sibling(trt_wrapper, ly_bool last_one);

/** Test if they are equivalent. */
static ly_bool trp_wrapper_eq(trt_wrapper, trt_wrapper);

/** Print "  |  " sequence. */
static void trp_print_wrapper(trt_wrapper, trt_printing*);


/**
 * @brief Package which only groups wrapper and indent in node.
 */
typedef struct
{
    trt_wrapper wrapper;            /**< "  |  |" sequence. */
    trt_indent_in_node in_node;     /**< Indent in node. */
} trt_pck_indent;

/* ================================== */
/* ------------ <status> ------------ */
/* ================================== */

static const char trd_status_current[] = "+";
static const char trd_status_deprecated[] = "x";
static const char trd_status_obsolete[] = "o";

typedef enum
{
    trd_status_type_empty = 0,
    trd_status_type_current,
    trd_status_type_deprecated,
    trd_status_type_obsolete,
} trt_status_type;

/** Print <status> of the node. */
static void trp_print_status(trt_status_type, trt_printing*);

/* ================================== */
/* ------------ <flags> ------------- */
/* ================================== */

static const char trd_flags_rw[] = "rw";
static const char trd_flags_ro[] = "ro";
static const char trd_flags_rpc_input_params[] = "-w";
static const char trd_flags_uses_of_grouping[] = "-u";
static const char trd_flags_rpc[] = "-x";
static const char trd_flags_notif[] = "-n";
static const char trd_flags_mount_point[] = "mp";

typedef enum
{
    trd_flags_type_empty = 0,
    trd_flags_type_rw,                  /**< rw */
    trd_flags_type_ro,                  /**< ro */
    trd_flags_type_rpc_input_params,    /**< -w */
    trd_flags_type_uses_of_grouping,    /**< -u */
    trd_flags_type_rpc,                 /**< -x */
    trd_flags_type_notif,               /**< -n */
    trd_flags_type_mount_point,         /**< mp */
} trt_flags_type;

/** Print <flags>. */
static void trp_print_flags(trt_flags_type, trt_printing*);
/** Get size of the <flags>. */
static size_t trp_print_flags_strlen(trt_flags_type);

/* ================================== */
/* ----------- <node_name> ----------- */
/* ================================== */

static const char trd_node_name_prefix_choice[] = "(";
static const char trd_node_name_prefix_case[] = ":(";
static const char trd_node_name_suffix_choice[] = ")";
static const char trd_node_name_suffix_case[] = ")";
static const char trd_node_name_triple_dot[] = "...";

static const char trd_node_name_rpc_input[] = "input";
static const char trd_node_name_rpc_output[] = "output";

/**
 * @brief Type of the node.
 *
 * Used mainly to complete the correct <opts> next to or around the <name>.
 */
typedef enum
{
    trd_node_else = 0,              /**< For some node which does not require special treatment. <name> */
    trd_node_case,                  /**< For case node. :(<name>) */
    trd_node_choice,                /**< For choice node. (<name>) */
    trd_node_optional_choice,       /**< For choice node with optional mark. (<name>)? */
    trd_node_optional,              /**< For an optional leaf, anydata, or anyxml. <name>? */
    trd_node_container,             /**< For a presence container. <name>! */
    trd_node_listLeaflist,          /**< For a leaf-list or list (without keys). <name>* */
    trd_node_keys,                  /**< For a list's keys. <name>* [<keys>] */
    trd_node_top_level1,            /**< For a top-level data node in a mounted module. <name>/ */
    trd_node_top_level2,            /**< For a top-level data node of a module identified in a mount point parent reference. <name>@ */
    trd_node_triple_dot             /**< For collapsed sibling nodes and their children. Special case which doesn't belong here very well. */
} trt_node_type;


/**
 * @brief Type of node and his name.
 */
typedef struct
{
    trt_node_type type;         /**< Type of the node relevant for printing. */
    const char* module_prefix;  /**< Prefix defined in the module where the node is defined. */
    const char* str;            /**< Name of the node. */
} trt_node_name;


/** Create trt_node_name as empty. */
static trt_node_name trp_empty_node_name();
/** Check if trt_node_name is empty. */
static ly_bool trp_node_name_is_empty(trt_node_name);
/** Print entire trt_node_name structure. */
static void trp_print_node_name(trt_node_name, trt_printing*);
/** Check if mark (?, !, *, /, @) is implicitly contained in trt_node_name. */
static ly_bool trp_mark_is_used(trt_node_name);

/* ============================== */
/* ----------- <opts> ----------- */
/* ============================== */

static const char trd_opts_optional[] = "?";        /**< For an optional leaf, choice, anydata, or anyxml. */
static const char trd_opts_container[] = "!";       /**< For a presence container. */
static const char trd_opts_list[] = "*";            /**< For a leaf-list or list. */
static const char trd_opts_slash[] = "/";           /**< For a top-level data node in a mounted module. */
static const char trd_opts_at_sign[] = "@";         /**< For a top-level data node of a module identified in a mount point parent reference. */
static const size_t trd_opts_mark_length = 1;       /**< Every opts mark has a length of one. */

static const char trd_opts_keys_prefix[] = "[";
static const char trd_opts_keys_suffix[] = "]";
static const char trd_opts_keys_delim[] = " ";

/** Check if [<keys>] are present in node. */
static ly_bool trp_opts_keys_are_set(trt_node_name);

/** 
 * @brief Print opts keys.
 *
 * @param[in] k flag if keys is present.
 * @param[in] ind number of spaces between name and [keys].
 * @param[in] pf basically a pointer to the function that prints the keys.
 * @param[in,out] p basically a pointer to a function that handles the printing itself.
 */
static void trp_print_opts_keys(trt_node_name name, trt_indent_btw ind, trt_cf_print_keys pf, trt_printing *p);

/* ============================== */
/* ----------- <type> ----------- */
/* ============================== */

static const char trd_type_leafref_keyword[] = "leafref";
static const char trd_type_target_prefix[] = "-> ";
static const char trd_type_anydata_keyword[] = "anydata";
static const char trd_type_anyxml_keyword[] = "anyxml";

/** 
 * @brief Type of the <type>
 */
typedef enum
{
    trd_type_name = 0,  /**< Type is just a name that does not require special treatment. */
    trd_type_target,    /**< Should have a form "-> TARGET", where TARGET is the leafref path. */
    trd_type_leafref,   /**< This type is set automatically by the algorithm. So set type as trd_type_target. */
    trd_type_empty      /**< Type is not used at all. */
} trt_type_type;


/** 
 * @brief <type> in the <node>.
 */
typedef struct
{
    trt_type_type type; /**< Type of the <type>. */
    const char* str;    /**< Path or name of the type. */
} trt_type;

/** Create empty trt_type. */
static trt_type trp_empty_type();
/** Check if trt_type is empty. */
static ly_bool trp_type_is_empty(trt_type);
/** Print entire trt_type structure. */
static void trp_print_type(trt_type, trt_printing*);

/* ==================================== */
/* ----------- <iffeatures> ----------- */
/* ==================================== */

static const char trd_iffeatures_prefix[] = "{";
static const char trd_iffeatures_suffix[] = "}?";
static const char trd_iffeatures_delimiter[] = ",";

/** 
 * @brief List of features in node.
 *
 * The iffeature is just ly_bool because printing is not provided by the printer component (trp).
 */
typedef ly_bool trt_iffeature;

/** Create empty trt_iffeature and note the absence of features. */
static trt_iffeature trp_empty_iffeature();
/** Check if trt_iffeature is empty. */
static ly_bool trp_iffeature_is_empty(trt_iffeature);

/**
 * @brief Print all iffeatures of node 
 *
 * @param[in] i flag if keys is present.
 * @param[in] pf basically a pointer to the function that prints the list of features.
 * @param[in,out] p basically a pointer to a function that handles the printing itself.
 */
static void trp_print_iffeatures(trt_iffeature i, trt_cf_print_iffeatures pf, trt_printing *p);

/* ============================== */
/* ----------- <node> ----------- */
/* ============================== */

/** 
 * @brief <node> data for printing.
 *
 * <status>--<flags> <name><opts> <type> <if-features>.
 * Item <opts> is moved to part trt_node_name.
 * For printing [<keys>] and trt_iffeature is required special functions which prints them.
 */
typedef struct
{
    trt_status_type status;     /**< <status>. */
    trt_flags_type flags;       /**< <flags>. */
    trt_node_name name;         /**< <node> with <opts> mark or [<keys>]. */
    trt_type type;              /**< <type> is the name of the type for leafs and leaf-lists. */
    trt_iffeature iffeatures;   /**< <if-features>. Printing function required. */
    ly_bool last_one;           /**< Information about whether the node is the last. */
} trt_node;

/** Create trt_node as empty. */
static trt_node trp_empty_node();
/** Check if trt_node is empty. */
static ly_bool trp_node_is_empty(trt_node);
/** Check if [<keys>], <type> and <iffeatures> are empty/not_set. */
static ly_bool trp_node_body_is_empty(trt_node);
/** Print just <status>--<flags> <name> with opts mark. */
static void trp_print_node_up_to_name(trt_node, trt_printing*);
/** Print alignment (spaces) instead of <status>--<flags> <name> for divided node. */
static void trp_print_divided_node_up_to_name(trt_node, trt_printing*);

/**
 * @brief Print trt_node structure.
 *
 * @param[in] n node structure for printing.
 * @param[in] ppck package of functions for printing [<keys>] and <iffeatures>.
 * @param[in] ind indent in node.
 * @param[in,out] p basically a pointer to a function that handles the printing itself.
 */
static void trp_print_node(trt_node n, trt_pck_print ppck, trt_indent_in_node ind, trt_printing *p);

/**
 * @brief Check if leafref target must be change to string 'leafref' because his target string is too long.
 *
 * @param[in] n node containing leafref target.
 * @param[in] wr for node immersion depth.
 * @param[in] mll max line length border.
 * @return true if must be change to string 'leafref'.
 */
static ly_bool trp_leafref_target_is_too_long(trt_node n, trt_wrapper wr, uint32_t mll);

/**
 * @brief Package which only groups indent and node.
 */
typedef struct
{
    trt_indent_in_node indent;
    trt_node node;
}trt_pair_indent_node;

/**
 * @brief Get the first half of the node based on the linebreak mark.
 *
 * Items in the second half of the node will be empty.
 *
 * @param[in] node the whole <node> to be split.
 * @param[in] ind contains information in which part of the <node> the first half ends.
 * @return first half of the node, indent is unchanged.
 */
static trt_pair_indent_node trp_first_half_node(trt_node node, trt_indent_in_node ind);

/**
 * @brief Get the second half of the node based on the linebreak mark.
 *
 * Items in the first half of the node will be empty.
 * Indentations belonging to the first node will be reset to zero.
 *
 * @param[in] node the whole <node> to be split.
 * @param[in] ind contains information in which part of the <node> the second half starts.
 * @return second half of the node, indent is newly set.
 */
static trt_pair_indent_node trp_second_half_node(trt_node node, trt_indent_in_node ind);

/** Get default indent in node based on node values. */
static trt_indent_in_node trp_default_indent_in_node(trt_node);

/* =================================== */
/* ----------- <statement> ----------- */
/* =================================== */

static const char trd_top_keyword_module[] = "module";
static const char trd_top_keyword_submodule[] = "submodule";

static const char trd_body_keyword_augment[] = "augment";
static const char trd_body_keyword_rpc[] = "rpcs";
static const char trd_body_keyword_notif[] = "notifications";
static const char trd_body_keyword_grouping[] = "grouping";
static const char trd_body_keyword_yang_data[] = "yang-data";

/**
 * @brief Type of the trt_keyword.
 */
typedef enum 
{
    trd_keyword_empty = 0,
    trd_keyword_module,
    trd_keyword_submodule,
    trd_keyword_augment,
    trd_keyword_rpc,
    trd_keyword_notif,
    trd_keyword_grouping,
    trd_keyword_yang_data,
} trt_keyword_type;

/**
 * @brief Main sign of the tree nodes.
 */
typedef struct
{
    trt_keyword_type type;      /**< String containing some of the top or body keyword. */
    const char* str;            /**< Name or path, it determines the type. */
} trt_keyword_stmt;

/** Create trt_keyword_stmt as empty. */
static trt_keyword_stmt trp_empty_keyword_stmt();
/** Check if trt_keyword_stmt is empty. */
static ly_bool trp_keyword_stmt_is_empty(trt_keyword_stmt);
/** Print .keyword based on .type. */
static void trt_print_keyword_stmt_begin(trt_keyword_stmt, trt_printing*);
/** Print .str which is string of name or path. */
static void trt_print_keyword_stmt_str(trt_keyword_stmt, uint32_t mll, trt_printing*);
/** Print separator based on .type. */
static void trt_print_keyword_stmt_end(trt_keyword_stmt, trt_printing*);
/** Print entire trt_keyword_stmt structure. */
static void trp_print_keyword_stmt(trt_keyword_stmt ks, uint32_t mll, trt_printing *p);
/** Get string length of stored keyword. */
static size_t trp_keyword_type_strlen(trt_keyword_type);

/* ======================================== */
/* ----------- <Modify getters> ----------- */
/* ======================================== */

struct trt_parent_cache;

/**
 * @brief Functions that change the state of the tree_ctx structure.
 *
 * For all, if the value cannot be returned,
 * its empty version obtained by relevant trp_empty* function is returned.
 */
struct trt_fp_modify_ctx
{
    ly_bool (*parent)(struct trt_tree_ctx*);                                            /**< Jump to parent node. Return true if parent exists. */
    void (*first_sibling)(struct trt_tree_ctx*);                                        /**< Jump on the first of the siblings. */
    trt_node (*next_sibling)(struct trt_parent_cache, struct trt_tree_ctx*);            /**< Jump to next sibling of the current node. */
    trt_node (*next_child)(struct trt_parent_cache, struct trt_tree_ctx*);              /**< Jump to the child of the current node. */
    trt_keyword_stmt (*next_augment)(struct trt_tree_ctx*);                             /**< Jump to the augment section. */
    trt_keyword_stmt (*get_rpcs)(struct trt_tree_ctx*);                                 /**< Jump to the rpcs section. */
    trt_keyword_stmt (*get_notifications)(struct trt_tree_ctx*);                        /**< Jump to the notifications section. */
    trt_keyword_stmt (*next_grouping)(struct trt_tree_ctx*);                            /**< Jump to the grouping section. */
    trt_keyword_stmt (*next_yang_data)(struct trt_tree_ctx*);                           /**< Jump to the yang-data section. */
};

/* ====================================== */
/* ----------- <Read getters> ----------- */
/* ====================================== */

/**
 * @brief Functions that do not change the state of the tree_structure.
 *
 * For all, if the value cannot be returned,
 * its empty version obtained by relevant trp_empty* function is returned.
 */
struct trt_fp_read
{
    trt_keyword_stmt (*module_name)(const struct trt_tree_ctx*);                        /**< Get name of the module. */
    trt_node (*node)(struct trt_parent_cache, const struct trt_tree_ctx*);              /**< Get current node. */
    ly_bool (*if_sibling_exists)(const struct trt_tree_ctx*);  /**< Check if node's sibling exists. */
};

/* ===================================== */
/* ----------- <All getters> ----------- */
/* ===================================== */

/**
 * @brief A set of all necessary functions that must be provided for the printer.
 */
struct trt_fp_all
{
    struct trt_fp_modify_ctx modify;    /**< Function pointers which modify state of trt_tree_ctx. */
    struct trt_fp_read read;            /**< Function pointers which only reads state of trt_tree_ctx. */
    struct trt_fp_print print;          /**< Functions pointers for printing special items in node. */
};

/* ========================================= */
/* ----------- <Printer context> ----------- */
/* ========================================= */

/**
 * @brief Main structure for trp component (printer part).
 */
struct trt_printer_ctx
{
    trt_printing print;         /**< The lowest layer over which it is printed. */
    struct trt_fp_all fp;       /**< Set of various function pointers. */
    uint32_t max_line_length;   /**< The maximum number of characters that can be printed on one line, including the last. */
};

/* ======================================== */
/* --------- <Main trp functions> --------- */
/* ======================================== */

/* --------- <Printing line> --------- */

/** Printing one line including wrapper and node which can be incomplete. */
static void trp_print_line(trt_node, trt_pck_print, trt_pck_indent, trt_printing*);

/** Printing one line including wrapper and <status>--<flags> <name><option_mark>. */
static void trp_print_line_up_to_node_name(trt_node, trt_wrapper, trt_printing*);

/* --------- <Printing node> --------- */

/** Printing of the wrapper and the whole node, which can be divided into several lines. */
static void trp_print_entire_node(trt_node, trt_pck_print, trt_pck_indent, uint32_t mll, trt_printing*);

/** Auxiliary function for trp_print_entire_node that prints split nodes. */
static void trp_print_divided_node(trt_node, trt_pck_print, trt_pck_indent, uint32_t mll, trt_printing*);

/* --------- <Special> --------- */

/**
 * @brief Get the correct alignment for the node.
 *
 * @return .type == trd_indent_in_node_divided - the node does not fit in the line, some .trt_indent_btw has negative value as a line break sign.
 * @return .type == trd_indent_in_node_normal - the node fits into the line, all .trt_indent_btw values has non-negative number.
 * @return .type == trd_indent_in_node_failed - the node does not fit into the line, all .trt_indent_btw has negative or zero values, function failed.
 */
static trt_pair_indent_node trp_try_normal_indent_in_node(trt_node, trt_pck_print, trt_pck_indent, uint32_t mll);

/* ======================================== */
/* --------- <Main trb functions> --------- */
/* ======================================== */

/* --------- <Printing tree> --------- */

/** 
 * @brief Print all parents and their children. 
 * 
 * This function is suitable for printing top-level nodes that do not have ancestors.
 * Function call print_subtree_nodes for all top-level siblings.
 * Use this function after 'module' keyword or 'augment' and so.
 */
static void trb_print_family_tree(trd_wrapper_type, struct trt_printer_ctx*, struct trt_tree_ctx*);

/**
 * @brief Print subtree of nodes.
 *
 * The current node is expected to be the root of the subtree.
 * Before root node is no linebreak printing. This must be addressed by the caller.
 * Root node will also be printed. Behind last printed node is no linebreak.
 *
 * @param[in] max_gap_begore_type is result from trb_try_unified_indent function for root node. Set parameter to 0 if distance does not matter.
 * @param[in] wr is wrapper saying how deep in the whole tree is the root of the subtree.
 * @param[in] ca is parent_cache from root's parent. If root is top-level node, insert result of the tro_empty_parent_cache function.
 * @param[in,out] pc is pointer to the printer (trp) context.
 * @param[in,out] tc is pointer to the tree (tro) context.
 */
static void trb_print_subtree_nodes(uint32_t max_gap_before_type, trt_wrapper wr, struct trt_parent_cache ca, struct trt_printer_ctx *pc, struct trt_tree_ctx *tc);

/**
 * @brief For the current node: recursively print all of its child nodes and all of its siblings, including their children.
 *
 * This function is an auxiliary function for trb_print_subtree_nodes.
 * The parent of the current node is expected to exist.
 * Nodes are printed, including unified sibling node alignment (align <type> to column).
 * Side-effect -> current node is set to the last sibling.
 */
static void trb_print_nodes(trt_wrapper, struct trt_parent_cache, struct trt_printer_ctx *, struct trt_tree_ctx *);

/**
 * @brief Print node.
 *
 * This function is wrapper for trp_print_entire_node function.
 * But difference is that take max_gap_before_type parameter which will be used to set the unified alignment.
 */
static void trb_print_entire_node(uint32_t max_gap_before_type, trt_wrapper, struct trt_parent_cache, struct trt_printer_ctx *, struct trt_tree_ctx *);

/* --------- <For browse tree> --------- */

/**
 * @brief Get number of siblings.
 *
 * Side-effect -> current node is set to the first sibling.
 */
static uint32_t trb_get_number_of_siblings(struct trt_fp_modify_ctx, struct trt_tree_ctx*);

/**
 * @brief Check if parent of the current node is the last of his siblings.
 *
 * To mantain stability use this function only if the current node is the first of the siblings.
 * Side-effect -> current node is set to the first sibling if node has a parent otherwise no side-effect.
 */
static ly_bool trb_parent_is_last_sibling(struct trt_fp_all, struct trt_tree_ctx*);

/* --------- <For unified indentation> --------- */

/**
 * @brief Find out if it is possible to unify the alignment before <type>.
 *
 * The goal is for all node siblings to have the same alignment for <type> as if they were in a column.
 * All siblings who cannot adapt because they do not fit on the line at all are ignored.
 * @return 0 if all siblings cannot fit on the line.
 * @return positive number indicating the maximum number of spaces before <type> if the length of the node name is 0.
 *  To calculate the btw_opts_type indent size for a particular node, use the trb_calc_btw_opts_type function.
*/
static uint32_t trb_try_unified_indent(struct trt_parent_cache, struct trt_printer_ctx *, struct trt_tree_ctx *);

/**
 * @brief Calculate the btw_opts_type indent size for a particular node.
 *
 * @param[in] name is the node for which we get btw_opts_type.
 * @param[in] max_len is the maximum value of btw_opts_type that it can have.
 * @return btw_opts_type for node.
 */
static trt_indent_btw trb_calc_btw_opts_type(trt_node_name, trt_indent_btw max_len);

/**
 * @brief Get size of node name.
 * @return positive value total size of the node name.
 * @return negative value as an indication that option mark is included in the total size.
 */
static int32_t trb_strlen_of_name_and_mark(trt_node_name);

/**
 * @brief Find sibling with the biggest node name and return that size.
 *
 * Side-effect -> Current node is set to the first sibling.
 * @return positive number lesser than upper_limit as a sign that only the node name is included in the size.
 * @return negative number whose absolute value is less than upper_limit and sign that node name and his opt mark is included in the size.
 */
static int32_t trb_maxlen_node_name(struct trt_parent_cache, struct trt_printer_ctx *, struct trt_tree_ctx *);

/**
 * @brief Find sibling with the nth biggest node name.
 *
 * Side-effect -> Current node is set to the first sibling.
 * @return max btw_opts_type value for rest of the siblings
 */
static trt_indent_btw trb_max_btw_opts_type4siblings(struct trt_parent_cache, struct trt_printer_ctx *, struct trt_tree_ctx *);

/* ======================================== */
/* --------- <Main trm functions> --------- */
/* ======================================== */

/** Print sections module, augment, rpcs, notifications, grouping, yang-data. */
static void trm_print_sections(struct trt_printer_ctx*, struct trt_tree_ctx*);

/** Print 'module' keyword, its name and all nodes. */
static void trm_print_module_section(struct trt_printer_ctx*, struct trt_tree_ctx*);

/** For all augment sections: print 'augment' keyword, its target node and all nodes. */
static void trm_print_augmentations(struct trt_printer_ctx*, struct trt_tree_ctx*);

/** For rpcs section: print 'rpcs' keyword and all its nodes. */
static void trm_print_rpcs(struct trt_printer_ctx*, struct trt_tree_ctx*);

/** For notifications section: print 'notifications' keyword and all its nodes. */
static void trm_print_notifications(struct trt_printer_ctx*, struct trt_tree_ctx*);

/** For all grouping sections: print 'grouping' keyword, its name and all nodes. */
static void trm_print_groupings(struct trt_printer_ctx*, struct trt_tree_ctx*);

/** For all yang-data sections: print 'yang-data' keyword and all its nodes. */
static void trm_print_yang_data(struct trt_printer_ctx*, struct trt_tree_ctx*);

/** General function to prevent repetitiveness code. */
static void trm_print_body_section(trt_keyword_stmt, struct trt_printer_ctx*, struct trt_tree_ctx*);

/**
 * @brief Get default settings for trt_printer_ctx.
 *
 * Get trt_printer_ctx containing all structure items correctly defined except for trt_printer_opts and max_line_length,
 * which are parameters of the printer tree module.
 */
static struct trt_printer_ctx trm_default_printer_ctx(struct ly_out *out, uint32_t max_line_length);

/**
 * @brief Get default settings for trt_tree_ctx.
 *
 * Pointers to current nodes will be set to module data.
 */
static struct trt_tree_ctx trm_default_tree_ctx(const struct lys_module*, struct trt_printer_ctx*);

/* ====================================== */
/* ----------- <Tree context> ----------- */
/* ====================================== */

typedef enum
{
    trd_sect_module = 0,
    trd_sect_augment,
    trd_sect_rpcs,
    trd_sect_notif,
    trd_sect_grouping,
    trd_sect_yang_data,
} trt_actual_section;

typedef uint32_t trt_opt;

/* These flags are used in trt_options.code. Functionality is not implemented. */
#define TRC_OPT_SECT_MODULE         (1u << 0)   /**< Don't print module section. */
#define TRC_OPT_SECT_AUGMENT        (1u << 1)   /**< Don't print augment section. */
#define TRC_OPT_SECT_RPCS           (1u << 2)   /**< Don't print rpcs section. */
#define TRC_OPT_SECT_NOTIF          (1u << 3)   /**< Don't print notifications section. */
#define TRC_OPT_SECT_GROUPING       (1u << 4)   /**< Don't print grouping section. */
#define TRC_OPT_SECT_YANGDATA       (1u << 5)   /**< Don't print yang-data section. */
#define TRC_OPT_MAX_LB_PER_SECT     (1u << 10)  /**< The number of line breaks in one section must not exceed. Functionality is not implemented. */

#define TRC_OPT_DEFAULT 0   /**< Default settings for trt_options.code variable. */

/** Ancestors who have a strong influence on their children. */
typedef enum
{
    trd_ancestor_else = 0,
    trd_ancestor_rpc_input,
    trd_ancestor_rpc_output,
    trd_ancestor_notif
} trt_ancestor_type;

/**
 * @brief Saved information when browsing the tree downwards.
 *
 * This structure helps prevent frequent retrieval of information from the tree.
 * Browsing functions (trb) are designed to preserve this structures during their recursive calls.
 * Browsing functions (trb) do not interfere in any way with this data.
 * This structure is used by Obtaining functions (tro) which, thanks to this structure, can return a node with the correct data.
 * The word parent is in the name, because this data refers to the last parent and at the same time the states of its ancestors data.
 * Only the function jumping on the child (next_child(...)) creates this structure,
 * because the pointer to the current node moves down the tree.
 * It's like passing the genetic code to children.
 * Some data must be inherited and there are two approaches to this problem.
 * Either it will always be determined which inheritance states belong to the current node
 * (which can lead to regular travel to the root node) or the inheritance states will be stored during the recursive calls.
 * So the problem was solved by the second option.
 * Why does the structure contain this data? Because it walks through the lysp tree.
 * In the future, this data may change if another type of tree (such as the lysc tree) is traversed.
 */
struct trt_parent_cache {
    trt_ancestor_type ancestor;             /**< Some types of nodes have a special effect on their children. */
    uint16_t lys_status;                    /**< Inherited status CURR, DEPRC, OBSLT. */
    uint16_t lys_config;                    /**< Inherited config W or R. */
    const struct lysp_node_list *last_list; /**< The last LYS_LIST passed. */
};

/** Return trt_parent_cache filled with default values. */
static struct trt_parent_cache tro_empty_parent_cache();

/**
 * @brief Get new trt_parent_cache if we apply the transfer to the child.
 *
 * @param[in] ca is parent cache for current node.
 * @param[in] pn is current node data from lysp_tree.
 * @return cache for the current node.
 */
static struct trt_parent_cache tro_parent_cache_for_child(struct trt_parent_cache ca, const struct lysp_node *pn);

/** Setting the behavior of this entire printer_tree module. */
typedef struct
{
    trt_opt code;
    uint32_t max_linebreaks;    /**< Max linebreaks per section. Functionality is not implemented. */
    uint32_t* cnt_linebreak;    /**< Pointer to trt_printing.cnt_linebreak counter. Functionality is not implemented. */
} trt_options;

/**
 * @brief Main structure for browsing the libyang tree
 */
struct trt_tree_ctx
{
    trt_actual_section section;         /**< To which section pn points. */
    int64_t index_within_section;       /**< For example in which 'grouping' we are right now. It has the number -1 if it is invalid. */
    int64_t index;                      /**< Some nodes contain an array of other nodes etc. list has actions. Number -1 means invalid value. */
    const struct lys_module *module;    /**< Schema tree structures. */
    const struct lysp_node *pn;         /**< Actual pointer to parsed node. */
    const struct lysp_node *tpn;        /**< Pointer to actual top-node. */
    trt_options opt;                    /**< Options for printing. */
};

/* --------- <Read getters> --------- */
/* Data in trt_tree_ctx should not be modified. */

/** Get name of the module. */
static trt_keyword_stmt tro_read_module_name(const struct trt_tree_ctx*);

/** Transformation of current lysp_node to trt_node. */
static trt_node tro_read_node(struct trt_parent_cache, const struct trt_tree_ctx *);

/** Find out if the current node has siblings. */
static ly_bool tro_read_if_sibling_exists(const struct trt_tree_ctx*);

/* --------- <Modify getters> --------- */
/* Data in trt_tree_ctx will be modified. */

/** 
 * @brief Change the pointer to the current node to its parent.
 * @return 1 if the node had parents and the change was successful
 * @return 0 if if the node did not have parents and the pointer to the current node did not change.
 */
static ly_bool tro_modi_parent(struct trt_tree_ctx *);

/** Change the pointer to the current node to its first sibling. */
static void tro_modi_first_sibling(struct trt_tree_ctx *);

/** Change the pointer to the current node to its next sibling. */
static trt_node tro_modi_next_sibling(struct trt_parent_cache, struct trt_tree_ctx *);

/** Change the pointer to the current node to its child. */
static trt_node tro_modi_next_child(struct trt_parent_cache, struct trt_tree_ctx *);

/** Get next augment section if exists. */
static trt_keyword_stmt tro_modi_next_augment(struct trt_tree_ctx*);
/** Get rpcs section if exists. */
static trt_keyword_stmt tro_modi_get_rpcs(struct trt_tree_ctx*);
/** Get norification section if exists. */
static trt_keyword_stmt tro_modi_get_notifications(struct trt_tree_ctx*);
/** Get next grouping section if exists. */
static trt_keyword_stmt tro_modi_next_grouping(struct trt_tree_ctx*);
/** Get next yang-data section if exists. */
static trt_keyword_stmt tro_modi_next_yang_data(struct trt_tree_ctx*);

/* --------- <Print getters> --------- */

/** Print current node's iffeatures. */
static void tro_print_features_names(const struct trt_tree_ctx*, trt_printing*);

/** Print current list's keys. */
static void tro_print_keys(const struct trt_tree_ctx*, trt_printing*);

/* --------- <Auxiliary functions for tro_ functions> --------- */

/** Check if list statement has keys. */
static ly_bool tro_lysp_list_has_keys(const struct lysp_node*);

/** Check if it contains at least one feature. */
static ly_bool tro_lysp_node_has_iffeature(const struct lysp_qname*);

/** Find out if leaf is also the key in last list. */
static ly_bool tro_lysp_leaf_is_key(struct trt_parent_cache, const struct trt_tree_ctx*);

/** Check if container's type is presence. */
static ly_bool tro_lysp_container_has_presence(const struct lysp_node*);

/** Get leaflist's path without lysp_node type control. */
static const char* tro_lysp_leaflist_refpath(const struct lysp_node*);

/** Get leaflist's type name without lysp_node type control. */
static const char* tro_lysp_leaflist_type_name(const struct lysp_node*);

/** Get leaf's path without lysp_node type control. */
static const char* tro_lysp_leaf_refpath(const struct lysp_node*);

/** Get leaf's type name without lysp_node type control. */
static const char* tro_lysp_leaf_type_name(const struct lysp_node*);

/** Getter function for tro_lysp_node_charptr function. */
typedef const char* (*trt_get_charptr_func)(const struct lysp_node* pn);

/**
 * @brief Get pointer to data using node type specification and getter function.
 *
 * @param[in] flags is node type specification. If it is the correct node, the getter function is called.
 * @param[in] f is getter function which provides the desired char pointer from the structure.
 * @return NULL if node has wrong type or getter function return pointer to NULL.
 * @return pointer to desired char pointer obtained from the node.
 */
static const char* tro_lysp_node_charptr(uint16_t flags, trt_get_charptr_func f, const struct lysp_node* pn);

/** Transformation of the lysp flags to Yang tree <status>. */
static trt_status_type tro_lysp_flags2status(uint16_t);

/** Transformation of the lysp flags to Yang tree <flags> but more specifically 'ro' or 'rw'. */
static trt_flags_type tro_lysp_flags2config(uint16_t);

/* =================================== */
/* ----------- <separator> ----------- */
/* =================================== */

typedef const char* const trt_separator;
static trt_separator trd_separator_colon = ":";
static trt_separator trd_separator_space = " ";
static trt_separator trd_separator_dashes = "--";
static trt_separator trd_separator_slash = "/";
static trt_separator trd_separator_linebreak = "\n";

/* =================================== */
/* ------ <auxiliary functions> ------ */
/* =================================== */

/** Get absolute value of integer. */
static uint32_t trg_abs(int32_t);

/** Print character n times. */
static void trg_print_n_times(int32_t n, char, trt_printing*);

/** Using ly_print_ function for printing */
static void trg_print_by_ly_print(void *out, int arg_count, va_list ap);

/** Test if the bit on the index is set. */
static ly_bool trg_test_bit(uint64_t number, uint32_t bit);

/** Print trd_separator_linebreak. */
static void trg_print_linebreak(trt_printing*);

/** Print a substring but limited to the maximum length. */
static const char* trg_print_substr(const char*, size_t len, trt_printing*);

/** Pointer is not NULL and does not point to an empty string. */
static ly_bool trg_charptr_has_data(const char*);

/** Check if 'word' in 'src' is present where words are delimited by 'delim'. */
static ly_bool trg_word_is_present(const char* src, const char* word, char delim);

/* ================================ */
/* ----------- <symbol> ----------- */
/* ================================ */

typedef const char* const trt_symbol;
static trt_symbol trd_symbol_sibling = "|";

/* ======================================== */
/* ---------- <Module interface> ---------- */
/* ======================================== */


/** Print Yang tree diagram. */
LY_ERR tree_print_parsed_and_compiled_module(struct ly_out *out, const struct lys_module *module, uint32_t options, size_t line_length);

/** Not implemented right now. */
LY_ERR tree_print_submodule(struct ly_out *out, const struct lys_module *module, const struct lysp_submodule *submodp, uint32_t options, size_t line_length);

/** Not implemented right now. */
LY_ERR tree_print_compiled_node(struct ly_out *out, const struct lysc_node *node, uint32_t options, size_t line_length);

/* -######-- Declarations end -#######- */
/* -######-- Definitions start -#######- */

/* ----------- <Definition of printer functions> ----------- */

void
trp_cnt_linebreak_reset(trt_printing* p)
{
    p->cnt_linebreak = 0;
}

void
trp_cnt_linebreak_increment(trt_printing* p)
{
    p->cnt_linebreak++;
}

void
trp_print(trt_printing* p, int arg_count, ...)
{
    va_list ap;
    va_start(ap, arg_count);
    p->pf(p->out, arg_count, ap);
    va_end(ap);
}

void
trp_injected_strlen(void *out, int arg_count, va_list ap)
{
    trt_counter* cnt = (trt_counter*)out;

    for(int i = 0; i < arg_count; i++) {
        char* item = va_arg(ap, char*);
        if(item)
            cnt->bytes += strlen(item);
    }
}

ly_bool
trp_indent_in_node_are_eq(trt_indent_in_node f, trt_indent_in_node s)
{
    const ly_bool a = f.type == s.type;
    const ly_bool b = f.btw_name_opts == s.btw_name_opts;
    const ly_bool c = f.btw_opts_type == s.btw_opts_type;
    const ly_bool d = f.btw_type_iffeatures == s.btw_type_iffeatures;
    return a && b && c && d;
}

trt_wrapper
trp_init_wrapper_top()
{
    /* module: <module-name>
     *   +--<node>
     *   |
     */
    trt_wrapper wr;
    wr.type = trd_wrapper_top;
    wr.actual_pos = 0;
    wr.bit_marks1 = 0;
    return wr;
}

trt_wrapper
trp_init_wrapper_body()
{
    /* module: <module-name>
     *   +--<node>
     *
     *   augment <target-node>:
     *     +--<node>
     */
    trt_wrapper wr;
    wr.type = trd_wrapper_body;
    wr.actual_pos = 0;
    wr.bit_marks1 = 0;
    return wr;
}

trt_wrapper
trp_wrapper_set_mark(trt_wrapper wr)
{
    wr.bit_marks1 |= 1U << wr.actual_pos;
    return trp_wrapper_set_shift(wr);
}

trt_wrapper
trp_wrapper_set_shift(trt_wrapper wr)
{
    /* +--<node>
     *    +--<node>
     */
    wr.actual_pos++;
    return wr;
}

trt_wrapper
trp_wrapper_if_last_sibling(trt_wrapper wr, ly_bool last_one)
{
    return last_one ? trp_wrapper_set_shift(wr) : trp_wrapper_set_mark(wr);
}

ly_bool
trp_wrapper_eq(trt_wrapper f, trt_wrapper s)
{
    const ly_bool a = f.type == s.type;
    const ly_bool b = f.bit_marks1 == s.bit_marks1;
    const ly_bool c = f.actual_pos == s.actual_pos;
    return a && b && c;
}

void
trp_print_wrapper(trt_wrapper wr, trt_printing* p)
{
    const char char_space = trd_separator_space[0];

    {
        uint32_t lb;
        if (wr.type == trd_wrapper_top) {
          lb = trd_indent_line_begin;
        } else if (wr.type == trd_wrapper_body) {
          lb = trd_indent_line_begin * 2;
        } else
          lb = trd_indent_line_begin;

        trg_print_n_times(lb, char_space, p);
    }

    if(trp_wrapper_eq(wr, trp_init_wrapper_top()))
        return;

    for(uint32_t i = 0; i < wr.actual_pos; i++) {
        if(trg_test_bit(wr.bit_marks1, i)){
            trp_print(p, 1, trd_symbol_sibling);
        } else {
            trp_print(p, 1, trd_separator_space);
        }

        if(i != wr.actual_pos)
            trg_print_n_times(trd_indent_btw_siblings, char_space, p);
    }
}

trt_node_name
trp_empty_node_name()
{
    trt_node_name ret;
    ret.str = NULL;
    return ret;
}

ly_bool
trp_node_name_is_empty(trt_node_name node_name)
{
    return node_name.str == NULL;
}

ly_bool
trp_opts_keys_are_set(trt_node_name node_name)
{
    return node_name.type == trd_node_keys;
}

trt_type
trp_empty_type()
{
    trt_type ret;
    ret.type = trd_type_empty;
    return ret;
}

ly_bool
trp_type_is_empty(trt_type type)
{
    return type.type == trd_type_empty;
}

trt_iffeature
trp_empty_iffeature()
{
    return 0;
}

ly_bool
trp_iffeature_is_empty(trt_iffeature iffeature)
{
    return !iffeature;
}

trt_node
trp_empty_node()
{
    trt_node ret = 
    {
        trd_status_type_empty, trd_flags_type_empty,
        trp_empty_node_name(), trp_empty_type(),
        trp_empty_iffeature(), 1
    };
    return ret;
}

ly_bool
trp_node_is_empty(trt_node node)
{
    const ly_bool a = trp_iffeature_is_empty(node.iffeatures);
    const ly_bool b = trp_type_is_empty(node.type);
    const ly_bool c = trp_node_name_is_empty(node.name);
    const ly_bool d = node.flags == trd_flags_type_empty;
    const ly_bool e = node.status == trd_status_type_empty;
    return a && b && c && d && e;
}

ly_bool
trp_node_body_is_empty(trt_node node)
{
    const ly_bool a = trp_iffeature_is_empty(node.iffeatures);
    const ly_bool b = trp_type_is_empty(node.type);
    const ly_bool c = !trp_opts_keys_are_set(node.name);
    return a && b && c;
}

trt_keyword_stmt
trp_empty_keyword_stmt()
{
    return (trt_keyword_stmt){trd_keyword_empty, NULL};
}

ly_bool
trp_keyword_stmt_is_empty(trt_keyword_stmt ks)
{
    return ks.type == trd_keyword_empty;
}

void
trp_print_status(trt_status_type a, trt_printing* p)
{
    switch(a) {
    case trd_status_type_current:
        trp_print(p, 1, trd_status_current);
        break;
    case trd_status_type_deprecated:
        trp_print(p, 1, trd_status_deprecated);
        break;
    case trd_status_type_obsolete:
        trp_print(p, 1, trd_status_obsolete);
        break;
    default:
        break;
    }
}

void
trp_print_flags(trt_flags_type a, trt_printing* p)
{
    switch(a) {
    case trd_flags_type_rw:
        trp_print(p, 1, trd_flags_rw);
        break;
    case trd_flags_type_ro:
        trp_print(p, 1, trd_flags_ro);
        break;
    case trd_flags_type_rpc_input_params:
        trp_print(p, 1, trd_flags_rpc_input_params);
        break;
    case trd_flags_type_uses_of_grouping:
        trp_print(p, 1, trd_flags_uses_of_grouping);
        break;
    case trd_flags_type_rpc:
        trp_print(p, 1, trd_flags_rpc);
        break;
    case trd_flags_type_notif:
        trp_print(p, 1, trd_flags_notif);
        break;
    case trd_flags_type_mount_point:
        trp_print(p, 1, trd_flags_mount_point);
        break;
    default:
        break;
    }
}

size_t
trp_print_flags_strlen(trt_flags_type a)
{
    return a == trd_flags_type_empty ? 0 : 2;
}

void
trp_print_node_name(trt_node_name a, trt_printing* p)
{
    if(trp_node_name_is_empty(a))
        return;

    const char* colon = a.module_prefix == NULL || a.module_prefix[0] == '\0' ? "" : trd_separator_colon;

    switch(a.type) {
    case trd_node_else:
        trp_print(p, 3, a.module_prefix, colon, a.str);
        break;
    case trd_node_case:
        trp_print(p, 5, trd_node_name_prefix_case, a.module_prefix, colon, a.str, trd_node_name_suffix_case);
        break;
    case trd_node_choice:
        trp_print(p, 5, trd_node_name_prefix_choice,  a.module_prefix, colon, a.str, trd_node_name_suffix_choice);
        break;
    case trd_node_optional_choice:
        trp_print(p, 6, trd_node_name_prefix_choice,  a.module_prefix, colon, a.str, trd_node_name_suffix_choice, trd_opts_optional);
        break;
    case trd_node_optional:
        trp_print(p, 4, a.module_prefix, colon, a.str, trd_opts_optional);
        break;
    case trd_node_container:
        trp_print(p, 4, a.module_prefix, colon, a.str, trd_opts_container);
        break;
    case trd_node_listLeaflist:
        trp_print(p, 4, a.module_prefix, colon, a.str, trd_opts_list);
        break;
    case trd_node_keys:
        trp_print(p, 4, a.module_prefix, colon, a.str, trd_opts_list);
        break;
    case trd_node_top_level1:
        trp_print(p, 4, a.module_prefix, colon, a.str, trd_opts_slash);
        break;
    case trd_node_top_level2:
        trp_print(p, 4, a.module_prefix, colon, a.str, trd_opts_at_sign);
        break;
    case trd_node_triple_dot:
        trp_print(p, 1, trd_node_name_triple_dot);
        break;
    default:
        break;
    }
}

ly_bool
trp_mark_is_used(trt_node_name a)
{
    if(trp_node_name_is_empty(a))
        return 0;

    switch(a.type) {
    case trd_node_else:
    case trd_node_case:
    case trd_node_keys:
        return 0;
    default:
        return 1;
    }
}

void
trp_print_opts_keys(trt_node_name a, trt_indent_btw btw_name_opts, trt_cf_print_keys cf, trt_printing* p)
{
    if(!trp_opts_keys_are_set(a))
        return;

    /* <name><mark>___<keys>*/
    trg_print_n_times(btw_name_opts, trd_separator_space[0], p);
    trp_print(p, 1, trd_opts_keys_prefix);
    cf.pf(cf.ctx, p);
    trp_print(p, 1, trd_opts_keys_suffix);
}

void
trp_print_type(trt_type a, trt_printing* p)
{
    if(trp_type_is_empty(a))
        return;

    switch(a.type) {
    case trd_type_name:
        trp_print(p, 1, a.str);
        break;
    case trd_type_target:
        trp_print(p, 2, trd_type_target_prefix, a.str);
        break;
    case trd_type_leafref:
        trp_print(p, 1, trd_type_leafref_keyword);
    default:
        break;
    }
}

void
trp_print_iffeatures(trt_iffeature a, trt_cf_print_iffeatures cf, trt_printing* p)
{
    if(trp_iffeature_is_empty(a))
        return;

    trp_print(p, 1, trd_iffeatures_prefix);
    cf.pf(cf.ctx, p);
    trp_print(p, 1, trd_iffeatures_suffix);
}

void
trp_print_node_up_to_name(trt_node a, trt_printing* p)
{
    if(a.name.type == trd_node_triple_dot) {
        trp_print_node_name(a.name, p);
        return;
    }
    /* <status>--<flags> */
    trp_print_status(a.status, p);
    trp_print(p, 1, trd_separator_dashes);
    /* If the node is a case node, there is no space before the <name> */
    /* also case node has no flags. */
    if(a.name.type != trd_node_case) {
        trp_print_flags(a.flags, p);
        trp_print(p, 1, trd_separator_space);
    }
    /* <name> */
    trp_print_node_name(a.name, p);
}

void
trp_print_divided_node_up_to_name(trt_node a, trt_printing* p)
{
    uint32_t space = trp_print_flags_strlen(a.flags);

    if(a.name.type == trd_node_case) {
        /* :(<name> */
        space += strlen(trd_node_name_prefix_case);
    } else if(a.name.type == trd_node_choice) {
        /* (<name> */
        space += strlen(trd_node_name_prefix_choice);
    } else {
        /* _<name> */
        space += strlen(trd_separator_space);
    }

    /* <name>
     * __
     */
    space += trd_indent_long_line_break;

    trg_print_n_times(space, trd_separator_space[0], p);
}

void
trp_print_node(trt_node a, trt_pck_print pck, trt_indent_in_node ind, trt_printing* p)
{
    if(trp_node_is_empty(a))
        return;

    /* <status>--<flags> <name><opts> <type> <if-features> */

    const ly_bool triple_dot = a.name.type == trd_node_triple_dot;
    const ly_bool divided = ind.type == trd_indent_in_node_divided;
    const char char_space = trd_separator_space[0];

    if(triple_dot) { 
        trp_print_node_name(a.name, p);
        return;
    } else if(!divided) {
        trp_print_node_up_to_name(a, p);
    } else {
        trp_print_divided_node_up_to_name(a, p);
    }

    /* <opts> */
    /* <name>___<opts>*/
    trt_cf_print_keys cf_print_keys = {pck.tree_ctx, pck.fps.print_keys};
    trp_print_opts_keys(a.name, ind.btw_name_opts, cf_print_keys, p);

    /* <opts>__<type> */
    trg_print_n_times(ind.btw_opts_type, char_space, p);

    /* <type> */
    trp_print_type(a.type, p);

    /* <type>__<iffeatures> */
    trg_print_n_times(ind.btw_type_iffeatures, char_space, p);

    /* <iffeatures> */
    trt_cf_print_keys cf_print_iffeatures = {pck.tree_ctx, pck.fps.print_features_names};
    trp_print_iffeatures(a.iffeatures, cf_print_iffeatures, p);
}

void
trt_print_keyword_stmt_begin(trt_keyword_stmt a, trt_printing* p)
{
    switch(a.type) {
    case trd_keyword_module:
        trp_print(p, 3, trd_top_keyword_module, trd_separator_colon, trd_separator_space);
        return;
    case trd_keyword_submodule:
        trp_print(p, 3, trd_top_keyword_submodule, trd_separator_colon, trd_separator_space);
        return;
    default:
        trg_print_n_times(trd_indent_line_begin, trd_separator_space[0], p);
        switch(a.type) {
        case trd_keyword_augment:
            trp_print(p, 2, trd_body_keyword_augment, trd_separator_space);
            break;
        case trd_keyword_rpc:
            trp_print(p, 1, trd_body_keyword_rpc);
            break;
        case trd_keyword_notif:
            trp_print(p, 1, trd_body_keyword_notif);
            break;
        case trd_keyword_grouping:
            trp_print(p, 2, trd_body_keyword_grouping, trd_separator_space);
            break;
        case trd_keyword_yang_data:
            trp_print(p, 2, trd_body_keyword_yang_data, trd_separator_space);
            break;
        default:
            break;
        }
        break;
    }
}

size_t
trp_keyword_type_strlen(trt_keyword_type a)
{
    switch(a) {
    case trd_keyword_module:
        return sizeof(trd_top_keyword_module) - 1;
    case trd_keyword_submodule:
        return sizeof(trd_top_keyword_submodule) - 1;
    case trd_keyword_augment:
        return sizeof(trd_body_keyword_augment) - 1;
    case trd_keyword_rpc:
        return sizeof(trd_body_keyword_rpc) - 1;
    case trd_keyword_notif:
        return sizeof(trd_body_keyword_notif) - 1;
    case trd_keyword_grouping:
        return sizeof(trd_body_keyword_grouping) - 1;
    case trd_keyword_yang_data:
        return sizeof(trd_body_keyword_yang_data) - 1;
    default:
        return 0;
    }
}

void
trt_print_keyword_stmt_str(trt_keyword_stmt a, uint32_t mll, trt_printing* p)
{
    if(a.str == NULL || a.str[0] == '\0')
        return;

    /* module name cannot be splitted */
    if(a.type == trd_keyword_module || a.type == trd_keyword_submodule) {
        trp_print(p, 1, a.str);
        return;
    }

    /* else for trd_keyword_stmt_body do */

    const char slash = trd_separator_slash[0];
    /* set begin indentation */
    const uint32_t ind_initial = trd_indent_line_begin + trp_keyword_type_strlen(a.type) + 1;
    const uint32_t ind_divided = ind_initial + trd_indent_long_line_break; 
    /* flag if path must be splitted to more lines */
    ly_bool linebreak_was_set = 0;
    /* flag if at least one subpath was printed */
    ly_bool subpath_printed = 0;
    /* the sum of the sizes of the substrings on the current line */
    uint32_t how_far = 0;

    /* pointer to start of the subpath */
    const char* sub_ptr = a.str;
    /* size of subpath from sub_ptr */
    size_t sub_len = 0;

    while(sub_ptr[0] != '\0') {
        /* skip slash */
        const char* tmp = sub_ptr[0] == slash ? sub_ptr + 1 : sub_ptr;
        /* get position of the end of substr */
        tmp = strchr(tmp, slash);
        /* set correct size if this is a last substring */
        sub_len = tmp == NULL ? strlen(sub_ptr) : (size_t)(tmp - sub_ptr);
        /* actualize sum of the substring's sizes on the current line */
        how_far += sub_len;
        /* correction due to colon character if it this is last substring */
        how_far = *(sub_ptr + sub_len) == '\0' ? how_far + 1 : how_far;
        /* choose indentation which depends on
         * whether the string is printed on multiple lines or not
         */
        uint32_t ind = linebreak_was_set ? ind_divided : ind_initial;
        if(ind + how_far <= mll) {
            /* printing before max line length */
            sub_ptr = trg_print_substr(sub_ptr, sub_len, p);
            subpath_printed = 1;
        } else {
            /* printing on new line */
            if(subpath_printed == 0) {
                /* first subpath is too long but print it at first line anyway */
                sub_ptr = trg_print_substr(sub_ptr, sub_len, p);
                subpath_printed = 1;
                continue;
            }
            trg_print_linebreak(p);
            trg_print_n_times(ind_divided, trd_separator_space[0], p);
            linebreak_was_set = 1;
            sub_ptr = trg_print_substr(sub_ptr, sub_len, p);
            how_far = sub_len;
            subpath_printed = 1;
        }
    }
}

void
trt_print_keyword_stmt_end(trt_keyword_stmt a, trt_printing* p)
{
    if(a.type != trd_keyword_module && a.type != trd_keyword_submodule)
        trp_print(p, 1, trd_separator_colon);
}

void
trp_print_keyword_stmt(trt_keyword_stmt a, uint32_t mll, trt_printing* p)
{
    if(trp_keyword_stmt_is_empty(a))
        return;
    trt_print_keyword_stmt_begin(a, p);
    trt_print_keyword_stmt_str(a, mll, p);
    trt_print_keyword_stmt_end(a, p);
}

void
trp_print_line(trt_node node, trt_pck_print pck, trt_pck_indent ind, trt_printing* p)
{
    trp_print_wrapper(ind.wrapper, p);
    trp_print_node(node, pck, ind.in_node, p);
}

void
trp_print_line_up_to_node_name(trt_node node, trt_wrapper wr, trt_printing* p)
{
    trp_print_wrapper(wr, p);
    trp_print_node_up_to_name(node, p);
}


ly_bool trp_leafref_target_is_too_long(trt_node node, trt_wrapper wr, uint32_t mll)
{
    if(node.type.type != trd_type_target)
        return 0;

    trt_counter cnt = {0};
    /* inject print function with strlen */
    trt_injecting_strlen func = {&cnt, trp_injected_strlen, 0};
    /* count number of printed bytes */
    trp_print_wrapper(wr, &func);
    trg_print_n_times(trd_indent_btw_siblings, trd_separator_space[0], &func);
    trp_print_divided_node_up_to_name(node, &func);

    return cnt.bytes + strlen(node.type.str) > mll;
}

trt_indent_in_node
trp_default_indent_in_node(trt_node node)
{
    trt_indent_in_node ret;
    ret.type = trd_indent_in_node_normal;

    /* btw_name_opts */
    ret.btw_name_opts = trp_opts_keys_are_set(node.name) ? 
        trd_indent_before_keys : 0;

    /* btw_opts_type */
    if(!trp_type_is_empty(node.type)) {
        ret.btw_opts_type = trp_mark_is_used(node.name) ? 
            trd_indent_before_type - trd_opts_mark_length:
            trd_indent_before_type;
    } else {
        ret.btw_opts_type = 0;
    }

    /* btw_type_iffeatures */
    ret.btw_type_iffeatures = !trp_iffeature_is_empty(node.iffeatures) ?
        trd_indent_before_iffeatures : 0;

    return ret;
}

void
trp_print_entire_node(trt_node node, trt_pck_print ppck, trt_pck_indent ipck, uint32_t mll, trt_printing* p)
{
    if(trp_leafref_target_is_too_long(node, ipck.wrapper, mll)) {
        node.type.type = trd_type_leafref;
    }

    /* check if normal indent is possible */
    trt_pair_indent_node ind_node1 = trp_try_normal_indent_in_node(node, ppck, ipck, mll);

    if(ind_node1.indent.type == trd_indent_in_node_normal) {
        /* node fits to one line */
        trp_print_line(node, ppck, ipck, p);
    } else if(ind_node1.indent.type == trd_indent_in_node_divided) {
        /* node will be divided */
        /* print first half */
        {
            trt_pck_indent tmp = {ipck.wrapper, ind_node1.indent};
            /* pretend that this is normal node */
            tmp.in_node.type = trd_indent_in_node_normal;
            trp_print_line(ind_node1.node, ppck, tmp, p);
        }
        trg_print_linebreak(p);
        /* continue with second half on new line */
        {
            trt_pair_indent_node ind_node2 = trp_second_half_node(node, ind_node1.indent);
            trt_pck_indent tmp = {trp_wrapper_if_last_sibling(ipck.wrapper, node.last_one), ind_node2.indent};
            trp_print_divided_node(ind_node2.node, ppck, tmp, mll, p);
        }
    } else if(ind_node1.indent.type == trd_indent_in_node_failed){
        /* node name is too long */
        trp_print_line_up_to_node_name(node, ipck.wrapper, p);
        if(trp_node_body_is_empty(node)) {
            return;
        } else {
            trg_print_linebreak(p);
            trt_pair_indent_node ind_node2 = trp_second_half_node(node, ind_node1.indent);
            ind_node2.indent.type = trd_indent_in_node_divided;
            trt_pck_indent tmp = {trp_wrapper_if_last_sibling(ipck.wrapper, node.last_one), ind_node2.indent};
            trp_print_divided_node(ind_node2.node, ppck, tmp, mll, p);
        }

    }
}

void
trp_print_divided_node(trt_node node, trt_pck_print ppck, trt_pck_indent ipck, uint32_t mll, trt_printing* p)
{
    trt_pair_indent_node ind_node = trp_try_normal_indent_in_node(node, ppck, ipck, mll);

    if(ind_node.indent.type == trd_indent_in_node_failed) {
        /* nothing can be done, continue as usual */
        ind_node.indent.type = trd_indent_in_node_divided;
    }

    trp_print_line(ind_node.node, ppck, (trt_pck_indent){ipck.wrapper, ind_node.indent}, p);

    const ly_bool entire_node_was_printed = trp_indent_in_node_are_eq(ipck.in_node, ind_node.indent);
    if(!entire_node_was_printed) {
        trg_print_linebreak(p);
        /* continue with second half node */
        ind_node = trp_second_half_node(node, ind_node.indent);
        /* continue with printing node */
        trp_print_divided_node(ind_node.node, ppck, (trt_pck_indent){ipck.wrapper, ind_node.indent}, mll, p);
    } else { 
        return;
    }
}

trt_pair_indent_node
trp_first_half_node(trt_node node, trt_indent_in_node ind)
{
    trt_pair_indent_node ret = {ind, node};

    if(ind.btw_name_opts == trd_linebreak) {
        ret.node.name.type = trp_opts_keys_are_set(node.name) ?
            trd_node_listLeaflist : node.name.type;
        ret.node.type = trp_empty_type();
        ret.node.iffeatures = trp_empty_iffeature();
    } else if(ind.btw_opts_type == trd_linebreak) {
        ret.node.type = trp_empty_type();
        ret.node.iffeatures = trp_empty_iffeature();
    } else if(ind.btw_type_iffeatures == trd_linebreak) {
        ret.node.iffeatures = trp_empty_iffeature();
    }

    return ret;
}

trt_pair_indent_node
trp_second_half_node(trt_node node, trt_indent_in_node ind)
{
    trt_pair_indent_node ret = {ind, node};

    if(ind.btw_name_opts < 0) {
        /* Logically, the information up to token <opts> should be deleted,
         * but the the trp_print_node function needs it to create
         * the correct indent.
         */
        ret.indent.btw_name_opts = 0;
        ret.indent.btw_opts_type = trp_type_is_empty(node.type) ?
            0 : trd_indent_before_type;
        ret.indent.btw_type_iffeatures = trp_iffeature_is_empty(node.iffeatures) ?
            0 : trd_indent_before_iffeatures;
    } else if(ind.btw_opts_type == trd_linebreak) {
        ret.node.name.type = trp_opts_keys_are_set(node.name) ?
            trd_node_listLeaflist : node.name.type;
        ret.indent.btw_name_opts = 0;
        ret.indent.btw_opts_type = 0;
        ret.indent.btw_type_iffeatures = trp_iffeature_is_empty(node.iffeatures) ?
            0 : trd_indent_before_iffeatures;
    } else if(ind.btw_type_iffeatures == trd_linebreak) {
        ret.node.name.type = trp_opts_keys_are_set(node.name) ?
            trd_node_listLeaflist : node.name.type;
        ret.node.type = trp_empty_type();
        ret.indent.btw_name_opts = 0;
        ret.indent.btw_opts_type = 0;
        ret.indent.btw_type_iffeatures = 0;
    }
    return ret;
}

trt_indent_in_node
trp_indent_in_node_place_break(trt_indent_in_node ind)
{
    /* somewhere must be set a line break in node */
    trt_indent_in_node ret = ind;
    /* gradually break the node from the end */
    if(ind.btw_type_iffeatures != trd_linebreak && ind.btw_type_iffeatures != 0) {
        ret.btw_type_iffeatures = trd_linebreak;
    } else if(ind.btw_opts_type != trd_linebreak && ind.btw_opts_type != 0) {
        ret.btw_opts_type = trd_linebreak;
    } else if(ind.btw_name_opts != trd_linebreak && ind.btw_name_opts != 0) {
        /* set line break between name and opts */
        ret.btw_name_opts = trd_linebreak;
    } else {
        /* it is not possible to place a more line breaks,
         * unfortunately the max_line_length constraint is violated
         */
        ret.type = trd_indent_in_node_failed;
    }
    return ret;
}

trt_pair_indent_node
trp_try_normal_indent_in_node(trt_node n, trt_pck_print p, trt_pck_indent ind, uint32_t mll)
{
    trt_counter cnt = {0};
    /* inject print function with strlen */
    trt_injecting_strlen func = {&cnt, trp_injected_strlen, 0};
    /* count number of printed bytes */
    trp_print_line(n, p, ind, &func);

    trt_pair_indent_node ret = {ind.in_node, n};

    if(cnt.bytes <= mll) {
        /* success */
        return ret;
    } else {
        ret.indent = trp_indent_in_node_place_break(ret.indent);
        if(ret.indent.type != trd_indent_in_node_failed) {
            /* erase information in node due to line break */
            ret = trp_first_half_node(n, ret.indent);
            /* check if line fits, recursive call */
            ret = trp_try_normal_indent_in_node(ret.node, p, (trt_pck_indent){ind.wrapper, ret.indent}, mll);
            /* make sure that the result will be with the status divided
             * or eventually with status failed */
            ret.indent.type = ret.indent.type == trd_indent_in_node_failed ?
                trd_indent_in_node_failed : trd_indent_in_node_divided;
        }
        return ret;
    }
}

/* ----------- <Definition of tree browsing functions> ----------- */

void
trb_print_family_tree(trd_wrapper_type wr_t, struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    trt_wrapper wr = wr_t == trd_wrapper_top ?
        trp_init_wrapper_top() :
        trp_init_wrapper_body();

    uint32_t total_parents = trb_get_number_of_siblings(pc->fp.modify, tc);
    struct trt_parent_cache ca = tro_empty_parent_cache();
    uint32_t max_gap_before_type = trb_try_unified_indent(ca, pc, tc);

    for(uint32_t i = 0; i < total_parents; i++) {
        trg_print_linebreak(&pc->print);
        trb_print_subtree_nodes(max_gap_before_type, wr, ca, pc, tc);
        pc->fp.modify.next_sibling(ca, tc);
    }
}

void
trb_print_subtree_nodes(uint32_t max_gap_before_type, trt_wrapper wr, struct trt_parent_cache ca, struct trt_printer_ctx *pc, struct trt_tree_ctx *tc)
{
    trb_print_entire_node(max_gap_before_type, wr, ca, pc, tc);
    /* go to the actual node's child */
    struct trt_parent_cache new_ca = tro_parent_cache_for_child(ca, tc->pn);
    trt_node node = pc->fp.modify.next_child(ca, tc);
    if (!trp_node_is_empty(node)) {
        /* print root's nodes */
        trb_print_nodes(wr, new_ca, pc, tc);
        /* get back from child node to actual node */
        pc->fp.modify.parent(tc);
  }
}

void
trb_print_nodes(trt_wrapper wr, struct trt_parent_cache ca, struct trt_printer_ctx *pc, struct trt_tree_ctx *tc)
{
    /* if node is last sibling, then do not add '|' to wrapper */
    wr = trb_parent_is_last_sibling(pc->fp, tc) ?
        trp_wrapper_set_shift(wr) : trp_wrapper_set_mark(wr);

    /* try unified indentation in node */
    const uint32_t max_gap_before_type = trb_try_unified_indent(ca, pc, tc);

    ly_bool sibling_flag = 0;
    ly_bool child_flag = 0;

    /* print all siblings */
    do {
    /* print linebreak before printing actual node */
    trg_print_linebreak(&pc->print);
    /* print node */
    trb_print_entire_node(max_gap_before_type, wr, ca, pc, tc);

    struct trt_parent_cache new_ca = tro_parent_cache_for_child(ca, tc->pn);
    /* go to the actual node's child or stay in actual node */
    trt_node node = pc->fp.modify.next_child(ca, tc);
    child_flag = !trp_node_is_empty(node);

    if(child_flag) {
        /* print all childs - recursive call */
        trb_print_nodes(wr, new_ca, pc, tc);
        /* get back from child node to actual node */
        pc->fp.modify.parent(tc);
    }

    /* go to the actual node's sibling */
    node = pc->fp.modify.next_sibling(ca, tc);
    sibling_flag = !trp_node_is_empty(node);

    /* go to the next sibling or stay in actual node */
    } while (sibling_flag);
}

void
trb_print_entire_node(uint32_t max_gap_before_type, trt_wrapper wr, struct trt_parent_cache ca, struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    trt_node node = pc->fp.read.node(ca, tc);
    trt_indent_in_node ind = trp_default_indent_in_node(node);
    if(max_gap_before_type > 0 && node.type.type != trd_type_empty) {
        /* print actual node with unified indent */
        ind.btw_opts_type = trb_calc_btw_opts_type(node.name, max_gap_before_type);
    }
    /* else - print actual node with default indent */
    trp_print_entire_node(node, (trt_pck_print){tc, pc->fp.print},
        (trt_pck_indent){wr, ind},
        pc->max_line_length, &pc->print);
}

ly_bool
trb_parent_is_last_sibling(struct trt_fp_all fp, struct trt_tree_ctx* tc)
{
    if(fp.modify.parent(tc)) {
        ly_bool ret = fp.read.if_sibling_exists(tc);
        fp.modify.next_child(tro_empty_parent_cache(), tc);
        return !ret;
    } else {
        return !fp.read.if_sibling_exists(tc);
    }
}

uint32_t
trb_get_number_of_siblings(struct trt_fp_modify_ctx fp, struct trt_tree_ctx* tc)
{
    /* including actual node */
    fp.first_sibling(tc);
    uint32_t ret = 1;
    trt_node node = trp_empty_node();
    while(!trp_node_is_empty(node = fp.next_sibling(tro_empty_parent_cache(), tc))) {
        ret++;
    }
    fp.first_sibling(tc);
    return ret;
}

int32_t
trb_strlen_of_name_and_mark(trt_node_name name)
{
    size_t name_len = strlen(name.str);
    if(name.type == trd_node_choice || name.type == trd_node_case) {
        /* counting also parentheses */
        name_len += 2;
    }
    return trp_mark_is_used(name) ?
        ((int32_t)(name_len + trd_opts_mark_length)) * (-1) :
        (int32_t)name_len;
}

trt_indent_btw
trb_calc_btw_opts_type(trt_node_name name, trt_indent_btw max_len4all)
{
    int32_t name_len = trb_strlen_of_name_and_mark(name);
    /* negative value indicate that in name is some opt mark */
    trt_indent_btw min_len = name_len < 0 ?
        trd_indent_before_type - trd_opts_mark_length :
        trd_indent_before_type;
    trt_indent_btw ret = trg_abs(max_len4all) - trg_abs(name_len);
    /* correction -> negative indicate that name is too long. */
    return ret < 0 ? min_len : ret;
}

int32_t
trb_maxlen_node_name(struct trt_parent_cache ca, struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    pc->fp.modify.first_sibling(tc);
    int32_t ret = 0;
    for(trt_node node = pc->fp.read.node(ca, tc);
        !trp_node_is_empty(node);
        node = pc->fp.modify.next_sibling(ca, tc))
    {
        int32_t maxlen = trb_strlen_of_name_and_mark(node.name);
        ret = trg_abs(maxlen) > trg_abs(ret) ? maxlen : ret; 
    }
    pc->fp.modify.first_sibling(tc);
    return ret;
}

trt_indent_btw
trb_max_btw_opts_type4siblings(struct trt_parent_cache ca, struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    int32_t maxlen_node_name = trb_maxlen_node_name(ca, pc, tc);
    trt_indent_btw ind_before_type = maxlen_node_name < 0 ?
        trd_indent_before_type - 1 : /* mark was present */
        trd_indent_before_type;
    return trg_abs(maxlen_node_name) + ind_before_type;
}

uint32_t
trb_try_unified_indent(struct trt_parent_cache ca, struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    return trb_max_btw_opts_type4siblings(ca, pc, tc);
}

/* ----------- <Definition of trm main functions> ----------- */

void
trm_print_sections(struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    trm_print_module_section(pc, tc);
    trp_cnt_linebreak_reset(&pc->print);

    trm_print_augmentations(pc, tc);
    trp_cnt_linebreak_reset(&pc->print);

    trm_print_rpcs(pc, tc);
    trp_cnt_linebreak_reset(&pc->print);

    trm_print_notifications(pc, tc);
    trp_cnt_linebreak_reset(&pc->print);

    trm_print_groupings(pc, tc);
    trp_cnt_linebreak_reset(&pc->print);

    trm_print_yang_data(pc, tc);
    trp_cnt_linebreak_reset(&pc->print);

    trg_print_linebreak(&pc->print);
}

void
trm_print_module_section(struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    trp_print_keyword_stmt(pc->fp.read.module_name(tc), pc->max_line_length, &pc->print);
    /* check if module section contains any data */
    if(tc->tpn != NULL)
        trb_print_family_tree(trd_wrapper_top, pc, tc);
}

void
trm_print_augmentations(struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    tc->index_within_section = -1;
    ly_bool once = 1;
    for(trt_keyword_stmt ks = pc->fp.modify.next_augment(tc);
        !trp_keyword_stmt_is_empty(ks); 
        ks = pc->fp.modify.next_augment(tc))
    {
        if(once) {
            trg_print_linebreak(&pc->print);
            trg_print_linebreak(&pc->print);
            once = 0;
        } else {
            trg_print_linebreak(&pc->print);
        }
        trm_print_body_section(ks, pc, tc);
    }
}

void
trm_print_rpcs(struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    tc->index_within_section = -1;
    trt_keyword_stmt rpc = pc->fp.modify.get_rpcs(tc);
    if(!trp_keyword_stmt_is_empty(rpc)) {
        trg_print_linebreak(&pc->print);
        trg_print_linebreak(&pc->print);
        trm_print_body_section(rpc, pc, tc);
    }
}

void
trm_print_notifications(struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    tc->index_within_section = -1;
    trt_keyword_stmt notifs = pc->fp.modify.get_notifications(tc);
    if(!trp_keyword_stmt_is_empty(notifs)) {
        trg_print_linebreak(&pc->print);
        trg_print_linebreak(&pc->print);
        trm_print_body_section(notifs, pc, tc);
    }
}

void
trm_print_groupings(struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    tc->index_within_section = -1;
    ly_bool once = 1;
    for(trt_keyword_stmt ks = pc->fp.modify.next_grouping(tc);
        !trp_keyword_stmt_is_empty(ks); 
        ks = pc->fp.modify.next_grouping(tc))
    {
        if(once) {
            trg_print_linebreak(&pc->print);
            trg_print_linebreak(&pc->print);
            once = 0;
        } else {
            trg_print_linebreak(&pc->print);
        }
        trm_print_body_section(ks, pc, tc);
    }
}

void
trm_print_yang_data(struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    tc->index_within_section = -1;
    ly_bool once = 1;
    for(trt_keyword_stmt ks = pc->fp.modify.next_yang_data(tc);
        !trp_keyword_stmt_is_empty(ks); 
        ks = pc->fp.modify.next_yang_data(tc))
    {
        if(once) {
            trg_print_linebreak(&pc->print);
            trg_print_linebreak(&pc->print);
            once = 0;
        } else {
            trg_print_linebreak(&pc->print);
        }
        trm_print_body_section(ks, pc, tc);
    }
}

void
trm_print_body_section(trt_keyword_stmt ks, struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    if(trp_keyword_stmt_is_empty(ks))
        return;
    trp_print_keyword_stmt(ks, pc->max_line_length, &pc->print);
    trb_print_family_tree(trd_wrapper_body, pc, tc);
}

struct trt_printer_ctx
trm_default_printer_ctx(struct ly_out *out, uint32_t max_line_length)
{
    return (struct trt_printer_ctx)
    {
        {out, trg_print_by_ly_print, 0},
        {
            {tro_modi_parent, tro_modi_first_sibling, tro_modi_next_sibling,
                tro_modi_next_child, tro_modi_next_augment, tro_modi_get_rpcs,
                tro_modi_get_notifications, tro_modi_next_grouping, tro_modi_next_yang_data},
            {tro_read_module_name, tro_read_node, tro_read_if_sibling_exists},
            {tro_print_features_names, tro_print_keys}
        },
        max_line_length
    };
}

struct trt_tree_ctx
trm_default_tree_ctx(const struct lys_module *module, struct trt_printer_ctx* pc)
{
    return (struct trt_tree_ctx)
    {
        trd_sect_module,
        -1,
        -1,
        module,
        module->parsed->data,
        module->parsed->data,
        {
            0,
            0,
            &pc->print.cnt_linebreak,
        }
    };
}


/* ----------- <Definition of tree functions> ----------- */

struct trt_parent_cache
tro_empty_parent_cache()
{
    return (struct trt_parent_cache)
    {
        trd_ancestor_else,
        LYS_STATUS_CURR,
        LYS_CONFIG_W,
        NULL
    };
}

ly_bool
tro_lysp_list_has_keys(const struct lysp_node* pn)
{
    const struct lysp_node_list* list = (const struct lysp_node_list*) pn;
    return trg_charptr_has_data(list->key);
}

ly_bool
tro_lysp_node_has_iffeature(const struct lysp_qname *iffs)
{
    LY_ARRAY_COUNT_TYPE u;
    ly_bool ret = 0;

    LY_ARRAY_FOR(iffs, u) {
        ret = 1;
        break;
    }
    return ret;
}

ly_bool
tro_lysp_leaf_is_key(struct trt_parent_cache ca, const struct trt_tree_ctx* tc)
{
    const struct lysp_node_leaf* leaf = (const struct lysp_node_leaf*) tc->pn;
    const struct lysp_node_list* list = ca.last_list;
    if(list == NULL)
        return 0;
    return trg_charptr_has_data(list->key) ? 
        trg_word_is_present(list->key, leaf->name, trd_opts_keys_delim[0]) : 0;
}

ly_bool
tro_lysp_container_has_presence(const struct lysp_node* pn)
{
    return trg_charptr_has_data(((struct lysp_node_container*)pn)->presence);
}

const char*
tro_lysp_leaflist_refpath(const struct lysp_node* pn)
{
    const struct lysp_node_leaflist* list = (const struct lysp_node_leaflist*) pn;
    return list->type.path != NULL ? list->type.path->expr : NULL;
}

const char*
tro_lysp_leaflist_type_name(const struct lysp_node* pn)
{
    const struct lysp_node_leaflist* list = (const struct lysp_node_leaflist*) pn;
    return list->type.name;
}

const char*
tro_lysp_leaf_refpath(const struct lysp_node* pn)
{
    const struct lysp_node_leaf* leaf = (const struct lysp_node_leaf*) pn;
    return leaf->type.path != NULL ? leaf->type.path->expr : NULL;
}

const char*
tro_lysp_leaf_type_name(const struct lysp_node* pn)
{
    const struct lysp_node_leaf* leaf = (const struct lysp_node_leaf*) pn;
    return leaf->type.name;
}

const char* 
tro_lysp_node_charptr(uint16_t flags, trt_get_charptr_func f, const struct lysp_node* pn)
{
    if(pn->nodetype & flags) {
        const char* ret = f(pn);
        return trg_charptr_has_data(ret) ? ret : NULL;
    } else 
        return NULL;
}

trt_status_type
tro_lysp_flags2status(uint16_t flags)
{
    return flags & LYS_STATUS_OBSLT ?   trd_status_type_obsolete :
        flags & LYS_STATUS_DEPRC ?      trd_status_type_deprecated :
        trd_status_type_current;
}

trt_flags_type
tro_lysp_flags2config(uint16_t flags)
{
    return flags & LYS_CONFIG_R ?
        trd_flags_type_ro : trd_flags_type_rw;
}

trt_keyword_stmt
tro_read_module_name(const struct trt_tree_ctx* a)
{
    assert(a != NULL && a->module != NULL && a->module->name != NULL);
    return (trt_keyword_stmt)
    {
        trd_keyword_module,
        a->module->name
    };
}

trt_node
tro_read_node(struct trt_parent_cache ca, const struct trt_tree_ctx* tc)
{
    assert(tc != NULL && tc->pn != NULL);
    const struct lysp_node *pn = tc->pn;
    assert(pn->nodetype != LYS_UNKNOWN);

    trt_node ret = trp_empty_node();

    /* remember:
     * - LYS_INTPUT and LYS_OUTPUT lysp_node don't have
     *      flags, name and iffeatures element in their structure.
     */

    /* <status> */
    ret.status =
        /* LYS_INPUT and LYS_OUTPUT is special case */
        pn->nodetype & (LYS_INPUT | LYS_OUTPUT) ?       tro_lysp_flags2status(ca.lys_status) :
        /* if ancestor's status is deprc or obslt */
        ca.lys_status & (LYS_STATUS_DEPRC | LYS_STATUS_OBSLT)
            /* and also node's status is not set */
            && !(pn->flags & (LYS_STATUS_CURR | LYS_STATUS_DEPRC | LYS_STATUS_OBSLT)) ?
                /* get ancestor's status */
                tro_lysp_flags2status(ca.lys_status) :
                /* else get node's status */
                tro_lysp_flags2status(pn->flags);

    /* TODO: trd_flags_type_mount_point aka "mp" is not supported right now. */
    /* <flags> */
    ret.flags = 
        pn->nodetype & LYS_INPUT 
            || ca.ancestor == trd_ancestor_rpc_input ?      trd_flags_type_rpc_input_params :
        pn->nodetype & LYS_OUTPUT
            || ca.ancestor == trd_ancestor_rpc_output ?     trd_flags_type_ro :
        ca.ancestor == trd_ancestor_notif ?                 trd_flags_type_ro :
        pn->nodetype & LYS_NOTIF ?                          trd_flags_type_notif :
        pn->nodetype & LYS_USES ?                           trd_flags_type_uses_of_grouping :
        pn->nodetype & (LYS_RPC | LYS_ACTION) ?             trd_flags_type_rpc :
        /* if config is not set then look at ancestor's config and get his config */
        !(pn->flags & (LYS_CONFIG_R | LYS_CONFIG_W)) ?      tro_lysp_flags2config(ca.lys_config) :
        tro_lysp_flags2config(pn->flags);

    /* TODO: trd_node_top_level1 aka '/' is not supported right now. */
    /* TODO: trd_node_top_level2 aka '@' is not supported right now. */
    /* set type of the node */
    ret.name.type =
        pn->nodetype & (LYS_INPUT | LYS_OUTPUT) ?       trd_node_else :
        pn->nodetype & LYS_CASE ?                       trd_node_case :
        pn->nodetype & LYS_CHOICE
            && !(pn->flags & LYS_MAND_TRUE) ?           trd_node_optional_choice :
        pn->nodetype & LYS_CHOICE ?                     trd_node_choice :
        pn->nodetype & LYS_CONTAINER
            && tro_lysp_container_has_presence(pn) ?    trd_node_container :
        pn->nodetype & LYS_LIST
            && tro_lysp_list_has_keys(pn) ?             trd_node_keys :
        pn->nodetype & (LYS_LIST | LYS_LEAFLIST) ?      trd_node_listLeaflist :
        pn->nodetype & (LYS_ANYDATA | LYS_ANYXML)
            && !(pn->flags & LYS_MAND_TRUE) ?           trd_node_optional :
        pn->nodetype & LYS_LEAF
            && !(pn->flags & LYS_MAND_TRUE)
            && !tro_lysp_leaf_is_key(ca, tc)?           trd_node_optional :
        trd_node_else;

    /* TODO: ret.name.module_prefix is not supported right now. */
    ret.name.module_prefix = NULL;

    /* set node's name */
    ret.name.str =
        pn->nodetype & (LYS_INPUT) ?    trd_node_name_rpc_input :
        pn->nodetype & (LYS_OUTPUT) ?   trd_node_name_rpc_output :
        pn->name;

    /* <type> */
    const char* tmp = NULL;
    if((tmp = tro_lysp_node_charptr(LYS_LEAFLIST, tro_lysp_leaflist_refpath, pn))) {
        ret.type = (trt_type){trd_type_target, tmp};
    } else if((tmp = tro_lysp_node_charptr(LYS_LEAFLIST, tro_lysp_leaflist_type_name, pn))) {
        ret.type = (trt_type){trd_type_name, tmp};
    } else if((tmp = tro_lysp_node_charptr(LYS_LEAF, tro_lysp_leaf_refpath, pn))) {
        ret.type = (trt_type){trd_type_target, tmp};
    } else if((tmp = tro_lysp_node_charptr(LYS_LEAF, tro_lysp_leaf_type_name, pn))) {
        ret.type = (trt_type){trd_type_name, tmp};
    } else if((pn->nodetype & LYS_ANYDATA) == LYS_ANYDATA) {
        ret.type = (trt_type){trd_type_name, trd_type_anydata_keyword};
    } else if(pn->nodetype & LYS_ANYXML) {
        ret.type = (trt_type){trd_type_name, trd_type_anyxml_keyword};
    } else {
        ret.type = trp_empty_type();
    }

    /* <iffeature> */
    ret.iffeatures =
        pn->nodetype & (LYS_INPUT | LYS_OUTPUT) ?   trp_empty_iffeature() :
        tro_lysp_node_has_iffeature(pn->iffeatures);

    ret.last_one = !tro_read_if_sibling_exists(tc);

    return ret;
}

ly_bool
tro_read_if_sibling_exists(const struct trt_tree_ctx* tc)
{
    /* this code is unfortunately very duplicated with the function tro_modi_next_sibling.
     * What's more, changes in the function tro_modi_next_sibling can be reflected in this function.
     */
    assert(tc != NULL && tc->pn != NULL && tc->module != NULL && tc->module->parsed != NULL);
    if(tc->pn->nodetype & (LYS_RPC | LYS_ACTION)){
        if(tc->section == trd_sect_rpcs && tc->tpn == tc->pn) {
            const struct lysp_action *arr = tc->module->parsed->rpcs;
            return arr && tc->index_within_section + 1 < (int64_t)LY_ARRAY_COUNT(arr);
        } else {
            const struct lysp_action* arr_actions = lysp_node_actions(tc->pn->parent);
            const struct lysp_notif* arr_notifs = lysp_node_notifs(tc->pn->parent);
            if(tc->index + 1 < (int64_t)LY_ARRAY_COUNT(arr_actions)) {
                return 1;
            } else if(arr_notifs){
                return 1;
            } else {
                return 0;
            }
        }
    } else if(tc->pn->nodetype & LYS_INPUT) {
        const struct lysp_action *parent = (struct lysp_action *)tc->pn->parent;
        const struct lysp_node *output_data = parent->output.data;
        /* if output action has data */
        return output_data ? 1 : 0;
    } else if(tc->pn->nodetype & LYS_OUTPUT) {
        return 0;
    } else if(tc->pn->nodetype & LYS_NOTIF) {
        if(tc->section == trd_sect_notif && tc->tpn == tc->pn) {
            const struct lysp_notif *arr = tc->module->parsed->notifs;
            return arr && tc->index_within_section + 1 < (int64_t)LY_ARRAY_COUNT(arr);
        } else {
            const struct lysp_notif* arr_notifs = lysp_node_notifs(tc->pn->parent);
            if(tc->index + 1 < (int64_t)LY_ARRAY_COUNT(arr_notifs)) {
                return 1;
            } else {
                return 0;
            }
        }
    } else {
        if(tc->pn->next != NULL) {
            return 1;
        } else {
            if(tc->pn->parent != NULL) {
                const struct lysp_action* arr_actions = lysp_node_actions(tc->pn->parent);
                const struct lysp_notif* arr_notifs = lysp_node_notifs(tc->pn->parent);
                if(arr_actions) {
                    return 1;
                } else if(arr_notifs) {
                    return 1;
                } else {
                    return 0;
                }
            } else {
                return 0;
            }
        }
    }
}

/* --------- <Modify getters> --------- */
ly_bool
tro_modi_parent(struct trt_tree_ctx* tc)
{
    assert(tc != NULL && tc->pn != NULL);
    /* If no parent exists, stay in actual node. */
    if(tc->pn != tc->tpn) {
        tc->pn = tc->pn->parent;
        return 1;
    } else {
        return 0;
    }
}

void
tro_modi_first_sibling(struct trt_tree_ctx* tc)
{
    assert(tc != NULL && tc->pn != NULL && tc->module != NULL && tc->module->parsed != NULL);

    if(tro_modi_parent(tc)) {
        tro_modi_next_child(tro_empty_parent_cache(), tc);
    } else {
        /* current node is top-node */

        struct lysp_module *pm = tc->module->parsed;

        switch(tc->section) {
        case trd_sect_module:
            tc->pn = pm->data;
            break;
        case trd_sect_augment:
            tc->pn = pm->augments[tc->index_within_section].child;
            break;
        case trd_sect_rpcs:
            tc->index_within_section = 0;
            tc->pn = (struct lysp_node*) pm->rpcs;
            break;
        case trd_sect_notif:
            tc->index_within_section = 0;
            tc->pn = (struct lysp_node*) pm->notifs;
            break;
        case trd_sect_grouping:
            tc->pn = pm->groupings[tc->index_within_section].data;
            break;
        case trd_sect_yang_data:
            /*TODO: yang-data is not supported now */
            break;
        }

        /* update pointer to top-node */
        tc->tpn = tc->pn;
    }
}

#define NEXT_SIBLING_BY_PARSED_TREE() {\
    if(arr && tc->index_within_section + 1 < (int64_t)LY_ARRAY_COUNT(arr)) {\
        tc->index_within_section++;\
        tc->pn = (const struct lysp_node*) (&(arr[tc->index_within_section]));\
        tc->tpn = tc->pn;\
        return tro_read_node(ca, tc);\
    } else {\
        return trp_empty_node();\
    }\
}

trt_node
tro_modi_next_sibling(struct trt_parent_cache ca, struct trt_tree_ctx* tc)
{
    /* if you make changes in this function, please make sure to
     * make a change to function tro_read_if_sibling_exists as well,
     * as it is essentially the same.
     */
    assert(tc != NULL && tc->pn != NULL && tc->module != NULL && tc->module->parsed != NULL);

    if(tc->pn->nodetype & (LYS_RPC | LYS_ACTION)){
        /* if current section is rpcs and current node is top-node */
        if(tc->section == trd_sect_rpcs && tc->tpn == tc->pn) {
            /* then next sibling is the next action located in parsed module */
            const struct lysp_action *arr = tc->module->parsed->rpcs;
            /* return next action or empty node */
            NEXT_SIBLING_BY_PARSED_TREE()
        /* else next sibling is located in parent */
        } else {
            const struct lysp_action* arr_actions = lysp_node_actions(tc->pn->parent);
            const struct lysp_notif* arr_notifs = lysp_node_notifs(tc->pn->parent);
            /* if next action exists in parent node */
            if(tc->index + 1 < (int64_t)LY_ARRAY_COUNT(arr_actions)) {
                tc->index++;
                tc->pn = (const struct lysp_node*) (&(arr_actions[tc->index]));
                /* return next action */
                return tro_read_node(ca, tc);
            /* if some notification exists in parent node */
            } else if(arr_notifs) {
                tc->index = 0;
                tc->pn = (const struct lysp_node*)arr_notifs;
                /* return first notification */
                return tro_read_node(ca, tc);
            } else {
                /* return empty node because sibling does not exist */
                return trp_empty_node();
            }
        }
    /* if current node is input action */
    } else if(tc->pn->nodetype & LYS_INPUT) {
        const struct lysp_action *parent = (struct lysp_action *)tc->pn->parent;
        const struct lysp_node *output_data = parent->output.data;
        /* if output action has data */
        if(output_data) {
            /* then next sibling is output action */
            tc->pn = (struct lysp_node*)&parent->output;
            return tro_read_node(ca, tc);
        } else {
            /* else input action has no sibling */
            return trp_empty_node();
        }
    /* if current node is output action */
    } else if(tc->pn->nodetype & LYS_OUTPUT) {
        /* then next sibling does not exist */
        return trp_empty_node();
    /* if current node is notification */
    } else if(tc->pn->nodetype & LYS_NOTIF) {
        /* if current section is notifications and current node is top-node */
        if(tc->section == trd_sect_notif && tc->tpn == tc->pn) {
            /* then next sibling is the next action located in parsed module */
            const struct lysp_notif *arr = tc->module->parsed->notifs;
            /* return next notification or empty node */
            NEXT_SIBLING_BY_PARSED_TREE()
        } else {
            /* else next sibling is located in parent */
            const struct lysp_notif* arr_notifs = lysp_node_notifs(tc->pn->parent);
            /* if next notification exists in parent node */
            if(tc->index + 1 < (int64_t)LY_ARRAY_COUNT(arr_notifs)) {
                tc->index++;
                tc->pn = (const struct lysp_node*) (&(arr_notifs[tc->index]));
                /* return next notification */
                return tro_read_node(ca, tc);
            } else {
                /* return empty node because sibling does not exist */
                return trp_empty_node();
            }
        }
    } else {
        /* else actual node is some node with 'next' element */
        if (tc->pn->next != NULL) {
            /* if current node is top-node */
            if(tc->tpn == tc->pn) {
                /* shift pointer to top-node too */
                tc->tpn = tc->pn->next;
            }
            tc->pn = tc->pn->next;
            /* return next sibling by 'next' element */
            return tro_read_node(ca, tc);
        } else {
            /* if parent exists */
            if(tc->pn->parent != NULL) {
                const struct lysp_action* arr_actions = lysp_node_actions(tc->pn->parent);
                const struct lysp_notif* arr_notifs = lysp_node_notifs(tc->pn->parent);
                /* if action exists in parent node */
                if(arr_actions) {
                    tc->index = 0;
                    tc->pn = (const struct lysp_node*) arr_actions;
                    /* return next sibling as action */
                    return tro_read_node(ca, tc);
                /* if notification exists in parent node */
                } else if(arr_notifs) {
                    tc->index = 0;
                    tc->pn = (const struct lysp_node*) arr_notifs;
                    /* return next sibling as notification */
                    return tro_read_node(ca, tc);
                } else {
                    /* sibling node does not exist */
                    return trp_empty_node();
                }
            } else {
                /* sibling node does not exist */
                return trp_empty_node();
            }
        }
    }
}

struct trt_parent_cache
tro_parent_cache_for_child(struct trt_parent_cache ca, const struct lysp_node *pn)
{
    struct trt_parent_cache ret = {};

    ret.ancestor =
        pn->nodetype & (LYS_INPUT) ?    trd_ancestor_rpc_input :
        pn->nodetype & (LYS_OUTPUT) ?   trd_ancestor_rpc_output :
        pn->nodetype & (LYS_NOTIF) ?    trd_ancestor_notif :
        ca.ancestor;

    ret.lys_status =
        pn->flags & (LYS_STATUS_CURR | LYS_STATUS_DEPRC | LYS_STATUS_OBSLT) ? pn->flags :
        ca.lys_status;

    ret.lys_config =
        ca.ancestor == trd_ancestor_rpc_input ?         0 : /* because <flags> will be -w */
        ca.ancestor == trd_ancestor_rpc_output ?        LYS_CONFIG_R :
        pn->flags & (LYS_CONFIG_R | LYS_CONFIG_W) ?     pn->flags :
        ca.lys_config;

    ret.last_list =
        pn->nodetype & (LYS_LIST) ?     (struct lysp_node_list*)pn :
        ca.last_list;

    return ret;
}

trt_node
tro_modi_next_child(struct trt_parent_cache ca, struct trt_tree_ctx* tc)
{
    assert(tc != NULL && tc->pn != NULL);

    struct trt_parent_cache new_ca = tro_parent_cache_for_child(ca, tc->pn);

    if(tc->pn->nodetype & (LYS_ACTION | LYS_RPC)) {
        const struct lysp_action *act = (struct lysp_action *)tc->pn;
        const struct lysp_node *input_data = act->input.data;
        const struct lysp_node *output_data = act->output.data;
        if(input_data) {
            /* go to LYS_INPUT */
            tc->pn = (const struct lysp_node*) &act->input;
            return tro_read_node(new_ca, tc);
        } else if(output_data){
            /* go to LYS_OUTPUT */
            tc->pn = (const struct lysp_node*) &act->output;
            return tro_read_node(new_ca, tc);
        } else {
            /* input action and output action are not set */
            return trp_empty_node();
        }
    } else {
        const struct lysp_node *pn = lysp_node_children(tc->pn);
        if(pn != NULL) {
            tc->pn = pn;
            return tro_read_node(new_ca, tc);
        } else {
            /* current node can't have children or has no children */
            const struct lysp_action* arr_actions = lysp_node_actions(tc->pn);
            const struct lysp_notif* arr_notifs = lysp_node_notifs(tc->pn);
            if(arr_actions) {
                tc->index = 0;
                tc->pn = (const struct lysp_node*)arr_actions;
                return tro_read_node(new_ca, tc);
            } else if(arr_notifs) {
                tc->index = 0;
                tc->pn = (const struct lysp_node*)arr_notifs;
                return tro_read_node(new_ca, tc);
            } else {
                return trp_empty_node();
            }
        }
    }
}

trt_keyword_stmt
tro_modi_next_augment(struct trt_tree_ctx* tc)
{
    assert(tc != NULL && tc->module != NULL && tc->module->parsed != NULL);
    const struct lysp_augment *arr = tc->module->parsed->augments;
    tc->section = trd_sect_augment;
    if(arr && tc->index_within_section + 1 < (int64_t)LY_ARRAY_COUNT(arr)) {
        tc->index_within_section++;
        const struct lysp_augment *item = &(arr[tc->index_within_section]);
        if(item->child) {
            tc->pn = item->child;
            tc->tpn = tc->pn;
            return (trt_keyword_stmt){trd_keyword_augment, item->nodeid};
        }
    }
    return trp_empty_keyword_stmt();
}

trt_keyword_stmt
tro_modi_get_rpcs(struct trt_tree_ctx* tc)
{
    assert(tc != NULL && tc->module != NULL && tc->module->parsed != NULL);
    const struct lysp_action *arr = tc->module->parsed->rpcs;
    if(arr == NULL)
        return trp_empty_keyword_stmt();

    tc->section = trd_sect_rpcs;
    tc->pn = (const struct lysp_node*)arr;
    tc->tpn = tc->pn;
    tc->index_within_section = 0;
    return (trt_keyword_stmt){trd_keyword_rpc, NULL};
}

trt_keyword_stmt
tro_modi_get_notifications(struct trt_tree_ctx* tc)
{
    assert(tc != NULL && tc->module != NULL && tc->module->parsed != NULL);
    const struct lysp_notif *arr = tc->module->parsed->notifs;
    if(arr == NULL)
        return trp_empty_keyword_stmt();

    tc->section = trd_sect_notif;
    tc->pn = (const struct lysp_node*)arr;
    tc->tpn = tc->pn;
    tc->index_within_section = 0;
    return (trt_keyword_stmt){trd_keyword_notif, NULL};
}

trt_keyword_stmt
tro_modi_next_grouping(struct trt_tree_ctx* tc)
{
    assert(tc != NULL && tc->module != NULL && tc->module->parsed != NULL);
    const struct lysp_grp *arr = tc->module->parsed->groupings;
    tc->section = trd_sect_grouping;
    if(arr && tc->index_within_section + 1 < (int64_t)LY_ARRAY_COUNT(arr)) {
        tc->index_within_section++;
        const struct lysp_grp *item = &(arr[tc->index_within_section]);
        if(item->data) {
            tc->pn = item->data;
            tc->tpn = tc->pn;
            return (trt_keyword_stmt){trd_keyword_grouping, item->name};
        }
    }
    return trp_empty_keyword_stmt();
}

trt_keyword_stmt
tro_modi_next_yang_data(struct trt_tree_ctx* tc)
{
    tc->section = trd_sect_yang_data;
    /* TODO: yang-data is not supported */
    return trp_empty_keyword_stmt();
}

/* --------- <Print getters> --------- */
void
tro_print_features_names(const struct trt_tree_ctx* a, trt_printing* p)
{
    const struct lysp_qname *iffs = a->pn->iffeatures;

    LY_ARRAY_COUNT_TYPE i;
    LY_ARRAY_FOR(iffs, i) {
        if(i == 0) {
            trp_print(p, 1, iffs[i].str);
        } else {
            trp_print(p, 2, trd_iffeatures_delimiter, iffs[i].str);
        }
    }

}

void
tro_print_keys(const struct trt_tree_ctx* a, trt_printing* p)
{
    const struct lysp_node *pn = a->pn;
    if(pn->nodetype != LYS_LIST)
        return;
    struct lysp_node_list* list = (struct lysp_node_list*) pn;
    if(trg_charptr_has_data(list->key)) {
        trp_print(p, 1, list->key);
    }
}


/* ----------- <Definition of the other functions> ----------- */

#define PRINT_N_TIMES_BUFFER_SIZE 16

void
trg_print_n_times(int32_t n, char c, trt_printing* p)
{
    if(n <= 0)
        return;
    
    static char buffer[PRINT_N_TIMES_BUFFER_SIZE];
    const uint32_t buffer_size = PRINT_N_TIMES_BUFFER_SIZE;
    buffer[buffer_size-1] = '\0';
    for(uint32_t i = 0; i < n / (buffer_size-1); i++) {
        memset(&buffer[0], c, buffer_size-1);
        trp_print(p, 1, &buffer[0]);
    }
    uint32_t rest = n % (buffer_size-1);
    buffer[rest] = '\0';
    memset(&buffer[0], c, rest);
    trp_print(p, 1, &buffer[0]);
}

void
trg_print_by_ly_print(void *out, int arg_count, va_list ap)
{
    if(arg_count <= 0)
        return;

    for(int i = 0; i < arg_count; i++) {
        char* item = va_arg(ap, char*);
        if(item)
            ly_print_(out, "%s", item);
    }
}

uint32_t
trg_abs(int32_t a)
{
    return a < 0 ? a * (-1) : a;
}

ly_bool
trg_test_bit(uint64_t number, uint32_t bit)
{
    return (number >> bit) & 1U;
}

void
trg_print_linebreak(trt_printing* p)
{
    trp_cnt_linebreak_increment(p);
    trp_print(p, 1, trd_separator_linebreak);
}

const char*
trg_print_substr(const char* str, size_t len, trt_printing* p)
{
    for(size_t i = 0; i < len; i++) {
        trg_print_n_times(1, str[0], p);
        str++;
    }
    return str;
}

ly_bool
trg_charptr_has_data(const char* ptr)
{
    return ptr != NULL && ptr[0] != '\0';
}

ly_bool
trg_word_is_present(const char* src, const char* word, char delim)
{
    if(src == NULL || src[0] == '\0' || word == NULL)
        return 0;

    const char* hit = strstr(src, word);
    if(hit) {
        /* word was founded at the begin of src
         * OR it match somewhere after delim
         */
        if(hit == src || *(hit - 1) == delim) {
            /* end of word was founded at the end of src
             * OR end of word was match somewhere before delim
             */
            char delim_or_end = (hit + strlen(word))[0];
            if(delim_or_end == '\0' || delim_or_end == delim)
                return 1;
        }
        /* else -> hit is just substr and it's not the whole word */
        /* jump to the next word */
        for(; src[0] != '\0' && src[0] != delim; src++);
        /* skip delim */
        src = src[0] == '\0' ? src : src + 1;
        /* continue with searching */
        return trg_word_is_present(src, word, delim);
    } else {
        return 0;
    }
}

/* ----------- <Definition of module interface> ----------- */

LY_ERR tree_print_parsed_and_compiled_module(struct ly_out *out, const struct lys_module *module, uint32_t UNUSED(options), size_t line_length)
{
    line_length = line_length == 0 ? 72 : line_length;
    struct trt_printer_ctx pc = trm_default_printer_ctx(out, line_length);
    struct trt_tree_ctx tc = trm_default_tree_ctx(module, &pc);
    trm_print_sections(&pc, &tc);
    return LY_SUCCESS;
}

//LY_ERR tree_print_submodule(struct ly_out *out, const struct lys_module *module, const struct lysp_submodule *submodp, uint32_t options, size_t line_length)
LY_ERR tree_print_submodule(struct ly_out *UNUSED(out), const struct lys_module *UNUSED(module), const struct lysp_submodule *UNUSED(submodp), uint32_t UNUSED(options), size_t UNUSED(line_length))
{
    return LY_SUCCESS;
}

//LY_ERR tree_print_compiled_node(struct ly_out *out, const struct lysc_node *node, uint32_t options, size_t line_length)
LY_ERR tree_print_compiled_node(struct ly_out *UNUSED(out), const struct lysc_node *UNUSED(node), uint32_t UNUSED(options), size_t UNUSED(line_length))
{
    return LY_SUCCESS;
}

/* -######-- Definitions end -#######- */
