use bitcoinsuite_core::{
    bytes::{read_array, read_bytes},
    script::Script,
    ser::read_compact_size,
    tx::{Tx, TxId},
};
use bitflags::bitflags;
use bytes::Bytes;
use thiserror::Error;

use crate::{cashtokens::ParseError::*, structs::Amount, token_id::TokenId};

const PREFIX_TOKEN: u8 = 0xef;

bitflags! {
    /// "token_bitfield" of the CashTokens spec
    #[derive(Clone, Copy, Debug, Eq, PartialEq)]
    pub struct CapabilityFlags: u8 {
        /// Mutable token
        const NFT_MUTABLE = 0x01;
        /// Minting token
        const NFT_MINTING = 0x02;
        /// Token has an amount
        const HAS_AMOUNT = 0x10;
        /// Token is a non-fungible token
        const HAS_NFT = 0x20;
        /// Token has a commitment
        const HAS_COMMITMENT_LENGTH = 0x40;
    }
}

/// Errors when parsing a SLP tx.
#[derive(Clone, Debug, Error, Eq, PartialEq)]
pub enum ParseError {
    /// Missing token category
    #[error("Missing token category")]
    MissingCategory,

    /// Missing token capabilities
    #[error("Missing token capabilities")]
    MissingCapabilities,

    /// Invalid token capabilities
    #[error("Invalid token capabilities: {0:02x}")]
    InvalidCapabilities(u8),

    /// Invalid token commitment length
    #[error("Invalid token commitment length")]
    InvalidCommitmentLength,

    /// Failed reading token commitment
    #[error("Failed reading token commitment, expected {0} bytes")]
    InvalidCommitment(u64),

    /// Invalid token amount
    #[error("Invalid token amount")]
    InvalidAmount,
}

/// Parsed CashTokens data
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ParsedData {
    /// "category_id" of the spec
    pub token_id: TokenId,
    /// <token_bitfield> of the spec
    pub capabilities: CapabilityFlags,
    /// <commitment> of the spec, for "NFTs"
    pub commitment: Bytes,
    /// <ft_amount> of the spec
    pub amount: Amount,
    /// Actual locking bytecode of the output
    pub script: Script,
}

/// Whether the tx has any outputs with PREFIX_TOKEN
pub fn has_any_prefix_token(tx: &Tx) -> bool {
    tx.outputs.iter().any(|output| {
        if output.script.bytecode().is_empty() {
            return false;
        }
        output.script.bytecode()[0] == PREFIX_TOKEN
    })
}

/// Parse the given script as token data
pub fn parse_script(script: &Script) -> Result<Option<ParsedData>, ParseError> {
    let mut bytecode = script.bytecode().clone();
    let Ok([PREFIX_TOKEN]) = read_array(&mut bytecode) else {
        return Ok(None);
    };
    let token_id =
        read_array::<32>(&mut bytecode).map_err(|_| MissingCategory)?;
    let token_id = TokenId::new(TxId::new(token_id));

    // Read capabilities
    let [capabilities_byte] =
        read_array(&mut bytecode).map_err(|_| MissingCapabilities)?;
    let capabilities = CapabilityFlags::from_bits(capabilities_byte)
        .ok_or(InvalidCapabilities(capabilities_byte))?;
    if capabilities.contains(CapabilityFlags::NFT_MUTABLE)
        && capabilities.contains(CapabilityFlags::NFT_MINTING)
    {
        return Err(InvalidCapabilities(capabilities_byte));
    }

    // Read "NFT" commitment
    let mut commitment = Bytes::new();
    if capabilities.contains(CapabilityFlags::HAS_COMMITMENT_LENGTH) {
        let length = read_compact_size(&mut bytecode)
            .map_err(|_| InvalidCommitmentLength)?;
        commitment = read_bytes(&mut bytecode, length as usize)
            .map_err(|_| InvalidCommitment(length))?;
    }

    // Read token amount
    let mut amount = 0;
    if capabilities.contains(CapabilityFlags::HAS_AMOUNT) {
        amount = read_compact_size(&mut bytecode).map_err(|_| InvalidAmount)?;
    }

    Ok(Some(ParsedData {
        token_id,
        capabilities,
        commitment,
        amount,
        script: Script::new(bytecode),
    }))
}
