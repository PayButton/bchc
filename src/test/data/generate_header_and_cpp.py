#!/usr/bin/env python3
# Copyright (c) 2018-2024 The Bitcoin developers

# Compresses the data read from `input_file` using zlib and outputs the compressed bytes to `output_cpp`.
# The `extern std::array` declaration is placed in `output_h`.

import sys
import zlib

def main(test_name, input_file, output_h, output_cpp):
    with open(input_file, "rb") as f:
        contents = f.read()

    compressed_bytes = zlib.compress(contents)
    uncompressed_len = len(contents)
    del contents

    preamble = """#include <array>
#include <cstddef>
#include <cstdint>

namespace json_tests {"""
    footer = "} // namespace json_tests"

    decl = f"const std::array<const uint8_t, {len(compressed_bytes)}> {test_name}"

    with open(output_h, "wt", encoding="utf8") as f:
        print("// Auto-generated header file\n\n#pragma once\n\n" + preamble, file=f)
        print(f"extern {decl};", file=f)
        print(f"inline constexpr size_t {test_name}_uncompressed_size = {uncompressed_len}u;", file=f)
        print(footer, file=f)

    with open(output_cpp, "wt", encoding="utf8") as f:
        print(preamble, file=f)
        print("extern " + decl + ";", file=f)  # Emit the "extern" version to ensure compiler doesn't warn
        print(decl + "{{", file=f)

        ctr = 0

        def formatter(bb: bytes) -> str:
            nonlocal ctr
            ret = f"0x{bb:02x}"
            # Allow for up to 20 items per line
            if ctr >= 20:
                ctr = 0
                ret = "\n" + ret
            ctr += 1
            return ret

        print(", ".join(map(formatter, compressed_bytes)), file=f)
        print("}};", file=f)
        print(footer, file=f)

if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("We need additional pylons!")
        sys.exit(1)

    main(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
