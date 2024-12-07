// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <primitives/transaction.h>
#include <script/script.h>
#include <serialize.h>
#include <span.h>

#include <algorithm>

bool CompressScript(const CScript &script, std::vector<uint8_t> &out);
unsigned int GetSpecialScriptSize(unsigned int nSize);
bool DecompressScript(CScript &script, unsigned int nSize, const std::vector<uint8_t> &out);

uint64_t CompressAmount(Amount nAmount);
Amount DecompressAmount(uint64_t nAmount);

/**
 * Compact serializer for scripts.
 *
 * It detects common cases and encodes them much more efficiently.
 * 3 special cases are defined:
 *  * Pay to pubkey hash (encoded as 21 bytes)
 *  * Pay to script hash (encoded as 21 bytes)
 *  * Pay to pubkey starting with 0x02, 0x03 or 0x04 (encoded as 33 bytes)
 *  * Future support for p2sh_32 would encode as 33 bytes (currently
 *    unimplemented).
 *
 * Other scripts up to 121 bytes require 1 byte + script length. Above that,
 * scripts up to 16505 bytes require 2 bytes + script length.
 */
struct ScriptCompression {
    /**
     * Legacy (pre-p2sh_32 support), has 6 special scripts.
     *
     * Note that to add support for compressing p2sh_32 (or possibly a future
     * p2pkh_32), one would need to upgrade the undo files as well as txdb in
     * existing installs, introducing backwards incompatibility in the data
     * file format. So, for now, we do not support p2sh_32 compression.
     */
    static constexpr unsigned int nSpecialScripts = 6u;

    template <typename Stream>
    static void Ser(Stream &s, const CScript &script) {
        std::vector<uint8_t> compr;
        if (CompressScript(script, compr)) {
            s << Span{compr};
            return;
        }
        unsigned int nSize = script.size() + nSpecialScripts;
        s << VARINT(nSize);
        s << Span{script};
    }

    template <typename Stream>
    static void Ser(Stream &s, const token::WrappedScriptPubKey &wspk) {
        Ser(s, CScript(wspk.data(), wspk.data() + wspk.size()));
    }

    template <typename Stream> static void Unser(Stream &s, CScript &script) {
        unsigned int nSize = 0;
        s >> VARINT(nSize);
        if (nSize < nSpecialScripts) {
            std::vector<uint8_t> vch(GetSpecialScriptSize(nSize), 0x00);
            s >> Span(vch);
            DecompressScript(script, nSize, vch);
            return;
        }
        nSize -= nSpecialScripts;
        script.resize(0);
        for (unsigned pos = 0, chunk; pos < nSize; pos += chunk) {
            // Read-in 5MB at a time to prevent over-allocation on bad/garbled data. This algorithm is similar to
            // the one used in Unserialize_vector in serialize.h.
            chunk = std::min(nSize - pos, MAX_VECTOR_ALLOCATE);
            script.resize_uninitialized(pos + chunk);
            s >> Span{script.data() + pos, chunk};
        }
    }

    template <typename Stream> static void Unser(Stream &s, token::WrappedScriptPubKey &wspk) {
        CScript tmp;
        Unser(s, tmp);
        wspk.assign(tmp.begin(), tmp.end());
    }
};

struct AmountCompression {
    template <typename Stream> static void Ser(Stream &s, Amount val) {
        s << VARINT(CompressAmount(val));
    }
    template <typename Stream> static void Unser(Stream &s, Amount &val) {
        uint64_t v;
        s >> VARINT(v);
        val = DecompressAmount(v);
    }
};

/** wrapper for CTxOut that provides a more compact serialization */
struct TxOutCompression {
    FORMATTER_METHODS(CTxOut, obj) {
        READWRITE(Using<AmountCompression>(obj.nValue));

        if (!ser_action.ForRead() && !obj.tokenDataPtr) {
            // Faster path when writing without token data, just do this as it's faster
            READWRITE(Using<ScriptCompression>(obj.scriptPubKey));
        } else {
            // Slower path, juggle the optional tokenData and pack/unpack it into the WrappedScriptPubKey
            // Note that for now, all TXOs that have tokenData do not get their wrapped scriptPubKeys compressed.
            token::WrappedScriptPubKey wspk;
            SER_WRITE(obj, token::WrapScriptPubKey(wspk, obj.tokenDataPtr, obj.scriptPubKey, s.GetVersion()));
            READWRITE(Using<ScriptCompression>(wspk));
            SER_READ(obj, token::UnwrapScriptPubKey(wspk, obj.tokenDataPtr, obj.scriptPubKey, s.GetVersion()));
            if (obj.scriptPubKey.size() > MAX_SCRIPT_SIZE) {
                // Overly long script, replace with a short invalid one
                // - This logic originally lived in ScriptCompression::Unser but was moved here
                // - Note the expression below is explicitly an assignment and not a `.resize(1, OP_RETURN)` so as to
                //   force the release of >10KB of memory obj.scriptPubKey has allocated.
                SER_READ(obj, obj.scriptPubKey = CScript() << OP_RETURN);
            }
        }
    }
};
