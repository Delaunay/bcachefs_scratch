#ifndef PROJECT_TEST_TESTS_ADD_HEADER
#define PROJECT_TEST_TESTS_ADD_HEADER

#include <gtest/gtest.h>

#include <symbolic.h>

TEST(add, eval)
{
    auto f = sym::add(sym::make_val(21), sym::make_val(32));
    sym::Context ctx;

    EXPECT_DOUBLE_EQ(21 + 32, f->full_eval(ctx));
}

TEST(add, derivate)
{
    auto x = sym::make_var("x");

    auto f = sym::add(x, sym::make_val(32));
    sym::Context ctx;

    auto deriv = f->derivate("x");

    EXPECT_DOUBLE_EQ(1, deriv->full_eval(ctx));
}


#endif

