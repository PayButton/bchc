# Property Tests for Big Integer Arithmetic Script Operations

Here we give an overview and methodology of property tests in [`/src/tests/bigint_script_property_tests.cpp`](../src/test/bigint_script_property_tests.cpp).

## Running

Make a `build` folder at the repository root, `cd` into it, then run:

```
ninja test_bitcoin
src/test/test_bitcoin -t bigint_script_property_tests
```

To run a specific test, append the test case name, e.g.:

```
src/test/test_bitcoin -t bigint_script_property_tests/op_1add_tests
```

## Assumptions

Maximum script number is defined based on maximum stack item size:

- `MAX_SCRIPTNUM == 2^(MAX_ELEM_SIZE * 8 - 1) - 1`.

Minimum script number is defined as maximum script number negated:

- `MIN_SCRIPTNUM == -MAX_SCRIPTNUM`.

## General Test Requirements

Except for specific overflow (`OP_1ADD`, `OP_NUM2BIN`), underflow (`OP_1SUB`), and range (`OP_ADD`, `OP_SUB`, `OP_MUL`) tests, the test input argument ranges are designed to not fail with stack element size or overflow error, while still testing the edges.

When testing overflow, we will test overflow of the operation's *output*.
There are no overflow tests for operation's input because, with this upgrade, any minimally-encoded stack item will be a valid input to arithmetic opcodes.
It will not be possible to overflow an operation's input, because it will not be possible to have a too big stack item to begin with, because any operation that would attempt to produce it (including push operations) would fail.

Tests must cover full valid range and edges of both positive and negative ranges, e.g. a single parameter will be tested for values at the edges of these two ranges:

- `[MIN_SCRIPNUM, 0)` and `[0, MAX_SCRIPTNUM]`,

meaning values MIN_SCRIPNUM, 0, MAX_SCRIPTNUM will definitely get tested, alongside a pick of random values in the range.

If the test could overflow for some inputs (due to possibility of involved opcodes outputting bigger outputs than inputs), then test range should be reduced, e.g. the test:

- Pass: `{stack: a} OP_DUP OP_1ADD OP_SWAP OP_SUB OP_1 OP_NUMEQUAL`

will have the test range of `a` reduced to:

- `[MIN_SCRIPNUM, 0)` and `[0, MAX_SCRIPTNUM - 1]`.

For tests with multiple operands, all combinations should be tested.
To avoid overflow, in some cases we must reduce the range for the following variable(s), e.g. for the test:

- Pass: `{stack: a, b} OP_2DUP OP_ADD OP_SWAP OP_ROT OP_ADD OP_NUMEQUAL`

we must first pick a random `a` from `[0, MAX_SCRIPTNUM]` and then pick a random `b` from `[0, MAX_SCRIPTNUM - b]`.
We will iterate such test for all the combinations of edges of `a` and `b`, and random picks of values between.

## Generic Tests

These test input and resulting stack depth, and operand's minimal encoding.
They are repeated for each opcode.

### Stack Depth Tests

Generate and run these scripts for the tested `{opcode}`:

- Fail: `{undersized stack} {opcode} OP_DEPTH OP_{depthOut} OP_NUMEQUALVERIFY {OP_DROP x depthOut} OP_1`
- Pass: `{exact-sized stack} {opcode} OP_DEPTH OP_{depthOut} OP_NUMEQUALVERIFY {OP_DROP x depthOut} OP_1`
- Fail: `{oversized stack} {opcode} OP_DEPTH OP_{depthOut} OP_NUMEQUALVERIFY {OP_DROP x depthOut} OP_1`

When failing, the above test scripts must fail with `ScriptError::INVALID_STACK_OPERATION` error.

We designed the stack depth test to fail on invalid stack state, but we have a "pass" test just to verify the test itself is good and will pass for a valid state.
This way we know that if there's some bug in the tested opcode where it could transform invalid stack state to valid, the test has a chance of actually passing for where it is expected to fail.

### Minimally-encoded Operand Tests

Generate and run these scripts for the tested `{opcode}`:

- Unary opcodes:
    - Fail: `{stack: 0, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN 0x0180 OP_CAT {opcode} OP_DROP OP_1`
    - Fail: `{stack: a, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN {opcode} OP_DROP OP_1`
- Binary opcodes:
    - Fail: `{stack: a, 0, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN 0x0180 OP_CAT {opcode} OP_DROP OP_1`
    - Fail: `{stack: a, 0, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN 0x0180 OP_CAT OP_SWAP {opcode} OP_DROP OP_1`
    - Fail: `{stack: a, b, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN {opcode} OP_DROP OP_1`
    - Fail: `{stack: a, b, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN OP_SWAP {opcode} OP_DROP OP_1`
- Ternary opcodes:
    - Fail: `{stack: a, b, 0, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN 0x0180 OP_CAT {opcode} OP_DROP OP_1`
    - Fail: `{stack: a, b, 0, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN 0x0180 OP_CAT OP_ROT {opcode} OP_DROP OP_1`
    - Fail: `{stack: a, b, 0, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN 0x0180 OP_CAT OP_ROT OP_ROT {opcode} OP_DROP OP_1`
    - Fail: `{stack: a, b, c, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN {opcode} OP_DROP OP_1`
    - Fail: `{stack: a, b, c, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN OP_ROT {opcode} OP_DROP OP_1`
    - Fail: `{stack: a, b, c, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN OP_ROT OP_ROT {opcode} OP_DROP OP_1`

All above test scripts must fail with `ScriptError::MINIMALNUM` error.

### Minimally-encoded Result Tests

This is tested implicitly by other property tests, because the result of tested opcode will be consumed by another arithmetic opcode, and that one would fail if it was not minimally-encoded.

## Cast Operations

These operations convert a stack item to / from a script number.
They also must work the same, for the full range of new limits.

### OP_NUM2BIN (0x80)

Unlike arithmetic opcodes, the number to be converted needs not be a minimally-encoded script number, so generic minimal-encoding tests do not apply here.

- Minimally-encoded Operand Test (the number to be converted needs NOT be a minimally encoded number):
    - Pass: `{stack: 0, m, n} OP_ROT OP_ROT OP_NUM2BIN 0x0180 OP_CAT OP_DUP OP_ROT OP_NUM2BIN OP_BIN2NUM OP_SWAP OP_BIN2NUM OP_NUMEQUAL`
    - Pass: `{stack: a, m, n} OP_ROT OP_ROT OP_NUM2BIN OP_DUP OP_ROT OP_NUM2BIN OP_BIN2NUM OP_SWAP OP_BIN2NUM OP_NUMEQUAL`
- Minimally-encoded Operand Test (the requested size MUST be a minimally encoded number):
    - Fail: `{stack: a, 0, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN 0x0180 OP_CAT {opcode} OP_DROP OP_1`
    - Fail: `{stack: a, b, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN {opcode} OP_DROP OP_1`
- Requested size must be sufficient to accommodate lossless encoding:
    - Fail: `{stack: a} OP_SIZE OP_1SUB OP_NUM2BIN OP_DUP OP_EQUAL` (must fail with `ScriptError::IMPOSSIBLE_ENCODING` error)
- Pad a number with n 0-bytes (while shifting the sign bit where present), then split and verify it matches the source number and requested size:
    - Pass: `{stack: a, n} OP_2DUP OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN OP_ROT OP_SIZE OP_ROT OP_SWAP OP_SPLIT OP_DUP OP_BIN2NUM OP_0 OP_NUMEQUALVERIFY OP_SIZE OP_ROT OP_ROT OP_CAT OP_BIN2NUM OP_ROT OP_NUMEQUALVERIFY OP_NUMEQUAL`
- Overflow
    - Pass: `{stack: a, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN OP_DUP OP_EQUAL`, where `size(a) + n <= MAX_ELEM_SIZE`
    - Fail: `{stack: a, n} OP_SWAP OP_SIZE OP_ROT OP_ADD OP_NUM2BIN OP_DUP OP_EQUAL`, where `size(a) + n > MAX_ELEM_SIZE` (must fail with `ScriptError::PUSH_SIZE` error)

### OP_BIN2NUM (0x81)

- Any stack item is a valid input, and the operation will produce a valid, minimally-encoded, script number:
    - Pass: `{stack: 0, n} OP_OVER OP_SIZE OP_ROT OP_ADD OP_NUM2BIN 0x0180 OP_CAT OP_BIN2NUM OP_NUMEQUAL`
    - Pass: `{stack: a, n} OP_OVER OP_SIZE OP_ROT OP_ADD OP_NUM2BIN OP_BIN2NUM OP_NUMEQUAL`

## Arithmetic Operations

### OP_1ADD (0x8b)

- Successor: `a < op1add(a)`
    - Pass: `{stack: a} OP_DUP OP_1ADD OP_LESSTHAN`
- Increment: `op1add(a) - a == 1`
    - Pass: `{stack: a} OP_DUP OP_1ADD OP_SWAP OP_SUB OP_1 OP_NUMEQUAL`
- Inverse: `a == op1sub(op1add(a))`
    - Pass: `{stack: a} OP_DUP OP_1ADD OP_1SUB OP_NUMEQUAL`
- Apply Multiple: `a + 3 == op1add(op1add(op1add(a)))`
    - Pass: `{stack: a} OP_DUP OP_3 OP_ADD OP_SWAP OP_1ADD OP_1ADD OP_1ADD OP_NUMEQUAL`
- Overflow
    - Pass: `{stack: a} OP_1ADD OP_DROP OP_1`, where `a < MAX_SCRIPTNUM`
    - Fail: `{stack: a} OP_1ADD OP_DROP OP_1`, where `a == MAX_SCRIPTNUM` (must fail with `ScriptError::INVALID_NUMBER_RANGE_BIG_INT` error)

### OP_1SUB (0x8c)

- Predecessor: `a > op1sub(a)`
    - Pass: `{stack: a} OP_DUP OP_1SUB OP_GREATERTHAN`
- Decrement: `a - op1sub(a) == 1`
    - Pass: `{stack: a} OP_DUP OP_1SUB OP_SUB OP_1 OP_NUMEQUAL`
- Inverse: `a == op1add(op1sub(a))`
    - Pass: `{stack: a} OP_DUP OP_1SUB OP_1ADD OP_NUMEQUAL`
- Apply Multiple: `a - 3 == op1sub(op1sub(op1sub(a)))`
    - Pass: `{stack: a} OP_DUP OP_3 OP_SUB OP_SWAP OP_1SUB OP_1SUB OP_1SUB OP_NUMEQUAL`
- Underflow:
    - Pass: `{stack: a} OP_1SUB OP_DROP OP_1`, where `a > -MAX_SCRIPTNUM`
    - Fail: `{stack: a} OP_1SUB OP_DROP OP_1`, where `a == -MAX_SCRIPTNUM` (must fail with `ScriptError::INVALID_NUMBER_RANGE_BIG_INT` error)

### OP_NEGATE (0x8f)

- Zero negation: `-0 == 0`
    - Pass: `OP_0 OP_NEGATE OP_0 OP_NUMEQUAL`
- Double negation: `a == -(-a)`
    - Pass: `{stack: a} OP_DUP OP_NEGATE OP_NEGATE OP_NUMEQUAL`
- Multiplication equivalence: `-a == a * (-1)`
    - Pass: `{stack: a} OP_DUP OP_NEGATE OP_SWAP OP_1NEGATE OP_MUL OP_NUMEQUAL`
- Zero sum: `-a + a == 0`
    - Pass: `{stack: a} OP_DUP OP_NEGATE OP_ADD OP_0 OP_NUMEQUAL`

### OP_ABS (0x90)

- Absolute of a positive number: `a == abs(a)`
    - Pass: `{stack: a} OP_DUP OP_ABS OP_NUMEQUAL`, where `a >= 0`
    - Fail: `{stack: a} OP_DUP OP_ABS OP_NUMEQUAL`, where `a < 0` (must fail with `ScriptError::EVAL_FALSE` error)
- Absolute of a negative number: `a == -abs(a)`
    - Pass: `{stack: a} OP_DUP OP_ABS OP_NEGATE OP_NUMEQUAL`, where `a <= 0`
    - Fail: `{stack: a} OP_DUP OP_ABS OP_NEGATE OP_NUMEQUAL`, where `a > 0` (must fail with `ScriptError::EVAL_FALSE` error)

### OP_NOT (0x91)

- Zero: `!0 == 1`
    - Pass: `OP_0 OP_NOT OP_1 OP_NUMEQUAL`
- Non-zero: `!a == 0`
    - Pass: `{stack: a} OP_NOT OP_0 OP_NUMEQUAL`, where `a > 0`
- Double: `!(!a) == !(a == 0)`
    - Pass: `{stack: a} OP_DUP OP_NOT OP_NOT OP_SWAP OP_0 OP_NUMEQUAL OP_NOT OP_NUMEQUAL`

Note: De Morgan's laws are tested under [OP_BOOLAND](#op_booland-0x9a) and [OP_BOOLOR](#op_boolor-0x9b)

### OP_0NOTEQUAL (0x92)

- Zero: `!(0 == 0) == 1`
    - Pass: `OP_0 OP_0NOTEQUAL OP_0 OP_NUMEQUAL`
- Non-zero: `!(a == 0) == 1`
    - Pass: `{stack: a} OP_0NOTEQUAL OP_1 OP_NUMEQUAL`
- Double: `!(!a) == !(!(a == 0) == 0)`
    - Pass: `{stack: a} OP_DUP OP_0NOTEQUAL OP_0NOTEQUAL OP_SWAP OP_NOT OP_NOT OP_NUMEQUAL`

### OP_ADD (0x93)

- Identity: `a + 0 == a && 0 + a == a`
    - Pass: `{stack: a} OP_DUP OP_0 OP_ADD OP_OVER OP_NUMEQUAL OP_0 OP_2 OP_PICK OP_ADD OP_ROT OP_NUMEQUAL OP_BOOLAND`
- Commutativity: `a + b == b + a`
    - Pass: `{stack: a, b} OP_2DUP OP_ADD OP_SWAP OP_ROT OP_ADD OP_NUMEQUAL`
- Successor: `a < a + b`
    - Pass: `{stack: a, b} OP_OVER OP_ADD OP_LESSTHAN`, where `b > 0`
    - Fail: `{stack: a, b} OP_OVER OP_ADD OP_LESSTHAN`, where `b <= 0` (must fail with `ScriptError::EVAL_FALSE` error)
- Inverse: `(a + b) - b == a`
    - Pass: `{stack: a, b} OP_2DUP OP_ADD OP_SWAP OP_SUB OP_NUMEQUAL`
- Range:
    - Pass: `{stack: a, b} OP_ADD OP_DROP OP_1`, where `a + b` is within `[-MAX_SCRIPTNUM, MAX_SCRIPTNUM]` range
    - Fail: `{stack: a, b} OP_ADD OP_DROP OP_1`, where `a + b` is out of `[-MAX_SCRIPTNUM, MAX_SCRIPTNUM]` range (must fail with `ScriptError::INVALID_NUMBER_RANGE_BIG_INT` error)
- Associativity: `(a + b) + c == a + (b + c)`
    - Pass: `{stack: a, b, c} OP_2 OP_PICK OP_2 OP_PICK OP_ADD OP_OVER OP_ADD OP_2SWAP OP_3 OP_ROLL OP_ADD OP_ADD OP_NUMEQUAL`

### OP_SUB (0x94)

- Identity: `a - 0 == a`
    - Pass: `{stack: a} OP_DUP OP_0 OP_SUB OP_NUMEQUAL`
- Sign: `0 - a == -a`
    - Pass: `{stack: a} OP_0 OP_OVER OP_SUB OP_SWAP OP_NEGATE OP_NUMEQUAL`
- Anti-commutativity: `a - b == -(b - a)`
    - Pass: `{stack: a, b} OP_2DUP OP_SUB OP_SWAP OP_ROT OP_SUB OP_NEGATE OP_NUMEQUAL`
- Predecessor: `a > a - b`
    - Pass: `{stack: a, b} OP_OVER OP_SWAP OP_SUB OP_GREATERTHAN`, where `b > 0`
    - Fail: `{stack: a, b} OP_OVER OP_SWAP OP_SUB OP_GREATERTHAN`, where `b <= 0` (must fail with `ScriptError::EVAL_FALSE` error)
- Inverse: `(a - b) + b == a`
    - Pass: `{stack: a, b} OP_2DUP OP_SUB OP_ADD OP_NUMEQUAL`
- Range:
    - Pass: `{stack: a, b} OP_SUB OP_DROP OP_1`, where `a - b` is within `[-MAX_SCRIPTNUM, MAX_SCRIPTNUM]` range
    - Fail: `{stack: a, b} OP_SUB OP_DROP OP_1`, where `a - b` is out of `[-MAX_SCRIPTNUM, MAX_SCRIPTNUM]` range (must fail with `ScriptError::INVALID_NUMBER_RANGE_BIG_INT` error)
- Non-associativity: `(a - b) - c == a - (b + c)`
    - Pass: `{stack: a, b, c} OP_2 OP_PICK OP_2 OP_PICK OP_SUB OP_OVER OP_SUB OP_2SWAP OP_3 OP_ROLL OP_ADD OP_SUB OP_NUMEQUAL`

### OP_MUL (0x95)

- Identity: `a * 1 == a && 1 * a == a`
    - Pass: `{stack: a} OP_DUP OP_1 OP_MUL OP_OVER OP_NUMEQUAL OP_1 OP_2 OP_PICK OP_MUL OP_ROT OP_NUMEQUAL OP_BOOLAND`
- Negation: `a * (-1) == -a`
    - Pass: `{stack: a} OP_DUP OP_1NEGATE OP_MUL OP_SWAP OP_NEGATE OP_NUMEQUAL`
- Zero: `a * 0 == 0 && 0 * a == 0`
    - Pass: `{stack: a} OP_DUP OP_0 OP_MUL OP_0 OP_NUMEQUAL OP_0 OP_ROT OP_MUL OP_0 OP_NUMEQUAL OP_BOOLAND`
- Equivalence with multiple additions: `a * 4 == a + a + a + a`
    - Pass: `{stack: a} OP_DUP OP_4 OP_MUL OP_OVER OP_2 OP_PICK OP_ADD OP_2 OP_PICK OP_ADD OP_ROT OP_ADD OP_NUMEQUAL`
- Equivalence with multiple subtractions: `a * (-4) == a - a - a - a - a - a`
    - Pass: `{stack: a} OP_DUP OP_4 OP_NEGATE OP_MUL OP_OVER OP_2 OP_PICK OP_SUB OP_2 OP_PICK OP_SUB OP_2 OP_PICK OP_SUB OP_2 OP_PICK OP_SUB OP_ROT OP_SUB OP_NUMEQUAL`
- Commutativity: `a * b == b * a`
    - Pass: `{stack: a, b} OP_2DUP OP_MUL OP_SWAP OP_ROT OP_MUL OP_NUMEQUAL`
- Inverse: `(a * b) / b == a, where b != 0`
    - Pass: `{stack: a, b} OP_2DUP OP_MUL OP_SWAP OP_DIV OP_NUMEQUAL`
- Range:
    - Pass: `{stack: a, b} OP_MUL OP_DROP OP_1`, where `a * b` is within `[-MAX_SCRIPTNUM, MAX_SCRIPTNUM]` range
    - Fail: `{stack: a, b} OP_MUL OP_DROP OP_1`, where `a * b` is out of `[-MAX_SCRIPTNUM, MAX_SCRIPTNUM]` range (must fail with `ScriptError::INVALID_NUMBER_RANGE_BIG_INT` error)
- Order: `a * b < a * c`
    - Pass: `{stack: a, b, c} OP_2 OP_PICK OP_ROT OP_MUL OP_ROT OP_ROT OP_MUL OP_LESSTHAN`, where `(a > 0 and b < c) or (a < 0 and b > c)`
    - Fail: `{stack: a, b, c} OP_2 OP_PICK OP_ROT OP_MUL OP_ROT OP_ROT OP_MUL OP_LESSTHAN`, otherwise (must fail with `ScriptError::EVAL_FALSE` error)
- Associativity: `(a * b) * c == a * (b * c)`
    - Pass: `{stack: a, b, c} OP_2 OP_PICK OP_2 OP_PICK OP_MUL OP_OVER OP_MUL OP_2SWAP OP_3 OP_ROLL OP_MUL OP_MUL OP_NUMEQUAL`
- Distributivity: `a * (b + c) == (a * b) + (a * c)`
    - Pass: `{stack: a, b, c} OP_3DUP OP_ADD OP_MUL OP_3 OP_PICK OP_3 OP_ROLL OP_MUL OP_2SWAP OP_MUL OP_ADD OP_NUMEQUAL`

### OP_DIV (0x96)

- Identity: `a / 1 == a`
    - Pass: `{stack: a} OP_DUP OP_1 OP_DIV OP_NUMEQUAL`
- Negation: `a / (-1) == -a`
    - Pass: `{stack: a} OP_DUP OP_1NEGATE OP_DIV OP_SWAP OP_NEGATE OP_NUMEQUAL`
- Division by zero: `a / 0` must fail.
    - Fail: `{stack: a} OP_0 OP_DIV OP_DROP OP_1` (must fail with `ScriptError::DIV_BY_ZERO` error)
- Self-division: `a / a == 1`, where `a != 0`
    - Pass: `{stack: a} OP_DUP OP_DIV OP_1 OP_NUMEQUAL`
- Dividing a zero: `0 / a == 0`, where `a != 0`
    - Pass: `{stack: a} OP_0 OP_SWAP OP_DIV OP_0 OP_NUMEQUAL`
- Inverse: `(a / b) * b + (a % b) == a`, where `b != 0`
    - Pass: `{stack: a, b} OP_2DUP OP_DIV OP_OVER OP_MUL OP_2 OP_PICK OP_ROT OP_MOD OP_ADD OP_NUMEQUAL`
- Distributivity: `(a + b) / c == a / c + b / c + (a % c + b % c - (a + b) % c) / c`, where `c != 0`
    - Pass: `{stack: a, b, c} OP_2 OP_PICK OP_2 OP_PICK OP_ADD OP_OVER OP_DIV OP_3 OP_PICK OP_2 OP_PICK OP_DIV OP_2OVER OP_DIV OP_ADD OP_4 OP_PICK OP_3 OP_PICK OP_MOD OP_4 OP_PICK OP_4 OP_PICK OP_MOD OP_ADD OP_2ROT OP_ADD OP_4 OP_PICK OP_MOD OP_SUB OP_3 OP_ROLL OP_DIV OP_ADD OP_NUMEQUAL`

### OP_MOD (0x97)

- Power identity: `(a * a) % a == 0`, where `a != 0`
    - Pass: `{stack: a} OP_DUP OP_DUP OP_MUL OP_SWAP OP_MOD OP_0 OP_NUMEQUAL`
- Modulo by zero: `a % 0` must fail.
    - Fail: `{stack: a} OP_0 OP_MOD OP_DROP OP_1` (must fail with `ScriptError::MOD_BY_ZERO` error)
- Repeat identity: `(a % b) % b == a % b`, where `b != 0`
    - Pass: `{stack: a, b} OP_2DUP OP_MOD OP_OVER OP_MOD OP_ROT OP_ROT OP_MOD OP_NUMEQUAL`
- Sign absorption: `a % (-b) == a % b`, where `b != 0`
    - Pass: `{stack: a, b} OP_2DUP OP_NEGATE OP_MOD OP_ROT OP_ROT OP_MOD OP_NUMEQUAL`
- Sign preservation: `(-a) % b == -(a % b)`, where `b != 0`
    - Pass: `{stack: a, b} OP_OVER OP_NEGATE OP_OVER OP_MOD OP_ROT OP_ROT OP_MOD OP_NEGATE OP_NUMEQUAL`

Note: consistency with OP_MUL and OP_DIV operations is part of [OP_DIV](#op_div-0x96) tests.

### OP_BOOLAND (0x9a)

- Idempotence: `(a && a) == (a != false)`
    - Pass: `{stack: a} OP_DUP OP_DUP OP_BOOLAND OP_SWAP OP_0 OP_NUMNOTEQUAL OP_NUMEQUAL`
- Casting: `(a && b) == (a != false && b != false)`
    - Pass: `{stack: a, b} OP_2DUP OP_BOOLAND OP_ROT OP_0 OP_NUMNOTEQUAL OP_ROT OP_0 OP_NUMNOTEQUAL OP_BOOLAND OP_NUMEQUAL`
- Commutativity: `(a && b) == (b && a)`
    - Pass: `{stack: a, b} OP_2DUP OP_BOOLAND OP_SWAP OP_ROT OP_BOOLAND OP_NUMEQUAL`
- De Morgan's law: `!(a && b) == (!a || !b)`
    - Pass: `{stack: a, b} OP_2DUP OP_BOOLAND OP_NOT OP_ROT OP_NOT OP_ROT OP_NOT OP_BOOLOR OP_NUMEQUAL`
- Absorption: `(a || (a && b)) == (a != false)`
    - Pass: `{stack: a, b} OP_OVER OP_2 OP_PICK OP_ROT OP_BOOLAND OP_BOOLOR OP_SWAP OP_0 OP_NUMNOTEQUAL OP_NUMEQUAL`
- Associativity: `((a && b) && c) == (a && (b && c))`
    - Pass: `{stack: a, b, c} OP_2 OP_PICK OP_2 OP_PICK OP_BOOLAND OP_OVER OP_BOOLAND OP_2SWAP OP_3 OP_ROLL OP_BOOLAND OP_BOOLAND OP_NUMEQUAL`
- Distributivity: `((a || b) && c) == ((a && c) || (b && c))`
    - Pass: `{stack: a, b, c} OP_2 OP_PICK OP_2 OP_PICK OP_BOOLOR OP_OVER OP_BOOLAND OP_3 OP_ROLL OP_2 OP_PICK OP_BOOLAND OP_2SWAP OP_BOOLAND OP_BOOLOR OP_NUMEQUAL`

### OP_BOOLOR (0x9b)

- Idempotence: `(a || a) == (a != false)`
    - Pass: `{stack: a} OP_DUP OP_DUP OP_BOOLOR OP_SWAP OP_0 OP_NUMNOTEQUAL OP_NUMEQUAL`
- Casting: `(a || b) == (a != false || b != false)`
    - Pass: `{stack: a, b} OP_2DUP OP_BOOLOR OP_ROT OP_0 OP_NUMNOTEQUAL OP_ROT OP_0 OP_NUMNOTEQUAL OP_BOOLOR OP_NUMEQUAL`
- Commutativity: `(a || b) == (b || a)`
    - Pass: `{stack: a, b} OP_2DUP OP_BOOLOR OP_SWAP OP_ROT OP_BOOLOR OP_NUMEQUAL`
- De Morgan's law: `!(a || b) == (!a && !b)`
    - Pass: `{stack: a, b} OP_2DUP OP_BOOLOR OP_NOT OP_ROT OP_NOT OP_ROT OP_NOT OP_BOOLAND OP_NUMEQUAL`
- Absorption: `(a && (a || b)) == (a != false)`
    - Pass: `{stack: a, b} OP_OVER OP_2 OP_PICK OP_ROT OP_BOOLOR OP_BOOLAND OP_SWAP OP_0 OP_NUMNOTEQUAL OP_NUMEQUAL`
- Associativity: `((a || b) || c) == (a || (b || c))`
    - Pass: `{stack: a, b, c} OP_2 OP_PICK OP_2 OP_PICK OP_BOOLOR OP_OVER OP_BOOLOR OP_2SWAP OP_3 OP_ROLL OP_BOOLOR OP_BOOLOR OP_NUMEQUAL`
- Distributivity: `((a && b) || c) == ((a || c) && (b || c))`
    - Pass: `{stack: a, b, c} OP_2 OP_PICK OP_2 OP_PICK OP_BOOLAND OP_OVER OP_BOOLOR OP_3 OP_ROLL OP_2 OP_PICK OP_BOOLOR OP_2SWAP OP_BOOLOR OP_BOOLAND OP_NUMEQUAL`

### OP_NUMEQUAL (0x9c)

- Reflexivity: `(a == a) == true`
    - Pass: `{stack: a} OP_DUP OP_NUMEQUAL OP_1 OP_NUMEQUAL`
- Commutativity: `(a == b) == (b == a)`
    - Pass: `{stack: a, b} OP_2DUP OP_NUMEQUAL OP_SWAP OP_ROT OP_NUMEQUAL OP_NUMEQUAL`
- Equivalence: `(a == b) == !((a < b) || (a > b))`
    - Pass: `{stack: a, b} OP_2DUP OP_NUMEQUAL OP_2 OP_PICK OP_2 OP_PICK OP_LESSTHAN OP_2SWAP OP_GREATERTHAN OP_BOOLOR OP_NOT OP_NUMEQUAL`

### OP_NUMEQUALVERIFY (0x9d)

- Reflexivity: `(a == a) == true`
    - Pass: `{stack: a} OP_DUP OP_NUMEQUALVERIFY OP_1 OP_1 OP_NUMEQUAL`
- Commutativity: `(a == b) == (b == a)`
    - Pass: `{stack: a, b} OP_2DUP OP_NUMEQUALVERIFY OP_1 OP_SWAP OP_ROT OP_NUMEQUALVERIFY OP_1 OP_NUMEQUAL`, where `a == b`
    - Fail: `{stack: a, b} OP_2DUP OP_NUMEQUALVERIFY OP_1 OP_SWAP OP_ROT OP_NUMEQUALVERIFY OP_1 OP_NUMEQUAL`, where `a != b` (must fail with `ScriptError::EVAL_FALSE` error)
- Equivalence: `(a == b) == !((a < b) || (a > b))`
    - Pass: `{stack: a, b} OP_2DUP OP_NUMEQUALVERIFY OP_1 OP_2 OP_PICK OP_2 OP_PICK OP_LESSTHAN OP_2SWAP OP_GREATERTHAN OP_BOOLOR OP_NOT OP_NUMEQUAL`, where `a == b`
    - Fail: `{stack: a, b} OP_2DUP OP_NUMEQUALVERIFY OP_1 OP_2 OP_PICK OP_2 OP_PICK OP_LESSTHAN OP_2SWAP OP_GREATERTHAN OP_BOOLOR OP_NOT OP_NUMEQUAL`, where `a != b` (must fail with `ScriptError::EVAL_FALSE` error)

### OP_NUMNOTEQUAL (0x9e)

- Reflexivity: `(a != a) == false`
    - Pass: `{stack: a} OP_DUP OP_NUMNOTEQUAL OP_0 OP_NUMEQUAL`
- Commutativity: `(a != b) == (b != a)`
    - Pass: `{stack: a, b} OP_2DUP OP_NUMNOTEQUAL OP_SWAP OP_ROT OP_NUMNOTEQUAL OP_NUMEQUAL`
- Equivalence: `(a != b) == ((a < b) || (a > b))`
    - Pass: `{stack: a, b} OP_2DUP OP_NUMNOTEQUAL OP_2 OP_PICK OP_2 OP_PICK OP_LESSTHAN OP_2SWAP OP_GREATERTHAN OP_BOOLOR OP_NUMEQUAL`

### OP_LESSTHAN (0x9f)

- Reflexivity: `(a < a) == false`
    - Pass: `{stack: a} OP_DUP OP_LESSTHAN OP_0 OP_NUMEQUAL`
- Anti-commutativity: `(a < b)` == `(-b < -a)`
    - Pass: `{stack: a, b} OP_2DUP OP_LESSTHAN OP_SWAP OP_NEGATE OP_ROT OP_NEGATE OP_LESSTHAN OP_NUMEQUAL`
- Equivalence: `(a < b) == !((a == b) || (a > b))`
    - Pass: `{stack: a, b} OP_2DUP OP_NUMNOTEQUAL OP_2 OP_PICK OP_2 OP_PICK OP_LESSTHAN OP_2SWAP OP_GREATERTHAN OP_BOOLOR OP_NUMEQUAL`
- Transitivity: `((a < c) && (a < b) && (b < c)) == ((a < b) && (b < c))`
    - Pass: `{stack: a, b, c} OP_2 OP_PICK OP_OVER OP_LESSTHAN OP_2OVER OP_LESSTHAN OP_BOOLAND OP_2 OP_PICK OP_2 OP_PICK OP_LESSTHAN OP_BOOLAND OP_3 OP_ROLL OP_3 OP_PICK OP_LESSTHAN OP_2SWAP OP_LESSTHAN OP_BOOLAND OP_NUMEQUAL`

### OP_GREATERTHAN (0xa0)

- Reflexivity: `(a > a) == false`
    - Pass: `{stack: a} OP_DUP OP_GREATERTHAN OP_0 OP_NUMEQUAL`
- Anti-commutativity: `(a > b) == (-b > -a)`
    - Pass: `{stack: a, b} OP_2DUP OP_GREATERTHAN OP_SWAP OP_NEGATE OP_ROT OP_NEGATE OP_GREATERTHAN OP_NUMEQUAL`
- Equivalence: `(a > b) == !((a == b) || (a < b))`
    - Pass: `{stack: a, b} OP_2DUP OP_NUMNOTEQUAL OP_2 OP_PICK OP_2 OP_PICK OP_GREATERTHAN OP_2SWAP OP_LESSTHAN OP_BOOLOR OP_NUMEQUAL`
- Transitivity: `((a > c) && (a > b) && (b > c)) == ((a > b) && (b > c))`
    - Pass: `{stack: a, b, c} OP_2 OP_PICK OP_OVER OP_GREATERTHAN OP_2OVER OP_GREATERTHAN OP_BOOLAND OP_2 OP_PICK OP_2 OP_PICK OP_GREATERTHAN OP_BOOLAND OP_3 OP_ROLL OP_3 OP_PICK OP_GREATERTHAN OP_2SWAP OP_GREATERTHAN OP_BOOLAND OP_NUMEQUAL`

### OP_LESSTHANOREQUAL (0xa1)

- Reflexivity: `(a <= a) == true`
    - Pass: `{stack: a} OP_DUP OP_LESSTHANOREQUAL OP_1 OP_NUMEQUAL`
- Anti-commutativity: `(a <= b) == (-b <= -a)`
    - Pass: `{stack: a, b} OP_2DUP OP_LESSTHANOREQUAL OP_SWAP OP_NEGATE OP_ROT OP_NEGATE OP_LESSTHANOREQUAL OP_NUMEQUAL`
- Equivalence: `(a <= b) == !(a > b)`
    - Pass: `{stack: a, b} OP_2DUP OP_LESSTHANOREQUAL OP_ROT OP_ROT OP_GREATERTHAN OP_NOT OP_NUMEQUAL`
- Transitivity: `((a <= c) && (a <= b) && (b <= c)) == ((a <= b) && (b <= c))`
    - Pass: `{stack: a, b, c} OP_2 OP_PICK OP_OVER OP_LESSTHANOREQUAL OP_2OVER OP_LESSTHANOREQUAL OP_BOOLAND OP_2 OP_PICK OP_2 OP_PICK OP_LESSTHANOREQUAL OP_BOOLAND OP_3 OP_ROLL OP_3 OP_PICK OP_LESSTHANOREQUAL OP_2SWAP OP_LESSTHANOREQUAL OP_BOOLAND OP_NUMEQUAL`

### OP_GREATERTHANOREQUAL (0xa2)

- Reflexivity: `(a >= a) == true`
    - Pass: `{stack: a} OP_DUP OP_GREATERTHANOREQUAL OP_1 OP_NUMEQUAL`
- Anti-commutativity: `(a >= b) == (-b >= -a)`
    - Pass: `{stack: a, b} OP_2DUP OP_GREATERTHANOREQUAL OP_SWAP OP_NEGATE OP_ROT OP_NEGATE OP_GREATERTHANOREQUAL OP_NUMEQUAL`
- Equivalence: `(a >= b) == !(a < b)`
    - Pass: `{stack: a, b} OP_2DUP OP_GREATERTHANOREQUAL OP_ROT OP_ROT OP_LESSTHAN OP_NOT OP_NUMEQUAL`
- Transitivity: `((a >= c) && (a >= b) && (b >= c)) == ((a >= b) && (b >= c))`
    - Pass: `{stack: a, b, c} OP_2 OP_PICK OP_OVER OP_GREATERTHANOREQUAL OP_2OVER OP_GREATERTHANOREQUAL OP_BOOLAND OP_2 OP_PICK OP_2 OP_PICK OP_GREATERTHANOREQUAL OP_BOOLAND OP_3 OP_ROLL OP_3 OP_PICK OP_GREATERTHANOREQUAL OP_2SWAP OP_GREATERTHANOREQUAL OP_BOOLAND OP_NUMEQUAL`

### OP_MIN (0xa3)

- Identity: `min(a, a) == a`
    - Pass: `{stack: a} OP_DUP OP_DUP OP_MIN OP_NUMEQUAL`
- Order: `(min(a, b) <= a && min(a, b) <= b) == true`
    - Pass: `{stack: a, b} OP_2DUP OP_MIN OP_2 OP_PICK OP_LESSTHANOREQUAL OP_ROT OP_2 OP_PICK OP_MIN OP_ROT OP_LESSTHANOREQUAL OP_BOOLAND OP_1 OP_NUMEQUAL`

### OP_MAX (0xa4)

- Identity: `max(a, a) == a`
    - Pass: `{stack: a} OP_DUP OP_DUP OP_MAX OP_NUMEQUAL`
- Order: `(max(a, b) >= a && max(a, b) >= b) == true`
    - Pass: `{stack: a, b} OP_2DUP OP_MAX OP_2 OP_PICK OP_GREATERTHANOREQUAL OP_ROT OP_2 OP_PICK OP_MAX OP_ROT OP_GREATERTHANOREQUAL OP_BOOLAND OP_1 OP_NUMEQUAL`

### OP_WITHIN (0xa5)

- Reflexivity: `within(a, a, a) == false`
    - Pass: `{stack: a} OP_DUP OP_DUP OP_WITHIN OP_0 OP_NUMEQUAL`
- Equivalence: `within(a, b, c) == (a >= b && a < c)`
    - Pass: `{stack: a, b, c} OP_3DUP OP_WITHIN OP_3 OP_PICK OP_3 OP_ROLL OP_GREATERTHANOREQUAL OP_2SWAP OP_LESSTHAN OP_BOOLAND OP_NUMEQUAL`
