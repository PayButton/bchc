// Copyright (c) 2017 The Bitcoin Core developers
// Copyright (c) 2018-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdio>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/version.hpp>

/** Filesystem operations and types */
namespace fs = boost::filesystem;

/** Bridge operations to C stdio + various versions of boost::filesystem */
namespace fsbridge {
FILE *fopen(const fs::path &p, const char *mode);

class FileLock {
public:
    FileLock() = delete;
    FileLock(const FileLock &) = delete;
    FileLock(FileLock &&) = delete;
    explicit FileLock(const fs::path &file);
    ~FileLock();
    bool TryLock();
    std::string GetReason() { return reason; }

private:
    std::string reason;
#ifndef WIN32
    int fd = -1;
#else
    // INVALID_HANDLE_VALUE
    void *hFile = (void *)-1;
#endif
};

std::string get_filesystem_error_message(const fs::filesystem_error &e);

inline auto get_overwrite_if_exists_option() {
    // see https://www.boost.org/users/history/version_1_74_0.html
#if BOOST_VERSION >= 107400
    return fs::copy_options::overwrite_existing;
#else
    return fs::copy_option::overwrite_if_exists;
#endif
}

inline int get_dir_iterator_level(const fs::recursive_directory_iterator &it) {
    // see https://www.boost.org/users/history/version_1_72_0.html
#if BOOST_VERSION >= 107200
    return it.depth();
#else
    return it.level();
#endif
}
}; // namespace fsbridge
