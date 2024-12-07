// Copyright (c) 2012-2016 The Bitcoin Core developers
// Copyright (c) 2017-2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <fs.h>
#include <prevector.h>
#include <streams.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(streams_tests, BasicTestingSetup)

template <typename VecT>
static bool test_generic_vector_writer() {
    uint8_t a(1);
    uint8_t b(2);
    uint8_t bytes[] = {3, 4, 5, 6};
    VecT vch;
    using T = typename VecT::value_type;

    auto ToUInt8Vec = [](const auto &v) {
        const uint8_t *begin = reinterpret_cast<const uint8_t *>(v.data());
        const uint8_t *end = reinterpret_cast<const uint8_t *>(v.data() + v.size());
        return std::vector<uint8_t>(begin, end);
    };

    // Each test runs twice. Serializing a second time at the same starting
    // point should yield the same results, even if the first test grew the
    // vector.

    GenericVectorWriter(SER_NETWORK, INIT_PROTO_VERSION, vch, 0, a, b);
    BOOST_CHECK((ToUInt8Vec(vch) == std::vector<uint8_t>{{1, 2}}));
    GenericVectorWriter(SER_NETWORK, INIT_PROTO_VERSION, vch, 0, a, b);
    BOOST_CHECK((ToUInt8Vec(vch) == std::vector<uint8_t>{{1, 2}}));
    vch.clear();

    GenericVectorWriter(SER_NETWORK, INIT_PROTO_VERSION, vch, 2, a, b);
    BOOST_CHECK((ToUInt8Vec(vch) == std::vector<uint8_t>{{0, 0, 1, 2}}));
    GenericVectorWriter(SER_NETWORK, INIT_PROTO_VERSION, vch, 2, a, b);
    BOOST_CHECK((ToUInt8Vec(vch) == std::vector<uint8_t>{{0, 0, 1, 2}}));
    vch.clear();

    vch.resize(5, T(0));
    GenericVectorWriter(SER_NETWORK, INIT_PROTO_VERSION, vch, 2, a, b);
    BOOST_CHECK((ToUInt8Vec(vch) == std::vector<uint8_t>{{0, 0, 1, 2, 0}}));
    GenericVectorWriter(SER_NETWORK, INIT_PROTO_VERSION, vch, 2, a, b);
    BOOST_CHECK((ToUInt8Vec(vch) == std::vector<uint8_t>{{0, 0, 1, 2, 0}}));
    vch.clear();

    vch.resize(4, T(0));
    GenericVectorWriter(SER_NETWORK, INIT_PROTO_VERSION, vch, 3, a, b);
    BOOST_CHECK((ToUInt8Vec(vch) == std::vector<uint8_t>{{0, 0, 0, 1, 2}}));
    GenericVectorWriter(SER_NETWORK, INIT_PROTO_VERSION, vch, 3, a, b);
    BOOST_CHECK((ToUInt8Vec(vch) == std::vector<uint8_t>{{0, 0, 0, 1, 2}}));
    vch.clear();

    vch.resize(4, T(0));
    GenericVectorWriter(SER_NETWORK, INIT_PROTO_VERSION, vch, 4, a, b);
    BOOST_CHECK((ToUInt8Vec(vch) == std::vector<uint8_t>{{0, 0, 0, 0, 1, 2}}));
    GenericVectorWriter(SER_NETWORK, INIT_PROTO_VERSION, vch, 4, a, b);
    BOOST_CHECK((ToUInt8Vec(vch) == std::vector<uint8_t>{{0, 0, 0, 0, 1, 2}}));
    vch.clear();

    GenericVectorWriter(SER_NETWORK, INIT_PROTO_VERSION, vch, 0, bytes);
    BOOST_CHECK((ToUInt8Vec(vch) == std::vector<uint8_t>{{3, 4, 5, 6}}));
    GenericVectorWriter(SER_NETWORK, INIT_PROTO_VERSION, vch, 0, bytes);
    BOOST_CHECK((ToUInt8Vec(vch) == std::vector<uint8_t>{{3, 4, 5, 6}}));
    vch.clear();

    vch.resize(4, T(8));
    GenericVectorWriter(SER_NETWORK, INIT_PROTO_VERSION, vch, 2, a, bytes, b);
    BOOST_CHECK((ToUInt8Vec(vch) == std::vector<uint8_t>{{8, 8, 1, 3, 4, 5, 6, 2}}));
    GenericVectorWriter(SER_NETWORK, INIT_PROTO_VERSION, vch, 2, a, bytes, b);
    BOOST_CHECK((ToUInt8Vec(vch) == std::vector<uint8_t>{{8, 8, 1, 3, 4, 5, 6, 2}}));
    vch.clear();

    return true;
}

BOOST_AUTO_TEST_CASE(streams_vector_writer) {
    BOOST_CHECK(test_generic_vector_writer<std::vector<uint8_t>>());
    BOOST_CHECK(test_generic_vector_writer<std::vector<char>>());
    BOOST_CHECK((test_generic_vector_writer<prevector<28, uint8_t>>()));
}

template <typename VecT>
static bool test_generic_vector_reader() {
    VecT vch;
    for (auto val : {1, 255, 3, 4, 5, 6}) vch.push_back(typename VecT::value_type(val));

    GenericVectorReader reader(SER_NETWORK, INIT_PROTO_VERSION, vch, 0);
    BOOST_CHECK_EQUAL(reader.size(), 6);
    BOOST_CHECK(!reader.empty());

    // Read a single byte as an uint8_t.
    uint8_t a;
    reader >> a;
    BOOST_CHECK_EQUAL(a, 1);
    BOOST_CHECK_EQUAL(reader.size(), 5);
    BOOST_CHECK_EQUAL(reader.GetPos(), 1);
    BOOST_CHECK(!reader.empty());

    // Read a single byte as a (signed) int8_t.
    int8_t b;
    reader >> b;
    BOOST_CHECK_EQUAL(b, -1);
    BOOST_CHECK_EQUAL(reader.size(), 4);
    BOOST_CHECK_EQUAL(reader.GetPos(), 2);
    BOOST_CHECK(!reader.empty());

    // Read a 4 bytes as an unsigned uint32_t.
    uint32_t c;
    reader >> c;
    // 100992003 = 3,4,5,6 in little-endian base-256
    BOOST_CHECK_EQUAL(c, 100992003);
    BOOST_CHECK_EQUAL(reader.size(), 0);
    BOOST_CHECK_EQUAL(reader.GetPos(), 6);
    BOOST_CHECK(reader.empty());

    // Reading after end of byte vector throws an error.
    int32_t d;
    BOOST_CHECK_THROW(reader >> d, std::ios_base::failure);

    // Read a 4 bytes as a (signed) int32_t from the beginning of the buffer.
    GenericVectorReader new_reader(SER_NETWORK, INIT_PROTO_VERSION, vch, 0);
    new_reader >> d;
    // 67370753 = 1,255,3,4 in little-endian base-256
    BOOST_CHECK_EQUAL(d, 67370753);
    BOOST_CHECK_EQUAL(new_reader.size(), 2);
    BOOST_CHECK_EQUAL(new_reader.GetPos(), 4);
    BOOST_CHECK(!new_reader.empty());

    // Reading after end of byte vector throws an error even if the reader is
    // not totally empty.
    BOOST_CHECK_THROW(new_reader >> d, std::ios_base::failure);

    // Read a 4 bytes as a (signed) int32_t from offset 1 of the buffer.
    GenericVectorReader new_reader2(SER_NETWORK, INIT_PROTO_VERSION, vch, 1);
    BOOST_CHECK_EQUAL(new_reader2.size(), 5);
    BOOST_CHECK_EQUAL(new_reader2.GetPos(), 1);
    new_reader2 >> d;
    // 84149247 = 255,3,4,5 in little-endian base-256
    BOOST_CHECK_EQUAL(d, 84149247);
    BOOST_CHECK_EQUAL(new_reader2.size(), 1);
    BOOST_CHECK_EQUAL(new_reader2.GetPos(), 5);
    BOOST_CHECK(!new_reader2.empty());
    new_reader2 >> a;
    BOOST_CHECK_EQUAL(a, 0x06u);
    BOOST_CHECK_EQUAL(new_reader2.size(), 0);
    BOOST_CHECK_EQUAL(new_reader2.GetPos(), 6);
    BOOST_CHECK(new_reader2.empty());

    return true;
}

BOOST_AUTO_TEST_CASE(streams_vector_reader) {
    BOOST_CHECK(test_generic_vector_reader<std::vector<uint8_t>>());
    BOOST_CHECK(test_generic_vector_reader<std::vector<char>>());
    BOOST_CHECK((test_generic_vector_reader<prevector<28, uint8_t>>()));
}

BOOST_AUTO_TEST_CASE(bitstream_reader_writer) {
    CDataStream data(SER_NETWORK, INIT_PROTO_VERSION);

    BitStreamWriter<CDataStream> bit_writer(data);
    bit_writer.Write(0, 1);
    bit_writer.Write(2, 2);
    bit_writer.Write(6, 3);
    bit_writer.Write(11, 4);
    bit_writer.Write(1, 5);
    bit_writer.Write(32, 6);
    bit_writer.Write(7, 7);
    bit_writer.Write(30497, 16);
    bit_writer.Flush();

    CDataStream data_copy(data);
    uint32_t serialized_int1;
    data >> serialized_int1;
    // NOTE: Serialized as LE
    BOOST_CHECK_EQUAL(serialized_int1, (uint32_t)0x7700C35A);
    uint16_t serialized_int2;
    data >> serialized_int2;
    // NOTE: Serialized as LE
    BOOST_CHECK_EQUAL(serialized_int2, (uint16_t)0x1072);

    BitStreamReader<CDataStream> bit_reader(data_copy);
    BOOST_CHECK_EQUAL(bit_reader.Read(1), 0);
    BOOST_CHECK_EQUAL(bit_reader.Read(2), 2);
    BOOST_CHECK_EQUAL(bit_reader.Read(3), 6);
    BOOST_CHECK_EQUAL(bit_reader.Read(4), 11);
    BOOST_CHECK_EQUAL(bit_reader.Read(5), 1);
    BOOST_CHECK_EQUAL(bit_reader.Read(6), 32);
    BOOST_CHECK_EQUAL(bit_reader.Read(7), 7);
    BOOST_CHECK_EQUAL(bit_reader.Read(16), 30497);
    BOOST_CHECK_THROW(bit_reader.Read(8), std::ios_base::failure);
}

BOOST_AUTO_TEST_CASE(streams_serializedata_xor) {
    std::vector<char> in;
    std::vector<char> expected_xor;
    std::vector<uint8_t> key;
    CDataStream ds(in, 0, 0);

    // Degenerate case

    key.push_back('\x00');
    key.push_back('\x00');
    ds.Xor(key);
    BOOST_CHECK_EQUAL(std::string(expected_xor.begin(), expected_xor.end()),
                      std::string(ds.begin(), ds.end()));

    in.push_back('\x0f');
    in.push_back('\xf0');
    expected_xor.push_back('\xf0');
    expected_xor.push_back('\x0f');

    // Single character key

    ds.clear();
    ds.insert(ds.begin(), in.begin(), in.end());
    key.clear();

    key.push_back('\xff');
    ds.Xor(key);
    BOOST_CHECK_EQUAL(std::string(expected_xor.begin(), expected_xor.end()),
                      std::string(ds.begin(), ds.end()));

    // Multi character key

    in.clear();
    expected_xor.clear();
    in.push_back('\xf0');
    in.push_back('\x0f');
    expected_xor.push_back('\x0f');
    expected_xor.push_back('\x00');

    ds.clear();
    ds.insert(ds.begin(), in.begin(), in.end());

    key.clear();
    key.push_back('\xff');
    key.push_back('\x0f');

    ds.Xor(key);
    BOOST_CHECK_EQUAL(std::string(expected_xor.begin(), expected_xor.end()),
                      std::string(ds.begin(), ds.end()));
}

BOOST_AUTO_TEST_CASE(streams_empty_vector) {
    std::vector<char> in;
    CDataStream ds(in, 0, 0);

    // read 0 bytes used to cause a segfault on some older systems.
    BOOST_CHECK_NO_THROW(ds.read(nullptr, 0));

    // Same goes for writing 0 bytes from a vector ...
    const std::vector<char> vdata{'f', 'o', 'o', 'b', 'a', 'r'};
    BOOST_CHECK_NO_THROW(ds.insert(ds.begin(), vdata.begin(), vdata.begin()));
    BOOST_CHECK_NO_THROW(ds.insert(ds.begin(), vdata.begin(), vdata.end()));

    // ... or an array.
    const char adata[6] = {'f', 'o', 'o', 'b', 'a', 'r'};
    BOOST_CHECK_NO_THROW(ds.insert(ds.begin(), &adata[0], &adata[0]));
    BOOST_CHECK_NO_THROW(ds.insert(ds.begin(), &adata[0], &adata[6]));
}

// Test move-assign and move-construct for CAutoFile
BOOST_AUTO_TEST_CASE(autofile_move) {
    auto dataDir = SetDataDir("autofile_move");
    FILE *f1 = fsbridge::fopen(dataDir / "data.bin", "wb+");
    FILE *f2 = fsbridge::fopen(dataDir / "data2.bin", "wb+");
    BOOST_REQUIRE(f1 && f2);
    BOOST_REQUIRE(f1 != f2);
    const int t1 = SER_DISK, t2 = SER_NETWORK;
    const int v1 = INIT_PROTO_VERSION, v2 = INIT_PROTO_VERSION + 1;
    CAutoFile af1(f1, t1, v1), af2(f2, t2, v2);
    BOOST_REQUIRE(af1.Get() == f1 && af2.Get() == f2);
    BOOST_REQUIRE(af1.GetType() == t1 && af1.GetVersion() == v1);
    BOOST_REQUIRE(af2.GetType() == t2 && af2.GetVersion() == v2);

    // write data to files
    af1 << uint32_t(42);
    af2 << uint32_t(43);

    // Test contents
    auto TestContents = [](const std::vector<CAutoFile *> &files, const std::vector<uint32_t> &vals) {
        BOOST_REQUIRE(files.size() == vals.size());
        for (size_t i = 0; i < files.size(); ++i) {
            CAutoFile *paf = files[i];
            BOOST_REQUIRE(paf != nullptr);
            BOOST_REQUIRE(!paf->IsNull());
            const auto tstval = vals[i];
            std::fseek(paf->Get(), 0, SEEK_SET);
            uint32_t v;
            *paf >> v;
            BOOST_REQUIRE_EQUAL(v, tstval);
        }
    };

    TestContents({&af1, &af2}, {42, 43});

    // Test move-construct
    BOOST_REQUIRE(!af2.IsNull() && f2 == af2.Get()); // sanity check
    CAutoFile af3(std::move(af2)); // move-construct
    BOOST_REQUIRE(af2.IsNull() && af2.GetType() == 0 && af2.GetVersion() == 0);  // moved-from is now null and empty
    BOOST_REQUIRE(!af3.IsNull() && f2 == af3.Get()); // moved-to is not null and is same FILE * as what af2 was
    BOOST_REQUIRE(af3.GetType() == t2 && af3.GetVersion() == v2); // ensure inherits type and version
    TestContents({&af1, &af3}, {42, 43});

    // Test move-assign
    af2 = std::move(af3);
    BOOST_REQUIRE(af3.IsNull() && af3.GetType() == 0 && af3.GetVersion() == 0);  // moved-from is now null and empty
    BOOST_REQUIRE(!af2.IsNull() && f2 == af2.Get()); // moved-to is not null and is same FILE * as what af2 was
    BOOST_REQUIRE(af2.GetType() == t2 && af2.GetVersion() == v2); // version should be also what we expect
    TestContents({&af1, &af2}, {42, 43});

    // Swap af1 and af2 via move-assign, and test they preserve everything after swap
    BOOST_REQUIRE(!af1.IsNull() && af1.GetType() == t1 && af1.GetVersion() == v1 && af1.Get() == f1);
    BOOST_REQUIRE(!af2.IsNull() && af2.GetType() == t2 && af2.GetVersion() == v2 && af2.Get() == f2);
    // do swap
    af3 = std::move(af2);
    af2 = std::move(af1);
    af1 = std::move(af3);
    // test that everything swapped and is what we expect
    BOOST_REQUIRE(!af1.IsNull() && af1.GetType() == t2 && af1.GetVersion() == v2 && af1.Get() == f2);
    BOOST_REQUIRE(!af2.IsNull() && af2.GetType() == t1 && af2.GetVersion() == v1 && af2.Get() == f1);
    BOOST_REQUIRE(af3.IsNull() && af3.GetType() == 0 && af3.GetVersion() == 0);
    TestContents({&af1, &af2}, {43, 42});
}

BOOST_AUTO_TEST_SUITE_END()
