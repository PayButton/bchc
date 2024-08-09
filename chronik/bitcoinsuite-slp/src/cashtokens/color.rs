use bitcoinsuite_core::tx::Tx;

use crate::{
    cashtokens::{parse_script, CapabilityFlags},
    color::{ColoredTx, ColoredTxSection, FailedParsing, ParseError},
    structs::{
        GenesisInfo, TokenCommitment, TokenMeta, TokenOutput, TokenVariant,
        TxType,
    },
    token_type::TokenType,
};

/// Color the tx with CashToken outputs
pub fn color(colored_tx: &mut ColoredTx, tx: &Tx) {
    for (out_idx, tx_output) in tx.outputs.iter().enumerate() {
        let parsed = match parse_script(&tx_output.script) {
            Ok(Some(parsed)) => parsed,
            Ok(None) => continue,
            Err(err) => {
                colored_tx.failed_parsings.push(FailedParsing {
                    pushdata_idx: None,
                    bytes: tx_output.script.bytecode().clone(),
                    error: ParseError::CashTokens(err),
                });
                continue;
            }
        };
        let pregenesis_input_idx = tx.inputs.iter().position(|input| {
            input.prev_out.txid == *parsed.token_id.txid()
                && input.prev_out.out_idx == 0
        });

        let meta = TokenMeta {
            token_id: parsed.token_id,
            token_type: TokenType::CashTokens,
        };
        let token_idx = match colored_tx
            .sections
            .iter_mut()
            .enumerate()
            .find(|(_, section)| section.meta == meta)
        {
            Some((token_idx, _)) => token_idx,
            None => {
                let token_idx = colored_tx.sections.len();
                colored_tx.sections.push(ColoredTxSection {
                    meta,
                    tx_type: if pregenesis_input_idx.is_some() {
                        TxType::GENESIS
                    } else {
                        TxType::SEND
                    },
                    required_input_sum: 0,
                    has_colored_out_of_range: false,
                    genesis_info: pregenesis_input_idx.map(
                        |pregenesis_input_idx| GenesisInfo {
                            pregenesis_input_idx: Some(pregenesis_input_idx),
                            ..Default::default()
                        },
                    ),
                });
                token_idx
            }
        };
        colored_tx.outputs[out_idx] = Some(TokenOutput {
            token_idx,
            variant: if parsed.capabilities.contains(CapabilityFlags::HAS_NFT) {
                TokenVariant::Commitment(TokenCommitment {
                    amount: parsed.amount,
                    capabilities: parsed.capabilities.bits(),
                    commitment: parsed.commitment,
                })
            } else {
                TokenVariant::Amount(parsed.amount)
            },
        });
    }
}
