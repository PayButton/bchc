#include <undo.h>
#include <flatfile.h>
#include <protocol.h>
#include <streams.h>
#include <util/system.h>
#include <node/blockstorage.h>
#include <clientversion.h>

#include <chronik-cpp/util/storage.h>

static FlatFileSeq UndoFileSeq() {
    return FlatFileSeq(GetBlocksDir(), "rev", UNDOFILE_CHUNK_SIZE);
}

/** Open an undo file (rev?????.dat) */
static FILE *OpenUndoFile(const FlatFilePos &pos, bool fReadOnly) {
    return UndoFileSeq().Open(pos, fReadOnly);
}

bool ReadTxUndoFromDisk(CTxUndo &tx_undo, const FlatFilePos &pos) {
    // Open undo file to read
    CAutoFile filein(OpenUndoFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        return error("ReadTxUndoFromDisk: OpenUndoFile failed for %s",
                     pos.ToString());
    }

    // Read undo data
    try {
        filein >> tx_undo;
    } catch (const std::exception &e) {
        return error("%s: Deserialize or I/O error - %s at %s", __func__,
                     e.what(), pos.ToString());
    }

    return true;
}

bool ReadTxFromDisk(CMutableTransaction &tx, const FlatFilePos &pos) {
    // Open history file to read
    CAutoFile filein(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        return error("ReadTxFromDisk: OpenBlockFile failed for %s",
                     pos.ToString());
    }

    // Read tx
    try {
        filein >> tx;
    } catch (const std::exception &e) {
        return error("%s: Deserialize or I/O error - %s at %s", __func__,
                     e.what(), pos.ToString());
    }

    return true;
}
