#include "version.hpp"

#include <gtest/gtest.h>

TEST(Version, MatchesProjectVersion) { EXPECT_EQ(hearth::version(), "0.1.0"); }
