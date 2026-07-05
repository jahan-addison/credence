# Language Frontend

Initially, the source-to-AST step was an entirely separate older Python project, [augur](https://github.com/jahan-addison/augur), built on [Lark](https://github.com/lark-parser/lark) and embedded into credence at runtime via pybind11 (the `"python"` `--ast-loader` path in [`main.cc`](/credence/main.cc)). This folder is a native C++ replacement for that step: a re2c-generated lexer and a hand-written recursive-descent parser that produce the exact same JSON AST shape as augur's [`transformer.py`](https://github.com/jahan-addison/augur/blob/master/augur/transformer.py). So that the rest of the frontend - [`resolver.h`](/credence/language/resolver.h), [`shunting_yard.h`](/credence/language/shunting_yard.h), and the [IR](/credence/ir/README.md) - did not need to change to consume it.

The lexer, datatype, resolver, and shunting-yard pieces are comparatively straightforward once you understand the parser's construction.

## Lexer

A lexer built with the lexer-generator re2c from [`lexer.re`](/credence/language/lexer.re) into `lexer.cc` (see `Token`, `Token_Type` in [`lexer.h`](/credence/language/lexer.h)).

## Parser

The original `grammar.lark` is a flat LALR(1) grammar with _no real operator precedence at the parse-tree level_. Lark's default parser resolves every `rvalue OP rvalue` the same way no matter which operator is involved, so the resulting tree is always right-associative:

```C
main() { auto x, a, b, c; x = a * b + c; }
```

parses as `a * (b + c)`, not `(a * b) + c` - the `*` ends up as the *outer* node simply because it was seen first, not because it binds tighter than `+`.

The shunting-yard `Shunting_Yard` in [`shunting_yard.h`](/credence/language/shunting_yard.h) corrects precedence downstream, by walking the resolved `Datatype::Expression` tree against the real precedence table in [`operators.h`](/credence/language/operators.h) rather than trusting the parse tree's shape. So the parser in `parser.cc` reproduces the original grammar's flat, right-associative structure instead of a fixed C-precedence tree.

### The ternary

A ternary immediately following a binary operator binds *tighter* than that operator, nesting inside its right operand instead of wrapping it.

```C
main() { auto x; x = 5 < 4 ? 10 : 1; }
```

```json
{
  "node": "relation_expression",
  "root": ["<"],
  "left": { "node": "integer_literal", "root": 5 },
  "right": {
    "node": "ternary_expression",
    "root": { "node": "integer_literal", "root": 4 },
    "left": { "node": "integer_literal", "root": 10 },
    "right": { "node": "integer_literal", "root": 1 }
  }
}
```

Semantically this is `(5 < 4) ? 10 : 1`, but structurally the `4` that completes the `<` comparison ends up as the ternary node's `root`, not as the relation's own `right` operand. This isn't a design choice - it's what a 1-token-lookahead parser does when it sees `?` before it's reduced. `<`. [`resolver.h`](/credence/language/resolver.h)'s `from_relation_expression_node` already has a branch that unwinds exactly this shape back into `(condition) ? then : else`.

### Other Weirdness

* A label with whitespace before its `:` (`loop :`) keeps that whitespace as part of its name. The original grammar lexed a label as a single regex token (`NAME [\s]* ":"`) and built its name by slicing off only the trailing `:`, so whatever whitespace preceded it survives.
* A char literal that is exactly one whitespace character (`' '`) keeps its surrounding quotes in `root`; every other char literal strips them. The original grammar had a second, redundant-looking rule matching a quoted space that never went through the same quote-stripping path as the general case.
* Consecutive expression-statements (`x = 1; y = 2;`) merge into a single `rvalue_statement` node rather than two separate statements, because `expression+` is embedded inside the `rvalue_statement` rule itself, not at the statement-list level above it.

## Datatype

[`datatype.h`](/credence/language/datatype.h) is the internal `(Value : Type : Size)` tuple that passes beyond the AST use:

```
(10:int:4)
("hello":string:5)
(55.5:float:4)
('c':char:1)
```

`Datatype::Type` is the algebraic sum of every expression shape the rest of the compiler cares about: `Literal`, `Array`, `Symbol`, `Unary`, `Relation`, `Function`, `LValue`.

## Resolver

[`resolver.h`](/credence/language/resolver.h)'s `Expression_Resolver` walks an `AST_Node` - built by `parser.cc`, or previously by augur - and resolves it against a symbol table into a `Datatype` tree. It's the same walk augur's transformer performed, except it also checks that every lvalue was actually declared with `auto` or `extrn` along the way, instead of trusting the source blindly.

## Shunting Yard

[`shunting_yard.h`](/credence/language/shunting_yard.h) is where operator precedence is actually fixed. It's an extension of the shunting-yard algorithm - **a stack-based virtual machine** - that takes the resolved `Datatype::Expression` tree and re-linearizes it into a queue ordered by the real precedence table in `operators.h`, extended to also handle function calls and their parameters. This is the step that makes the parser's flat, unordered tree above fixed: by the time code generation sees an expression, it's already in the right order, regardless of how the parse tree happened to nest it.

## Example

```C
add(a, b) {
  return(a + b);
}
```

```json
{
  "node": "program",
  "root": "definitions",
  "left": [
    {
      "node": "function_definition",
      "root": "add",
      "left": [
        { "node": "lvalue", "root": "a" },
        { "node": "lvalue", "root": "b" }
      ],
      "right": {
        "node": "statement",
        "root": "block",
        "left": [
          {
            "node": "statement",
            "root": "return",
            "left": [
              {
                "node": "relation_expression",
                "root": ["+"],
                "left": { "node": "lvalue", "root": "a" },
                "right": { "node": "lvalue", "root": "b" }
              }
            ]
          }
        ]
      }
    }
  ]
}
```
