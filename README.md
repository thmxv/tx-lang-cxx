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

- Tests for all the error_token paths
- File name in runtime error
- Multi-line entry in REPL
- Fix error in REPL in entry that follows an error
- Support expression result in REPL
- Reserve keywords pub, with, type, Type, abstract, override, final
- Better float formatting: 1.0e20 instead of 1.e20. Test print/parse loop
- Check arity/signature of native fn
- Better and more conform error messages. compile and runtime
- Fix TODOs in the code
- Replace runtime errors by compile time (keep runtime error in degug)
- Go over all the tests one more time (Some do not make much sense for tx
  and some others do not test all code paths)

### To consider

- Sync GC and allocator, meaning the GC is always triggered
  before the pool allocator falls-back to the upstream allocator.
- Chapter 25 challenges
- Chapter 26 challenges
- Allow "var a = a;" in the case of shadowing and not assigning to self
- Option to set maximum recursion level, starting stack and frame array size, 
  and minimum memory consumption (do not round to next power of 2)
- Optimize instruction_prt access (chapter 24 challenge) + benchmark
- Allow function overload with diff arities (if possible)
- Universal Function Call (UFC)
- Add OP CODES for common constant values (0,1,2,-1,"")
- Clean up utf8 conv, utf8 lenght, resize once
- Loop/block labels with support in break and continue
- Allow to use break to break out of a block (using labels only)
- Better const correctness for container elements
- Use char8_t and u8string_view (needs libfmt support)

## Roadmap

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
- [X] Tests
- [X] Closure
- [X] Garbage collection
- [ ] Backlog/FIXMEs/TODOs
- [ ] CI, coverage, fuzzing, ...

### v0.2.0

- [ ] Type checking
- [ ] Documentation

### v0.3.0

- [ ] Struct and instances
- [ ] Methods and initializer
- [ ] Traits
- [ ] Inheritance?
- [ ] String concatenation
- [ ] String interpolation
- [ ] Array and Map values (generic types)
- [ ] Tuples (need variadic generic types)
- [ ] Range based for loop
- [ ] Pattern matching
- [ ] Documentation

### v0.4.0

- [ ] Modules
- [ ] Optimizations
- [ ] Documentation

## Minimum target for realease candidate

- [ ] Final syntax/grammar
- [ ] Reserve all future keywords
- [ ] Benchmarks
- [ ] Editors support (LSP)
- [ ] Modules for core and std
- [ ] Type alias
- [ ] Recursive types
- [ ] Documentation

## Later 1.0 versions

- [ ] Syntax highlighting and completion in REPL
- [ ] Fibers
- [ ] Threads
- [ ] Async and await
- [ ] Add 'with' statement
- [ ] Package/dependency manager
- [ ] Tools (debugger, ...)
- [ ] Virtual env

## v2.0.0

- [ ] Refactor instruction set/Switch to register based VM
- [ ] Better GC

