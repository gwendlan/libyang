#include <iostream>
#include "print_func.hpp"
#include "unit_testing.hpp"

extern "C"
{
#include "new.h"
}

int main()
{

UNIT_TESTING_START;

using out_t = Out::VecLines;
using std::string;
out_t out;

TEST(keywordStmt, module)
{
    string name = "x";
    out_t check = {"module: x"};
    trt_keyword_stmt k =
    {
        trd_keyword_stmt_top,
        trd_keyword_module,
        name.c_str()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_keyword_stmt(k, 72, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(keywordStmt, noPath)
{
    string path = "nopath";
    out_t check = {"  augment " + path + ":"};
    trt_keyword_stmt k =
    {
        trd_keyword_stmt_body,
        trd_keyword_augment,
        path.c_str()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_keyword_stmt(k, 72, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(keywordStmt, rootPath)
{
    string path = "/nopath";
    out_t check = {"  augment " + path + ":"};
    trt_keyword_stmt k =
    {
        trd_keyword_stmt_body,
        trd_keyword_augment,
        path.c_str()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_keyword_stmt(k, 72, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(keywordStmt, oneTooBigPath)
{
    string path = "/efghijkl";
    out_t base =         {"  augment /efghijkl:"};
    uint32_t mll = strlen("                 ^");
    string check1 =       "  augment /efghijkl:";
    out_t check = {check1};
    trt_keyword_stmt k =
    {
        trd_keyword_stmt_body,
        trd_keyword_augment,
        path.c_str()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_keyword_stmt(k, mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(keywordStmt, splitPath2len)
{
    string path = "/abcd/efghijkl";
    out_t base =         {"  augment /abcd/efghijkl:"};
    uint32_t mll = strlen("                      ^");
    string check1 =       "  augment /abcd";
    string check2 =       "            /efghijkl:";
    out_t check = {check1, check2};
    trt_keyword_stmt k =
    {
        trd_keyword_stmt_body,
        trd_keyword_augment,
        path.c_str()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_keyword_stmt(k, mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(keywordStmt, longPathSplitMll10)
{
    string path1 = "/y:pretty-long-identifier-name";
    string path2 = "/y:shorter";
    string path3 = "/y:another-long-identifier-name";
    string path4 = "/y:also-short";
    string path5 = "/y:but-this-is-long-again";
    string path = path1 + path2 + path3 + path4 + path5;
    string indent = "            ";
    uint32_t mll = 10;
    string check1 = string("  augment ") + path1;
    string check2 = indent + path2;
    string check3 = indent + path3;
    string check4 = indent + path4;
    string check5 = indent + path5;
    out_t check = {check1, check2, check3, check4, check5 + ":"};
    trt_keyword_stmt k =
    {
        trd_keyword_stmt_body,
        trd_keyword_augment,
        path.c_str()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_keyword_stmt(k, mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}


TEST(keywordStmt, longPathSplitMll50)
{
    string path1 = "/y:pretty-long-identifier-name";
    string path2 = "/y:shorter";
    string path3 = "/y:another-long-identifier-name";
    string path4 = "/y:also-short";
    string path5 = "/y:but-this-is-long-again";
    string path = path1 + path2 + path3 + path4 + path5;
    string indent = "            ";
    uint32_t mll = 50;
    string check1 = string("  augment ") + path1 + path2;
    string check2 = indent + path3;
    string check3 = indent + path4;
    string check4 = indent + path5;
    out_t check = {check1, check2, check3, check4 + ":"};
    trt_keyword_stmt k =
    {
        trd_keyword_stmt_body,
        trd_keyword_augment,
        path.c_str()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_keyword_stmt(k, mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

TEST(keywordStmt, longPathSplitMll1000)
{
    string path1 = "/y:pretty-long-identifier-name";
    string path2 = "/y:shorter";
    string path3 = "/y:another-long-identifier-name";
    string path4 = "/y:also-short";
    string path5 = "/y:but-this-is-long-again";
    string path = path1 + path2 + path3 + path4 + path5;
    string indent = "            ";
    uint32_t mll = 1000;
    string check1 = string("  augment ") + path1 + path2 + path3 + path4 + path5;
    out_t check = {check1 + ":"};
    trt_keyword_stmt k =
    {
        trd_keyword_stmt_body,
        trd_keyword_augment,
        path.c_str()
    };
    trt_printing p = {&out, Out::print_vecLines, 0};
    trp_print_keyword_stmt(k, mll, &p);

    EXPECT_EQ(out, check);
    out.clear();
}

PRINT_TESTS_STATS();

UNIT_TESTING_END;

}
