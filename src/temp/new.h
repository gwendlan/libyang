
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

#ifndef NEW_H_
#define NEW_H_

#include <stdint.h> /* uint_, int_ */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h> /* NULL */

#define LY_ERR int
#define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
typedef uint8_t ly_bool;
struct ly_out;
struct lys_module;
struct lysp_submodule;
struct lysc_node;


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
void trp_cnt_linebreak_reset(trt_printing*);
/** Increment .cnt_linebreak by one. */
void trp_cnt_linebreak_increment(trt_printing*);

/**
 * @brief Print variadic number of char* pointers.
 *
 * @param[in] p struct ly_out* or other auxiliary structure for printing.
 * @param[in] arg_count number of arguments in va_list.
 */
void trp_print(trt_printing* p, int arg_count, ...);

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
void trp_injected_strlen(void *out, int arg_count, va_list ap); 

typedef struct
{
    const char* src;
    const char* substr_start;
    size_t substr_size;
} trt_breakable_str;

trt_breakable_str trp_empty_breakable_str();
trt_breakable_str trp_set_breakable_str(const char*);
bool trp_breakable_str_is_empty(trt_breakable_str);;
bool trp_breakable_str_begin_will_be_printed(trt_breakable_str);
bool trp_breakable_str_end_will_be_printed(trt_breakable_str);
void trp_print_breakable_str(trt_breakable_str, trt_printing);

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
    void (*print_features_names)(const struct trt_tree_ctx*, trt_printing*);   /**< Print list of features. */
    void (*print_keys)(const struct trt_tree_ctx*, trt_printing*);            /**< Print list's keys. */
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

/** Create trt_indent_in_node as empty. */
trt_indent_in_node trp_empty_indent_in_node();
/** Check that they can be considered equivalent. */
ly_bool trp_indent_in_node_are_eq(trt_indent_in_node, trt_indent_in_node);

/**
 * @brief Writing a linebreak constant.
 *
 * The order where the linebreak tag can be placed is from the end.
 *
 * @param[in] item containing alignment lengths or already line break marks
 * @return with a newly placed linebreak tag.
 * @return .type = trd_indent_in_node_failed if it is not possible to place a more line breaks.
 */
trt_indent_in_node trp_indent_in_node_place_break(trt_indent_in_node item);

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
trt_wrapper trp_init_wrapper_top();

/** Get wrapper related to e.g. Augmenations or Groupings. */
trt_wrapper trp_init_wrapper_body();

/** Setting '|' symbol because node is divided or it is not last sibling. */
trt_wrapper trp_wrapper_set_mark(trt_wrapper);

/** Setting ' ' symbol because node is last sibling. */
trt_wrapper trp_wrapper_set_shift(trt_wrapper);

/** Test if they are equivalent. */
ly_bool trp_wrapper_eq(trt_wrapper, trt_wrapper);

/** Print "  |  " sequence. */
void trp_print_wrapper(trt_wrapper, trt_printing*);


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
void trp_print_status(trt_status_type, trt_printing*);

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
    trd_flags_type_rw,
    trd_flags_type_ro,
    trd_flags_type_rpc_input_params,
    trd_flags_type_uses_of_grouping,
    trd_flags_type_rpc,
    trd_flags_type_notif,
    trd_flags_type_mount_point,
} trt_flags_type;

/** Print <flags>. */
void trp_print_flags(trt_flags_type, trt_printing*);
/** Get size of the <flags>. */
size_t trp_print_flags_strlen(trt_flags_type);

/* ================================== */
/* ----------- <node_name> ----------- */
/* ================================== */

static const char trd_node_name_prefix_choice[] = "(";
static const char trd_node_name_prefix_case[] = ":(";
static const char trd_node_name_suffix_choice[] = ")";
static const char trd_node_name_suffix_case[] = ")";
static const char trd_node_name_triple_dot[] = "...";

/**
 * @brief Type of the node.
 */
typedef enum
{
    trd_node_else = 0,              /**< For some node which does not require special treatment. */
    trd_node_case,                  /**< For case node. */
    trd_node_choice,                /**< For choice node. */
    trd_node_optional_choice,       /**< For choice node with optional mark (?). */
    trd_node_optional,              /**< For an optional leaf, anydata, or anyxml. */
    trd_node_container,             /**< For a presence container. */
    trd_node_listLeaflist,          /**< For a leaf-list or list (without keys). */
    trd_node_keys,                  /**< For a list's keys. */
    trd_node_top_level1,            /**< For a top-level data node in a mounted module. */
    trd_node_top_level2,            /**< For a top-level data node of a module identified in a mount point parent reference. */
    trd_node_triple_dot             /**< For collapsed sibling nodes and their children. */
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
trt_node_name trp_empty_node_name();
/** Check if trt_node_name is empty. */
ly_bool trp_node_name_is_empty(trt_node_name);
/** Print entire trt_node_name structure. */
void trp_print_node_name(trt_node_name, trt_printing*);
/** Check if mark (?, !, *, /, @) is implicitly contained in trt_node_name. */
ly_bool trp_mark_is_used(trt_node_name);

/* ============================== */
/* ----------- <opts> ----------- */
/* ============================== */

static const char trd_opts_optional[] = "?";        /**< For an optional leaf, choice, anydata, or anyxml. */
static const char trd_opts_container[] = "!";       /**< For a presence container. */
static const char trd_opts_list[] = "*";            /**< For a leaf-list or list. */
static const char trd_opts_slash[] = "/";           /**< For a top-level data node in a mounted module. */
static const char trd_opts_at_sign[] = "@";         /**< For a top-level data node of a module identified in a mount point parent reference. */
static const size_t trd_opts_mark_length = 1;       /**< Every opts mark has a length of one. */

typedef const char* trt_opts_keys_prefix;
static const char trd_opts_keys_prefix[] = "[";
typedef const char* trt_opts_keys_suffix;
static const char trd_opts_keys_suffix[] = "]";

/** Check if [<keys>] are present in node. */
ly_bool trp_opts_keys_are_set(trt_node_name);

/** 
 * @brief Print opts keys.
 *
 * @param[in] k flag if keys is present.
 * @param[in] ind number of spaces between name and [keys].
 * @param[in] pf basically a pointer to the function that prints the keys.
 * @param[in,out] p basically a pointer to a function that handles the printing itself.
 */
void trp_print_opts_keys(trt_node_name name, trt_indent_btw ind, trt_cf_print_keys pf, trt_printing *p);

/* ============================== */
/* ----------- <type> ----------- */
/* ============================== */

typedef const char* trt_type_leafref;
static const char trd_type_leafref_keyword[] = "leafref";
typedef const char* trt_type_target_prefix;
static const char trd_type_target_prefix[] = "-> ";

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
trt_type trp_empty_type();
/** Check if trt_type is empty. */
ly_bool trp_type_is_empty(trt_type);
/** Print entire trt_type structure. */
void trp_print_type(trt_type, trt_printing*);

/* ==================================== */
/* ----------- <iffeatures> ----------- */
/* ==================================== */

typedef const char* trt_iffeatures_prefix;
static const char trd_iffeatures_prefix[] = "{";
typedef const char* trt_iffeatures_suffix;
static const char trd_iffeatures_suffix[] = "}?";

/** 
 * @brief List of features in node.
 *
 * The iffeature is just ly_bool because printing is not provided by the printer component (trp).
 */
typedef ly_bool trt_iffeature;

/** Create trt_iffeature and note the presence of features. */
trt_iffeature trp_set_iffeature();
/** Create empty trt_iffeature and note the absence of features. */
trt_iffeature trp_empty_iffeature();
/** Check if trt_iffeature is empty. */
ly_bool trp_iffeature_is_empty(trt_iffeature);

/**
 * @brief Print all iffeatures of node 
 *
 * @param[in] i flag if keys is present.
 * @param[in] pf basically a pointer to the function that prints the list of features.
 * @param[in,out] p basically a pointer to a function that handles the printing itself.
 */
void trp_print_iffeatures(trt_iffeature i, trt_cf_print_iffeatures pf, trt_printing *p);

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
    trt_status_type status;          /**< <status>. */
    trt_flags_type flags;            /**< <flags>. */
    trt_node_name name;         /**< <node> with <opts> mark or [<keys>]. */
    trt_type type;              /**< <type> is the name of the type for leafs and leaf-lists. */
    trt_iffeature iffeatures;   /**< <if-features>. Printing function required. */
} trt_node;

/** Create trt_node as empty. */
trt_node trp_empty_node();
/** Check if trt_node is empty. */
ly_bool trp_node_is_empty(trt_node);
/** Check if [<keys>], <type> and <iffeatures> are empty/not_set. */
ly_bool trp_node_body_is_empty(trt_node);
/** Print just <status>--<flags> <name> with opts mark. */
void trp_print_node_up_to_name(trt_node, trt_printing*);
/** Print alignment (spaces) instead of <status>--<flags> <name> for divided node. */
void trp_print_divided_node_up_to_name(trt_node, trt_printing*);

/**
 * @brief Print trt_node structure.
 *
 * @param[in] n node structure for printing.
 * @param[in] ppck package of functions for printing [<keys>] and <iffeatures>.
 * @param[in] ind indent in node.
 * @param[in,out] p basically a pointer to a function that handles the printing itself.
 */
void trp_print_node(trt_node n, trt_pck_print ppck, trt_indent_in_node ind, trt_printing *p);

/**
 * @brief Check if leafref target must be change to string 'leafref' because his target string is too long.
 *
 * @param[in] n node containing leafref target.
 * @param[in] wr for node immersion depth.
 * @param[in] mll max line length border.
 * @return true if must be change to string 'leafref'.
 */
ly_bool trp_leafref_target_is_too_long(trt_node n, trt_wrapper wr, uint32_t mll);

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
trt_pair_indent_node trp_first_half_node(trt_node node, trt_indent_in_node ind);

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
trt_pair_indent_node trp_second_half_node(trt_node node, trt_indent_in_node ind);

/** Get default indent in node based on node values. */
trt_indent_in_node trp_default_indent_in_node(trt_node);

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
 * @brief Type of the trt_keyword_stmt.
 */
typedef enum
{
    trd_keyword_stmt_top = 0,   /**< Indicates the section with the keyword module. */
    trd_keyword_stmt_body,      /**< Indicates the section with the keyword e.g. augment, grouping.*/
} trt_keyword_stmt_type;

/**
 * @brief Type of the trt_keyword.
 */
typedef enum 
{
    trd_keyword_module = 0,     /**< Used when trd_keyword_stmt_top is set. */
    trd_keyword_submodule,      /**< Used when trd_keyword_stmt_top is set. */
    trd_keyword_augment,        /**< Used when trd_keyword_stmt_body is set. */
    trd_keyword_rpc,            /**< Used when trd_keyword_stmt_body is set. */
    trd_keyword_notif,          /**< Used when trd_keyword_stmt_body is set. */
    trd_keyword_grouping,       /**< Used when trd_keyword_stmt_body is set. */
    trd_keyword_yang_data       /**< Used when trd_keyword_stmt_body is set. */
} trt_keyword_type;

/**
 * @brief Main sign of the tree nodes.
 */
typedef struct
{
    trt_keyword_stmt_type type; /**< Type of the keyword_stmt. */
    trt_keyword_type keyword;   /**< String containing some of the top or body keyword. */
    const char* str;            /**< Name or path, it determines the type. */
} trt_keyword_stmt;

/** Create trt_keyword_stmt as empty. */
trt_keyword_stmt trp_empty_keyword_stmt();
/** Check if trt_keyword_stmt is empty. */
ly_bool trp_keyword_stmt_is_empty(trt_keyword_stmt);
/** Print .keyword based on .type. */
void trt_print_keyword_stmt_begin(trt_keyword_stmt, trt_printing*);
/** Print .str which is string of name or path. */
void trt_print_keyword_stmt_str(trt_keyword_stmt, uint32_t mll, trt_printing*);
/** Print separator based on .type. */
void trt_print_keyword_stmt_end(trt_keyword_stmt, trt_printing*);
/** Print entire trt_keyword_stmt structure. */
void trp_print_keyword_stmt(trt_keyword_stmt ks, uint32_t mll, trt_printing *p);
/** Get string length of stored keyword. */
size_t trp_keyword_type_strlen(trt_keyword_type);

/* ======================================== */
/* ----------- <Modify getters> ----------- */
/* ======================================== */

/**
 * @brief Functions that change the state of the tree_ctx structure.
 *
 * For all, if the value cannot be returned,
 * its empty version obtained by relevant trp_empty* function is returned.
 */
struct trt_fp_modify_ctx
{
    trt_node (*parent)(struct trt_tree_ctx*);                       /**< Jump to parent node. */
    trt_node (*next_sibling)(struct trt_tree_ctx*);                 /**< Jump to next sibling of the current node. */
    trt_node (*next_child)(struct trt_tree_ctx*);                   /**< Jump to the child of the current node. */
    trt_keyword_stmt (*next_augment)(struct trt_tree_ctx*);         /**< Jump to the augment section. */
    trt_keyword_stmt (*get_rpcs)(struct trt_tree_ctx*);             /**< Jump to the rpcs section. */
    trt_keyword_stmt (*get_notifications)(struct trt_tree_ctx*);    /**< Jump to the notifications section. */
    trt_keyword_stmt (*next_grouping)(struct trt_tree_ctx*);        /**< Jump to the grouping section. */
    trt_keyword_stmt (*next_yang_data)(struct trt_tree_ctx*);       /**< Jump to the yang-data section. */
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
    trt_keyword_stmt (*module_name)(const struct trt_tree_ctx*);    /**< Get name of the module. */
    trt_node (*node)(const struct trt_tree_ctx*);                   /**< Get current node. */
    trt_node (*next_sibling)(const struct trt_tree_ctx*);           /**< Get next sibling of the current node. */
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
void trp_print_line(trt_node, trt_pck_print, trt_pck_indent, trt_printing*);

/** Printing one line including wrapper and <status>--<flags> <name><option_mark>. */
void trp_print_line_up_to_node_name(trt_node, trt_wrapper, trt_printing*);

/* --------- <Printing node> --------- */

/** Printing of the wrapper and the whole node, which can be divided into several lines. */
void trp_print_entire_node(trt_node, trt_pck_print, trt_pck_indent, uint32_t mll, trt_printing*);

/** Auxiliary function for trp_print_entire_node that prints split nodes. */
void trp_print_divided_node(trt_node, trt_pck_print, trt_pck_indent, uint32_t mll, trt_printing*);

/* --------- <Special> --------- */

/**
 * @brief Get the correct alignment for the node.
 *
 * @return .type == trd_indent_in_node_divided - the node does not fit in the line, some .trt_indent_btw has negative value as a line break sign.
 * @return .type == trd_indent_in_node_normal - the node fits into the line, all .trt_indent_btw values has non-negative number.
 * @return .type == trd_indent_in_node_failed - the node does not fit into the line, all .trt_indent_btw has negative or zero values, function failed.
 */
trt_pair_indent_node trp_try_normal_indent_in_node(trt_node, trt_pck_print, trt_pck_indent, uint32_t mll);

/* ======================================== */
/* --------- <Main trb functions> --------- */
/* ======================================== */

/* --------- <Printing tree> --------- */

/** 
 * @brief Print all parents and their children. 
 * 
 * Function call print_subtree_nodes for all parents.
 * Use this function after 'module' keyword or 'augment' and so.
 */
void trb_print_family_tree(trd_wrapper_type, struct trt_printer_ctx*, struct trt_tree_ctx*);

/**
 * @brief Print subtree of nodes.
 *
 * The current node is expected to be the root of the subtree.
 * Before root node is no linebreak printing. This must be addressed by the caller.
 * Root node will also be printed. Behind last printed node is no linebreak.
 */
void trb_print_subtree_nodes(trt_wrapper, struct trt_printer_ctx*, struct trt_tree_ctx*);

/**
 * @brief For the current node: recursively print all of its child nodes and all of its siblings, including their children.
 *
 * This function is an auxiliary function for trb_print_subtree_nodes.
 * The parent of the current node is expected to exist.
 * Nodes are printed, including unified sibling node alignment (align <type> to column).
 * Side-effect -> current node is set to the last sibling.
 */
void trb_print_nodes(trt_wrapper, struct trt_printer_ctx*, struct trt_tree_ctx*);

/* --------- <For browse tree> --------- */

/** Modify trt_tree_ctx so that current node is first sibling. */
void trb_jump_to_first_sibling(struct trt_fp_modify_ctx, struct trt_tree_ctx*);

/**
 * @brief Get number of siblings.
 *
 * Side-effect -> current node is set to the first sibling.
 */
uint32_t trb_get_number_of_siblings(struct trt_fp_modify_ctx, struct trt_tree_ctx*);

/**
 * @brief Check if parent of the current node is the last of his siblings.
 *
 * Side-effect -> current node is set to the first sibling.
 */
ly_bool trb_parent_is_last_sibling(struct trt_fp_all, struct trt_tree_ctx*);

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
uint32_t trb_try_unified_indent(trt_wrapper, struct trt_printer_ctx*, struct trt_tree_ctx*);

/**
 * @brief Calculate the btw_opts_type indent size for a particular node.
 *
 * @param[in] name is the node for which we get btw_opts_type.
 * @param[in] max_len is the maximum value of btw_opts_type that it can have.
 * @return btw_opts_type for node.
 */
trt_indent_btw trb_calc_btw_opts_type(trt_node_name, trt_indent_btw max_len);

/**
 * @brief Print node.
 *
 * This function is wrapper for trp_print_entire_node function.
 * But difference is that take max_gap_before_type parameter which will be used to set the unified alignment.
 */
void trb_print_entire_node(uint32_t max_gap_before_type, trt_wrapper, struct trt_printer_ctx*, struct trt_tree_ctx*);

/**
 * @brief Get size of node name.
 * @return positive value total size of the node name.
 * @return negative value as an indication that option mark is included in the total size.
 */
int32_t trb_strlen_of_name_and_mark(trt_node_name);

/**
 * @brief Find sibling with the biggest node name and return that size.
 *
 * Side-effect -> Current node is set to the first sibling.
 * @param[in] upper_limit delimits the return value from above.
 * @return positive number lesser than upper_limit as a sign that only the node name is included in the size.
 * @return negative number whose absolute value is less than upper_limit and sign that node name and his opt mark is included in the size.
 */
int32_t trb_maxlen_node_name(struct trt_printer_ctx*, struct trt_tree_ctx*, int32_t upper_limit);

/**
 * @brief Find sibling with the nth biggest node name and return that size.
 *
 * Function has the same return value as trb_maxlen_node_name but for nth biggest node name.
 * Side-effect -> Current node is set to the first sibling.
 */
int32_t trb_nth_maxlen_node_name(uint32_t nth, struct trt_printer_ctx*, struct trt_tree_ctx*);

/**
 * @brief Find sibling with the nth biggest node name.
 *
 * Side-effect -> Current node is set to the first sibling.
 * @return max btw_opts_type value for rest of the siblings
 */
trt_indent_btw trb_max_btw_opts_type4siblings(uint32_t nth_biggest_node, struct trt_printer_ctx*, struct trt_tree_ctx*);

/* ======================================== */
/* --------- <Main trm functions> --------- */
/* ======================================== */

/** Print sections module, augment, rpcs, notifications, grouping, yang-data. */
void trm_print_sections(struct trt_printer_ctx*, struct trt_tree_ctx*);

/** Print 'module' keyword, its name and all nodes. */
void trm_print_module_section(struct trt_printer_ctx*, struct trt_tree_ctx*);

/** For all augment sections: print 'augment' keyword, its target node and all nodes. */
void trm_print_augmentations(struct trt_printer_ctx*, struct trt_tree_ctx*);

/** For rpcs section: print 'rpcs' keyword and all its nodes. */
void trm_print_rpcs(struct trt_printer_ctx*, struct trt_tree_ctx*);

/** For notifications section: print 'notifications' keyword and all its nodes. */
void trm_print_notifications(struct trt_printer_ctx*, struct trt_tree_ctx*);

/** For all grouping sections: print 'grouping' keyword, its name and all nodes. */
void trm_print_groupings(struct trt_printer_ctx*, struct trt_tree_ctx*);

/** For all yang-data sections: print 'yang-data' keyword and all its nodes. */
void trm_print_yang_data(struct trt_printer_ctx*, struct trt_tree_ctx*);

/** General function to prevent repetitiveness code. */
void trm_print_body_section(trt_keyword_stmt, struct trt_printer_ctx*, struct trt_tree_ctx*);

/**
 * @brief Get default settings for trt_printer_ctx.
 *
 * Get trt_printer_ctx containing all structure items correctly defined except for trt_printer_opts and max_line_length,
 * which are parameters of the printer tree module.
 * TODO: add correct definition, add ly_out pointer.
 */
struct trt_printer_ctx trm_default_printer_ctx(uint32_t max_line_length);

/**
 * @brief Get default settings for trt_tree_ctx.
 *
 * TODO: set pointer in trt_tree_ctx to trt_printer_ctx->print.cnt_linebreak
 */
struct trt_tree_ctx trm_default_tree_ctx(struct trt_printer_ctx*);

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

/* These flags are used in trt_options.code. */
#define TRC_OPT_SECT_MODULE         (1u << 0)   /**< Don't print module section. */
#define TRC_OPT_SECT_AUGMENT        (1u << 1)   /**< Don't print augment section. */
#define TRC_OPT_SECT_RPCS           (1u << 2)   /**< Don't print rpcs section. */
#define TRC_OPT_SECT_NOTIF          (1u << 3)   /**< Don't print notifications section. */
#define TRC_OPT_SECT_GROUPING       (1u << 4)   /**< Don't print grouping section. */
#define TRC_OPT_SECT_YANGDATA       (1u << 5)   /**< Don't print yang-data section. */
#define TRC_OPT_MAX_LB_PER_SECT     (1u << 10)  /**< The number of line breaks in one section must not exceed. Functionality is not implemented. */

#define TRC_OPT_DEFAULT 0   /**< Default settings for trt_options.code variable. */

/** Setting the behavior of this entire printer_tree module. */
typedef struct
{
    trt_opt code;
    uint32_t max_linebreaks;    /**< Max linebreaks per section. Variable is used if TRC_OPT_MAX_LB_PER_SECT in code is set. */
    uint32_t* cnt_linebreak;    /**< Pointer to trt_printing.cnt_linebreak counter. Value is used if TRC_OPT_MAX_LB_PER_SECT in code is set. */
} trt_options;

/**
 * @brief Saved information when browsing the lysp tree.
 *
 * This structure helps prevent frequent retrieval of information from the tree.
 */
typedef struct 
{
    uint16_t lys_status;    /**< Inherited status CURR, DEPRC or OBSLT. */
    uint16_t lys_config;    /**< Inherited config W or R.*/
} trt_lysp_cache;

#if 0

/**
 * @brief Main structure for browsing the libyang tree
 */
struct trt_tree_ctx
{
    trt_actual_section section;
    const struct lys_module *module;
    struct lysp_node *pn;               /**< Actual pointer to parsed node. */
    trt_lysp_cache pc;                  /**< Cache memory for browsing the lysp tree. */
    trt_options opt;                    /**< Options for printing. */
};

/* --------- <Read getters> --------- */
trt_keyword_stmt tro_read_module_name(const struct trt_tree_ctx*);
trt_node tro_read_node(const struct trt_tree_ctx*);
trt_node tro_read_next_sibling(const struct trt_tree_ctx*);

/* --------- <Modify getters> --------- */
trt_node tro_modi_parent(struct trt_tree_ctx*);
trt_node tro_modi_next_sibling(struct trt_tree_ctx*);
trt_node tro_modi_next_child(struct trt_tree_ctx*);
trt_node tro_modi_next_augment(struct trt_tree_ctx*);
trt_node tro_modi_next_rpcs(struct trt_tree_ctx*);
trt_node tro_modi_next_notifications(struct trt_tree_ctx*);
trt_node tro_modi_next_grouping(struct trt_tree_ctx*);
trt_node tro_modi_next_yang_data(struct trt_tree_ctx*);

/* --------- <Print getters> --------- */
void tro_print_features_names(const struct trt_tree_ctx*, trt_printing*);
void tro_print_keys(const struct trt_tree_ctx*, trt_printing*);

#endif

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
uint32_t trg_abs(int32_t);

/** Print character n times. */
void trg_print_n_times(int32_t n, char, trt_printing*);

/** Test if the bit on the index is set. */
ly_bool trg_test_bit(uint64_t number, uint32_t bit);

/** Print trd_separator_linebreak. */
void trg_print_linebreak(trt_printing*);

/** Print a substring but limited to the maximum length. */
const char* trg_print_substr(const char*, size_t len, trt_printing*);

/* ================================ */
/* ----------- <symbol> ----------- */
/* ================================ */

typedef const char* const trt_symbol;
static trt_symbol trd_symbol_sibling = "|";

/* ======================================== */
/* ---------- <Module interface> ---------- */
/* ======================================== */


LY_ERR tree_print_parsed_and_compiled_module(struct ly_out *out, const struct lys_module *module, uint32_t options, size_t line_length);

LY_ERR tree_print_submodule(struct ly_out *out, const struct lys_module *module, const struct lysp_submodule *submodp, uint32_t options, size_t line_length);

LY_ERR tree_print_compiled_node(struct ly_out *out, const struct lysc_node *node, uint32_t options, size_t line_length);

/* -######-- Declarations end -#######- */
#endif
