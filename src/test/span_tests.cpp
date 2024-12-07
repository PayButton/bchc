// Copyright (c) 2022-2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <span.h>

#include <test/check_assert.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <vector>

BOOST_FIXTURE_TEST_SUITE(span_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(span_from_vector) {
    const std::vector<uint8_t> vec{2, 3, 1};

    auto span = Span(vec);
    BOOST_CHECK_EQUAL(span.size(), 3);
    BOOST_CHECK_EQUAL(span.front(), 2);
    BOOST_CHECK_EQUAL(span.back(), 1);
    BOOST_CHECK_EQUAL(span[1], 3);

    BOOST_CHECK_EQUAL(&vec[1], &span[1]);
}

BOOST_AUTO_TEST_CASE(span_from_c_array) {
    uint8_t array[]{5, 6, 4};

    auto span = Span(array);
    BOOST_CHECK_EQUAL(span.size(), 3);
    BOOST_CHECK_EQUAL(span.front(), 5);
    BOOST_CHECK_EQUAL(span.back(), 4);
    BOOST_CHECK_EQUAL(span[1], 6);

    BOOST_CHECK_EQUAL(&array[1], &span[1]);
}

BOOST_AUTO_TEST_CASE(span_pop) {
    const std::vector<uint8_t> vec{7, 6, 5, 4, 3, 2, 1};

    auto span = Span(vec);

    BOOST_CHECK_EQUAL(span.pop_front(), 7);
    BOOST_CHECK_EQUAL(span.size(), 6);
    BOOST_CHECK_EQUAL(span.front(), 6);
    BOOST_CHECK_EQUAL(span.back(), 1);

    BOOST_CHECK_EQUAL(span.pop_back(), 1);
    BOOST_CHECK_EQUAL(span.size(), 5);
    BOOST_CHECK_EQUAL(span.front(), 6);
    BOOST_CHECK_EQUAL(span.back(), 2);

    BOOST_CHECK_EQUAL(span.pop_back(), 2);
    BOOST_CHECK_EQUAL(span.size(), 4);

    BOOST_CHECK_EQUAL(span.pop_back(), 3);
    BOOST_CHECK_EQUAL(span.size(), 3);

    BOOST_CHECK_EQUAL(span.pop_front(), 6);
    BOOST_CHECK_EQUAL(span.size(), 2);

    BOOST_CHECK_EQUAL(span.pop_back(), 4);
    BOOST_CHECK_EQUAL(span.size(), 1);
    BOOST_CHECK_EQUAL(span.front(), 5);
    BOOST_CHECK_EQUAL(span.front(), span.back());

    BOOST_CHECK_EQUAL(span.pop_back(), 5);
    BOOST_CHECK_EQUAL(span.size(), 0);
    BOOST_CHECK(span.empty());

    // popping an empty span should trigger an assert - unless sanitizers or unsupported platform
    // which is why we use BCHN_CHECK_ASSERT_IF_SUPPORTED macro
    BCHN_CHECK_ASSERT_IF_SUPPORTED(span.pop_back());
    BCHN_CHECK_ASSERT_IF_SUPPORTED(span.pop_front());
    BOOST_CHECK_EQUAL(span.size(), 0);
}

BOOST_AUTO_TEST_CASE(span_compare) {
    {
        // test equality operators
        std::vector<uint8_t> vec{3, 2, 1};
        uint8_t array[]{3, 2, 1};

        auto span1 = Span(vec);
        auto span2 = Span(array);

        BOOST_CHECK(span1 == span2);
        BOOST_CHECK_EQUAL(span1 != span2, false);

        span1[1] = 123;
        BOOST_CHECK(span1 != span2);
        BOOST_CHECK_EQUAL(span1 == span2, false);

        span2[1] = 123;
        BOOST_CHECK(span1 == span2);
        BOOST_CHECK_EQUAL(span1 != span2, false);
    }
    {
        // test comparison operators
        std::vector<uint8_t> vec1{1, 2, 3, 4, 5, 6};
        std::vector<uint8_t> vec2{5, 5, 5, 5, 5, 5};

        auto span1 = Span(vec1);
        auto span2 = Span(vec2);

        BOOST_CHECK(span1 < span2);
        BOOST_CHECK(!(span1 > span2));
        BOOST_CHECK(span1 <= span2);
        BOOST_CHECK(!(span1 >= span2));

        vec2 = {0, 0, 0, 0, 0, 1};
        span2 = Span(vec2);

        BOOST_CHECK(!(span1 < span2));
        BOOST_CHECK(span1 > span2);
        BOOST_CHECK(!(span1 <= span2));
        BOOST_CHECK(span1 >= span2);

        vec2 = {6, 5, 4, 3, 2, 1};
        span2 = Span(vec2);

        BOOST_CHECK(span1 < span2);
        BOOST_CHECK(!(span1 > span2));
        BOOST_CHECK(span1 <= span2);
        BOOST_CHECK(!(span1 >= span2));
    }
}

BOOST_AUTO_TEST_CASE(span_subspan) {
    const std::vector<uint8_t> vec1{7, 6, 5, 4, 3, 2, 1};
    const std::vector<uint8_t> vec2{   6, 5, 4, 3      };

    auto span1 = Span(vec1);
    auto span2 = Span(vec2);
    auto span3 = span1.subspan(1, 4);

    BOOST_CHECK_EQUAL(span1.size(), 7);
    BOOST_CHECK_EQUAL(span2.size(), 4);
    BOOST_CHECK_EQUAL(span3.size(), 4);

    BOOST_CHECK(span1 != span2);
    BOOST_CHECK(span1 != span3);
    BOOST_CHECK(span3 == span1.subspan(1, 4));
    BOOST_CHECK(span3 == span2);

    BOOST_CHECK(span1.subspan(0, 0).empty());

    BOOST_CHECK_EQUAL(span1.pop_front(), 7);
    BOOST_CHECK(span1.first(4) == span2);
    BOOST_CHECK(span1.first(4).last(3) == span2.last(3));
    BOOST_CHECK_EQUAL(span1.pop_back(), 1);
    BOOST_CHECK_EQUAL(span1.pop_back(), 2);
    BOOST_CHECK(span1 == span2);
    BOOST_CHECK(span1 == span3);

    // subspan out of bounds is UB as per std::span, so we don't test for that
}

BOOST_AUTO_TEST_CASE(span_iteration) {
    const std::vector<uint8_t> vec{7, 6, 5, 4, 3, 2, 1};
    auto span = Span(vec);

    uint8_t val = 7;
    for (auto const &it : span) {
        BOOST_CHECK_EQUAL(it, val--);
    }
}

BOOST_AUTO_TEST_CASE(span_corner_cases) {
    {
        // test empty span
        const std::vector<uint8_t> vec;

        auto span = Span(vec);
        BOOST_CHECK_EQUAL(span.size(), 0);
        BOOST_CHECK(span.empty());
    }

    {
        // test large span
        const std::vector<uint8_t> vec(1024 * 1024, 123);

        auto span = Span(vec);
        BOOST_CHECK(!span.empty());
        BOOST_CHECK_EQUAL(span.size(), 1024 * 1024);
        BOOST_CHECK_EQUAL(span[1023], 123);
    }
}

BOOST_AUTO_TEST_SUITE_END()
