// Copyright (c) 2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <ec_multiset.h>

#include <key.h>
#include <util/defer.h>

#include <secp256k1.h>
#include <secp256k1_multiset.h>

#include <cassert>

namespace {
    const auto CtxDeleter = [](secp256k1_context *p){ secp256k1_context_destroy(p); };
    using CtxUPtr = std::unique_ptr<secp256k1_context, decltype(CtxDeleter)>;
    // The below code gets a single static instance of a context object. Current secp256k1 code for multiset doesn't
    // use the context object at all internally, but it requires a valid one as part of the API.
    const CtxUPtr s_ctx{secp256k1_context_create(SECP256K1_CONTEXT_NONE), CtxDeleter};

    const secp256k1_context *GetCtx() {
        const secp256k1_context * const ret = s_ctx.get();
        assert(ret != nullptr && "The global context is nullptr, this should never happen!");
        return ret;
    }
} // namespace

struct ECMultiSet::Priv {
    secp256k1_multiset ms;

    Priv() { Clear(); }
    void Clear() { secp256k1_multiset_init(GetCtx(), &ms); }
};

ECMultiSet::ECMultiSet() : p{std::make_unique<Priv>()} {}

ECMultiSet::ECMultiSet(ByteSpan item) : ECMultiSet() { Add(item); }

ECMultiSet::ECMultiSet(const CPubKey &pubKey) : ECMultiSet() { SetPubKey(pubKey); }

ECMultiSet & ECMultiSet::SetPubKey(const CPubKey &pubKey) noexcept {
    if (pubKey.IsFullyValid() && pubKey.IsCompressed()) {
        static_assert(CPubKey::COMPRESSED_PUBLIC_KEY_SIZE == 33u);
        const int res = secp256k1_multiset_parse(GetCtx(), &p->ms, pubKey.data());
        assert(res != 0 && "secp256k1_multiset_parse failed (this should never happen!");
    } else {
        Clear();
    }
    return *this;
}

ECMultiSet::ECMultiSet(const PubKeyBytes &bytes) : ECMultiSet(CPubKey(bytes.begin(), bytes.end())) {}

ECMultiSet & ECMultiSet::SetPubKeyBytes(const PubKeyBytes &bytes) noexcept {
    return SetPubKey(CPubKey(bytes.begin(), bytes.end()));
}

ECMultiSet::ECMultiSet(std::initializer_list<ByteSpan> items) : ECMultiSet() {
    for (const auto & item : items) {
        Add(item);
    }
}

ECMultiSet::~ECMultiSet() noexcept {}

ECMultiSet::ECMultiSet(const ECMultiSet &o) : ECMultiSet() { Combine(o); }

ECMultiSet::ECMultiSet(ECMultiSet &&o) noexcept : p{std::move(o.p)} { o.p = std::make_unique<Priv>(); }

ECMultiSet & ECMultiSet::operator=(const ECMultiSet &o) noexcept {
    if (this != &o) {
         Clear();
         Combine(o);
    }
    return *this;
}

ECMultiSet & ECMultiSet::operator=(ECMultiSet &&o) noexcept {
    if (this != &o) {
        p = std::move(o.p);
        o.p = std::make_unique<Priv>(); // ensure consistent state for moved-from object
    }
    return *this;
}

bool ECMultiSet::IsEmpty() const noexcept { return secp256k1_multiset_is_empty(GetCtx(), &p->ms); }

ECMultiSet & ECMultiSet::Add(ByteSpan item) noexcept {
    const int res = secp256k1_multiset_add(GetCtx(), &p->ms, item.data(), item.size());
    assert(res != 0 && "secp256k1_multiset_add failed (this should never happen)!");
    return *this;
}

ECMultiSet & ECMultiSet::Remove(ByteSpan item) noexcept {
    const int res = secp256k1_multiset_remove(GetCtx(), &p->ms, item.data(), item.size());
    assert(res != 0 && "secp256k1_multiset_remove failed (this should never happen)!");
    return *this;
}

ECMultiSet & ECMultiSet::Combine(const ECMultiSet &o) noexcept {
    const int res = secp256k1_multiset_combine(GetCtx(), &p->ms, &o.p->ms);
    assert(res != 0 && "secp256k1_multiset_combine failed (this should never happen)!");
    return *this;
}

void ECMultiSet::Clear() noexcept { p->Clear(); }

uint256 ECMultiSet::GetHash() const noexcept {
    uint256 ret{uint256::Uninitialized};
    static_assert(uint256::size() == 32);
    const int res = secp256k1_multiset_finalize(GetCtx(), ret.data(), &p->ms);
    assert(res != 0 && "secp256k1_multiset_finalize failed (this should never happen)!");
    return ret;
}

auto ECMultiSet::GetPubKeyBytes() const noexcept -> PubKeyBytes {
    PubKeyBytes buf;
    // Note: secp256k1 fills-in all-zeroes here in the empty case. However rather than relying on that assumption,
    // we will do our own zeroing here to enforce all-zeroes as a unique value we return for an empty set.
    if (!IsEmpty()) {
        const int res = secp256k1_multiset_serialize(GetCtx(), buf.data(), &p->ms);
        assert(res != 0 && "secp256k1_multiset_serialize failed (this should never happen)!");
    } else {
        buf.fill(0x0u);
    }
    return buf;
}

CPubKey ECMultiSet::GetPubKey() const noexcept {
    CPubKey ret;
    if (!IsEmpty()) {
        const PubKeyBytes buf = GetPubKeyBytes();
        ret.Set(buf.begin(), buf.end());
        assert(ret.IsValid() && "Deserialized CPubKey is not valid (this should never happen)!");
    }
    return ret;
}
