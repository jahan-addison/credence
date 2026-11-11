# Brocoli for VS Code

Syntax highlighting for [Brocoli](https://github.com/jahan-addison/Brocoli), the language of the credence compiler, in `.b` source files.

* Union definitions and their tags, union declarations, member access, and `is` expressions
* The standard library and the syscalls of each target, highlighted apart from program functions
* `printf` format specifiers and escape sequences in strings
* `float` and `double` literals, e.g. `5.55f`, and C style constant literals

## Installation

```bash
npm run package
code --install-extension brocoli.vsix
```

`npm run package` uses [`vsce`](https://github.com/microsoft/vscode-vsce), installed with `npm install -g @vscode/vsce` or `brew install vsce`.

## Grammar Changes

The grammar is [`syntaxes/brocoli.tmLanguage.json`](syntaxes/brocoli.tmLanguage.json). After a change to it, or to [`language-configuration.json`](language-configuration.json), package and install the extension again, then run **Developer: Reload Window** in VS Code:

```bash
npm run package
code --install-extension brocoli.vsix
```

**Note**: **Developer: Inspect Editor Tokens and Scopes** shows the scopes of the token under the cursor, to check a change against a theme.

## Syscalls

The syscall names are read from the Linux and Darwin tables in [`syscall.h`](/credence/target/common/syscall.h). After a syscall is added to the compiler, run the following, and package the extension again:

```bash
npm run syscalls
```
