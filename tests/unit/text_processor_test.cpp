#include "search/errors.hpp"
#include "search/text_processor.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <vector>

using local_search::TextProcessor;
using local_search::ValidationError;

TEST_CASE("Russian and English text is lowercased", "[text_processor]") {
    TextProcessor processor;
    const std::vector<std::string> expected{
        "привет", "world", "привет", "123"
    };

    REQUIRE(processor.Process("Привет, WORLD! Привет 123") == expected);
}

TEST_CASE("Empty input and separators produce no tokens", "[text_processor]") {
    TextProcessor processor;

    REQUIRE(processor.Process("").empty());
    REQUIRE(processor.Process(" \t\r\n  ").empty());
    REQUIRE(processor.Process("!!! ,.;: () [] {} +_-").empty());
}

TEST_CASE("Repeated separators do not create empty tokens", "[text_processor]") {
    TextProcessor processor;
    const std::vector<std::string> expected{"кот", "дом", "мир"};

    REQUIRE(processor.Process("  кот\t\tдом\r\n\nмир  ") == expected);
}

TEST_CASE("Token order and duplicates are preserved", "[text_processor]") {
    TextProcessor processor;
    const std::vector<std::string> expected{"кот", "дом", "кот", "кот"};

    REQUIRE(processor.Process("кот дом кот кот") == expected);
}

TEST_CASE("Punctuation splits words", "[text_processor]") {
    TextProcessor processor;
    const std::vector<std::string> expected{"c", "hello", "world", "foo", "bar"};

    REQUIRE(processor.Process("C++ hello_world foo-bar") == expected);
}

TEST_CASE("Yo and Ye remain different letters", "[text_processor][unicode]") {
    TextProcessor processor;
    const std::vector<std::string> expected{"ёж", "ёж", "еж"};

    REQUIRE(processor.Process("ЁЖ ёж ЕЖ") == expected);
}

TEST_CASE("Canonically equivalent spellings normalize equally", "[text_processor][unicode]") {
    TextProcessor processor;
    const std::vector<std::string> russian{"ёж"};
    const std::vector<std::string> latin{"café"};

    REQUIRE(processor.Process("ЁЖ") == russian);
    REQUIRE(processor.Process("Е\u0308Ж") == russian);
    REQUIRE(processor.Process("CAFÉ") == latin);
    REQUIRE(processor.Process("CAFE\u0301") == latin);
}

TEST_CASE("Stress marks inside Russian words are preserved", "[text_processor][stress]") {
    TextProcessor processor;
    const std::vector<std::string> expected{"мо\u0301ре", "вода\u0301"};
    const std::vector<std::string> distinct{"море", "мо\u0301ре"};

    REQUIRE(processor.Process("МО\u0301РЕ, ВОДА\u0301!") == expected);
    REQUIRE(processor.Process("море мо\u0301ре") == distinct);
}

TEST_CASE("NFC orders multiple combining marks", "[text_processor][stress]") {
    TextProcessor processor;
    const std::vector<std::string> expected{"а\u0323\u0301"};

    REQUIRE(processor.Process("а\u0301\u0323") == expected);
    REQUIRE(processor.Process("а\u0323\u0301") == expected);
}

TEST_CASE("Combining marks cannot start a token", "[text_processor][stress]") {
    TextProcessor processor;
    const std::vector<std::string> expected{"кот", "дом"};

    REQUIRE(processor.Process("\u0301кот \u0300\u0301дом") == expected);
    REQUIRE(processor.Process("кот,\u0301дом") == expected);
}

TEST_CASE("Standalone combining marks produce no tokens", "[text_processor][stress]") {
    TextProcessor processor;

    REQUIRE(processor.Process("\u0301").empty());
    REQUIRE(processor.Process("\u0301\u0300 \u0323").empty());
    REQUIRE(processor.Process("\u093E \u20DD").empty());
}

TEST_CASE("Spacing and enclosing marks can continue a token", "[text_processor][unicode]") {
    TextProcessor processor;
    const std::vector<std::string> expected{
        "\u0915\u093E", // Devanagari KA + spacing vowel sign AA (Mc).
        "a\u20DD"      // Letter a + combining enclosing circle (Me).
    };

    REQUIRE(processor.Process("\u0915\u093E a\u20DD") == expected);
}

TEST_CASE("Unicode letters and decimal digits are retained", "[text_processor][unicode]") {
    TextProcessor processor;
    const std::vector<std::string> expected{"кот", "猫", "１２", "a١٢"};

    REQUIRE(processor.Process("кот 猫 １２ a١٢") == expected);
}

TEST_CASE("Emoji split words without breaking iteration", "[text_processor][unicode]") {
    TextProcessor processor;
    const std::vector<std::string> expected{"кот", "дом"};

    REQUIRE(processor.Process("\U0001F600кот\U0001F600\U0001F600дом\U0001F600") == expected);
    REQUIRE(processor.Process("\U0001F600\U0001F600").empty());
}

TEST_CASE("Letters outside the BMP are lowercased and retained", "[text_processor][unicode]") {
    TextProcessor processor;
    const std::vector<std::string> expected{"\U00010428", "\U00010428"};

    REQUIRE(processor.Process("\U00010400 \U00010428") == expected);
}

TEST_CASE("Leading BOM is ignored and internal BOM separates tokens", "[text_processor][unicode]") {
    TextProcessor processor;
    const std::string bom = "\xEF\xBB\xBF";
    const std::vector<std::string> first{"привет"};
    const std::vector<std::string> middle{"ко", "т"};

    REQUIRE(processor.Process(bom + "ПРИВЕТ") == first);
    REQUIRE(processor.Process(bom).empty());
    REQUIRE(processor.Process(std::string("ко") + bom + "т") == middle);
}

TEST_CASE("Malformed UTF-8 is rejected", "[text_processor][validation]") {
    TextProcessor processor;
    const std::vector<std::string> invalid{
        "\xFF",             // Forbidden byte.
        "\xD0",             // Truncated two-byte sequence.
        "\x80",             // Continuation byte without a lead byte.
        "\xC0\xAF",         // Overlong encoding.
        "\xE2\x28\xA1",     // Invalid continuation byte.
        "\xED\xA0\x80",     // Encoded surrogate U+D800.
        "\xF4\x90\x80\x80"  // Code point above U+10FFFF.
    };

    for (const auto& input : invalid) {
        CAPTURE(input);
        REQUIRE_THROWS_AS(processor.Process(input), ValidationError);
    }
}

TEST_CASE("NUL is rejected even inside a sized string", "[text_processor][validation]") {
    TextProcessor processor;
    const std::string with_nul("a\0b", 3);
    const std::string only_nul(1, '\0');

    REQUIRE_THROWS_AS(processor.Process(with_nul), ValidationError);
    REQUIRE_THROWS_AS(processor.Process(only_nul), ValidationError);
}

TEST_CASE("One processor can handle repeated calls without modifying input", "[text_processor]") {
    TextProcessor processor;
    const std::string input = "ПРИВЕТ, мир!";
    const std::string original = input;
    const std::vector<std::string> first{"привет", "мир"};
    const std::vector<std::string> second{"кот"};

    REQUIRE(processor.Process(input) == first);
    REQUIRE(input == original);
    REQUIRE(processor.Process("\u0301КОТ") == second);
    REQUIRE(processor.Process("").empty());
    REQUIRE(processor.Process(input) == first);
}
