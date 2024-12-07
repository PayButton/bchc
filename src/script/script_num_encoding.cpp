// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/script_num_encoding.h>

bool ScriptNumEncoding::IsMinimallyEncoded(const std::vector<uint8_t> &vch, size_t maxIntegerSize) {
    if (vch.size() > maxIntegerSize) {
        return false;
    }

    if (vch.size() > 0) {
        // Check that the number is encoded with the minimum possible number
        // of bytes.
        //
        // If the most-significant-byte - excluding the sign bit - is zero
        // then we're not minimal. Note how this test also rejects the
        // negative-zero encoding, 0x80.
        if ((vch.back() & 0x7f) == 0) {
            // One exception: if there's more than one byte and the most
            // significant bit of the second-most-significant-byte is set it
            // would conflict with the sign bit. An example of this case is
            // +-255, which encode to 0xff00 and 0xff80 respectively.
            // (big-endian).
            if (vch.size() <= 1 || (vch[vch.size() - 2] & 0x80) == 0) {
                return false;
            }
        }
    }

    return true;
}

bool ScriptNumEncoding::MinimallyEncode(std::vector<uint8_t> &data) {
    if (data.size() == 0) {
        return false;
    }

    // If the last byte is not 0x00 or 0x80, we are minimally encoded.
    uint8_t last = data.back();
    if (last & 0x7f) {
        return false;
    }

    // If the script is one byte long, then we have a zero, which encodes as an
    // empty array.
    if (data.size() == 1) {
        data = {};
        return true;
    }

    // If the next byte has it sign bit set, then we are minimaly encoded.
    if (data[data.size() - 2] & 0x80) {
        return false;
    }

    // We are not minimally encoded, we need to figure out how much to trim.
    for (size_t i = data.size() - 1; i > 0; i--) {
        // We found a non zero byte, time to encode.
        if (data[i - 1] != 0) {
            if (data[i - 1] & 0x80) {
                // We found a byte with it sign bit set so we need one more
                // byte.
                data[i++] = last;
            } else {
                // the sign bit is clear, we can use it.
                data[i - 1] |= last;
            }

            data.resize(i);
            return true;
        }
    }

    // If we the whole thing is zeros, then we have a zero.
    data = {};
    return true;
}
