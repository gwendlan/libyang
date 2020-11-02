
/**
 * \file unit_testing.hpp
 * \brief Auxiliary macros for unit testing.
 * \author Adam Piecek <piecek@cesnet.cz>
 * \date 2020
 * \see \ref UNIT_TESTING_details
 */

/*
 * Copyright (C) 2014-2018 CESNET
 *
 * LICENSE TERMS
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

/** \defgroup UNIT_TESTING UNIT_TESTING
 *
 * \anchor UNIT_TESTING_details
 *
 * \brief Auxiliary macros for unit testing
 *
 * \details Small unit testing framework in one header file. For unit testing
 * please <b> use only predefined macros</b>. You can safely ignore functions in
 * namespace \ref ::Unit_testing_impl.
 *
 * \attention
 * \arg Macro \ref UT_ASSERT will \b not work properly if it will be located in
 * \p for/while \b cycles or in lambda function or ...
 * \arg Macro parameter must has implementation for \p operator<<. And also the
 * relevant comparison operations you want to test. Of course, primitive \p C++
 * types already have these operations built in, so you can rely on them.
 * \arg For pointer testing use <tt> *_EQ(ptr, nullptr)</tt> and not \p NULL
 * because \p nullptr is typed.
 * \arg <b>All macros must be in \p main function.</b>
 * \arg Macros \ref UNIT_TESTING_START and \ref UNIT_TESTING_END must be
 * straight in the \p main block of scope \p {}.
 * \arg After \ref UNIT_TESTING_END no other code in the function will not be
 * executed.
 * \arg Macros is not thread-safe.
 *
 * \note
 * \arg This header file using features introduced in \p C++11.
 * \arg \ref UT_EXPECT macros can be in \p if condition statement.
 * \arg You can freely use block of scope \p {} around \ref TEST and \ref
 * UT_EXPECT macros. But beware to \b not placing \ref UT_ASSERT in \p
 * for/while cycles.
 * \arg It is possible to solve the setup/teardown phase for the test out of
 * \ref TEST scope. And it actually gives a bit of freedom for the testing
 * itself when it is possible to reuse some pieces of code.
 * \arg Except for the \ref UNIT_TESTING_END macro, no \p catch exception
 * construction is used anywhere in the macros.
 * \arg Anytime you can \p return 1 from function. There is not used any dynamic
 * memory acquisition in \ref TEST.
 * \arg In \ref TEST you can write Test suite name and Test name with
 * spaces and without quotes.
 *
 * \remark
 * \arg It is very unlikely that the macro itself will throw an exception,
 * perhaps from \p std::string (\p std::length_error).
 * \arg Main inspiration for this file is from <a
 * href="https://github.com/google/googletest">Gooletest framework</a>. It is
 * lightweight version of it and not everything is the same.
 * \arg If you want add some functionalities and see generated documentation in
 * your browser then in your \a Doxyfile set variable \p INTERNAL_DOCS to \p
 * YES. Only then you can see documentation of functions in namespace \ref
 * ::Unit_testing_impl. Please add \p \\internal doxygen tag to new
 * function/macro in namespace \ref ::Unit_testing_impl. Also use postfix \p
 * _fHDakD2v for new macros (It is some random characters).
 *
 * \section UT_Kinds_of_tests Kinds of tests
 *
 * \code{.cpp}
 *
 * #include "unit_testing.hpp"
 *
 * int main()
 * {
 *    UNIT_TESTING_START;
 *
 *    // Demonstration of differences between EXPECT and ASSERT:
 *
 *    TEST(my_func, parameter_with_zero_value)
 *    {
 *       bool variable = true;
 *       if (EXPECT_TRUE(variable)) {
 *          EXPECT_FALSE(true);
 *          std::cout << "ping" << std::endl;
 *       } else {
 *          std::cout << "king" << std::endl;
 *       }
 *    }
 *    // FAILED at EXPECT_FALSE(true) and "ping" is printed to stdout
 *
 *    TEST(my_class method_get, general_use)
 *    {
 *       bool variable = true;
 *       ASSERT_TRUE(variable);
 *       ASSERT_FALSE(variable);
 *       std::cout << "pong" << std::endl;
 *    }
 *    // FAILED at ASSERT_FALSE(variable) and "pong" is NOT printed to stdout
 *
 *    PRINT_TESTS_STATS();
 *
 *    UNIT_TESTING_END;
 * }
 *
 * \endcode
 *
 * \code{.cpp}
 *
 * #include "unit_testing.hpp"
 *
 * int main()
 * {
 *    UNIT_TESTING_START;
 *
 *    // Demonstration of differences between SUCCESS and FAIL:
 *
 *    TEST(my_func, parameter_with_zero_value)
 *    {
 *       SUCCESS();
 *       std::cout << "ping" << std::endl;
 *    }
 *    // PASSED and "ping" is NOT printed to stdout
 *
 *    TEST(my_func, parameter_with_zero_value)
 *    {
 *       FAIL();
 *       std::cout << "pong" << std::endl;
 *    }
 *    // FAILED and "pong" is NOT printed to stdout
 *
 *    UNIT_TESTING_END;
 * }
 *
 * \endcode
 *
 * \subsection UT_EXPECT EXPECT
 * \arg In case of failure, \ref TEST is allowing to continue running and a
 * report is printed to \p stderr.
 * \arg In case of success, macro return true.
 *
 * \subsection UT_ASSERT ASSERT
 * \arg In case of failure, \ref TEST is finished and a report is printed to \p
 * stderr.
 * \arg In case of success, nothing will happen.
 *
 * \section UT_ADVANCED Advanced usage
 * If there are a lot of unit tests and you want to speed up testing, then you
 * can use natural parallelization using \p std::threads. Until now, it was
 * strongly recommended in the documentation to call macros only in the main
 * function. But because UNIT_TESTING does not use any global variables, it is
 * possible to use macros like this:
 *
 * \code{.cpp}
 *
 * #include "unit_testing.hpp"
 * #include <thread> // CPP_FLAG -pthread
 * #include <future>
 *
 * using namespace std;
 *
 * int task1()
 * {
 *    UNIT_TESTING_START;
 *    TEST(my_func1, test1)
 *    {
 *    }
 *    // more TESTs ...
 *    UNIT_TESTING_END;
 * }
 *
 * int task2()
 * {
 *    UNIT_TESTING_START;
 *    TEST(my_func2, test1)
 *    {
 *    }
 *    // more TESTs ...
 *    UNIT_TESTING_END;
 * }
 *
 * int main()
 * {
 *    auto future = std::async(task1);
 *    int res2 = task2();
 *    int res1 = future.get();
 *    return res1 | res2;
 * }
 *
 * \endcode
 *
 * \{
 */

/** \file */

#if !defined(UNIT_TESTING_MACROS_HPP)
#define UNIT_TESTING_MACROS_HPP

#include <cstring>
#include <exception>
#include <iostream>
#include <string>

/** \name UNIT_TESTING */

/** \{ */

/** \brief Start UNIT_TESTING framework.
 * Write this macro in main function \b once before first \ref TEST call.
 */
#define UNIT_TESTING_START                                                     \
   Unit_testing_impl::State state = {"", "", {__LINE__, true}, 0, 0, 0, 0};    \
   try {

/** \brief End UNIT_TESTING framework and finish whole program.
 * Write this macro <b> once as last command in \p main function</b>.
 * \return if all tests passed main function returns 0 otherwise 1.
 */
#define UNIT_TESTING_END                                                       \
   }                                                                           \
   catch (std::exception & e)                                                  \
   {                                                                           \
      state.overall = 1;                                                       \
      ::Unit_testing_impl::print_exception_header(                             \
         {state.suite, state.test, __FILE__, e.what(), state.result.line});    \
   }                                                                           \
   catch (...)                                                                 \
   {                                                                           \
      state.overall = 1;                                                       \
      ::Unit_testing_impl::print_exception_header(                             \
         {state.suite, state.test, __FILE__, "Unexpected exception",           \
          state.result.line});                                                 \
   }                                                                           \
   return state.overall;

/** \brief One test.
 * Write this macro for every unit test.
 * \pre \ref UNIT_TESTING_START is positioned correctly.
 * \post \ref UNIT_TESTING_END is positioned correctly.
 *
 * \code
 *    TEST(my_func, parameter_with_zero_value)
 *    {
 *       setup parameters before calling my_func
 *       call my_func
 *       check result with EXPECT_*() macros
 *       teardown(delete ..., free(...), ...)
 *    }
 * \endcode
 * \param[in] TEST_SUITE_NAME String without quotes. Which unit is being tested.
 * \param[in] TEST_NAME String without quotes. What is actually being tested.
 */
#define TEST(TEST_SUITE_NAME, TEST_NAME)                                       \
   state.failed_start = state.failed;                                          \
   ++state.tests_count;                                                        \
   state.suite = #TEST_SUITE_NAME;                                             \
   state.test = #TEST_NAME;                                                    \
   state.result.line = __LINE__;                                               \
   for (int i_fHDakD2v = 0; i_fHDakD2v < 1; ++i_fHDakD2v)

/** \brief Print the total number of successful and unsuccessful tests.
 */
#define PRINT_TESTS_STATS()                                                    \
   do {                                                                        \
      std::cout << std::endl;                                                  \
      if (state.failed == 0)                                                   \
         std::cout << "TESTS PASSED" << std::endl;                             \
      std::cout << "Total tests " << state.tests_count << ", ";                \
      std::cout << "Failed tests: " << state.failed << std::endl;              \
   } while (0)

/** \} */
// end of UNIT_TESTING

/** \name Direct commands */

/** \{ */

/** \brief Directly end the \ref TEST as a success. */
#define SUCCESS()                                                              \
   if (true) {                                                                 \
      state.result.line = __LINE__;                                            \
      state.result.test = true;                                                \
      BREAK_fHDakD2v                                                           \
   } else                                                                      \
      (void)0;

/** \brief Directly end the \ref TEST as a failed. */
#define FAIL()                                                                 \
   if (true) {                                                                 \
      ::Unit_testing_impl::print_command_msg(                                  \
         {state.suite, state.test, __FILE__, __LINE__});                       \
      state.result.line = __LINE__;                                            \
      state.result.test = false;                                               \
      state.overall = 1;                                                       \
      ++state.failed;                                                          \
      BREAK_fHDakD2v                                                           \
   } else                                                                      \
      (void)0;

/** \} */
// end of Direct commands

/** \name EXPECT checks */

/** \{ */

/** \brief \ref UT_EXPECT which verifies \b == */
#define EXPECT_EQ(FIRST, SECOND)                                               \
   (state.result = ::Unit_testing_impl::expect_eq(                             \
       FIRST, SECOND,                                                          \
       {state.suite, state.test, #FIRST, #SECOND, "==", __FILE__, __LINE__},   \
       {state.overall, state.failed, state.failed_start}))                     \
      .test

/** \brief \ref UT_EXPECT which verifies \b != */
#define EXPECT_NE(FIRST, SECOND)                                               \
   (state.result = ::Unit_testing_impl::expect_ne(                             \
       FIRST, SECOND,                                                          \
       {state.suite, state.test, #FIRST, #SECOND, "!=", __FILE__, __LINE__},   \
       {state.overall, state.failed, state.failed_start}))                     \
      .test

/** \brief \ref UT_EXPECT which verifies \b < */
#define EXPECT_LT(FIRST, SECOND)                                               \
   (state.result = ::Unit_testing_impl::expect_lt(                             \
       FIRST, SECOND,                                                          \
       {state.suite, state.test, #FIRST, #SECOND, "<", __FILE__, __LINE__},    \
       {state.overall, state.failed, state.failed_start}))                     \
      .test

/** \brief \ref UT_EXPECT which verifies \b <= */
#define EXPECT_LE(FIRST, SECOND)                                               \
   (state.result = ::Unit_testing_impl::expect_le(                             \
       FIRST, SECOND,                                                          \
       {state.suite, state.test, #FIRST, #SECOND, "<=", __FILE__, __LINE__},   \
       {state.overall, state.failed, state.failed_start}))                     \
      .test

/** \brief \ref UT_EXPECT which verifies \b > */
#define EXPECT_GT(FIRST, SECOND)                                               \
   (state.result = ::Unit_testing_impl::expect_gt(                             \
       FIRST, SECOND,                                                          \
       {state.suite, state.test, #FIRST, #SECOND, ">", __FILE__, __LINE__},    \
       {state.overall, state.failed, state.failed_start}))                     \
      .test

/** \brief \ref UT_EXPECT which verifies \b >= */
#define EXPECT_GE(FIRST, SECOND)                                               \
   (state.result = ::Unit_testing_impl::expect_ge(                             \
       FIRST, SECOND,                                                          \
       {state.suite, state.test, #FIRST, #SECOND, ">=", __FILE__, __LINE__},   \
       {state.overall, state.failed, state.failed_start}))                     \
      .test

/** \brief \ref UT_EXPECT which verifies C strings with \b strcmp function */
#define EXPECT_STREQ(FIRST, SECOND)                                            \
   (state.result = ::Unit_testing_impl::expect_streq(                          \
       FIRST, SECOND,                                                          \
       {state.suite, state.test, #FIRST, #SECOND, "==", __FILE__, __LINE__},   \
       {state.overall, state.failed, state.failed_start}))                     \
      .test

/** \brief \ref UT_EXPECT which verifies \b true */
#define EXPECT_TRUE(FIRST)                                                     \
   (state.result = ::Unit_testing_impl::expect_true(                           \
       FIRST, {state.suite, state.test, #FIRST, "", "T", __FILE__, __LINE__},  \
       {state.overall, state.failed, state.failed_start}))                     \
      .test

/** \brief \ref UT_EXPECT which verifies \b false */
#define EXPECT_FALSE(FIRST)                                                    \
   (state.result = ::Unit_testing_impl::expect_false(                          \
       FIRST, {state.suite, state.test, #FIRST, "", "F", __FILE__, __LINE__},  \
       {state.overall, state.failed, state.failed_start}))                     \
      .test

/** \} */
// end of EXPECT checks

/** \name ASSERT checks */

/** \{ */

/** \brief \ref UT_ASSERT which verifies \b == */
#define ASSERT_EQ(FIRST, SECOND)                                               \
   if (!EXPECT_EQ(FIRST, SECOND)) {                                            \
      BREAK_fHDakD2v                                                           \
   }

/** \brief \ref UT_ASSERT which verifies \b != */
#define ASSERT_NE(FIRST, SECOND)                                               \
   if (!EXPECT_NE(FIRST, SECOND)) {                                            \
      BREAK_fHDakD2v                                                           \
   }

/** \brief \ref UT_ASSERT which verifies \b < */
#define ASSERT_LT(FIRST, SECOND)                                               \
   if (!EXPECT_LT(FIRST, SECOND)) {                                            \
      BREAK_fHDakD2v                                                           \
   }

/** \brief \ref UT_ASSERT which verifies \b <= */
#define ASSERT_LE(FIRST, SECOND)                                               \
   if (!EXPECT_LE(FIRST, SECOND)) {                                            \
      BREAK_fHDakD2v                                                           \
   }

/** \brief \ref UT_ASSERT which verifies \b > */
#define ASSERT_GT(FIRST, SECOND)                                               \
   if (!EXPECT_GT(FIRST, SECOND)) {                                            \
      BREAK_fHDakD2v                                                           \
   }

/** \brief \ref UT_ASSERT which verifies \b >= */
#define ASSERT_GE(FIRST, SECOND)                                               \
   if (!EXPECT_GE(FIRST, SECOND)) {                                            \
      BREAK_fHDakD2v                                                           \
   }

/** \brief \ref UT_ASSERT which verifies C strings with \b strcmp function */
#define ASSERT_STREQ(FIRST, SECOND)                                            \
   if (!EXPECT_STREQ(FIRST, SECOND)) {                                         \
      BREAK_fHDakD2v                                                           \
   }

/** \brief \ref UT_ASSERT which verifies \b true */
#define ASSERT_TRUE(FIRST)                                                     \
   if (!EXPECT_TRUE(FIRST)) {                                                  \
      BREAK_fHDakD2v                                                           \
   }

/** \brief \ref UT_ASSERT which verifies \b false */
#define ASSERT_FALSE(FIRST)                                                    \
   if (!EXPECT_FALSE(FIRST)) {                                                 \
      BREAK_fHDakD2v                                                           \
   }

/** \} */
// end of ASSERT checks
//

/** \} */
// end of UNIT_TESTING group

/** \brief Implementation of UNIT_TESTING macros.
 * \see \ref UNIT_TESTING_details
 */
namespace Unit_testing_impl
{
using std::cerr;
using std::endl;
using std::nullptr_t;
using std::string;

/** \name Print output */

/** \{ */

/** \internal
 * \brief Structure for printing error message by \ref FAIL.
 * \endinternal
 */
struct Command_msg {
   string &suite; ///< Name of test suite.
   string &test;  ///< Name of test.
   string path;   ///< Path to file where test fail.
   int line;      ///< Line number where the macro was called.
};

/** \internal
 * \brief Structure for printing error message about failed comparison.
 * \endinternal
 */
struct Compar_msg {
   string &suite;    ///< Name of test suite.
   string &test;     ///< Name of test.
   string first;     ///< First macro parameter as string.
   string second;    ///< Second macro parameter as string.
   string operation; ///< Used operation.
   string path;      ///< Path to file where test fail.
   int line;         ///< Line number where the macro was called.
};

/** \internal
 * \brief Structure for printing exception message.
 * \endinternal
 */
struct Exception_msg {
   string &suite; ///< Name of test suite.
   string &test;  ///< Name of test.
   string path;   ///< Path to file where test fail.
   string what;   ///< Message from exception.
   int last_line; ///< A last recorded line number.
};

/** \internal
 * \brief Default function for print value.
 * Print value to stderr.
 * \tparam T can be every type which has operator<<
 * \param[in] value to print.
 * \endinternal
 */
template <typename T> inline void print_value(const T &value) { cerr << value; }

/** \internal
 * \brief Default function for print bool.
 * Print boolean as word to stderr.
 * \param[in] value to print.
 * \endinternal
 */
template <> inline void print_value<bool>(const bool &value)
{
   cerr << (value ? "true" : "false");
}

/** \internal
 * \brief Default function for print std::string.
 * Print std::string with quotes to stderr.
 * \param[in] value to print.
 * \endinternal
 */
template <> inline void print_value<string>(const string &value)
{
   cerr << "\"" << value << "\"";
}

/** \internal
 * \brief Default function for print nullptr.
 * Print nullptr as string "nullptr" to stderr
 * \endinternal
 */
template <> inline void print_value<nullptr_t>(const nullptr_t &)
{
   cerr << "nullptr";
}

/** \internal
 * \brief Print new line with some sugar.
 * \endinternal
 */
inline void newline() { cerr << endl << "> "; }

/** \internal
 * \brief Print starting delimiter to \ref TEST
 * \endinternal
 */
inline void print_test_delimiter_start() { cerr << endl << "> "; }

/** \internal
 * \brief Print ending delimiter to \ref TEST
 * \endinternal
 */
inline void print_test_delimiter_end() { cerr << endl << endl; }

/** \internal
 * \brief Print message header for fail.
 * \param[in] suite is name of test suite.
 * \param[in] test is name of test.
 * \param[in] path to file where test fail.
 * \param[in] line number where test fail.
 * \endinternal
 */
inline void print_header(const string &suite, const string &test, const string &path,
                  int line)
{
   cerr << "FAIL: " << suite << " " << test;
   newline();
   cerr << "line " << line << " in file " << path;
   newline();
}

/** \internal
 * \brief Print where approximately the exception was thrown.
 * \param[in] msg has information for fail message.
 * \endinternal
 */
inline void print_between_msg(const Exception_msg &msg)
{
   cerr << "Somewhere between line ";
   cerr << msg.last_line << " and "
        << "next UNIT_TESTING macro";
   cerr << " in file " << msg.path;
   newline();
   cerr << "An exception was thrown with a message: ";
   newline();
   cerr << "   \"" << msg.what << "\"";
}

/** \internal
 * \brief Decide which exception message can be print.
 * \param[in] msg has information for fail message.
 * \endinternal
 */
inline void print_exception_header(Exception_msg msg)
{
   print_test_delimiter_start();
   if (msg.suite.empty()) {
      // Exception between unit tests
      cerr << "Unexpected FAIL:";
      newline();
      print_between_msg(msg);
   } else {
      // Exception in TEST() between some macros
      cerr << "Unexpected FAIL: " << msg.suite << " " << msg.test;
      newline();
      print_between_msg(msg);
   }
   print_test_delimiter_end();
}

/** \internal
 * \brief Print message header for \ref FAIL
 * \param[in] msg has information for fail message.
 * \endinternal
 */
inline void print_command_msg(const Command_msg &msg)
{
   print_test_delimiter_start();
   print_header(msg.suite, msg.test, msg.path, msg.line);
   cerr << "FAIL() was called ";
   print_test_delimiter_end();
}

/** \internal
 * \brief Main function for formatting fail message.
 * \tparam T1 can be every type which has operator<<.
 * \tparam T2 can be every type which has operator<<.
 * \param[in] first is first value written to macro.
 * \param[in] second is second value written to macro.
 * \param[in] msg has information for fail message.
 * \endinternal
 */
template <typename T1, typename T2>
void print_compar_msg(const T1 &first, const T2 &second, const Compar_msg &msg)
{
   print_test_delimiter_start();
   print_header(msg.suite, msg.test, msg.path, msg.line);

   cerr << "Expected: "
        << "(" << msg.first << ")";
   if (msg.operation == "T" || msg.operation == "F") {
      cerr << " should be " << (msg.operation == "T" ? "TRUE" : "FALSE");
      cerr << " but value is - ";
      print_value(first);
   } else {
      cerr << " " << msg.operation << " ";
      cerr << "(" << msg.second << ")";
      newline();

      cerr << msg.first;
      newline();
      cerr << "   Which is: ";
      print_value(first);
      newline();

      cerr << msg.second;
      newline();
      cerr << "   Which is: ";
      print_value(second);
   }
   print_test_delimiter_end();
}

/** \} */

/** \name Core */

/** \{ */

/** \internal
 * \brief Result of single comparison.
 * \endinternal
 */
struct Test_result {
   int line;  ///< Line number where the last macro was called.
   bool test; ///< Result of comparison.
};

/** \internal
 * \brief State of UNIT_TESTING framework.
 * \endinternal
 */
struct State {
   string suite;              ///< Name of test suite.
   string test;               ///< Name of test.
   Test_result result;        ///< Actual state passed/failed.
   int overall;               ///< Overall tests result.
   unsigned int tests_count;  ///< Number of tests.
   unsigned int failed;       ///< Number of failed tests.
   unsigned int failed_start; ///< Number of failed tests before TEST macro.
};

/** \internal
 * \brief Request and informations to update State.
 * \endinternal
 */
struct Update {
   int &overall;              ///< Line number where the last macro was called.
   unsigned int &failed;      ///< Result of comparison.
   unsigned int failed_start; ///< Number of failed tests before TEST macro.
};

/** \internal
 * \brief Evaluate operation, decide if failed message must be written and
 * update state.
 * \tparam T1 is type of first parameter. It can be every type
 * which has operator<< and it has a corresponding operation for comparison.
 * \tparam T2 si type of second parameter. Operator<< required.
 * \param[in] first is first value written to macro.
 * \param[in] second is second value written to macro.
 * \param[in] test result of compare function.
 * \param[in] msg has information for fail message.
 * \param[in,out] upd is for update of testing state.
 * Item upd.overall is set to 1 if compare fail
 * otherwise original value is keeped.
 * Item upd.failed is increment only once for one TEST if compare fail.
 * Item upd.failed_start is for correct update of upd.failed item.
 * \return .test is true if compare passed otherwise .test = false.
 * \endinternal
 */
template <typename T1, typename T2>
Test_result expect(const T1 &first, const T2 &second, bool test,
                   const Compar_msg &msg, Update &upd)
{
   upd.overall = test ? upd.overall : 1;
   if (test == false) {
      if (upd.failed == upd.failed_start)
         ++upd.failed;
      print_compar_msg(first, second, msg);
   }
   return Test_result{msg.line, test};
}

/** \} */

/** \name Binary operations */

/** \{ */

/**
 * \internal
 *
 * \brief Create operation and continue with other function
 * \tparam T1 is type of first parameter. It can be every type which has
 * operator<< and it has a corresponding operation for comparison.
 * \tparam T2 si type of second parameter. Operator<< required.
 * \param[in] first is first value written to macro.
 * \param[in] second is second value written to macro.
 * \param[in] msg has information for fail message.
 * \param[in,out] upd is for update of testing state.
 * Item upd.overall is set to 1 if compare fail
 * otherwise original value is keeped.
 * Item upd.failed is increment only once for one TEST if compare fail.
 * Item upd.failed_start is for correct update of upd.failed item.
 * \return .test is true if compare passed otherwise .test = false.
 * \endinternal
 */
template <typename T1, typename T2>
Test_result expect_eq(const T1 &first, const T2 &second, const Compar_msg &msg,
                      Update upd)
{
   return expect(first, second, first == second, msg, upd);
}

/** \copydoc ::Unit_testing_impl::expect_eq() */
template <typename T1, typename T2>
Test_result expect_ne(const T1 &first, const T2 &second, const Compar_msg &msg,
                      Update upd)
{
   return expect(first, second, first != second, msg, upd);
}

/** \copydoc ::Unit_testing_impl::expect_eq() */
template <typename T1, typename T2>
Test_result expect_lt(const T1 &first, const T2 &second, const Compar_msg &msg,
                      Update upd)
{
   return expect(first, second, first < second, msg, upd);
}

/** \copydoc ::Unit_testing_impl::expect_eq() */
template <typename T1, typename T2>
Test_result expect_le(const T1 &first, const T2 &second, const Compar_msg &msg,
                      Update upd)
{
   return expect(first, second, first <= second, msg, upd);
}

/** \copydoc ::Unit_testing_impl::expect_eq() */
template <typename T1, typename T2>
Test_result expect_gt(const T1 &first, const T2 &second, const Compar_msg &msg,
                      Update upd)
{
   return expect(first, second, first > second, msg, upd);
}

/** \copydoc ::Unit_testing_impl::expect_eq() */
template <typename T1, typename T2>
Test_result expect_ge(const T1 &first, const T2 &second, const Compar_msg &msg,
                      Update upd)
{
   return expect(first, second, first >= second, msg, upd);
}

/**
 * \internal
 *
 * \brief Compare two C strings and continue with other function
 * \param[in] first is first value written to macro.
 * \param[in] second is second value written to macro.
 * \param[in] msg has information for fail message.
 * \param[in,out] upd is for update of testing state.
 * \return .test is true if compare passed otherwise .test = false.
 * \endinternal
 */
Test_result expect_streq(const char* first, const char* second,
                         const Compar_msg &msg, Update upd)
{

   if(first == nullptr) {
      if(second == nullptr) {
         return expect(nullptr, nullptr, true, msg, upd);
      } else if(second[0] == '\0') {
         return expect(nullptr, std::string(), false, msg, upd);
      } else {
         return expect(nullptr, second, false, msg, upd);
      }
   } else if(second == nullptr) {
      if(first[0] == '\0') {
         return expect(std::string(), nullptr, false, msg, upd);
      } else {
         return expect(first, nullptr, false, msg, upd);
      }
   } else {
      if(first[0] == '\0') {
         if(second[0] == '\0') {
            return expect(std::string(), std::string(), true, msg, upd);
         } else {
            return expect(std::string(), second, false, msg, upd);
         }
      } else if(second[0] == '\0') {
         if(first[0] == '\0') {
            return expect(std::string(), std::string(), true, msg, upd);
         } else {
            return expect(first, std::string(), false, msg, upd);
         }
      } else {
         return expect(first, second, std::strcmp(first, second) == 0, msg, upd);
      }
   }
}

/** \} */

/** \name Unary operations */

/** \{ */

/** \internal
 * \brief Create operation and continue with other function.
 * \tparam T can be every type which has operator<<.
 * \param[in] first is first value written to macro.
 * \param[in] msg has information for fail message.
 * \param[in,out] upd is for update of testing state.
 * Item upd.overall is set to 1 if compare fail
 * otherwise original value is keeped.
 * Item upd.failed is increment only once for one TEST if compare fail.
 * Item upd.failed_start is for correct update of upd.failed item.
 * \return .test.true if compare passed otherwise .test.false.
 * \endinternal
 */
template <typename T>
Test_result expect_true(const T &first, const Compar_msg &msg, Update upd)
{
   return expect(first, true, first == true, msg, upd);
}

/** \copydoc ::Unit_testing_impl::expect_true() */
template <typename T>
Test_result expect_false(const T &first, const Compar_msg &msg, Update upd)
{
   return expect(first, false, first == false, msg, upd);
}

/** \} */

/** \internal
 * \brief Update overall success and if test failed then call break.
 * \endinternal
 */
#define BREAK_fHDakD2v                                                         \
   if (true) {                                                                 \
      state.suite = "";                                                        \
      state.test = "";                                                         \
      continue;                                                                \
   } else                                                                      \
      (void)0;

} // namespace Unit_testing_impl

#endif
