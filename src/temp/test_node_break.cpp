#include <iostream>
#include "print_func.hpp"
#include "unit_testing.hpp"

extern "C"
{
#include "new.h"
}

void p_iff(const struct trt_tree_ctx*, trt_printing* p)
{
    trp_print(p, 1, "iffeature");
}

void p_key(const struct trt_tree_ctx*, trt_printing* p)
{
    trp_print(p, 1, "key1 key2");
}

int main()
{

UNIT_TESTING_START;

using out_t = Out::VecLines;
using std::string;
out_t out;


TEST(nodeBreak, fits)
{
    out_t check = {"  +--rw prefix:node* [key1 key2]    type {iffeature}?"};
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_keys, "prefix", "node"},
        {trd_type_name, "type"},
        trp_set_iffeature()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_entire_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        (trt_pck_indent){trp_init_wrapper_top(), trp_default_indent_in_node(node)},
        72, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(nodeBreak, fitsTight)
{
    out_t check =        {"  +--rw prefix:node* [key1 key2]    type {iffeature}?"};
    uint32_t mll = strlen("                                                    ^");
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_keys, "prefix", "node"},
        {trd_type_name, "type"},
        trp_set_iffeature()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_entire_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        (trt_pck_indent){trp_init_wrapper_top(), trp_default_indent_in_node(node)},
        mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(nodeBreak, btwNameOpts)
{
    out_t base   =       {"  +--rw xxxprefix:node* [key1 key2]"};
    uint32_t mll = strlen("                       ^");
    string check1 =       "  +--rw xxxprefix:node*";
    string check2 =       "  |       [key1 key2]";
    out_t check = {check1, check2};
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_keys, "xxxprefix", "node"},
        {trd_type_empty, ""},
        trp_empty_iffeature()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_entire_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        (trt_pck_indent){trp_init_wrapper_top(), trp_default_indent_in_node(node)},
        mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(nodeBreak, btwOptsType)
{
    out_t base   =       {"  +--rw xxxprefix:node*   string"};
    uint32_t mll = strlen("                       ^");
    string check1 =       "  +--rw xxxprefix:node*";
    string check2 =       "  |       string";
    out_t check = {check1, check2};
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_listLeaflist, "xxxprefix", "node"},
        {trd_type_name, "string"},
        trp_empty_iffeature()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_entire_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        (trt_pck_indent){trp_init_wrapper_top(), trp_default_indent_in_node(node)},
        mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(nodeBreak, btwOptsTypeWithIffeatures)
{
    out_t base   =       {"  +--rw xxxprefix:node*   st {iffeature}?"};
    uint32_t mll = strlen("                         ^");
    string check1 =       "  +--rw xxxprefix:node*";
    string check2 =       "  |       st {iffeature}?";
    out_t check = {check1, check2};
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_listLeaflist, "xxxprefix", "node"},
        {trd_type_name, "st"},
        trp_set_iffeature()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_entire_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        (trt_pck_indent){trp_init_wrapper_top(), trp_default_indent_in_node(node)},
        mll, &p);


    EXPECT_EQ(out, check);
    out.clear();
}

TEST(nodeBreak, btwTypeIffeatures)
{
    out_t base   =       {"  +--rw xxxprefix:node* {iffeature}?"};
    uint32_t mll = strlen("                       ^");
    string check1 =       "  +--rw xxxprefix:node*";
    string check2 =       "  |       {iffeature}?";
    out_t check = {check1, check2};
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_listLeaflist, "xxxprefix", "node"},
        {trd_type_empty, ""},
        trp_set_iffeature()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_entire_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        (trt_pck_indent){trp_init_wrapper_top(), trp_default_indent_in_node(node)},
        mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(nodeBreak, btwTypeIffeaturesWithKeys)
{
    out_t base   =       {"  +--rw xxxprefix:node* [key1 key2] {iffeature}?"};
    uint32_t mll = strlen("                                           ^");
    string check1 =       "  +--rw xxxprefix:node* [key1 key2]";
    string check2 =       "  |       {iffeature}?";
    out_t check = {check1, check2};
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_keys, "xxxprefix", "node"},
        {trd_type_empty, ""},
        trp_set_iffeature()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_entire_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        (trt_pck_indent){trp_init_wrapper_top(), trp_default_indent_in_node(node)},
        mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(nodeBreak, btwTypeIffeaturesWithKeysType)
{
    out_t base   =       {"  +--rw xxxprefix:node* [key1 key2]    string {iffeature}?"};
    uint32_t mll = strlen("                                             ^");
    string check1 =       "  +--rw xxxprefix:node* [key1 key2]    string";
    string check2 =       "  |       {iffeature}?";
    out_t check = {check1, check2};
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_keys, "xxxprefix", "node"},
        {trd_type_name, "string"},
        trp_set_iffeature()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_entire_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        (trt_pck_indent){trp_init_wrapper_top(), trp_default_indent_in_node(node)},
        mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(nodeBreak, allInNewLines)
{
    out_t base   =       {"  +--rw xxxprefix:node* [key1 key2]    string {iffeature}?"};
    uint32_t mll = strlen("                     ^");
    string check1 =       "  +--rw xxxprefix:node*";
    string check2 =       "  |       [key1 key2]";
    string check3 =       "  |       string";
    string check4 =       "  |       {iffeature}?";
    out_t check = {check1, check2, check3, check4};
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_keys, "xxxprefix", "node"},
        {trd_type_name, "string"},
        trp_set_iffeature()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_entire_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        (trt_pck_indent){trp_init_wrapper_top(), trp_default_indent_in_node(node)},
        mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(nodeBreak, typeIsToolong)
{
    out_t base   =       {"  +--rw node*   longString"};
    uint32_t mll = strlen("              ^");
    string check1 =       "  +--rw node*";
    string check2 =       "  |       longString";
    out_t check = {check1, check2};
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_listLeaflist, "", "node"},
        {trd_type_name, "longString"},
        trp_empty_iffeature()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_entire_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        (trt_pck_indent){trp_init_wrapper_top(), trp_default_indent_in_node(node)},
        mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(nodeBreak, nodeNameIsTooLong)
{
    out_t base   =       {"  +--rw longNodeName"};
    uint32_t mll = strlen("                 ^");
    string check1 =       "  +--rw longNodeName";
    out_t check = {check1};
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_else, "", "longNodeName"},
        {trd_type_empty, ""},
        trp_empty_iffeature()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_entire_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        (trt_pck_indent){trp_init_wrapper_top(), trp_default_indent_in_node(node)},
        mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(nodeBreak, breakLeafrefTarget)
{
    out_t base   =       {"  +--rw longNodeName    /y:longStr/short"};
    uint32_t mll = strlen("                             ^");
    string check1 =       "  +--rw longNodeName";
    string check2 =       "  |       -> /y:longStr/short";
    out_t check = {check1, check2};
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_else, "", "longNodeName"},
        {trd_type_target, "/y:longStr/short"},
        trp_empty_iffeature()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_entire_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        (trt_pck_indent){trp_init_wrapper_top(), trp_default_indent_in_node(node)},
        mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(nodeBreak, changeLeafrefTargetToLeafrefKeyword)
{
    out_t base   =       {"  +--rw node    /y:longStr/short/eventuallyIsReallyLong"};
    uint32_t mll = strlen("                         ^");
    string check1 =       "  +--rw node    leafref";
    out_t check = {check1};
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_else, "", "node"},
        {trd_type_target, "/y:longStr/short/eventuallyIsReallyLong"},
        trp_empty_iffeature()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_entire_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        (trt_pck_indent){trp_init_wrapper_top(), trp_default_indent_in_node(node)},
        mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(nodeBreak, changeLeafrefTargetToLeafrefKeywordWithIffeature)
{
    out_t base   =       {"  +--rw node    /y:longStr/short/eventuallyIsReallyLong {iffeature}?"};
    uint32_t mll = strlen("                         ^");
    string check1 =       "  +--rw node    leafref";
    string check2 =       "  |       {iffeature}?";
    out_t check = {check1, check2};
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_else, "", "node"},
        {trd_type_target, "/y:longStr/short/eventuallyIsReallyLong"},
        trp_set_iffeature()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_entire_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        (trt_pck_indent){trp_init_wrapper_top(), trp_default_indent_in_node(node)},
        mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

PRINT_TESTS_STATS();

UNIT_TESTING_END;

}
