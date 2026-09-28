#include "hashing.hpp"
#include "test_support.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

using bb::test::TempDir;

TEST(Hashing, MatchesBlake3TestVectors) {
    EXPECT_EQ(bb::hash_bytes(""), "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262");
    EXPECT_EQ(bb::hash_bytes("abc"),
              "6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85");
}

TEST(Hashing, HashIsLowercaseHex) {
    const auto hash = bb::hash_bytes("bb");
    EXPECT_EQ(hash.size(), 64u);
    EXPECT_EQ(hash.find_first_not_of("0123456789abcdef"), std::string::npos);
}

TEST(Hashing, FileHashEqualsHashOfItsContents) {
    TempDir dir;

    // Longer than hash_file's 64 KiB read buffer and not a multiple of it, so the short final
    // read matters.
    std::string contents(2 * 64 * 1024 + 17, '\0');
    for (std::size_t i = 0; i < contents.size(); ++i) {
        contents[i] = static_cast<char>(i * 31);
    }

    for (const std::size_t size : {std::size_t{0}, std::size_t{5}, contents.size()}) {
        const auto part = std::string_view{contents}.substr(0, size);
        bb::test::write_file(dir / "file", part);

        const auto hash = bb::hash_file(dir / "file");
        ASSERT_TRUE(hash) << hash.error();
        EXPECT_EQ(*hash, bb::hash_bytes(part)) << "size " << size;
    }
}

TEST(Hashing, MissingFileIsAnError) {
    TempDir dir;
    const auto hash = bb::hash_file(dir / "missing");
    ASSERT_FALSE(hash);
    EXPECT_NE(hash.error().find("missing"), std::string::npos);
}
