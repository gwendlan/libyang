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
    trp_print(p, 1, "key");
}

int main()
{

UNIT_TESTING_START;

using out_t = std::string;
out_t out;

TEST(node, fully)
{
    out_t check = "+--rw prefix:node!   -> target {iffeature}?";
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_container, "prefix", "node"},
        {trd_type_target, "target"},
        trp_set_iffeature()
    };
    trt_printing p = {&out, Out::print_string, 0};
    trp_print_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        trp_default_indent_in_node(node), &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(node, onlyIffeature)
{
    out_t check = "+--rw node {iffeature}?";
    trt_node node =
    {
        trd_status_type_current, trd_flags_type_rw,
        {trd_node_else, "", "node"},
        {trd_type_empty, ""},
        trp_set_iffeature()
    };
    trt_printing p = {&out, Out::print_string, 0};
    trp_print_node(node, (trt_pck_print){NULL, {p_iff, p_key}},
        trp_default_indent_in_node(node), &p);

    EXPECT_EQ(out, check);
    out.clear();
}

PRINT_TESTS_STATS();

UNIT_TESTING_END;

}
