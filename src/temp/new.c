
#include "new.h"
#include <string.h> // strlen

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

    for(int i = 0; i < arg_count; i++)
        cnt->bytes += strlen(va_arg(ap, char*));
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
trp_set_iffeature()
{
    return 1;
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
        trp_empty_iffeature()
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
    trt_keyword_stmt ret;
    ret.str = NULL;
    return ret;
}

ly_bool
trp_keyword_stmt_is_empty(trt_keyword_stmt ks)
{
    return ks.str == NULL;
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
    trp_print_flags(a.flags, p);
    /* If the node is a case node, there is no space before the <name> */
    if(a.name.type != trd_node_case)
        trp_print(p, 1, trd_separator_space);
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
    case trd_keyword_stmt_top:
        switch(a.keyword) {
        case trd_keyword_module:
            trp_print(p, 1, trd_top_keyword_module);
            break;
        case trd_keyword_submodule:
            trp_print(p, 1, trd_top_keyword_submodule);
            break;
        default:
            break;
        }
        trp_print(p, 2, trd_separator_colon, trd_separator_space);
        break;
    case trd_keyword_stmt_body:
        trg_print_n_times(trd_indent_line_begin, trd_separator_space[0], p);
        switch(a.keyword) {
        case trd_keyword_augment:
            trp_print(p, 1, trd_body_keyword_augment);
            break;
        case trd_keyword_rpc:
            trp_print(p, 1, trd_body_keyword_rpc);
            break;
        case trd_keyword_notif:
            trp_print(p, 1, trd_body_keyword_notif);
            break;
        case trd_keyword_grouping:
            trp_print(p, 1, trd_body_keyword_grouping);
            break;
        case trd_keyword_yang_data:
            trp_print(p, 1, trd_body_keyword_yang_data);
            break;
        default:
            break;
        }
        trp_print(p, 1, trd_separator_space);
        break;
    default:
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
    if(a.type == trd_keyword_stmt_top) {
        trp_print(p, 1, a.str);
        return;
    }

    /* else for trd_keyword_stmt_body do */

    const char slash = trd_separator_slash[0];
    /* set begin indentation */
    const uint32_t ind_initial = trd_indent_line_begin + trp_keyword_type_strlen(a.keyword) + 1;
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
        how_far = *(sub_ptr + sub_len + 1) == '\0' ? how_far + 1 : how_far;
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
    if(a.type == trd_keyword_stmt_body)
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
            trt_pck_indent tmp = {trp_wrapper_set_mark(ipck.wrapper), ind_node2.indent};
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
            trt_pck_indent tmp = {trp_wrapper_set_mark(ipck.wrapper), ind_node2.indent};
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
    for(uint32_t i = 0; i < total_parents; i++) {
        trg_print_linebreak(&pc->print);
        trb_print_subtree_nodes(wr, pc, tc);
    }
}

void
trb_print_subtree_nodes(trt_wrapper wr, struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    /* print root node */
    trt_node root = pc->fp.read.node(tc);
    trp_print_entire_node(root, (trt_pck_print){tc, pc->fp.print},
        (trt_pck_indent){wr, trp_default_indent_in_node(root)}, pc->max_line_length, &pc->print);
    /* go to the actual node's child or stay in actual node */
    if(!trp_node_is_empty(pc->fp.modify.next_child(tc))) {
        /* print root's nodes */
        trb_print_nodes(wr, pc, tc);
        /* get back from child node to actual node */
        pc->fp.modify.parent(tc);
    }
}

void
trb_print_nodes(trt_wrapper wr, struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    /* if node is last sibling, then do not add '|' to wrapper */
    wr = trb_parent_is_last_sibling(pc->fp, tc) ?
        trp_wrapper_set_shift(wr) :
        trp_wrapper_set_mark(wr);
    /* try unified indentation in node */
    uint32_t max_gap_before_type = trb_try_unified_indent(wr, pc, tc);

    /* print all siblings */
    do {
        /* print linebreak before printing actual node */
        trg_print_linebreak(&pc->print);
        /* print node */
        trb_print_entire_node(max_gap_before_type, wr, pc, tc);
        /* go to the actual node's child or stay in actual node */
        if(!trp_node_is_empty(pc->fp.modify.next_child(tc))) {
            /* print all childs - recursive call */
            trb_print_nodes(wr, pc, tc);
            /* get back from child node to actual node */
            pc->fp.modify.parent(tc);
        }
        /* go to the next sibling or stay in actual node */
    } while(!trp_node_is_empty(pc->fp.modify.next_sibling(tc)));
}

void
trb_jump_to_first_sibling(struct trt_fp_modify_ctx fp, struct trt_tree_ctx* tc)
{
    /* expect that parent exists */
    fp.parent(tc);
    fp.next_child(tc);
}

uint32_t
trb_get_number_of_siblings(struct trt_fp_modify_ctx fp, struct trt_tree_ctx* tc)
{
    /* including actual node */
    trb_jump_to_first_sibling(fp, tc);
    uint32_t ret = 1;
    while(!trp_node_is_empty(fp.next_sibling(tc))) {
        ret++;
    }
    trb_jump_to_first_sibling(fp, tc);
    return ret;
}

int32_t
trb_strlen_of_name_and_mark(trt_node_name name)
{
    return trp_mark_is_used(name) ?
        (strlen(name.str) + trd_opts_mark_length) * (-1) :
        strlen(name.str);
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
trb_maxlen_node_name(struct trt_printer_ctx* pc, struct trt_tree_ctx* tc, int32_t upper_limit)
{
    trb_jump_to_first_sibling(pc->fp.modify, tc);
    int32_t ret = 0;
    for(trt_node node = pc->fp.read.node(tc); !trp_node_is_empty(node); node = pc->fp.modify.next_sibling(tc)) {
        int32_t maxlen = trb_strlen_of_name_and_mark(node.name);
        ret = trg_abs(maxlen) > trg_abs(ret) && trg_abs(maxlen) < trg_abs(upper_limit) ? maxlen : ret; 
    }
    trb_jump_to_first_sibling(pc->fp.modify, tc);
    return ret;
}

int32_t
trb_nth_maxlen_node_name(uint32_t nth, struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    trb_jump_to_first_sibling(pc->fp.modify, tc);
    int32_t upper_limit = INT32_MAX;
    for(uint32_t i = 0; i <= nth; i++) {
        upper_limit = trb_maxlen_node_name(pc, tc, upper_limit);
    }
    trb_jump_to_first_sibling(pc->fp.modify, tc);
    return upper_limit;
}

trt_indent_btw
trb_max_btw_opts_type4siblings(uint32_t nth_biggest_node, struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    int32_t maxlen_node_name = trb_nth_maxlen_node_name(nth_biggest_node, pc, tc);
    trt_indent_btw ind_before_type = maxlen_node_name < 0 ?
        trd_indent_before_type - 1 : /* mark was present */
        trd_indent_before_type;
    return trg_abs(maxlen_node_name) + ind_before_type;
}

uint32_t
trb_try_unified_indent(trt_wrapper wr, struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    /* expect that tc point to non-empty node */
    uint32_t ret; /* max_gap_before_type for all siblings */
    uint32_t total_siblings = trb_get_number_of_siblings(pc->fp.modify, tc);
    ly_bool succ = 0;
    /* tolerance of the number of divided nodes = tdn */
    for(uint32_t tdn = 0; tdn < total_siblings; tdn++) {
        /* get max_gap_before_type (aka unified_indent or indent_before_type) from nth node */
        ret = trb_max_btw_opts_type4siblings(tdn, pc, tc);
        uint32_t j; /* iterator over all siblings */
        uint32_t tdn_cnt = 0; /* number of divided nodes */
        /* for all nodes try if unified indent can be applied */
        for(j = 0; j < total_siblings; j++) {
            trt_node node = pc->fp.read.node(tc);
            trt_indent_in_node ind = trp_default_indent_in_node(node);

            /* calculate btw_opts_type for node from actual unified_indent */
            ind.btw_opts_type = trb_calc_btw_opts_type(node.name, ret);

            /* check if node will not be divided with indent_before_type >= 4. */
            trt_pair_indent_node ind_node = trp_try_normal_indent_in_node(
                node, (trt_pck_print){tc, pc->fp.print},
                (trt_pck_indent){wr, ind}, pc->max_line_length);

            if(ind_node.indent.type != trd_indent_in_node_normal) {
                if(tdn_cnt == tdn) {
                    /* The tolerance of the maximum number of divided nodes has been exceeded.
                     * Some node should have a unified gap with siblings but unexpectedly cannot.
                     */
                    break;
                } else {
                    /* The node with the long name will be divided.
                     * It is not possible for him to have a unified gap with siblings.
                     */
                    tdn_cnt++;
                }
            }
            /* else - node fits to the unified gap and will not be divided.
             * Success is coming. Continue with rest nodes.
             */
            pc->fp.modify.next_sibling(tc);
        }
        /* Check if all siblings was tested with max_gap_before_type (ret). */
        if(j == total_siblings) {
            /* jump out from loop. The unified max gap was finded. */
            succ = 1;
            break;
        }
        /* Try another node to create a new max unified length. */
    }

    /* tc is set to the first sibling */
    trb_jump_to_first_sibling(pc->fp.modify, tc);

    /* if all nodes will be divided anyway, then return 0.
     * Otherwise it is possible to unified least one node.
     * (Ignore that unifying spaces for one node doesn't make sense.)
     */
    return succ ? ret : 0;
}

void
trb_print_entire_node(uint32_t max_gap_before_type, trt_wrapper wr, struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    trt_node node = pc->fp.read.node(tc);
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
    fp.modify.parent(tc);
    ly_bool ret = trp_node_is_empty(fp.read.next_sibling(tc));
    fp.modify.next_child(tc);
    return ret;
}

/* ----------- <Definition of trm main functions> ----------- */

void
trm_print_sections(struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    trm_print_module_section(pc, tc);
    trg_print_linebreak(&pc->print);
    trg_print_linebreak(&pc->print);
    trp_cnt_linebreak_reset(&pc->print);

    trm_print_augmentations(pc, tc);
    trg_print_linebreak(&pc->print);
    trg_print_linebreak(&pc->print);
    trp_cnt_linebreak_reset(&pc->print);

    trm_print_rpcs(pc, tc);
    trg_print_linebreak(&pc->print);
    trg_print_linebreak(&pc->print);
    trp_cnt_linebreak_reset(&pc->print);

    trm_print_notifications(pc, tc);
    trg_print_linebreak(&pc->print);
    trg_print_linebreak(&pc->print);
    trp_cnt_linebreak_reset(&pc->print);

    trm_print_groupings(pc, tc);
    trg_print_linebreak(&pc->print);
    trg_print_linebreak(&pc->print);
    trp_cnt_linebreak_reset(&pc->print);

    trm_print_yang_data(pc, tc);
    trg_print_linebreak(&pc->print);
    trp_cnt_linebreak_reset(&pc->print);
}

void
trm_print_module_section(struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    trp_print_keyword_stmt(pc->fp.read.module_name(tc), pc->max_line_length, &pc->print);
    trg_print_linebreak(&pc->print);
    trb_print_family_tree(trd_wrapper_top, pc, tc);
}

void
trm_print_augmentations(struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    for(trt_keyword_stmt ks = pc->fp.modify.next_augment(tc);
        !trp_keyword_stmt_is_empty(ks); 
        ks = pc->fp.modify.next_augment(tc))
    {
        trm_print_body_section(ks, pc, tc);
    }
}

void
trm_print_rpcs(struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    trm_print_body_section(pc->fp.modify.get_rpcs(tc), pc, tc);
}

void
trm_print_notifications(struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    trm_print_body_section(pc->fp.modify.get_notifications(tc), pc, tc);
}

void
trm_print_groupings(struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    for(trt_keyword_stmt ks = pc->fp.modify.next_grouping(tc);
        !trp_keyword_stmt_is_empty(ks); 
        ks = pc->fp.modify.next_grouping(tc))
    {
        trm_print_body_section(ks, pc, tc);
    }
}

void
trm_print_yang_data(struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    for(trt_keyword_stmt ks = pc->fp.modify.next_yang_data(tc);
        !trp_keyword_stmt_is_empty(ks); 
        ks = pc->fp.modify.next_yang_data(tc))
    {
        trm_print_body_section(ks, pc, tc);
    }
}

void
trm_print_body_section(trt_keyword_stmt ks, struct trt_printer_ctx* pc, struct trt_tree_ctx* tc)
{
    if(trp_keyword_stmt_is_empty(ks))
        return;
    trp_print_keyword_stmt(ks, pc->max_line_length, &pc->print);
    trg_print_linebreak(&pc->print);
    trb_print_family_tree(trd_wrapper_body, pc, tc);
}

struct trt_printer_ctx
trm_default_printer_ctx(uint32_t max_line_length)
{
    /* TODO: change NULL pointers to correct pointers. */
    return (struct trt_printer_ctx)
    {
        {NULL, NULL, 0},
        {
            {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
            {NULL, NULL, NULL},
            {NULL, NULL}
        },
        max_line_length
    };
}


/* ----------- <Definition of tree functions> ----------- */

#if 0

trt_keyword_stmt
tro_read_module_name(const struct trt_tree_ctx*);

trt_node
tro_read_node(const struct trt_tree_ctx* a)
{
    struct lysp_node *pn = a->pn;
    trt_node ret = trp_empty_node();
    if((pn == NULL) || (pn->nodetype == LYS_UNKNOWN))
        return ret;

    /* define <status> */
    ret.status =
        pn->flags & LYS_STATUS_CURR  ? trd_status_type_current :
        pn->flags & LYS_STATUS_DEPRC ? trd_status_type_deprecated :
        pn->flags & LYS_STATUS_OBSLT ? trd_status_type_obsolete :
        /* TODO: inheritance */
        trd_status_type_empty;

    /* TODO: trd_flags_type_mount_point aka "mp" is not supported right now. */
    /* define <flags> */
    ret.flags = 
        pn->nodetype & LYS_INPUT ?              trd_flags_type_rpc_input_params :
        pn->nodetype & LYS_GROUPING ?           trd_flags_type_uses_of_grouping :
        pn->nodetype & (LYS_RPC | LYS_ACTION) ? trd_flags_type_rpc :
        pn->nodetype & LYS_NOTIF ?              trd_flags_type_notif :
        pn->flags & LYS_CONFIG_W ?              trd_flags_type_rw :
        pn->flags & LYS_CONFIG_R ?              trd_flags_type_ro :
        trd_flags_type_empty;

    /* TODO: trd_node_top_level1 aka '/' is not supported right now. */
    /* TODO: trd_node_top_level2 aka '@' is not supported right now. */
    ret.name.type =
        pn->nodetype & LYS_CASE ?           trd_node_case :
        pn->nodetype & LYS_CHOICE
            && pn->flags & LYS_MAND_TRUE ?  trd_node_optional_choice :
        pn->nodetype & LYS_CHOICE ?         trd_node_choice :
        pn->nodetype & LYS_CONTAINER ?      trd_node_container :
        /* TODO: trd_node_listLeaflist (without keys) */
        /* TODO: trd_node_keys [keys] */
        pn->flags & LYS_MAND_TRUE ?         trd_node_optional :
        trd_node_else;

    /* TODO: ret.name.module_prefix is not supported right now. */

    ret.name.str = pn->name;

    /* TODO: set trt_type */
    if(pn->nodetype & (LYS_LEAFLIST | LYS_LEAF) ) {
        /* ret.type.type = trd_type_target; */
        /* ret.type.type = trd_type_name; */
        /* ret.type.str = ; */
        ;
    } else {
        ret.type.type = trd_type_empty;
    }

    ret.iffeatures = pn->iffeatures != NULL;

    return ret;
}

trt_node
tro_read_next_sibling(const struct trt_tree_ctx*);

/* --------- <Modify getters> --------- */
trt_node
tro_modi_parent(struct trt_tree_ctx*);

trt_node
tro_modi_next_sibling(struct trt_tree_ctx*);

trt_node
tro_modi_next_child(struct trt_tree_ctx*);

trt_node
tro_modi_next_augment(struct trt_tree_ctx*);

trt_node
tro_modi_next_rpcs(struct trt_tree_ctx*);

trt_node
tro_modi_next_notifications(struct trt_tree_ctx*);

trt_node
tro_modi_next_grouping(struct trt_tree_ctx*);

trt_node
tro_modi_next_yang_data(struct trt_tree_ctx*);

#endif

/* --------- <Print getters> --------- */
void
tro_print_features_names(const struct trt_tree_ctx*, trt_printing*);

void
tro_print_keys(const struct trt_tree_ctx*, trt_printing*);


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

/* ----------- <Definition of module interface> ----------- */

//LY_ERR tree_print_parsed_and_compiled_module(struct ly_out *out, const struct lys_module *module, uint32_t options, size_t line_length)
LY_ERR tree_print_parsed_and_compiled_module(struct ly_out *UNUSED(out), const struct lys_module *UNUSED(module), uint32_t UNUSED(options), size_t UNUSED(line_length))
{
    return 0;
}

//LY_ERR tree_print_submodule(struct ly_out *out, const struct lys_module *module, const struct lysp_submodule *submodp, uint32_t options, size_t line_length)
LY_ERR tree_print_submodule(struct ly_out *UNUSED(out), const struct lys_module *UNUSED(module), const struct lysp_submodule *UNUSED(submodp), uint32_t UNUSED(options), size_t UNUSED(line_length))
{
    return 0;
}

//LY_ERR tree_print_compiled_node(struct ly_out *out, const struct lysc_node *node, uint32_t options, size_t line_length)
LY_ERR tree_print_compiled_node(struct ly_out *UNUSED(out), const struct lysc_node *UNUSED(node), uint32_t UNUSED(options), size_t UNUSED(line_length))
{
    return 0;
}


/* -######-- Definitions end -#######- */
