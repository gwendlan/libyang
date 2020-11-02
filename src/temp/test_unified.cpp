#include <iostream>
#include <vector>
#include <string>
#include "print_func.hpp"
#include "node_rel.hpp"
#include "unit_testing.hpp"

extern "C"
{
#include "new.h"
}

/* Comment trt_tree_ctx struct definition in new.h */

using Node = Node;
using Tree = Tree;
using tree_iter = tree_iter;
using out_t = Out::VecLines;
using std::string;
out_t out;

int main()
{

UNIT_TESTING_START;

TEST(tryUnifIndent, allFits)
{
    Tree tree
    ({
        {"A", {"Bnode", "Cnode"}}
    }, 
    {
        {"Bnode", {trd_node_listLeaflist, "type", false}}
    });
    string check1 = "  +--rw A";
    string check2 = "     +--rw Bnode*   type";
    string check_unif =        "^       ^";
    string check3 = "     +--rw Cnode";
    uint32_t mll = 72;
    out_t check = {check1, check2, check3};
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    struct trt_printer_ctx pc = 
    {
        (trt_printing){&out, Out::print_vecLines, 0},
        { /* trt_fp_all */
            {parent, next_sibling, next_child, NULL, NULL, NULL, NULL, NULL},
            {NULL, node, next_sibling_read},
            {NULL, NULL}
        },
        mll
    };
    pc.fp.modify.next_child(&ctx);
    trt_wrapper wr = trp_wrapper_set_shift(trp_init_wrapper_top());
    uint32_t uut = trb_try_unified_indent(wr, &pc, &ctx);
    EXPECT_EQ(uut, check_unif.size());
    pc.fp.modify.parent(&ctx);
    trb_print_subtree_nodes(trp_init_wrapper_top(), &pc, &ctx);
    EXPECT_EQ(out, check);
    out.clear();
}

TEST(tryUnifIndent, biggerHasPriority)
{
    Tree tree
    ({
        {"A", {"Bnode", "CnodeIsBigger"}}
    }, 
    {
        {"Bnode", {trd_node_listLeaflist, "type", false}},
        {"CnodeIsBigger", {trd_node_else, "type", false}}
    });
    string check1 = "  +--rw A";
    string check2 = "     +--rw Bnode*           type";
    string check3 = "     +--rw CnodeIsBigger    type";
    string check_unif =        "^               ^";
    uint32_t mll = 72;
    out_t check = {check1, check2, check3};
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    struct trt_printer_ctx pc = 
    {
        (trt_printing){&out, Out::print_vecLines, 0},
        { /* trt_fp_all */
            {parent, next_sibling, next_child, NULL, NULL, NULL, NULL, NULL},
            {NULL, node, next_sibling_read},
            {NULL, NULL}
        },
        mll
    };
    pc.fp.modify.next_child(&ctx);
    trt_wrapper wr = trp_wrapper_set_shift(trp_init_wrapper_top());
    uint32_t uut = trb_try_unified_indent(wr, &pc, &ctx);
    EXPECT_EQ(uut, check_unif.size());
    pc.fp.modify.parent(&ctx);
    trb_print_subtree_nodes(trp_init_wrapper_top(), &pc, &ctx);
    EXPECT_EQ(out, check);
    out.clear();
}

TEST(tryUnifIndent, biggestIsTooBig)
{
    Tree tree
    ({
        {"A", {"Bnode", "CnodeIsBigger", "Dnode", "E", "G"}}
    }, 
    {
        {"Bnode", {trd_node_listLeaflist, "type", false}},
        {"CnodeIsBigger", {trd_node_else, "type", false}},
        {"E", {trd_node_else, "longType", false}},
        {"G", {trd_node_listLeaflist, "type", false}}
    });
    string check1 = "  +--rw A";
    string check2 = "     +--rw Bnode*   type";
    string check_unif =        "^       ^";
    string check3 = "     +--rw CnodeIsBigger";
    string check4 = "     |       type";
    string check5 = "     +--rw Dnode";
    string check6 = "     +--rw E        longType";
    string check7 = "     +--rw G*       type";
    uint32_t
    mll = string(   "                             ^").size();
    out_t check = 
    {check1, check2, check3, check4, check5, check6, check7};
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    struct trt_printer_ctx pc = 
    {
        (trt_printing){&out, Out::print_vecLines, 0},
        { /* trt_fp_all */
            {parent, next_sibling, next_child, NULL, NULL, NULL, NULL, NULL},
            {NULL, node, next_sibling_read},
            {NULL, NULL}
        },
        mll
    };
    pc.fp.modify.next_child(&ctx);
    trt_wrapper wr = trp_wrapper_set_shift(trp_init_wrapper_top());
    uint32_t uut = trb_try_unified_indent(wr, &pc, &ctx);
    EXPECT_EQ(uut, check_unif.size());
    pc.fp.modify.parent(&ctx);
    trb_print_subtree_nodes(trp_init_wrapper_top(), &pc, &ctx);
    EXPECT_EQ(out, check);
    out.clear();
}

TEST(tryUnifIndent, TwoBiggestAreTooBig)
{
    Tree tree
    ({
        {"A", {"Bnode", "CnodeIsBigger", "Dnode", "E", "FnodeIsBigToo", "G"}}
    }, 
    {
        {"Bnode", {trd_node_listLeaflist, "type", false}},
        {"CnodeIsBigger", {trd_node_else, "type", false}},
        {"E", {trd_node_else, "longType", false}},
        {"FnodeIsBigToo", {trd_node_else, "Ftype", false}},
        {"G", {trd_node_listLeaflist, "type", false}}
    });
    string check1 = "  +--rw A";
    string check2 = "     +--rw Bnode*   type";
    string check_unif =        "^       ^";
    string check3 = "     +--rw CnodeIsBigger";
    string check4 = "     |       type";
    string check5 = "     +--rw Dnode";
    string check6 = "     +--rw E        longType";
    string check7 = "     +--rw FnodeIsBigToo";
    string check8 = "     |       Ftype";
    string check9 = "     +--rw G*       type";
    uint32_t
    mll = string(   "                             ^").size();
    out_t check = {check1, check2, check3, check4, check5,
        check6, check7, check8, check9};
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    struct trt_printer_ctx pc = 
    {
        (trt_printing){&out, Out::print_vecLines, 0},
        { /* trt_fp_all */
            {parent, next_sibling, next_child, NULL, NULL, NULL, NULL, NULL},
            {NULL, node, next_sibling_read},
            {NULL, NULL}
        },
        mll
    };
    pc.fp.modify.next_child(&ctx);
    trt_wrapper wr = trp_wrapper_set_shift(trp_init_wrapper_top());
    uint32_t uut = trb_try_unified_indent(wr, &pc, &ctx);
    EXPECT_EQ(uut, check_unif.size());
    pc.fp.modify.parent(&ctx);
    trb_print_subtree_nodes(trp_init_wrapper_top(), &pc, &ctx);
    EXPECT_EQ(out, check);
    out.clear();
}

TEST(tryUnifIndent, allIsTooBig)
{
    Tree tree
    ({
        {"A", {"BnodeIsBig", "CnodeIsBig"}}
    }, 
    {
        {"BnodeIsBig", {trd_node_listLeaflist, "type", false}},
        {"CnodeIsBig", {trd_node_else, "type", false}}
    });
    string check1 = "  +--rw A";
    string check2 = "     +--rw BnodeIsBig*";
    string check3 = "     |       type";
    string check4 = "     +--rw CnodeIsBig";
    string check5 = "     |       type";
    string check_unif = ""; /* zero */
    uint32_t
    mll = string(   "                          ^").size();
    out_t check = {check1, check2, check3, check4, check5};
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    struct trt_printer_ctx pc = 
    {
        (trt_printing){&out, Out::print_vecLines, 0},
        { /* trt_fp_all */
            {parent, next_sibling, next_child, NULL, NULL, NULL, NULL, NULL},
            {NULL, node, next_sibling_read},
            {NULL, NULL}
        },
        mll
    };
    pc.fp.modify.next_child(&ctx);
    trt_wrapper wr = trp_wrapper_set_shift(trp_init_wrapper_top());
    uint32_t uut = trb_try_unified_indent(wr, &pc, &ctx);
    EXPECT_EQ(uut, check_unif.size());
    out.clear();
}

PRINT_TESTS_STATS();

UNIT_TESTING_END;

}
