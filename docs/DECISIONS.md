## 2026-07-05

- Dropped ANTLR4 for re2c + a hand-written recursive-descent parser - ANTLR4 doesn't support bounded repetition (`{0,7}`) cleanly, and wanted more direct control over the generated code
- New parser matches the original Lark grammar's AST exactly, quirks included (whitespace in label names, single-space char literal quotes, ternary-nested-in-relation, the inverted `indirect_lvalue(assignment(...))` shape) - normalizing these would mean touching every downstream consumer for no real gain
- No Pratt parser - `parser.cc` stays a flat, right-associative mirror of the old grammar, precedence gets fixed downstream in `Shunting_Yard` alone
- `Symbol_Table_Builder` replaces augur's side-effect-based symbol table, walking `AST_Node` post-order to match the Python transformer's exact timing - including copying a dead-code quirk (`"void"` key never actually set true in the original)

## 2026-07-07

- Renamed `Expression_Resolver` → `Datatype_Parser` → `Node_Parser` - settled on naming it for what it takes in (`Node`), the same relationship `Parser` has to tokens
- Found the real precedence bug: `Shunting_Yard` never actually flattened right-associative chains, because `Node_Parser::parse_from_node` wraps every compound node in one `Pointer` layer, hiding the `Relation` a naive check was looking for. Fixed by detecting a real chain link at exactly one `Pointer` deep, a parenthesized sub-expression at two
- Found and fixed a register-allocation bug in both the x86_64 and arm64 backends - an accumulator register was getting overwritten with a stale value instead of the real one, only surfacing once real chains started compiling correctly
- Added `--dump-queue` as a debug hook (one `queue_dump_stream` pointer in `ir/temporary.h`) instead of a new `--target`, to avoid re-implementing the symbol-table tracking `ir::emit` already does correctly
- Moved `clang-tidy` off `pocc`'s serial per-file pre-commit hook onto a parallel `tidy` CMake target with compile-command deduplication (shared lib+test sources were getting linted twice) - cut a ~20 minute lint run down to ~1-2 minutes
