#include "search/bm25.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <initializer_list>
#include <limits>
#include <stdexcept>

using local_search::Bm25Contribution;

TEST_CASE("BM25 matches the reference example", "[bm25]") {
    const double score = Bm25Contribution(2, 2, 3, 4, 3.0);

    REQUIRE(score == Catch::Approx(0.5908617053374962).margin(1e-9));
    REQUIRE(score == Catch::Approx(
        Bm25Contribution(2, 2, 3, 4, 3.0, 1.2, 0.75)).margin(1e-12));
}

TEST_CASE("BM25 returns zero when the word is absent", "[bm25]") {
    REQUIRE(Bm25Contribution(0, 2, 3, 4, 3.0) == 0.0);
    REQUIRE(Bm25Contribution(0, 0, 0, 0, 0.0) == 0.0);
}

TEST_CASE("BM25 gives a larger contribution to a rarer word", "[bm25]") {
    const double rare = Bm25Contribution(2, 1, 10, 20, 20.0);
    const double common = Bm25Contribution(2, 8, 10, 20, 20.0);

    REQUIRE(common > 0.0);
    REQUIRE(rare > common);
}

TEST_CASE("BM25 term frequency grows with saturation", "[bm25]") {
    const double once = Bm25Contribution(1, 2, 10, 100, 100.0);
    const double twice = Bm25Contribution(2, 2, 10, 100, 100.0);
    const double three_times = Bm25Contribution(3, 2, 10, 100, 100.0);
    const double many_times = Bm25Contribution(100, 2, 10, 100, 100.0);

    REQUIRE(once > 0.0);
    REQUIRE(twice > once);
    REQUIRE(three_times > twice);
    REQUIRE(many_times > three_times);
    REQUIRE(twice < 2.0 * once);
    REQUIRE(three_times - twice < twice - once);
    REQUIRE(many_times < 2.2 * once);
}

TEST_CASE("BM25 penalizes longer documents at the same frequency", "[bm25]") {
    const double short_document = Bm25Contribution(2, 2, 10, 10, 20.0);
    const double average_document = Bm25Contribution(2, 2, 10, 20, 20.0);
    const double long_document = Bm25Contribution(2, 2, 10, 40, 20.0);

    REQUIRE(short_document > average_document);
    REQUIRE(average_document > long_document);
}

TEST_CASE("BM25 keeps IDF positive when the word occurs everywhere", "[bm25]") {
    const double score = Bm25Contribution(1, 10, 10, 20, 20.0);

    REQUIRE(std::isfinite(score));
    REQUIRE(score > 0.0);
    REQUIRE(score == Catch::Approx(std::log1p(0.5 / 10.5)).margin(1e-12));
}

TEST_CASE("BM25 handles a collection with one document", "[bm25]") {
    const double score = Bm25Contribution(1, 1, 1, 1, 1.0);

    REQUIRE(score == Catch::Approx(0.2876820724517809).margin(1e-12));
}

TEST_CASE("BM25 disables length normalization when b is zero", "[bm25]") {
    const double short_document = Bm25Contribution(2, 2, 10, 10, 20.0, 1.2, 0.0);
    const double long_document = Bm25Contribution(2, 2, 10, 100, 20.0, 1.2, 0.0);

    REQUIRE(short_document == Catch::Approx(long_document).margin(1e-12));
}

TEST_CASE("BM25 accepts full length normalization when b is one", "[bm25]") {
    const double score = Bm25Contribution(1, 5, 10, 10, 20.0, 1.0, 1.0);

    REQUIRE(score == Catch::Approx(0.9241962407465937).margin(1e-12));
}

TEST_CASE("BM25 uses custom k1 and b values", "[bm25]") {
    const double score = Bm25Contribution(3, 5, 10, 20, 10.0, 2.0, 0.5);

    REQUIRE(score == Catch::Approx(1.0397207708399179).margin(1e-12));
}

TEST_CASE("BM25 rejects invalid document statistics", "[bm25][validation]") {
    SECTION("Zero document count") {
        REQUIRE_THROWS_AS(
            Bm25Contribution(1, 1, 0, 10, 10.0), std::invalid_argument);
    }

    SECTION("Zero document frequency") {
        REQUIRE_THROWS_AS(
            Bm25Contribution(1, 0, 10, 10, 10.0), std::invalid_argument);
    }

    SECTION("Document frequency exceeds the document count") {
        REQUIRE_THROWS_AS(
            Bm25Contribution(1, 11, 10, 10, 10.0), std::invalid_argument);
    }

    SECTION("Term frequency exceeds the document length") {
        REQUIRE_THROWS_AS(
            Bm25Contribution(11, 2, 10, 10, 10.0), std::invalid_argument);
    }

    SECTION("Positive term frequency in an empty document") {
        REQUIRE_THROWS_AS(
            Bm25Contribution(1, 2, 10, 0, 10.0), std::invalid_argument);
    }
}

TEST_CASE("BM25 rejects invalid average document lengths", "[bm25][validation]") {
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    for (double average_length : {0.0, -1.0, infinity, -infinity, nan}) {
        CAPTURE(average_length);
        REQUIRE_THROWS_AS(
            Bm25Contribution(2, 2, 10, 20, average_length), std::invalid_argument);
    }
}

TEST_CASE("BM25 rejects invalid k1 values", "[bm25][validation]") {
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    for (double k1 : {0.0, -1.0, infinity, -infinity, nan}) {
        CAPTURE(k1);
        REQUIRE_THROWS_AS(
            Bm25Contribution(2, 2, 10, 20, 20.0, k1, 0.75), std::invalid_argument);
    }
}

TEST_CASE("BM25 rejects invalid b values", "[bm25][validation]") {
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    for (double b : {-0.01, 1.01, infinity, -infinity, nan}) {
        CAPTURE(b);
        REQUIRE_THROWS_AS(
            Bm25Contribution(2, 2, 10, 20, 20.0, 1.2, b), std::invalid_argument);
    }
}
