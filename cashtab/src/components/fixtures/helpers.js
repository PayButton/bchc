import { MockChronikClient } from '../../../../apps/mock-chronik-client';
import 'fake-indexeddb/auto';
import localforage from 'localforage';
import { cashtabSettings } from 'config/cashtabSettings';
import cashtabCache from 'config/cashtabCache';

/**
 * Get expected mock values for chronik client for a given mock wallet
 * Used to support integration testing in Cashtab
 * Default methods may be overwritten in individual unit tests to test special conditions
 * @param {object | boolean} wallet A mock Cashtab wallet
 * @param {boolean} apiError Default false. If true, return a mockedChronik that throws errors.
 * @returns
 */
export const initializeCashtabStateForTests = async (
    wallet,
    apiError = false,
) => {
    // Mock successful utxos calls in chronik
    const chronikClient = new MockChronikClient();

    if (wallet === false) {
        // No info to give to chronik, do not populate mocks
        return chronikClient;
        // We do not expect anything in localforage for this case
    }

    // Set localforage items. All defaults may be overwritten in a test for
    // specific purposes of the test.
    await localforage.setItem('cashtabCache', cashtabCache);
    await localforage.setItem('settings', cashtabSettings);
    // 'contactList' key will be empty if user has never added contacts
    await localforage.setItem('savedWallets', [wallet]);
    await localforage.setItem('wallet', wallet);

    // mock chronik endpoint returns
    const CASHTAB_TESTS_TIPHEIGHT = 800000;
    chronikClient.setMock('blockchainInfo', {
        output: apiError
            ? new Error('Error fetching blockchainInfo')
            : { tipHeight: CASHTAB_TESTS_TIPHEIGHT },
    });
    // Mock scriptutxos to match context
    // Cashtab only supports p2pkh addresses
    const CASHTAB_ADDRESS_TYPE = 'p2pkh';
    chronikClient.setScript(CASHTAB_ADDRESS_TYPE, wallet.Path1899.hash160);
    chronikClient.setUtxos(
        CASHTAB_ADDRESS_TYPE,
        wallet.Path1899.hash160,
        apiError
            ? new Error('Error fetching utxos')
            : [
                  {
                      outputScript: `76a914${wallet.Path1899.hash160}88ac`,
                      utxos: wallet.state.nonSlpUtxos.concat(
                          wallet.state.slpUtxos,
                      ),
                  },
              ],
    );

    // We set legacy paths to contain no utxos
    chronikClient.setScript(CASHTAB_ADDRESS_TYPE, wallet.Path145.hash160);
    chronikClient.setUtxos(
        CASHTAB_ADDRESS_TYPE,
        wallet.Path145.hash160,
        apiError ? new Error('Error fetching utxos') : [],
    );
    chronikClient.setScript(CASHTAB_ADDRESS_TYPE, wallet.Path245.hash160);
    chronikClient.setUtxos(
        CASHTAB_ADDRESS_TYPE,
        wallet.Path245.hash160,
        apiError ? new Error('Error fetching utxos') : [],
    );

    // TX history mocks
    chronikClient.setTxHistory(
        CASHTAB_ADDRESS_TYPE,
        wallet.Path1899.hash160,
        apiError
            ? new Error('Error fetching history')
            : wallet.state.parsedTxHistory,
    );
    // We set legacy paths to contain no utxos
    chronikClient.setTxHistory(
        CASHTAB_ADDRESS_TYPE,
        wallet.Path145.hash160,
        apiError ? new Error('Error fetching history') : [],
    );
    chronikClient.setTxHistory(
        CASHTAB_ADDRESS_TYPE,
        wallet.Path245.hash160,
        apiError ? new Error('Error fetching history') : [],
    );

    // Mock chronik.tx(tokenId) calls for tokens in tx history
    for (const tx of wallet.state.parsedTxHistory) {
        const mockedTokenResponse = {
            slpTxData: { genesisInfo: tx.parsed.genesisInfo },
        };
        if (tx.parsed.isEtokenTx) {
            chronikClient.setMock('tx', {
                input: tx.parsed.slpMeta.tokenId,
                output: mockedTokenResponse,
            });
        }
    }

    return chronikClient;
};
