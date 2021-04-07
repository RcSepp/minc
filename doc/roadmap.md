# Roadmap

## Free memory

All memory leaks will be fixed before the release of Minc 1.0.

## Add thread-safety

Currently, Minc is not thread-safe. *The release version of Minc will lock individual AST branches during execution.*

Currently, block execution modifies a few variables in the `MincBlockExpr` class (variables marked as mutable). This prohibits concurrent execution of blocks.
*For Minc 1.0 runtime state of `MincBlockExpr` will be persisted in the `MincRuntime` class.*

## Develop parser-free resolver

Static parsers enforce major limitations on to the flexibility of Minc. Replacing it with a parser-free expression resolver is an important step towards unleashing the full potential of Minc. Minc 1.0 will likely still ship with static parsers.