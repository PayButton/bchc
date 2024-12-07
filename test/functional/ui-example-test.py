#!/usr/bin/env python3
# Copyright (c) 2021-2022 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
""" Example UI test plan boilerplate code """

from test_framework.test_framework import BitcoinTestFramework
from test_framework.ui import RunTestPlan

class UITestFramework(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [["-splash=0", "-ui"]]

    def run_test(self):
        # self.meaningfulWork()
        self.testPlan.waitUntilMaxReachedStep(1)

        # self.moreMeaningfulWork()
        self.testPlan.waitUntilMaxReachedStep(2)

if __name__ == '__main__':
    steps = [{
                'description': "Be a good guy"
            }, {
                'description': "Have a nice day"
            }]

    framework = UITestFramework()
    RunTestPlan(steps, framework)
