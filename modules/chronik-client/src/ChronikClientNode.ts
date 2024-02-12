import * as proto from '../proto/chronikNode';
import { BlockchainInfo, OutPoint } from './ChronikClient';
import { FailoverProxy } from './failoverProxy';
import { fromHex, toHex, toHexRev } from './hex';

/**
 * Client to access an in-node Chronik instance.
 * Plain object, without any connections.
 */
export class ChronikClientNode {
    private _proxyInterface: FailoverProxy;
    /**
     * Create a new client. This just creates an object, without any connections.
     *
     * @param {string[]} urls Array of valid urls. A valid url comes with schema and without a trailing slash.
     * e.g. '['https://chronik.be.cash/xec2', 'https://chronik-native.fabien.cash']
     * The approach of accepting an array of urls as input is to ensure redundancy if the
     * first url encounters downtime.
     * @throws {error} throws error on invalid constructor inputs
     */
    constructor(urls: string[]) {
        // Instantiate FailoverProxy with the urls array
        this._proxyInterface = new FailoverProxy(urls);
    }

    // For unit test verification
    public proxyInterface(): FailoverProxy {
        return this._proxyInterface;
    }

    /**
     * Broadcasts the `rawTx` on the network.
     * If `skipTokenChecks` is false, it will be checked that the tx doesn't burn
     * any tokens before broadcasting.
     */
    public async broadcastTx(
        rawTx: Uint8Array | string,
        skipTokenChecks = false,
    ): Promise<{ txid: string }> {
        const request = proto.BroadcastTxRequest.encode({
            rawTx: typeof rawTx === 'string' ? fromHex(rawTx) : rawTx,
            skipTokenChecks,
        }).finish();
        const data = await this._proxyInterface.post('/broadcast-tx', request);
        const broadcastResponse = proto.BroadcastTxResponse.decode(data);
        return {
            txid: toHexRev(broadcastResponse.txid),
        };
    }

    /**
     * Broadcasts the `rawTxs` on the network, only if all of them are valid.
     * If `skipTokenChecks` is false, it will be checked that the txs don't burn
     * any tokens before broadcasting.
     */
    public async broadcastTxs(
        rawTxs: (Uint8Array | string)[],
        skipTokenChecks = false,
    ): Promise<{ txids: string[] }> {
        const request = proto.BroadcastTxsRequest.encode({
            rawTxs: rawTxs.map(rawTx =>
                typeof rawTx === 'string' ? fromHex(rawTx) : rawTx,
            ),
            skipTokenChecks,
        }).finish();
        const data = await this._proxyInterface.post('/broadcast-txs', request);
        const broadcastResponse = proto.BroadcastTxsResponse.decode(data);
        return {
            txids: broadcastResponse.txids.map(toHexRev),
        };
    }

    /** Fetch current info of the blockchain, such as tip hash and height. */
    public async blockchainInfo(): Promise<BlockchainInfo> {
        const data = await this._proxyInterface.get(`/blockchain-info`);
        const blockchainInfo = proto.BlockchainInfo.decode(data);
        return convertToBlockchainInfo(blockchainInfo);
    }

    /** Fetch info about the current running chronik server */
    public async chronikInfo(): Promise<ChronikInfo> {
        const data = await this._proxyInterface.get(`/chronik-info`);
        const chronikServerInfo = proto.ChronikInfo.decode(data);
        return convertToChronikInfo(chronikServerInfo);
    }

    /** Fetch the block given hash or height. */
    public async block(hashOrHeight: string | number): Promise<Block_InNode> {
        const data = await this._proxyInterface.get(`/block/${hashOrHeight}`);
        const block = proto.Block.decode(data);
        return convertToBlock(block);
    }

    /** Fetch the tx history of a block given hash or height. */
    public async blockTxs(
        hashOrHeight: string | number,
        page = 0, // Get the first page if unspecified
        pageSize = 25, // Must be less than 200, let server handle error as server setting could change
    ): Promise<TxHistoryPage_InNode> {
        const data = await this._proxyInterface.get(
            `/block-txs/${hashOrHeight}?page=${page}&page_size=${pageSize}`,
        );
        const blockTxs = proto.TxHistoryPage.decode(data);
        return convertToTxHistoryPage(blockTxs);
    }

    /**
     * Fetch block info of a range of blocks.
     * `startHeight` and `endHeight` are inclusive ranges.
     */
    public async blocks(
        startHeight: number,
        endHeight: number,
    ): Promise<BlockInfo_InNode[]> {
        const data = await this._proxyInterface.get(
            `/blocks/${startHeight}/${endHeight}`,
        );
        const blocks = proto.Blocks.decode(data);
        return blocks.blocks.map(convertToBlockInfo);
    }

    /** Fetch tx details given the txid. */
    public async tx(txid: string): Promise<Tx_InNode> {
        const data = await this._proxyInterface.get(`/tx/${txid}`);
        const tx = proto.Tx.decode(data);
        return convertToTx(tx);
    }

    /** Fetch tx details given the txid. */
    public async rawTx(txid: string): Promise<RawTx> {
        const data = await this._proxyInterface.get(`/raw-tx/${txid}`);
        const rawTx = proto.RawTx.decode(data);
        return convertToRawTx(rawTx);
    }

    /** Create object that allows fetching script history or UTXOs. */
    public script(
        scriptType: ScriptType_InNode,
        scriptPayload: string,
    ): ScriptEndpointInNode {
        return new ScriptEndpointInNode(
            this._proxyInterface,
            scriptType,
            scriptPayload,
        );
    }
}

/** Allows fetching script history and UTXOs. */
export class ScriptEndpointInNode {
    private _proxyInterface: FailoverProxy;
    private _scriptType: string;
    private _scriptPayload: string;

    constructor(
        proxyInterface: FailoverProxy,
        scriptType: string,
        scriptPayload: string,
    ) {
        this._proxyInterface = proxyInterface;
        this._scriptType = scriptType;
        this._scriptPayload = scriptPayload;
    }

    /**
     * Fetches the tx history of this script, in anti-chronological order.
     * This means it's ordered by first-seen first, i.e. TxHistoryPage_InNode.txs[0]
     * will be the most recent tx. If the tx hasn't been seen
     * by the indexer before, it's ordered by the block timestamp.
     * @param page Page index of the tx history.
     * @param pageSize Number of txs per page.
     */
    public async history(
        page = 0, // Get the first page if unspecified
        pageSize = 25, // Must be less than 200, let server handle error as server setting could change
    ): Promise<TxHistoryPage_InNode> {
        const data = await this._proxyInterface.get(
            `/script/${this._scriptType}/${this._scriptPayload}/history?page=${page}&page_size=${pageSize}`,
        );
        const historyPage = proto.TxHistoryPage.decode(data);
        return {
            txs: historyPage.txs.map(convertToTx),
            numPages: historyPage.numPages,
            numTxs: historyPage.numTxs,
        };
    }

    /**
     * Fetches the current UTXO set for this script.
     * It is grouped by output script, in case a script type can match multiple
     * different output scripts (e.g. Taproot on Lotus).
     */
    public async utxos(): Promise<ScriptUtxos_InNode> {
        const data = await this._proxyInterface.get(
            `/script/${this._scriptType}/${this._scriptPayload}/utxos`,
        );
        const scriptUtxos = proto.ScriptUtxos.decode(data);
        return {
            outputScript: toHex(scriptUtxos.script),
            utxos: scriptUtxos.utxos.map(convertToUtxo),
        };
    }
}

function convertToBlockchainInfo(
    blockchainInfo: proto.BlockchainInfo,
): BlockchainInfo {
    return {
        tipHash: toHexRev(blockchainInfo.tipHash),
        tipHeight: blockchainInfo.tipHeight,
    };
}

function convertToChronikInfo(chronikInfo: proto.ChronikInfo): ChronikInfo {
    if (chronikInfo.version === undefined) {
        throw new Error('chronikInfo has no version');
    }
    return {
        version: chronikInfo.version.length !== 0 ? chronikInfo.version : '',
    };
}

function convertToBlock(block: proto.Block): Block_InNode {
    if (block.blockInfo === undefined) {
        throw new Error('Block has no blockInfo');
    }
    return {
        blockInfo: convertToBlockInfo(block.blockInfo),
    };
}

function convertToTxHistoryPage(
    blockTxs: proto.TxHistoryPage,
): TxHistoryPage_InNode {
    const { txs, numPages, numTxs } = blockTxs;
    const convertedTxs = txs.map(convertToTx);
    return {
        txs: convertedTxs,
        numPages,
        numTxs,
    };
}

function convertToBlockInfo(block: proto.BlockInfo): BlockInfo_InNode {
    return {
        ...block,
        hash: toHexRev(block.hash),
        prevHash: toHexRev(block.prevHash),
        timestamp: parseInt(block.timestamp),
        blockSize: parseInt(block.blockSize),
        numTxs: parseInt(block.numTxs),
        numInputs: parseInt(block.numInputs),
        numOutputs: parseInt(block.numOutputs),
        sumInputSats: parseInt(block.sumInputSats),
        sumCoinbaseOutputSats: parseInt(block.sumCoinbaseOutputSats),
        sumNormalOutputSats: parseInt(block.sumNormalOutputSats),
        sumBurnedSats: parseInt(block.sumBurnedSats),
    };
}

function convertToTx(tx: proto.Tx): Tx_InNode {
    return {
        txid: toHexRev(tx.txid),
        version: tx.version,
        inputs: tx.inputs.map(convertToTxInput),
        outputs: tx.outputs.map(convertToTxOutput),
        lockTime: tx.lockTime,
        block:
            tx.block !== undefined ? convertToBlockMeta(tx.block) : undefined,
        timeFirstSeen: parseInt(tx.timeFirstSeen),
        size: tx.size,
        isCoinbase: tx.isCoinbase,
    };
}

function convertToTxInput(input: proto.TxInput): TxInput_InNode {
    if (input.prevOut === undefined) {
        throw new Error('Invalid proto, no prevOut');
    }
    return {
        prevOut: {
            txid: toHexRev(input.prevOut.txid),
            outIdx: input.prevOut.outIdx,
        },
        inputScript: toHex(input.inputScript),
        outputScript:
            input.outputScript.length > 0
                ? toHex(input.outputScript)
                : undefined,
        value: parseInt(input.value),
        sequenceNo: input.sequenceNo,
    };
}

function convertToTxOutput(output: proto.TxOutput): TxOutput_InNode {
    return {
        value: parseInt(output.value),
        outputScript: toHex(output.outputScript),
        spentBy:
            output.spentBy !== undefined
                ? {
                      txid: toHexRev(output.spentBy.txid),
                      outIdx: output.spentBy.inputIdx,
                  }
                : undefined,
    };
}

function convertToBlockMeta(block: proto.BlockMetadata): BlockMetadata_InNode {
    return {
        height: block.height,
        hash: toHexRev(block.hash),
        timestamp: parseInt(block.timestamp),
    };
}

function convertToRawTx(rawTx: proto.RawTx): RawTx {
    return {
        rawTx: toHex(rawTx.rawTx),
    };
}

function convertToUtxo(utxo: proto.ScriptUtxo): Utxo_InNode {
    if (utxo.outpoint === undefined) {
        throw new Error('UTXO outpoint is undefined');
    }
    return {
        outpoint: {
            txid: toHexRev(utxo.outpoint.txid),
            outIdx: utxo.outpoint.outIdx,
        },
        blockHeight: utxo.blockHeight,
        isCoinbase: utxo.isCoinbase,
        value: parseInt(utxo.value),
        isFinal: utxo.isFinal,
    };
}

/** Info about connected chronik server */
export interface ChronikInfo {
    version: string;
}

/**  BlockInfo interface for in-node chronik */
export interface BlockInfo_InNode {
    /** Block hash of the block, in 'human-readable' (big-endian) hex encoding. */
    hash: string;
    /** Block hash of the prev block, in 'human-readable' (big-endian) hex encoding. */
    prevHash: string;
    /** Height of the block; Genesis block has height 0. */
    height: number;
    /** nBits field of the block, encodes the target compactly. */
    nBits: number;
    /**
     * Timestamp of the block. Filled in by the miner,
     * so might not be 100 % precise.
     */
    timestamp: number;
    /** Is this block avalanche finalized? */
    isFinal: boolean;
    /** Block size of this block in bytes (including headers etc.). */
    blockSize: number;
    /** Number of txs in this block. */
    numTxs: number;
    /** Total number of tx inputs in block (including coinbase). */
    numInputs: number;
    /** Total number of tx output in block (including coinbase). */
    numOutputs: number;
    /** Total number of satoshis spent by tx inputs. */
    sumInputSats: number;
    /** Total block reward for this block. */
    sumCoinbaseOutputSats: number;
    /** Total number of satoshis in non-coinbase tx outputs. */
    sumNormalOutputSats: number;
    /** Total number of satoshis burned using OP_RETURN. */
    sumBurnedSats: number;
}

/** Block interface for in-node chronik */
export interface Block_InNode {
    /** Contains the blockInfo object defined above */
    blockInfo: BlockInfo_InNode;
}

/** A page of in-node chronik tx history */
export interface TxHistoryPage_InNode {
    /** Txs of the page */
    txs: Tx_InNode[];
    /** How many pages there are total */
    numPages: number;
    /** How many txs there are total */
    numTxs: number;
}

/** The hex bytes of a raw tx */
export interface RawTx {
    rawTx: string;
}

/** A transaction on the blockchain or in the mempool. */
export interface Tx_InNode {
    /** Transaction ID. */
    txid: string;
    /** `version` field of the transaction. */
    version: number;
    /** Inputs of this transaction. */
    inputs: TxInput_InNode[];
    /** Outputs of this transaction. */
    outputs: TxOutput_InNode[];
    /** `locktime` field of the transaction, tx is not valid before this time. */
    lockTime: number;
    /** Block data for this tx, or undefined if not mined yet. */
    block: BlockMetadata_InNode | undefined;
    /**
     * UNIX timestamp when this tx has first been seen in the mempool.
     * 0 if unknown -> make sure to check.
     */
    timeFirstSeen: number;
    /** Serialized size of the tx. */
    size: number;
    /** Whether this tx is a coinbase tx. */
    isCoinbase: boolean;
}

/** Input of a tx, spends an output of a previous tx. */
export interface TxInput_InNode {
    /** Points to an output spent by this input. */
    prevOut: OutPoint;
    /**
     * Script unlocking the output, in hex encoding.
     * Aka. `scriptSig` in bitcoind parlance.
     */
    inputScript: string;
    /**
     * Script of the output, in hex encoding.
     * Aka. `scriptPubKey` in bitcoind parlance.
     */
    outputScript: string | undefined;
    /** Value of the output spent by this input, in satoshis. */
    value: number;
    /** `sequence` field of the input; can be used for relative time locking. */
    sequenceNo: number;
}

/** Output of a tx, creates new UTXOs. */
export interface TxOutput_InNode {
    /** Value of the output, in satoshis. */
    value: number;
    /**
     * Script of this output, locking the coins.
     * Aka. `scriptPubKey` in bitcoind parlance.
     */
    outputScript: string;
    /**
     * Transaction & input index spending this output, or undefined if
     * unspent.
     */
    spentBy: OutPoint | undefined;
}

/** Metadata of a block, used in transaction data. */
export interface BlockMetadata_InNode {
    /** Height of the block. */
    height: number;
    /** Hash of the block. */
    hash: string;
    /**
     * Timestamp of the block; useful if `timeFirstSeen` of a transaction is
     * unknown.
     */
    timestamp: number;
}

/** Group of UTXOs by output script. */
export interface ScriptUtxos_InNode {
    /** Output script in hex. */
    outputScript: string;
    /** UTXOs of the output script. */
    utxos: Utxo_InNode[];
}

/** An unspent transaction output (aka. UTXO, aka. "Coin") of a script. */
export interface Utxo_InNode {
    /** Outpoint of the UTXO. */
    outpoint: OutPoint;
    /** Which block this UTXO is in, or -1 if in the mempool. */
    blockHeight: number;
    /** Whether this UTXO is a coinbase UTXO
     * (make sure it's buried 100 blocks before spending!) */
    isCoinbase: boolean;
    /** Value of the UTXO in satoshis. */
    value: number;
    /** Is this utxo avalanche finalized */
    isFinal: boolean;
}

/**
 * Script type queried in the `script` method.
 * - `other`: Script type not covered by the standard script types; payload is
 *   the raw hex.
 * - `p2pk`: Pay-to-Public-Key (`<pk> OP_CHECKSIG`), payload is the hex of the
 *   pubkey (compressed (33 bytes) or uncompressed (65 bytes)).
 * - `p2pkh`: Pay-to-Public-Key-Hash
 *   (`OP_DUP OP_HASH160 <pkh> OP_EQUALVERIFY OP_CHECKSIG`).
 *   Payload is the 20 byte public key hash.
 * - `p2sh`: Pay-to-Script-Hash (`OP_HASH160 <sh> OP_EQUAL`).
 *   Payload is the 20 byte script hash.
 */
export type ScriptType_InNode = 'other' | 'p2pk' | 'p2pkh' | 'p2sh';
