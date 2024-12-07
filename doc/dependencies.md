Dependencies
============

These are the dependencies currently used by Bitcoin Cash Node. You can find
instructions for installing them in the `build-*.md` file for your platform.

These dependencies are required:

| Dependency | Version used | Minimum required | CVEs | Shared | [Bundled Qt library](https://doc.qt.io/qt-5/configure-options.html) | Purpose | Description |
| --- | --- | --- | --- | --- | --- |--- | --- |
| Berkeley DB | [5.3.28](http://www.oracle.com/technetwork/database/database-technologies/berkeleydb/downloads/index.html) | 5.3 | No |  |  | Wallet storage | Only needed when wallet enabled  |
| Boost | [1.70.0](http://www.boost.org/users/download/) | 1.58.0 | No |  |  |  Utility          | Library for threading, data structures, etc
| Clang |  | [8.0.0](http://llvm.org/releases/download.html) (C++17 support) |  |  |  |  |  |
| CMake |  | [3.13](https://cmake.org/download/) |  |  |  |  |  |
| Expat | [2.2.7](https://libexpat.github.io/) |  | No | Yes |  |  |  |
| fontconfig | [2.12.6](https://www.freedesktop.org/software/fontconfig/release/) |  | No | Yes |  |  |  |
| FreeType | [2.7.1](http://download.savannah.gnu.org/releases/freetype) |  | No |  |  |  |  |
| GCC |  | [8.3.0](https://gcc.gnu.org/) (C++17 support) |  |  |  |  |  |
| HarfBuzz-NG |  |  |  |  |  |  |  |
| help2man |  |  |  |  |  | Manpages | Optional, used for building manpages |
| jemalloc | [5.2.1](https://github.com/jemalloc/jemalloc/releases) |  |  |  |  |
| libevent | [2.1.12-stable](https://github.com/libevent/libevent/releases) | 2.0.22 | No |  |  |  Networking       | OS independent asynchronous networking |
| libgmp | [6.3.0](https://gmplib.org/download/gmp/) | 6.1.2 | No |  |  |  Math       | Large integer support |
| libjpeg |  |  |  |  | Yes |  |  |
| libnatpmp | commit [07004b9...](https://github.com/miniupnp/libnatpmp/commit/07004b97cf691774efebe70404cf22201e4d330d) |  | No |  |  | NAT-PMP Support  | Firewall-jumping support |
| libpng |  |  |  |  | Yes |  |  |
| MiniUPnPc | [2.2.2](http://miniupnp.free.fr/files) | 1.6 | No |  |  | UPnP Support     | Firewall-jumping support |
| Ninja |  | [1.5.1](https://github.com/ninja-build/ninja/releases) |  |  |  |  |  |
| OpenSSL | [1.1.1n](https://www.openssl.org/source) |  | Yes |  |  | Crypto | Random Number Generation, Elliptic Curve Cryptography
| PCRE |  |  |  |  | Yes |  |  |
| Python (tests) |  | [3.6](https://www.python.org/downloads) |  |  |  |  |  |
| qrencode | [3.4.4](https://fukuchi.org/works/qrencode) |  | No |  |  | QR codes in GUI  | Optional for generating QR codes (only needed when GUI enabled)
| Qt | [5.15.3](https://download.qt.io/official_releases/qt/) | 5.5.1 | No |  |  |  GUI              | GUI toolkit (only needed when GUI enabled) |
| univalue |||||   | Utility          | JSON parsing and encoding (bundled version will be used unless --with-system-univalue passed to configure)
| XCB |  |  |  |  | Yes (Linux only) |  |  |
| xkbcommon |  |  |  |  | Yes (Linux only) |  |  |
| ZeroMQ | [4.3.1](https://github.com/zeromq/libzmq/releases) | 4.1.5 | No |  |  | ZMQ notification | Optional, allows generating ZMQ notifications (requires ZMQ version >= 4.1.5)
| zlib | [1.2.11](http://zlib.net/) |  |  |  | No |  |  |
