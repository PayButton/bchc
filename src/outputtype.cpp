// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2019-2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <outputtype.h>

#include <keystore.h>
#include <pubkey.h>
#include <script/script.h>
#include <script/standard.h>
#include <util/vector.h>

#include <cassert>
#include <string>

static const std::string OUTPUT_TYPE_STRING_LEGACY = "legacy";

bool ParseOutputType(const std::string &type, OutputType &output_type) {
    if (type == OUTPUT_TYPE_STRING_LEGACY) {
        output_type = OutputType::LEGACY;
        return true;
    }
    return false;
}

const std::string &FormatOutputType(OutputType type) {
    switch (type) {
        case OutputType::LEGACY:
            return OUTPUT_TYPE_STRING_LEGACY;
        default:
            assert(false);
    }
}

CTxDestination GetDestinationForKey(const CPubKey &key, OutputType type) {
    switch (type) {
        case OutputType::LEGACY:
            return key.GetID();
        default:
            assert(false);
    }
}

std::vector<CTxDestination> GetAllDestinationsForKey(const CPubKey &key) {
    return Vector(CTxDestination{key.GetID()});
}

CTxDestination AddAndGetDestinationForScript(CKeyStore &keystore,
                                             const CScript &script,
                                             OutputType type, bool is_p2sh32, bool chipVmLimitsEnabled) {
    // Add script to keystore
    keystore.AddCScript(script, is_p2sh32, chipVmLimitsEnabled);
    // Note that scripts over 520 bytes are not yet supported.
    switch (type) {
        case OutputType::LEGACY:
            return ScriptID(script, is_p2sh32);
        default:
            assert(false);
    }
}
