#!/usr/bin/env python3
# Copyright (c) 2023 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
""" Test for the ImportWallet RPC progress bar display when run from GUI """

import os

from test_framework.address import byte_to_base58
from test_framework.key import ECKey
from test_framework.script import (
    CScript,
    hash160,
    OP_CHECKSIG, OP_DUP, OP_EQUALVERIFY, OP_HASH160,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.ui import RunTestPlan


# this number of keys, and scripts, will be generated
NUM_ITEMS = 5000

# number of junk comment lines to add, to extend Scanning duration
COMMENT_LINES = 590000


def privkey_to_wif(private_key):
    ''' private_key must be bytes '''
    version = 0xEF  # 0xEF for testnets, incl. regtest
    return byte_to_base58(private_key, version)


class UIImportWalletTestFramework(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [["-splash=0", "-ui"]]
        self.rpc_timeout = 120

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def create_large_wallet_dump(self):
        wallet_dump_filename = os.path.join(
            os.getcwd(), "bigwallet.dump")

        dump_file = open(wallet_dump_filename, 'wt', encoding="utf8")

        # generate keys
        test_addr_count = NUM_ITEMS
        privkeys = []
        for i in range(0, test_addr_count):
            priv_key = ECKey()
            priv_key.generate(compressed=True)
            privkeys.append(priv_key)
            dump_file.write("{} 2009-01-01:00:00Z reserve=1\n".format(privkey_to_wif(priv_key.get_bytes())))

        # generate equal amount of 1-of-1 multisig addresses to test scripts dump
        scripts_count = NUM_ITEMS
        for i in range(0, scripts_count):
            # Make script wrapping p2pkh
            pub_key = privkeys[i].get_pubkey().get_bytes()
            p2pkh_spk = CScript([OP_DUP, OP_HASH160, hash160(pub_key), OP_EQUALVERIFY, OP_CHECKSIG])
            dump_file.write("{} 2009-01-01:00:00Z script=1\n".format(str(p2pkh_spk.hex())))

        # add some filler comments to make the 'Scanning' phase progress better visible
        dump_file.write("#\n" * COMMENT_LINES)

        dump_file.close()


    def run_test(self):
        # 1. Create deterministic wallet dump files
        # we inform the user first about what we're going to do, since
        # the dump file generation is itself a lengthy step.
        self.testPlan.waitUntilMaxReachedStep(1)
        self.create_large_wallet_dump()

        # 2. Invite user to inspect generated dump file
        self.testPlan.waitUntilMaxReachedStep(2)

        # 3. Ask user to open console window
        self.testPlan.waitUntilMaxReachedStep(3)

        # 4. Use importwallet on the dump file
        self.testPlan.waitUntilMaxReachedStep(4)

        # 5. Observe progress bars
        self.testPlan.waitUntilMaxReachedStep(5)


if __name__ == '__main__':
    dump_file = os.path.join(os.getcwd(), "bigwallet.dump")
    steps = [{
                'description': "First step will be to generate a wallet dump file with {} keys and {} scripts for import.\n".format(NUM_ITEMS, NUM_ITEMS)
                               + "This may take some time, depending on the size of the dump file contents.\n"
                               "Please press Advance now - a new dialog (step 2) will appear once the dump file has been generated."
            }, {
                'description': "The wallet dump file has been generated.\n\n"
                               "It should contain {} lines in total.\n\n".format(NUM_ITEMS * 2 + COMMENT_LINES)
                               + "The extra comment lines are added to make the file scanning progress bar stay up longer to be more visible during this test.\n\n"
                               + "If you wish to inspect it with a text file viewer, the file path is\n\n"
                               + dump_file + "\n\n"
                               + "Press Advance when you are ready to carry on."
            }, {
                'description': "Please open the Console window\n"
                               "(either via Window->Console menu item, or with Ctrl-T)\n\n"
                               + "You should now have a flashing cursor at the console prompt.\n\n"
                               + "Press Advance to carry on."
            }, {
                'description': "Enter the following console command (you can copy from below and paste into the Console window):\n\n"
                               "importwallet {}".format(dump_file + "\n\n")
                               + "Press Advance once you have entered the command."
            }, {
                'description': "Observe the progress bar does the following:\n\n"
                               "1. First, a 'Scanning...' progress bar will appear and complete quickly from 0 to 100 percent\n"
                               "2. Then an 'Importing...' progress bar will appear and complete more slowly, but also smoothly, from 0 to 100 percent\n"
                               "3. After reaching 100% the progress bar window disappears\n\n"
                               "The console displays a result of 'null' for the importwallet command, indicating it has completed successfully.\n\n"
                               "Press 'Finish' to close down the test."
            }]

    framework = UIImportWalletTestFramework()
    RunTestPlan(steps, framework)
