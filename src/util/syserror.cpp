// Copyright (c) 2020-2022 The Bitcoin Core developers
// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <tinyformat.h>
#include <util/syserror.h>

#include <string.h> /* for strerror_r or strerror_s */

std::string SysErrorString(const int err) {
    char buf[1024];
    buf[0] = 0;
    /**
     * Too bad there are three incompatible implementations of the
     * thread-safe strerror.
     */
    const char *s = nullptr;
#ifdef _WIN32
    if (strerror_s(buf, sizeof(buf), err) == 0) {
        s = buf;
    }
#else
#ifdef STRERROR_R_CHAR_P
    /* GNU variant can return a pointer outside the passed buffer */
    s = strerror_r(err, buf, sizeof(buf));
#else
    /* POSIX variant always returns message in buffer */
    if (strerror_r(err, buf, sizeof(buf)) == 0) {
        s = buf;
    }
#endif /* STRERROR_R_CHAR_P */
#endif /* _WIN32 */
    if (s != nullptr) {
        return strprintf("%s (%d)", s, err);
    } else {
        return strprintf("Unknown error (%d)", err);
    }
}
