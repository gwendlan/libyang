#include <iostream>
#include <vector>
#include <string>
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
using std::string;

int main()
{

UNIT_TESTING_START;

TEST(nodeRel, firstNode)
{
    Tree tree
    ({
        {"A", {"B", "C"}},
        {"B", {}}, {"C", {}}
    });
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    trt_node uut;

    uut = node(&ctx);
    ASSERT_STREQ(uut.name.str, "A");
}


TEST(nodeRel, firstGetFirstChild)
{
    Tree tree
    ({
        {"A", {"B", "C"}},
        {"B", {}}, {"C", {}}
    });
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    trt_node uut;

    uut = next_child(&ctx);
    ASSERT_STREQ(uut.name.str, "B");
    uut = node(&ctx);
    ASSERT_STREQ(uut.name.str, "B");
}

TEST(nodeRel, firstNoSiblings)
{
    Tree tree
    ({
        {"A", {"B", "C"}},
        {"B", {}}, {"C", {}}
    });
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    trt_node uut;

    uut = next_sibling(&ctx);
    EXPECT_STREQ(uut.name.str, nullptr);
    uut = node(&ctx);
    ASSERT_STREQ(uut.name.str, "A");
}

TEST(nodeRel, childThenSibling)
{
    Tree tree
    ({
        {"A", {"B", "C"}},
        {"B", {}}, {"C", {}}
    });
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    trt_node uut;

    uut = next_child(&ctx);
    ASSERT_STREQ(uut.name.str, "B");
    uut = next_sibling(&ctx);
    ASSERT_STREQ(uut.name.str, "C");
    uut = node(&ctx);
    ASSERT_STREQ(uut.name.str, "C");
}


TEST(nodeRel, endOfSiblings)
{
    Tree tree
    ({
        {"A", {"B", "C"}},
        {"B", {}}, {"C", {}}
    });
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    trt_node uut;

    uut = next_child(&ctx);
    ASSERT_STREQ(uut.name.str, "B");
    uut = next_sibling(&ctx);
    ASSERT_STREQ(uut.name.str, "C");
    uut = next_sibling(&ctx);
    EXPECT_STREQ(uut.name.str, nullptr);
    uut = node(&ctx);
    ASSERT_STREQ(uut.name.str, "C");
}


TEST(nodeRel, noChilds)
{
    Tree tree
    ({
        {"A", {"B", "C"}},
        {"B", {}}, {"C", {}}
    });
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    trt_node uut;

    uut = next_child(&ctx);
    ASSERT_STREQ(uut.name.str, "B");
    uut = next_sibling(&ctx);
    ASSERT_STREQ(uut.name.str, "C");
    uut = next_child(&ctx);
    EXPECT_STREQ(uut.name.str, nullptr);
    uut = node(&ctx);
    ASSERT_STREQ(uut.name.str, "C");
}


TEST(nodeRel, rootParent)
{
    Tree tree
    ({
        {"A", {"B", "C"}},
        {"B", {}}, {"C", {}}
    });
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    trt_node uut;

    uut = node(&ctx);
    ASSERT_STREQ(uut.name.str, "A");
    uut = parent(&ctx);
    ASSERT_EQ(trp_node_is_empty(uut), true);
}


TEST(nodeRel, parentFromChild)
{
    Tree tree
    ({
        {"A", {"B", "C"}},
        {"B", {}}, {"C", {}}
    });
    trt_tree_ctx ctx = {tree, tree.map.begin(), -1};
    trt_node uut;

    uut = node(&ctx);
    uut = next_child(&ctx);
    uut = next_sibling(&ctx);
    uut = parent(&ctx);
    ASSERT_STREQ(uut.name.str, "A");
}


TEST(nodeRel, parentFromParent)
{
    Tree tree
    ({
        {"A", {"B", "C"}},
        {"B", {}}, {"C", {}}
    });
    trt_tree_ctx ctx = {tree, tree.map.find("B"), -1};
    trt_node uut;

    uut = node(&ctx);
    ASSERT_STREQ(uut.name.str, "B");
    uut = parent(&ctx);
    ASSERT_STREQ(uut.name.str, "A");
}

PRINT_TESTS_STATS();

UNIT_TESTING_END;

}
