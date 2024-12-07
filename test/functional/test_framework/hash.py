from .script import hash160  # noqa
from .messages import sha256

def hex_be_sha256(data: bytes) -> str:
    return sha256(data)[::-1].hex()
