#!/usr/bin/env python3
# Copyright (c) 2024 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
This script is to be manually run once to generate bigint_test_vectors.json.
"""

import json
import random
import sys
from collections import defaultdict


def main():
    sys.set_int_max_str_digits(25_000)
    dd = defaultdict(list)
    bitlengths = (2, 3, 8, 12, 16, 24, 32, 48, 96, 128, 192, 256, 512, 1024, 5000)
    num_iters = 1_000
    OLD_CBITS = 258 * 8 - 1  # Older version of the BigInt CHIP bit size limit
    CBITS = 10_000 * 8 - 1  # Consensus BigInt number bit size limit

    # Push some special values to the list of numbers for tests
    dd["numbers"] = [str(x) for x in [
        2 ** 63 - 1,  # INT64_MAX
        2 ** 63,  # INT64_MAX + 1
        -(2 ** 63),  # INT64_MIN
        1 + -(2 ** 63),  # INT64_MIN + 1
        2 ** 31 - 1,  # INT32_MAX
        2 ** 31,  # INT32_MAX + 1
        -(2 ** 31),  # INT32_MIN
        1 + -(2 ** 31),  # INT32_MIN + 1
        2 ** OLD_CBITS,  # Older BigInt CHIP consensus max + 1
        2 ** OLD_CBITS - 1,  # Older BigInt CHIP consensus max
        -(2 ** OLD_CBITS),  # One less than older BigInt CHIP consensus min
        1 + -(2 ** OLD_CBITS),  # Older BigInt CHIP consensus min
        2 ** CBITS,  # BigInt consensus max + 1
        2 ** CBITS - 1,  # BigInt consensus max
        -(2 ** CBITS),  # One less than BigInt consensus min
        1 + -(2 ** CBITS),  # BigInt consensus min
    ]]

    for ii in range(num_iters):
        op1, op2 = (random.getrandbits(random.choice(bitlengths)), random.getrandbits(random.choice(bitlengths)))
        # Randomly make them negative
        if random.choice((True, False)):
            op1 = -op1
        if random.choice((True, False)):
            op2 = -op2
        # Decide ahead of time how they compare
        comp = 0 if op1 == op2 else (1 if op1 > op2 else -1)

        # To reduce output file size, we push the numbers to a list and then refer to them in the ops dicts by index
        op1idx = len(dd["numbers"])
        op2idx = op1idx + 1
        dd["numbers"].extend([str(op1), str(op2)])


        # Write out test cases for each op

        dd["+"].append([op1idx, op2idx, str(op1 + op2)])
        dd["-"].append([op1idx, op2idx, str(op1 - op2)])
        dd["*"].append([op1idx, op2idx, str(op1 * op2)])
        # Emulate C++ integer division for negatives
        if op2 == 0:
            divresult = "exception"
        else:
            divresult = abs(op1) // abs(op2)
            if int(op1 < 0) != int(op2 < 0):
                divresult = -divresult
        dd["/"].append([op1idx, op2idx, str(divresult)])
        # Emulate C++ integer modulo for negatives
        if op2 == 0:
            modresult = "exception"
        else:
            modresult = abs(op1) % abs(op2)
            if int(op1 < 0):
                modresult = -modresult
        dd["%"].append([op1idx, op2idx, str(modresult)])

        dd["&"].append([op1idx, op2idx, str(op1 & op2)])
        dd["|"].append([op1idx, op2idx, str(op1 | op2)])
        dd["^"].append([op1idx, op2idx, str(op1 ^ op2)])

        dd["&&"].append([op1idx, op2idx, bool(op1) and bool(op2)])
        dd["||"].append([op1idx, op2idx, bool(op1) or bool(op2)])

        # Compare ops, write the two operands and the expected compare result
        dd["<=>"].append([op1idx, op2idx, comp])

        shiftamt = random.getrandbits(min(op1.bit_length(), 12))
        dd["<<"].append([op1idx, shiftamt, str(op1 << shiftamt)])
        dd["<<"].append([op2idx, shiftamt, str(op2 << shiftamt)])
        dd[">>"].append([op1idx, shiftamt, str(op1 >> shiftamt)])
        dd[">>"].append([op2idx, shiftamt, str(op2 >> shiftamt)])

        dd["++"].append([op1idx, str(op1 + 1)])
        dd["++"].append([op2idx, str(op2 + 1)])
        dd["--"].append([op1idx, str(op1 - 1)])
        dd["--"].append([op2idx, str(op2 - 1)])

    print(json.dumps(dd, indent=2))

if __name__ == "__main__":
    main()
