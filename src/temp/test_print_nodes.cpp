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

TEST(printNodes, oneNode)
{
    Tree tree
    ({
        Node{"A", {}}
    });
    out_t check = {"  +--rw A"};
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    struct trt_printer_ctx pc = 
    {
        (trt_printing){&out, Out::print_vecLines, 0},
        { /* trt_fp_all */
            {parent, next_sibling, next_child, NULL, NULL, NULL, NULL, NULL},
            {NULL, node, next_sibling_read},
            {NULL, NULL}
        },
        72
    };
    trb_print_subtree_nodes(trp_init_wrapper_top(), &pc, &ctx);
    EXPECT_EQ(out, check);
    out.clear();
}

TEST(printNodes, twoSiblings)
{
    Tree tree
    ({
        {"A", {"B", "C"}},
        {"B", {}}, {"C", {}}
    });
    string check1 = "  +--rw A";
    string check2 = "     +--rw B";
    string check3 = "     +--rw C";
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
        72
    };
    trb_print_subtree_nodes(trp_init_wrapper_top(), &pc, &ctx);
    EXPECT_EQ(out, check);
    out.clear();
}

TEST(printNodes, twoSiblingsFirstOneHasChild)
{
    Tree tree
    ({
        {"A", {"B", "C"}},
        {"B", {"D"}}, {"C", {}},
        {"D", {}}
    });
    string check1 = "  +--rw A";
    string check2 = "     +--rw B";
    string check3 = "     |  +--rw D";
    string check4 = "     +--rw C";
    out_t check = {check1, check2, check3, check4};
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    struct trt_printer_ctx pc = 
    {
        (trt_printing){&out, Out::print_vecLines, 0},
        { /* trt_fp_all */
            {parent, next_sibling, next_child, NULL, NULL, NULL, NULL, NULL},
            {NULL, node, next_sibling_read},
            {NULL, NULL}
        },
        72
    };
    trb_print_subtree_nodes(trp_init_wrapper_top(), &pc, &ctx);
    EXPECT_EQ(out, check);
    out.clear();
}

TEST(printNodes, twoSiblingsSecondOneHasChild)
{
    Tree tree
    ({
        {"A", {"B", "C"}},
        {"B", {}}, {"C", {"D"}},
        {"D", {}}
    });
    string check1 = "  +--rw A";
    string check2 = "     +--rw B";
    string check3 = "     +--rw C";
    string check4 = "        +--rw D";
    out_t check = {check1, check2, check3, check4};
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    struct trt_printer_ctx pc = 
    {
        (trt_printing){&out, Out::print_vecLines, 0},
        { /* trt_fp_all */
            {parent, next_sibling, next_child, NULL, NULL, NULL, NULL, NULL},
            {NULL, node, next_sibling_read},
            {NULL, NULL}
        },
        72
    };
    trb_print_subtree_nodes(trp_init_wrapper_top(), &pc, &ctx);
    EXPECT_EQ(out, check);
    out.clear();
}

TEST(printNodes, twoRoots)
{
    Tree tree
    ({
        {"A", {"B", "C"}},
        {"A", {"B", "C"}},
        {"B", {}}, {"C", {}},
        {"D", {"E", "F"}},
        {"E", {}}, {"F", {}},
    });
    string check1 = "  +--rw A";
    string check2 = "  |  +--rw B";
    string check3 = "  |  +--rw C";
    string check4 = "  +--rw D";
    string check5 = "     +--rw E";
    string check6 = "     +--rw F";
    out_t check = {check1, check2, check3, check4, check5, check6};
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    struct trt_printer_ctx pc = 
    {
        (trt_printing){&out, Out::print_vecLines, 0},
        { /* trt_fp_all */
            {parent, next_sibling, next_child, NULL, NULL, NULL, NULL, NULL},
            {NULL, node, next_sibling_read},
            {NULL, NULL}
        },
        72
    };
    trb_print_subtree_nodes(trp_init_wrapper_top(), &pc, &ctx);
    trg_print_linebreak(&pc.print);
    pc.fp.modify.next_sibling(&ctx);
    trb_print_subtree_nodes(trp_init_wrapper_top(), &pc, &ctx);
    EXPECT_EQ(out, check);
    out.clear();
}

TEST(printNodes, rootChildChildChildWithSiblings)
{
    Tree tree
    ({
        {"A", {"B", "G"}},
        {"B", {"C", "F"}},
        {"C", {"D", "E"}},
        {"D", {}},
        {"E", {}},
        {"F", {}},
        {"G", {"H", "I"}},
        {"H", {}},
        {"I", {}},
    });
    string check1 = "  +--rw A";
    string check2 = "     +--rw B";
    string check3 = "     |  +--rw C";
    string check4 = "     |  |  +--rw D";
    string check5 = "     |  |  +--rw E";
    string check6 = "     |  +--rw F";
    string check7 = "     +--rw G";
    string check8 = "        +--rw H";
    string check9 = "        +--rw I";
    out_t check = {check1, check2, check3, check4,
        check5, check6, check7, check8, check9};
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    struct trt_printer_ctx pc = 
    {
        (trt_printing){&out, Out::print_vecLines, 0},
        { /* trt_fp_all */
            {parent, next_sibling, next_child, NULL, NULL, NULL, NULL, NULL},
            {NULL, node, next_sibling_read},
            {NULL, NULL}
        },
        72
    };
    trb_print_subtree_nodes(trp_init_wrapper_top(), &pc, &ctx);
    EXPECT_EQ(out, check);
    out.clear();
}

TEST(printNodes, twoSiblingsFirstOneHasChild)
{
    Tree tree
    ({
        {"A", {"B", "C"}},
        {"B", {"D"}}, {"C", {}},
        {"D", {}}
    });
    string check1 = "  +--rw A";
    string check2 = "     +--rw B";
    string check3 = "     |  +--rw D";
    string check4 = "     +--rw C";
    out_t check = {check1, check2, check3, check4};
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    struct trt_printer_ctx pc = 
    {
        (trt_printing){&out, Out::print_vecLines, 0},
        { /* trt_fp_all */
            {parent, next_sibling, next_child, NULL, NULL, NULL, NULL, NULL},
            {NULL, node, next_sibling_read},
            {NULL, NULL}
        },
        72
    };
    trb_print_subtree_nodes(trp_init_wrapper_top(), &pc, &ctx);
    EXPECT_EQ(out, check);
    out.clear();
}

PRINT_TESTS_STATS();

UNIT_TESTING_END;

}
