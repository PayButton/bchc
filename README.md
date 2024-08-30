![BCHC Logo](share/pixmaps/bchclogo.png "BCHC")

What is BCHC?
--------------------

BCHC is a high performance full node & indexer which acts as the blockchain gateway for apps built on Bitcoin Cash. It is a fork of the [Bitcoin ABC](https://bitcoinabc.org)
software project.

A [BCHN](https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node) bridge node is required to properly connect to the Bitcoin Cash (BCH) network.

Build Documentation
----------------

To build, follow the same process as would be done on Bitcoin ABC or BCHN, but using `-DBUILD_BITCOIN_CHRONIK=on`.

Then, ensure `chronik=1` is set in the node's config for startup.

Indexer Documentation
----------------

BCHC uses the Chronik indexer. Documentation can be found [here](https://docs.chronik.xyz/).

License
-------

BCHC is released under the terms of the MIT license. See
[COPYING](COPYING) for more information or see
<https://opensource.org/licenses/MIT>.
