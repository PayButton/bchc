// Copyright (c) 2012-2015 The Bitcoin Core developers
// Copyright (c) 2021-2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/script.h>

#include <test/scriptnum10.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <climits>
#include <cstdint>

BOOST_FIXTURE_TEST_SUITE(scriptnum_tests, BasicTestingSetup)

static constexpr int64_t int64_t_min = std::numeric_limits<int64_t>::min();
static constexpr int64_t int64_t_max = std::numeric_limits<int64_t>::max();
static constexpr int64_t int64_t_min_8_bytes = int64_t_min + 1;

static const int64_t values[] = {0,
                                 1,
                                 -1,
                                 -2,
                                 127,
                                 128,
                                 -255,
                                 256,
                                 (1LL << 15) - 1,
                                 -(1LL << 16),
                                 (1LL << 24) - 1,
                                 (1LL << 31),
                                 1 - (1LL << 32),
                                 1LL << 40,
                                 int64_t_min_8_bytes,
                                 int64_t_min,
                                 int64_t_max};

static const int64_t offsets[] = {1, 0x79, 0x80, 0x81, 0xFF, 0x7FFF, 0x8000, 0xFFFF, 0x10000};

template <typename ScriptNumType>
std::enable_if_t<std::is_same_v<ScriptNumType, CScriptNum> || std::is_same_v<ScriptNumType, ScriptBigInt>, bool>
/* bool */ verify(const CScriptNum10 &bignum, const ScriptNumType &scriptnum) {
    return bignum.getvch() == scriptnum.getvch() &&
           bignum.getint() == scriptnum.getint32();
}

static
void CheckCreateVchOldRules(int64_t x) {
    size_t const maxIntegerSize = CScriptNum::MAXIMUM_ELEMENT_SIZE_32_BIT;

    CScriptNum10 bigx(x);
    auto const scriptx = CScriptNum::fromIntUnchecked(x);
    BOOST_CHECK(verify(bigx, scriptx));
    auto const scriptx2 = ScriptBigInt::fromIntUnchecked(x);
    BOOST_CHECK(verify(bigx, scriptx2));

    CScriptNum10 bigb(bigx.getvch(), false);
    CScriptNum scriptb(scriptx.getvch(), false, maxIntegerSize);
    BOOST_CHECK(verify(bigb, scriptb));
    ScriptBigInt scriptb2(scriptx2.getvch(), false, maxIntegerSize);
    BOOST_CHECK(verify(bigb, scriptb2));

    CScriptNum10 bigx3(scriptb.getvch(), false);
    CScriptNum scriptx3(bigb.getvch(), false, maxIntegerSize);
    BOOST_CHECK(verify(bigx3, scriptx3));
    ScriptBigInt scriptx3_2(bigb.getvch(), false, maxIntegerSize);
    BOOST_CHECK(verify(bigx3, scriptx3_2));
}

static
void CheckCreateVchNewRules(int64_t x) {
    size_t const maxIntegerSize = CScriptNum::MAXIMUM_ELEMENT_SIZE_64_BIT;

    auto res = CScriptNum::fromInt(x);
    auto res2 = ScriptBigInt::fromInt(x);
    BOOST_REQUIRE(res2);// Creation for BigInt based script num should work for all possible int64's
    if ( ! res) {
        BOOST_CHECK(x == int64_t_min);
        return;
    }
    auto const scriptx = *res;
    auto const scriptx2 = *res2;

    CScriptNum10 bigx(x);
    BOOST_CHECK(verify(bigx, scriptx));
    BOOST_CHECK(verify(bigx, scriptx2));

    CScriptNum10 bigb(bigx.getvch(), false, maxIntegerSize);
    CScriptNum scriptb(scriptx.getvch(), false, maxIntegerSize);
    BOOST_CHECK(verify(bigb, scriptb));
    ScriptBigInt scriptb2(scriptx2.getvch(), false, maxIntegerSize);
    BOOST_CHECK(verify(bigb, scriptb2));

    CScriptNum10 bigx3(scriptb.getvch(), false, maxIntegerSize);
    CScriptNum scriptx3(bigb.getvch(), false, maxIntegerSize);
    BOOST_CHECK(verify(bigx3, scriptx3));
    ScriptBigInt scriptx3_2(bigb.getvch(), false, maxIntegerSize);
    BOOST_CHECK(verify(bigx3, scriptx3_2));
}

static
void CheckCreateIntOldRules(int64_t x) {
    auto const scriptx = CScriptNum::fromIntUnchecked(x);
    CScriptNum10 const bigx(x);
    auto const scriptx2 = ScriptBigInt ::fromIntUnchecked(x);
    BOOST_CHECK(verify(bigx, scriptx));
    BOOST_CHECK(verify(bigx, scriptx2));
    BOOST_CHECK(verify(CScriptNum10(bigx.getint()), CScriptNum::fromIntUnchecked(scriptx.getint32())));
    BOOST_CHECK(verify(CScriptNum10(bigx.getint()), ScriptBigInt::fromIntUnchecked(scriptx2.getint32())));
    BOOST_CHECK(verify(CScriptNum10(scriptx.getint32()), CScriptNum::fromIntUnchecked(bigx.getint())));
    BOOST_CHECK(verify(CScriptNum10(scriptx2.getint32()), ScriptBigInt::fromIntUnchecked(bigx.getint())));
    BOOST_CHECK(verify(CScriptNum10(CScriptNum10(scriptx.getint32()).getint()),
                       CScriptNum::fromIntUnchecked(CScriptNum::fromIntUnchecked(bigx.getint()).getint32())));
    BOOST_CHECK(verify(CScriptNum10(CScriptNum10(scriptx.getint32()).getint()),
                       ScriptBigInt::fromIntUnchecked(ScriptBigInt::fromIntUnchecked(bigx.getint()).getint32())));
}

static
void CheckCreateIntNewRules(int64_t x) {
    auto res = CScriptNum::fromInt(x);
    auto res2 = ScriptBigInt::fromInt(x);
    BOOST_REQUIRE(res2);
    if ( ! res) {
        BOOST_CHECK(x == int64_t_min);
        return;
    }
    auto const scriptx = *res;
    auto const scriptx2 = *res2;

    CScriptNum10 const bigx(x);
    BOOST_CHECK(verify(bigx, scriptx));
    BOOST_CHECK(verify(bigx, scriptx2));
    BOOST_CHECK(verify(CScriptNum10(bigx.getint()), CScriptNum::fromIntUnchecked(scriptx.getint32())));
    BOOST_CHECK(verify(CScriptNum10(bigx.getint()), ScriptBigInt::fromIntUnchecked(scriptx2.getint32())));
    BOOST_CHECK(verify(CScriptNum10(scriptx.getint32()), CScriptNum::fromIntUnchecked(bigx.getint())));
    BOOST_CHECK(verify(CScriptNum10(scriptx2.getint32()), ScriptBigInt::fromIntUnchecked(bigx.getint())));
    BOOST_CHECK(verify(CScriptNum10(CScriptNum10(scriptx.getint32()).getint()),
                       CScriptNum::fromIntUnchecked(CScriptNum::fromIntUnchecked(bigx.getint()).getint32())));
    BOOST_CHECK(verify(CScriptNum10(CScriptNum10(scriptx2.getint32()).getint()),
                       ScriptBigInt::fromIntUnchecked(ScriptBigInt::fromIntUnchecked(bigx.getint()).getint32())));
}

static
void CheckAddOldRules(int64_t a, int64_t b) {
    if (a == int64_t_min || b == int64_t_min) {
        return;
    }

    CScriptNum10 const biga(a);
    CScriptNum10 const bigb(b);
    auto const scripta = CScriptNum::fromIntUnchecked(a);
    auto const scriptb = CScriptNum::fromIntUnchecked(b);
    auto const scripta2 = ScriptBigInt::fromIntUnchecked(a);
    auto const scriptb2 = ScriptBigInt::fromIntUnchecked(b);

    // int64_t overflow is undefined.
    bool overflowing = (b > 0 && a > int64_t_max - b) ||
                       (b < 0 && a < int64_t_min_8_bytes - b);

    if ( ! overflowing) {
        auto res = scripta.safeAdd(scriptb);
        auto res2 = scripta2.safeAdd(scriptb2);
        BOOST_CHECK(res);
        BOOST_CHECK(res2);
        BOOST_CHECK(verify(biga + bigb, *res));
        BOOST_CHECK(verify(biga + bigb, *res2));
        res = scripta.safeAdd(b);
        res2 = scripta2.safeAdd(b);
        BOOST_CHECK(res);
        BOOST_CHECK(res2);
        BOOST_CHECK(verify(biga + bigb, *res));
        BOOST_CHECK(verify(biga + bigb, *res2));
        res = scriptb.safeAdd(scripta);
        res2 = scriptb2.safeAdd(scripta2);
        BOOST_CHECK(res);
        BOOST_CHECK(res2);
        BOOST_CHECK(verify(biga + bigb, *res));
        BOOST_CHECK(verify(biga + bigb, *res2));
        res = scriptb.safeAdd(a);
        res2 = scriptb2.safeAdd(a);
        BOOST_CHECK(res);
        BOOST_CHECK(res2);
        BOOST_CHECK(verify(biga + bigb, *res));
        BOOST_CHECK(verify(biga + bigb, *res2));
    } else {
        BOOST_CHECK(!scripta.safeAdd(scriptb));
        BOOST_CHECK(!scripta.safeAdd(b));
        BOOST_CHECK(!scriptb.safeAdd(a));
        // BigInt version won't overflow in this case
        BOOST_CHECK(scripta2.safeAdd(scriptb2));
        BOOST_CHECK(scripta2.safeAdd(b));
        BOOST_CHECK(scriptb2.safeAdd(a));
    }
}

static
void CheckAddNewRules(int64_t a, int64_t b) {
    auto res = CScriptNum::fromInt(a);
    auto res2 = ScriptBigInt::fromInt(a);
    BOOST_REQUIRE(res2);
    if ( ! res) {
        BOOST_CHECK(a == int64_t_min);
        BOOST_CHECK(*res2 == int64_t_min);
        return;
    }
    auto const scripta = *res;
    auto const scripta2 = *res2;

    res = CScriptNum::fromInt(b);
    res2 = ScriptBigInt::fromInt(b);
    BOOST_REQUIRE(res2);
    if ( ! res) {
        BOOST_CHECK(b == int64_t_min);
        return;
    }
    auto const scriptb = *res;
    auto const scriptb2 = *res2;

    bool overflowing = (b > 0 && a > int64_t_max - b) ||
                       (b < 0 && a < int64_t_min_8_bytes - b);

    res = scripta.safeAdd(scriptb);
    BOOST_CHECK(bool(res) != overflowing);
    BOOST_CHECK( ! res || a + b == res->getint64());
    res2 = scripta2.safeAdd(scriptb2);
    BOOST_CHECK(bool(res2));
    BOOST_CHECK(BigInt(a) + b == res2->getBigInt());

    res = scripta.safeAdd(b);
    BOOST_CHECK(bool(res) != overflowing);
    BOOST_CHECK( ! res || a + b == res->getint64());
    res2 = scripta2.safeAdd(b);
    BOOST_CHECK(bool(res2));
    BOOST_CHECK(BigInt(a) + b == res2->getBigInt());

    res = scriptb.safeAdd(scripta);
    BOOST_CHECK(bool(res) != overflowing);
    BOOST_CHECK( ! res || b + a == res->getint64());
    res2 = scriptb2.safeAdd(scripta2);
    BOOST_CHECK(bool(res2));
    BOOST_CHECK(BigInt(b) + a == res2->getBigInt());

    res = scriptb.safeAdd(a);
    BOOST_CHECK(bool(res) != overflowing);
    BOOST_CHECK( ! res || b + a == res->getint64());
    res2 = scriptb2.safeAdd(a);
    BOOST_CHECK(bool(res2));
    BOOST_CHECK(BigInt(b) + a == res2->getBigInt());
}

static
void CheckSubtractOldRules(int64_t a, int64_t b) {
    if (a == int64_t_min || b == int64_t_min) {
        return;
    }

    CScriptNum10 const biga(a);
    CScriptNum10 const bigb(b);
    auto const scripta = CScriptNum::fromIntUnchecked(a);
    auto const scriptb = CScriptNum::fromIntUnchecked(b);
    auto const scripta2 = ScriptBigInt::fromIntUnchecked(a);
    auto const scriptb2 = ScriptBigInt::fromIntUnchecked(b);

    // int64_t overflow is undefined.
    bool overflowing = (b > 0 && a < int64_t_min_8_bytes + b) ||
                       (b < 0 && a > int64_t_max + b);

    if ( ! overflowing) {
        auto res = scripta.safeSub(scriptb);
        auto res2 = scripta2.safeSub(scriptb2);
        BOOST_CHECK(res);
        BOOST_CHECK(res2);
        BOOST_CHECK(verify(biga - bigb, *res));
        BOOST_CHECK(verify(biga - bigb, *res2));
        res = scripta.safeSub(b);
        res2 = scripta2.safeSub(b);
        BOOST_CHECK(res);
        BOOST_CHECK(res2);
        BOOST_CHECK(verify(biga - bigb, *res));
        BOOST_CHECK(verify(biga - bigb, *res2));
    } else {
        BOOST_CHECK(!scripta.safeSub(scriptb));
        BOOST_CHECK(!scripta.safeSub(b));
        // BigInt won't overflow here
        BOOST_CHECK(scripta2.safeSub(scriptb2));
        BOOST_CHECK(scripta2.safeSub(b));
    }

    overflowing = (a > 0 && b < int64_t_min_8_bytes + a) ||
                  (a < 0 && b > int64_t_max + a);

    if ( ! overflowing) {
        auto res = scriptb.safeSub(scripta);
        auto res2 = scriptb2.safeSub(scripta2);
        BOOST_CHECK(res);
        BOOST_CHECK(verify(bigb - biga, *res));
        BOOST_CHECK(res2);
        BOOST_CHECK(verify(bigb - biga, *res2));
        res = scriptb.safeSub(a);
        res2 = scriptb2.safeSub(a);
        BOOST_CHECK(res);
        BOOST_CHECK(verify(bigb - biga, *res));
        BOOST_CHECK(res2);
        BOOST_CHECK(verify(bigb - biga, *res2));
    } else {
        BOOST_CHECK(!scriptb.safeSub(scripta));
        BOOST_CHECK(!scriptb.safeSub(a));
        // BigInt won't overflow here
        BOOST_CHECK(scriptb2.safeSub(scripta2));
        BOOST_CHECK(scriptb2.safeSub(a));
    }
}

static
void CheckSubtractNewRules(int64_t a, int64_t b) {
    auto res = CScriptNum::fromInt(a);
    auto res2 = ScriptBigInt::fromInt(a);
    BOOST_REQUIRE(res2);
    if ( ! res) {
        BOOST_CHECK(a == int64_t_min);
        return;
    }
    auto const scripta = *res;
    auto const scripta2 = *res2;

    res = CScriptNum::fromInt(b);
    res2 = ScriptBigInt::fromInt(b);
    BOOST_REQUIRE(res2);
    if ( ! res) {
        BOOST_CHECK(b == int64_t_min);
        return;
    }
    auto const scriptb = *res;
    auto const scriptb2 = *res2;

    bool overflowing = (b > 0 && a < int64_t_min_8_bytes + b) ||
                       (b < 0 && a > int64_t_max + b);

    res = scripta.safeSub(scriptb);
    BOOST_CHECK(bool(res) != overflowing);
    BOOST_CHECK( ! res || a - b == res->getint64());
    res2 = scripta2.safeSub(scriptb2);
    BOOST_CHECK(res2);
    BOOST_CHECK(BigInt(a) - b == res2->getBigInt());

    res = scripta.safeSub(b);
    BOOST_CHECK(bool(res) != overflowing);
    BOOST_CHECK( ! res || a - b == res->getint64());
    res2 = scripta2.safeSub(b);
    BOOST_CHECK(res2);
    BOOST_CHECK(BigInt(a) - b == res2->getBigInt());

    overflowing = (a > 0 && b < int64_t_min_8_bytes + a) ||
                  (a < 0 && b > int64_t_max + a);

    res = scriptb.safeSub(scripta);
    BOOST_CHECK(bool(res) != overflowing);
    BOOST_CHECK( ! res || b - a == res->getint64());
    res2 = scriptb2.safeSub(scripta2);
    BOOST_CHECK(res2);
    BOOST_CHECK(BigInt(b) - a == res2->getBigInt());

    res = scriptb.safeSub(a);
    BOOST_CHECK(bool(res) != overflowing);
    BOOST_CHECK( ! res || b - a == res->getint64());
    res2 = scriptb2.safeSub(a);
    BOOST_CHECK(res2);
    BOOST_CHECK(BigInt(b) - a == res2->getBigInt());
}

static
void CheckMultiply(int64_t a, int64_t b) {
    auto res = CScriptNum::fromInt(a);
    auto res2 = ScriptBigInt::fromInt(a);
    BOOST_REQUIRE(res2);
    if ( ! res) {
        BOOST_CHECK(a == int64_t_min);
        return;
    }
    auto const scripta = *res;
    auto const scripta2 = *res2;

    res = CScriptNum::fromInt(b);
    res2 = ScriptBigInt::fromInt(b);
    BOOST_REQUIRE(res2);
    if ( ! res) {
        BOOST_CHECK(b == int64_t_min);
        return;
    }
    auto const scriptb = *res;
    auto const scriptb2 = *res2;

    res = scripta.safeMul(scriptb);
    BOOST_CHECK( ! res || a * b == res->getint64());
    res = scripta.safeMul(b);
    BOOST_CHECK( ! res || a * b == res->getint64());
    res = scriptb.safeMul(scripta);
    BOOST_CHECK( ! res || b * a == res->getint64());
    res = scriptb.safeMul(a);
    BOOST_CHECK( ! res || b * a == res->getint64());

    res2 = scripta2.safeMul(scriptb2);
    BOOST_CHECK(res2 && BigInt(a) * b == res2->getBigInt());
    res2 = scripta2.safeMul(b);
    BOOST_CHECK(res2 && BigInt(a) * b == res2->getBigInt());
    res2 = scriptb2.safeMul(scripta2);
    BOOST_CHECK(res2 && BigInt(b) * a == res2->getBigInt());
    res2 = scriptb2.safeMul(a);
    BOOST_CHECK(res2 && BigInt(b) * a == res2->getBigInt());
}

static
void CheckDivideOldRules(int64_t a, int64_t b) {
    CScriptNum10 const biga(a);
    CScriptNum10 const bigb(b);
    auto const scripta = CScriptNum::fromIntUnchecked(a);
    auto const scriptb = CScriptNum::fromIntUnchecked(b);
    auto const scripta2 = ScriptBigInt::fromIntUnchecked(a);
    auto const scriptb2 = ScriptBigInt::fromIntUnchecked(b);

    // int64_t overflow is undefined.
    bool overflowing = a == int64_t_min && b == -1;

    if (b != 0) {
        if ( ! overflowing) {
            auto res = scripta / scriptb;
            auto res2 = scripta2 / scriptb2;
            BOOST_CHECK(verify(CScriptNum10(a / b), res));
            BOOST_CHECK(verify(CScriptNum10(a / b), res2));
            res = scripta / b;
            res2 = scripta2 / b;
            BOOST_CHECK(verify(CScriptNum10(a / b), res));
            BOOST_CHECK(verify(CScriptNum10(a / b), res2));
        } else {
            BOOST_CHECK(scripta / scriptb == scripta);
            BOOST_CHECK(verify(biga, scripta / b));
            // BigInt-based impl doesn't overflow here, so check sanity
            BOOST_CHECK(BigInt(a) / b == (scripta2 / scriptb2).getBigInt());
        }
    } else {
        // BigInt-based impl will throw on divide-by-zero, so check that behavior.
        BOOST_CHECK_THROW(BigInt(a) / b, std::invalid_argument);
        BOOST_CHECK_THROW(scripta2 / b, std::invalid_argument);
        BOOST_CHECK_THROW(scripta2 / scriptb2, std::invalid_argument);
    }

    overflowing = b == int64_t_min && a == -1;

    if (a != 0) {
        if ( ! overflowing) {
            auto res = scriptb / scripta;
            auto res2 = scriptb2 / scripta2;
            BOOST_CHECK(verify(CScriptNum10(b / a), res));
            BOOST_CHECK(verify(CScriptNum10(b / a), res2));
            res = scriptb / a;
            res2 = scriptb2 / a;
            BOOST_CHECK(verify(CScriptNum10(b / a), res));
            BOOST_CHECK(verify(CScriptNum10(b / a), res2));
        } else {
            BOOST_CHECK(scriptb / scripta == scriptb);
            BOOST_CHECK(verify(bigb, scriptb / a));
            // BigInt-based impl doesn't overflow here, so check sanity
            BOOST_CHECK(BigInt(b) / a == (scriptb2 / scripta2).getBigInt());
        }
    } else {
        // BigInt-based impl will throw on divide-by-zero, so check that behavior.
        BOOST_CHECK_THROW(BigInt(b) / a, std::invalid_argument);
        BOOST_CHECK_THROW(scriptb2 / a, std::invalid_argument);
        BOOST_CHECK_THROW(scriptb2 / scripta2, std::invalid_argument);
    }
}

static
void CheckDivideNewRules(int64_t a, int64_t b) {
    auto res = CScriptNum::fromInt(a);
    auto res2 = ScriptBigInt::fromInt(a);
    BOOST_REQUIRE(res2);
    if ( ! res) {
        BOOST_CHECK(a == int64_t_min);
        return;
    }
    auto const scripta = *res;
    auto const scripta2 = *res2;

    res = CScriptNum::fromInt(b);
    res2 = ScriptBigInt::fromInt(b);
    BOOST_REQUIRE(res2);
    if ( ! res) {
        BOOST_CHECK(b == int64_t_min);
        return;
    }
    auto const scriptb = *res;
    auto const scriptb2 = *res2;

    if (b != 0) { // Prevent divide by 0
        auto val = scripta / scriptb;
        BOOST_CHECK(a / b == val.getint64());
        val = scripta / b;
        BOOST_CHECK(a / b == val.getint64());

        // Check BigInt also conforms (it has a slightly different interface for getint64())
        auto val2 = scripta2 / scriptb2;
        auto opti = val2.getint64();
        BOOST_REQUIRE(opti);
        BOOST_CHECK(a / b == *opti);
        val2 = scripta2 / b;
        opti = val2.getint64();
        BOOST_REQUIRE(opti);
        BOOST_CHECK(a / b == *opti);
    } else {
        // BigInt-based impl will throw on divide-by-zero, so check that behavior.
        BOOST_CHECK_THROW(BigInt(a) / b, std::invalid_argument);
        BOOST_CHECK_THROW(scripta2 / b, std::invalid_argument);
        BOOST_CHECK_THROW(scripta2 / scriptb2, std::invalid_argument);
    }
    if (a != 0) { // Prevent divide by 0
        auto val = scriptb / scripta;
        BOOST_CHECK(b / a == val.getint64());
        val = scriptb / a;
        BOOST_CHECK(b / a == val.getint64());
    } else {
        // BigInt-based impl will throw on divide-by-zero, so check that behavior.
        BOOST_CHECK_THROW(BigInt(b) / a, std::invalid_argument);
        BOOST_CHECK_THROW(scriptb2 / a, std::invalid_argument);
        BOOST_CHECK_THROW(scriptb2 / scripta2, std::invalid_argument);
    }
}

static
void CheckNegateOldRules(int64_t x) {
    const CScriptNum10 bigx(x);
    auto const scriptx = CScriptNum::fromIntUnchecked(x);
    auto const scriptx2 = ScriptBigInt::fromIntUnchecked(x);

    // -INT64_MIN is undefined
    if (x != int64_t_min) {
        BOOST_CHECK(verify(-bigx, -scriptx));
        BOOST_CHECK(verify(-bigx, -scriptx2));
    }
}

static
void CheckNegateNewRules(int64_t x) {
    auto res = CScriptNum::fromInt(x);
    auto res2 = ScriptBigInt::fromInt(x);
    BOOST_REQUIRE(res2);
    if ( ! res) {
        BOOST_CHECK(x == int64_t_min);
        return;
    }
    auto const scriptx = *res;
    CScriptNum10 const bigx(x);
    BOOST_CHECK(verify(-bigx, -scriptx));
    BOOST_CHECK(verify(-(-bigx), -(-scriptx)));

    auto const scriptx2 = *res2;
    BOOST_CHECK(verify(-bigx, -scriptx2));
    BOOST_CHECK(verify(-(-bigx), -(-scriptx2)));
}

static
void CheckCompare(int64_t a, int64_t b) {
    const CScriptNum10 biga(a);
    const CScriptNum10 bigb(b);
    auto const scripta = CScriptNum::fromIntUnchecked(a);
    auto const scriptb = CScriptNum::fromIntUnchecked(b);
    auto const scripta2 = ScriptBigInt::fromIntUnchecked(a);
    auto const scriptb2 = ScriptBigInt::fromIntUnchecked(b);

    // vs CScriptNum
    BOOST_CHECK((biga == biga) == (scripta == scripta));
    BOOST_CHECK((biga != biga) == (scripta != scripta));
    BOOST_CHECK((biga < biga) == (scripta < scripta));
    BOOST_CHECK((biga > biga) == (scripta > scripta));
    BOOST_CHECK((biga >= biga) == (scripta >= scripta));
    BOOST_CHECK((biga <= biga) == (scripta <= scripta));
    // vs ScriptBigNum
    BOOST_CHECK((biga == biga) == (scripta2 == scripta2));
    BOOST_CHECK((biga != biga) == (scripta2 != scripta2));
    BOOST_CHECK((biga < biga) == (scripta2 < scripta2));
    BOOST_CHECK((biga > biga) == (scripta2 > scripta2));
    BOOST_CHECK((biga >= biga) == (scripta2 >= scripta2));
    BOOST_CHECK((biga <= biga) == (scripta2 <= scripta2));

    // vs CScriptNum
    BOOST_CHECK((biga == biga) == (scripta == a));
    BOOST_CHECK((biga != biga) == (scripta != a));
    BOOST_CHECK((biga < biga) == (scripta < a));
    BOOST_CHECK((biga > biga) == (scripta > a));
    BOOST_CHECK((biga >= biga) == (scripta >= a));
    BOOST_CHECK((biga <= biga) == (scripta <= a));
    // vs ScriptBigNum
    BOOST_CHECK((biga == biga) == (scripta2 == a));
    BOOST_CHECK((biga != biga) == (scripta2 != a));
    BOOST_CHECK((biga < biga) == (scripta2 < a));
    BOOST_CHECK((biga > biga) == (scripta2 > a));
    BOOST_CHECK((biga >= biga) == (scripta2 >= a));
    BOOST_CHECK((biga <= biga) == (scripta2 <= a));

    // vs CScriptNum
    BOOST_CHECK((biga == bigb) == (scripta == scriptb));
    BOOST_CHECK((biga != bigb) == (scripta != scriptb));
    BOOST_CHECK((biga < bigb) == (scripta < scriptb));
    BOOST_CHECK((biga > bigb) == (scripta > scriptb));
    BOOST_CHECK((biga >= bigb) == (scripta >= scriptb));
    BOOST_CHECK((biga <= bigb) == (scripta <= scriptb));
    // vs ScriptBigNum
    BOOST_CHECK((biga == bigb) == (scripta2 == scriptb2));
    BOOST_CHECK((biga != bigb) == (scripta2 != scriptb2));
    BOOST_CHECK((biga < bigb) == (scripta2 < scriptb2));
    BOOST_CHECK((biga > bigb) == (scripta2 > scriptb2));
    BOOST_CHECK((biga >= bigb) == (scripta2 >= scriptb2));
    BOOST_CHECK((biga <= bigb) == (scripta2 <= scriptb2));

    // vs CScriptNum
    BOOST_CHECK((biga == bigb) == (scripta == b));
    BOOST_CHECK((biga != bigb) == (scripta != b));
    BOOST_CHECK((biga < bigb) == (scripta < b));
    BOOST_CHECK((biga > bigb) == (scripta > b));
    BOOST_CHECK((biga >= bigb) == (scripta >= b));
    BOOST_CHECK((biga <= bigb) == (scripta <= b));
    // vs ScriptBigNum
    BOOST_CHECK((biga == bigb) == (scripta2 == b));
    BOOST_CHECK((biga != bigb) == (scripta2 != b));
    BOOST_CHECK((biga < bigb) == (scripta2 < b));
    BOOST_CHECK((biga > bigb) == (scripta2 > b));
    BOOST_CHECK((biga >= bigb) == (scripta2 >= b));
    BOOST_CHECK((biga <= bigb) == (scripta2 <= b));
}

static
void RunCreateOldRules(CScriptNum const& scriptx) {
    size_t const maxIntegerSize = CScriptNum::MAXIMUM_ELEMENT_SIZE_32_BIT;
    int64_t const x = scriptx.getint64();
    CheckCreateIntOldRules(x);
    if (scriptx.getvch().size() <= maxIntegerSize) {
        CheckCreateVchOldRules(x);
    } else {
        BOOST_CHECK_THROW(CheckCreateVchOldRules(x), scriptnum10_error);
    }
}

static
void RunCreateOldRulesSet(int64_t v, int64_t o) {
    auto const value = CScriptNum::fromIntUnchecked(v);
    auto const offset = CScriptNum::fromIntUnchecked(o);
    auto const value2 = ScriptBigInt::fromIntUnchecked(v);
    auto const offset2 = ScriptBigInt::fromIntUnchecked(o);

    RunCreateOldRules(value);

    auto res = value.safeAdd(offset);
    if (res) {
        RunCreateOldRules(*res);
    }

    auto res2 = value2.safeAdd(offset2);
    BOOST_REQUIRE(res2);

    res = value.safeSub(offset);
    if (res) {
        RunCreateOldRules(*res);
    }

    res2 = value2.safeSub(offset2);
    BOOST_REQUIRE(res2);
}

static
void RunCreateNewRules(CScriptNum const& scriptx) {
    size_t const maxIntegerSize = CScriptNum::MAXIMUM_ELEMENT_SIZE_64_BIT;
    int64_t const x = scriptx.getint64();
    CheckCreateIntNewRules(x);
    if (scriptx.getvch().size() <= maxIntegerSize) {
        CheckCreateVchNewRules(x);
    } else {
        BOOST_CHECK_THROW(CheckCreateVchNewRules(x), scriptnum10_error);
    }
}

static
void RunCreateNewRulesSet(int64_t v, int64_t o) {
    auto res = CScriptNum::fromInt(v);
    auto res2 = ScriptBigInt::fromInt(v);
    BOOST_REQUIRE(res2);
    if ( ! res) {
        BOOST_CHECK(v == int64_t_min);
        return;
    }
    auto const value = *res;
    auto const value2 = *res2;

    res = CScriptNum::fromInt(o);
    res2 = ScriptBigInt::fromInt(o);
    BOOST_REQUIRE(res2);
    if ( ! res) {
        BOOST_CHECK(o == int64_t_min);
        return;
    }
    auto const offset = *res;
    auto const offset2 = *res2;

    RunCreateNewRules(value);

    res = value.safeAdd(offset);
    res2 = value2.safeAdd(offset2);
    BOOST_REQUIRE(res2);
    if (res) {
        RunCreateNewRules(*res);
    }

    res = value.safeSub(offset);
    res2 = value2.safeSub(offset2);
    BOOST_REQUIRE(res2);
    if (res) {
        RunCreateNewRules(*res);
    }
}

static
void RunOperators(int64_t a, int64_t b) {
    CheckAddOldRules(a, b);
    CheckAddNewRules(a, b);
    CheckSubtractOldRules(a, b);
    CheckSubtractNewRules(a, b);
    CheckMultiply(a, b);
    CheckDivideOldRules(a, b);
    CheckDivideNewRules(a, b);
    CheckNegateOldRules(a);
    CheckNegateNewRules(a);
    CheckCompare(a, b);
}

BOOST_AUTO_TEST_CASE(creation) {
    for (auto value : values) {
        for (auto offset : offsets) {
            RunCreateOldRulesSet(value, offset);
            RunCreateNewRulesSet(value, offset);
        }
    }
}

BOOST_AUTO_TEST_CASE(operators) {
    // Prevent potential UB below
    auto negate = [](int64_t x) { return x != int64_t_min ? -x : int64_t_min; };

    for (auto a : values) {
        RunOperators(a, a);
        RunOperators(a, negate(a));
        for (auto b : values) {
            RunOperators(a, b);
            RunOperators(a, negate(b));
            if (a != int64_t_max && a != int64_t_min && a != int64_t_min_8_bytes &&
                b != int64_t_max && b != int64_t_min && b != int64_t_min_8_bytes) {
                RunOperators(a + b, a);
                RunOperators(a + b, b);
                RunOperators(a - b, a);
                RunOperators(a - b, b);
                RunOperators(a + b, a + b);
                RunOperators(a + b, a - b);
                RunOperators(a - b, a + b);
                RunOperators(a - b, a - b);
                RunOperators(a + b, negate(a));
                RunOperators(a + b, negate(b));
                RunOperators(a - b, negate(a));
                RunOperators(a - b, negate(b));
            }
        }
    }
}

static
void CheckMinimalyEncode(std::vector<uint8_t> data, const std::vector<uint8_t> &expected) {
    bool alreadyEncoded = CScriptNum::IsMinimallyEncoded(data, data.size());
    bool hasEncoded = CScriptNum::MinimallyEncode(data);
    BOOST_CHECK_EQUAL(hasEncoded, !alreadyEncoded);
    BOOST_CHECK(data == expected);
}

BOOST_AUTO_TEST_CASE(minimize_encoding_test) {
    CheckMinimalyEncode({}, {});

    for (const size_t elemSize : {MAX_SCRIPT_ELEMENT_SIZE_LEGACY, may2025::MAX_SCRIPT_ELEMENT_SIZE}) {
        // Check that positive and negative zeros encode to nothing.
        std::vector<uint8_t> zero, negZero;
        for (size_t i = 0; i < elemSize; i++) {
            zero.push_back(0x00);
            CheckMinimalyEncode(zero, {});

            negZero.push_back(0x80);
            CheckMinimalyEncode(negZero, {});

            // prepare for next round.
            negZero[negZero.size() - 1] = 0x00;
        }

        // Keep one leading zero when sign bit is used.
        std::vector<uint8_t> n{0x80, 0x00}, negn{0x80, 0x80};
        std::vector<uint8_t> npadded = n, negnpadded = negn;
        for (size_t i = 0; i < elemSize; i++) {
            CheckMinimalyEncode(npadded, n);
            npadded.push_back(0x00);

            CheckMinimalyEncode(negnpadded, negn);
            negnpadded[negnpadded.size() - 1] = 0x00;
            negnpadded.push_back(0x80);
        }

        // Mege leading byte when sign bit isn't used.
        std::vector<uint8_t> k{0x7f}, negk{0xff};
        std::vector<uint8_t> kpadded = k, negkpadded = negk;
        for (size_t i = 0; i < elemSize; i++) {
            CheckMinimalyEncode(kpadded, k);
            kpadded.push_back(0x00);

            CheckMinimalyEncode(negkpadded, negk);
            negkpadded[negkpadded.size() - 1] &= 0x7f;
            negkpadded.push_back(0x80);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
