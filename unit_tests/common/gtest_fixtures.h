#ifndef LOCUS_GTEST_FIXTURES_H
#define LOCUS_GTEST_FIXTURES_H

#include <gtest/gtest.h>

#include "test_utilities.h"

class FilesystemTest : public ::testing::Test {
   protected:
    void* state = nullptr;
    test_fs_t* fs = nullptr;

    void SetUp() override {
        ASSERT_EQ(test_setup_fs(&state), 0);
        ASSERT_NE(state, nullptr);
        fs = static_cast<test_fs_t*>(state);
    }

    void TearDown() override {
        EXPECT_EQ(test_teardown_fs(&state), 0);
        EXPECT_EQ(state, nullptr);
        fs = nullptr;
    }
};

#endif
