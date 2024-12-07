#!/usr/bin/env python3
# Copyright (c) 2018-2022 The Bitcoin developers

import sys


def main(test_name, input_file):
    with open(input_file, "rb") as f:
        contents = f.read()

    print("#include <cstdint>\n")
    print("namespace json_tests {")
    print("static const uint8_t {}[] = {{".format(test_name))

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

    print(", ".join(map(formatter, contents)))
    print("};")
    print("} // namespace json_tests")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("We need additional pylons!")
        sys.exit(1)

    main(sys.argv[1], sys.argv[2])
