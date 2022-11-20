# The Tx programming language

<!-- [![ci](https://github.com/thmxv/tx-lang/actions/workflows/ci.yml/badge.svg)](https://github.com/thmxv/tx-lang) -->
<!-- [![codecov](https://codecov.io/gh/thmxv/tx-lang/branch/main/graph/badge.svg)](https://codecov.io/gh/thmxv/tx-lang) -->
<!-- [![Language grade: C++](https://img.shields.io/lgtm/grade/cpp/github/thmxv/tx-lang)](https://lgtm.com/projects/g/thmxv/tx-lang/context:cpp) -->
<!-- [![CodeQL](https://github.com/thmxv/tx-lang/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/thmxv/tx-lang/actions/workflows/codeql-analysis.yml) -->

## About Tx

Tx aims to be oriented towards safety, speed, power and minimalism.

Tx is inspired by Lua. The code and implementation is based on the 
book "Crafting Interpreters" by Robert Nystrom. The syntax and feature set
are inspired by languages such as Python, Rust, Wren and Swift among others.

The code is written, as much as possible, in modern C++. Trying to be 
almost completely constexpr.

## Disclaimer

Tx is still very much a work in progress.

## Documentation

- [Main documentation page](./doc/index.md)

## Backlog

- Fix TODOs in the code
- Tests for: if expr, fn expr
- Better float formatting: 1.0e20 instead of 1.e20
- Print name of unknowm variale in error
- check arity/signature of native fn
- error when assigning to constant golbal
- forbid global redefinition, unless specifically specified (REPL)
- allow "var a = a;" in the case of shadowing and not assigning to self
- replace runtime errors by compile time (keep runtime error in degug and REPL)
- better and more conform error messages. compile and runtime
- Ensure enough stack space
- Go over all the tests one more time

### To consider

- Optimize instruction_prt access (chapter 24 challenge) + benchmark
- Allow function overload with diff arities (if possible)
- Universal Function Call (UFC)
- Add OP CODES for common constant values (0,1,2,-1,"")
- Clean up utf8 conv, utf8 lenght, resize once
- Loop/block labels with support in break and continue
- Allow to use break to break out of a block (using labels only)
- Fix 'could be const' warnings with containers when elements are const
- Use char8_t and u8string_view (needs libfmt support)

## Roadmap/TODO list

### v0.1.0

- [X] Bytecode array
- [X] Interpret bytecode
- [X] Scanning tokens
- [X] Parsing/compiling expressions
- [X] Value types
- [X] String
- [X] Hash table
- [X] Global variables
- [X] Local variables
- [X] Jumping
- [X] Functions and call
- [ ] Tests
- [ ] Closure
- [ ] Garbage collection
- [ ] Documentation
- [ ] Backlog

### v0.2.0

- [ ] Struct and instances
- [ ] Methods and initializer
- [ ] Type checking
- [ ] Traits
- [ ] Inheritance
- [ ] String concatenation
- [ ] String interpolation
- [ ] Array and Map values (generic types)
- [ ] Tuples (need variadic generic types)
- [ ] Range based for loop
- [ ] Pattern matching
- [ ] Documentation

### v0.3.0

- [ ] Modules
- [ ] Optimizations
- [ ] Documentation

## Minimum target for realease candidate

- [ ] Final syntax/grammar
- [ ] Reserve all future keywords
- [ ] Benchmarks
- [ ] Editors support
- [ ] Modules for core and std
- [ ] Type alias
- [ ] Recursive types
- [ ] Documentation

## Later versions

- [ ] Syntax highlighting and completion in REPL
- [ ] Fibers
- [ ] Threads
- [ ] Async and await
- [ ] Package/dependency manager
- [ ] Tools
- [ ] Virtual env

