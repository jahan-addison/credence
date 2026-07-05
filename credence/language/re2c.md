### Re2c


#### Basic usage

1. Create a source file with a `/*!re2c ... */` block wherever you want a scan point. Inside it: `name = pattern;` definitions, then `pattern_or_name { action }` rules.
2. re2c matches the *longest* rule at the current position ("maximal munch"). Two rules matching the same length are broken by declaration order, not by which pattern is "more specific."
3. Configure how re2c talks to your surrounding code with `re2c:define:X = Y;` directives, most commonly `YYCTYPE` (character type) and `YYCURSOR`/`YYMARKER`/`YYLIMIT` (the scan cursor, backtrack marker, and end-of-buffer pointer).
4. Generate with `re2c -o output.cc input.re`, then compile the *output*. Never hand-edit generated code - see the warning at the top of `lexer.re`.

#### Example from `lexer.re`:

```re2c
name = [a-zA-Z_][a-zA-Z0-9_.]{0,7};

"true"          { return make_token(Token_Type::BOOL_LITERAL); }
"false"         { return make_token(Token_Type::BOOL_LITERAL); }
...
name            { return make_token(Token_Type::IDENTIFIER); }
```

`"true"` and `name` both match the input `true` at the same length (4 characters), so this is exactly the tie case from step 2 above: re2c resolves it by picking whichever rule was declared first. Keywords are listed before the identifier rule for that reason, not for readability - reverse the order and every keyword silently becomes a plain `IDENTIFIER` token instead.

Two more things worth carrying keeping in mind:

- `{0,7}` above is a genuine bounded-repetition quantifier - re2c is a real regex engine. This is *not* true of every "regex-like" grammar tool: ANTLR4, for instance, only has `?`/`*`/`+` and silently parses a bare `{...}` as an embedded action block instead, which fails in a confusing way rather than a clear one.
- This lexer loads the whole source into one buffer up front and disables re2c's refill mechanism (`re2c:yyfill:enable = 0;`), padding the buffer by `YYMAXFILL` bytes (from `/*!max:re2c*/`) so no rule can ever read past the end. For streaming input too large to buffer, re2c instead generates calls to a `YYFILL` function you provide - a different, still-common configuration this project doesn't need.