#include <catch2/catch_test_macros.hpp>

#include "../src/htmldecode.h"

using namespace std::literals;

TEST_CASE("Text without mnemonics", "[HtmlDecode]") {
    CHECK(HtmlDecode(""sv) == ""s);
    CHECK(HtmlDecode("hello"sv) == "hello"s);
    CHECK(HtmlDecode("A string with spaces and punctuation.!@#$%^*()_+"sv) == "A string with spaces and punctuation.!@#$%^*()_+ "s);
}

TEST_CASE("Decoding single mnemonics", "[HtmlDecode]") {
    // Нижний регистр без ;
    CHECK(HtmlDecode("&lt") == "<");
    CHECK(HtmlDecode("&gt") == ">");
    CHECK(HtmlDecode("&amp") == "&");
    CHECK(HtmlDecode("&apos") == "'");
    CHECK(HtmlDecode("&quot") == "\"");

    // Нижний регистр с ;
    CHECK(HtmlDecode("&lt;") == "<");
    CHECK(HtmlDecode("&gt;") == ">");
    CHECK(HtmlDecode("&amp;") == "&");
    CHECK(HtmlDecode("&apos;") == "'");
    CHECK(HtmlDecode("&quot;") == "\"");

    // Верхний регистр без ;
    CHECK(HtmlDecode("&LT") == "<");
    CHECK(HtmlDecode("&GT") == ">");
    CHECK(HtmlDecode("&AMP") == "&");
    CHECK(HtmlDecode("&APOS") == "'");
    CHECK(HtmlDecode("&QUOT") == "\"");

    // Верхний регистр с ;
    CHECK(HtmlDecode("&LT;") == "<");
    CHECK(HtmlDecode("&GT;") == ">");
    CHECK(HtmlDecode("&AMP;") == "&");
    CHECK(HtmlDecode("&APOS;") == "'");
    CHECK(HtmlDecode("&QUOT;") == "\"");
}

TEST_CASE("Decoding combined with text", "[HtmlDecode]") {
    CHECK(HtmlDecode("Cat &lt;says&gt; &quot;Meow&quot;. M&amp;M&apos;s") == "Cat <says> \"Meow\". M&M's");
    CHECK(HtmlDecode("Johnson&amp;Johnson") == "Johnson&Johnson");
    CHECK(HtmlDecode("Johnson&AMPJohnson") == "Johnson&Johnson");
    CHECK(HtmlDecode("left &lt; middle &gt; right") == "left < middle > right");
}

TEST_CASE("Invalid, mixed-case, and unknown mnemonics are not decoded", "[HtmlDecode]") {
    // Смешанные
    CHECK(HtmlDecode("&Lt;") == "&Lt;");
    CHECK(HtmlDecode("&gT") == "&gT");
    CHECK(HtmlDecode("&aMp;") == "&aMp;");
    CHECK(HtmlDecode("&ApOs") == "&ApOs");

    // Неизвестные мнемоники
    CHECK(HtmlDecode("&abracadabra;") == "&abracadabra;");
    CHECK(HtmlDecode("&hello") == "&hello");

    // & на конце
    CHECK(HtmlDecode("hello&") == "hello&");
    
    // Неполные мнемоники
    CHECK(HtmlDecode("&l") == "&l");
    CHECK(HtmlDecode("&am") == "&am");
    CHECK(HtmlDecode("&ap") == "&ap");
}

TEST_CASE("Decoded characters are not re-decoded", "[HtmlDecode]") {
    CHECK(HtmlDecode("&amp;lt;") == "&lt;");
    CHECK(HtmlDecode("&amp;gt;") == "&gt;");
    CHECK(HtmlDecode("&amp;amp;") == "&amp;");
    CHECK(HtmlDecode("&AMP;apos;") == "&apos;");
    CHECK(HtmlDecode("&amp;QUOT;") == "&QUOT;");
}

TEST_CASE("Complex cases from task description", "[HtmlDecode]") {
    CHECK(HtmlDecode("M&amp;M&APOSs") == "M&M's");
    CHECK(HtmlDecode("M&amp;M&apos;s") == "M&M's");
}