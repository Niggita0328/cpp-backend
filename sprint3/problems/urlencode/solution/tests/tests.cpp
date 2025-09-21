#include <gtest/gtest.h>

#include "../src/urlencode.h"

using namespace std::literals;

TEST(UrlEncodeTestSuite, OrdinaryCharsAreNotEncoded) {
    EXPECT_EQ(UrlEncode("hello"sv), "hello"s);
}

TEST(UrlEncodeTestSuite, EmptyString) {
    EXPECT_EQ(UrlEncode(""sv), ""s);
}

TEST(UrlEncodeTestSuite, Spaces) {
    EXPECT_EQ(UrlEncode("hello world"sv), "hello%20world"s);
}

TEST(UrlEncodeTestSuite, SpecialChars) {
    EXPECT_EQ(UrlEncode(";,/?:@&=+$#"sv), "%3B%2C%2F%3F%3A%40%26%3D%2B%24%23"s);
}

TEST(UrlEncodeTestSuite, ControlAndExtendedChars) {
    EXPECT_EQ(UrlEncode("\x01\x80"sv), "%01%80"s);
}

