# The Tx programming language

<!-- [![ci](https://github.com/thmxv/tx-lang/actions/workflows/ci.yml/badge.svg)](https://github.com/thmxv/tx-lang) -->
<!-- [![codecov](https://codecov.io/gh/thmxv/tx-lang/branch/main/graph/badge.svg)](https://codecov.io/gh/thmxv/tx-lang) -->
<!-- [![Language grade: C++](https://img.shields.io/lgtm/grade/cpp/github/thmxv/tx-lang)](https://lgtm.com/projects/g/thmxv/tx-lang/context:cpp) -->
<!-- [![CodeQL](https://github.com/thmxv/tx-lang/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/thmxv/tx-lang/actions/workflows/codeql-analysis.yml) -->

## About Tx

Tx is inspired by Lua. The code base and implementation is inspired by the 
book "Crafting Interpreters" by Robert Nystrom. The syntax and feature set
are inspired by languages such as Python, Rust and Swift among others.

Tx design desitions are supposed to be oriented towards safety, speed, power 
and minimalism.

The code base is written, as much as possible, in modern C++. Trying to be 
almost completely constexpr.

## Disclaimer

Tx is still verry much a work in progress.

## Backlog

- clean up utf8 conv, utf8 lenght, resize once
- String concatenation
- String interpolation
- Tests
- Use gsl::narrow_cast where necessary
- Ensure enough stack space
- Add OP CODES for common constant values (0,1,2,-1,"")

### To consider
- Fix 'could be const' warnings with containers when elements are const
- constexpr (needs support from stdlib for allocation, codecvt and from_char)
- use char8_t and u8string_view (needs libfmt support)

## Roadmap/TODO list

- [X] Bytecode array
- [X] Interpret bytecode
- [X] Scanning tokens
- [X] Parsing/compiling expressions
- [X] Value types
- [X] String
- [X] Hash table
- [ ] Global variables
- [ ] Local variables
- [ ] Type checking
- [ ] Jumping
- [ ] Functions and call
- [ ] Closure
- [ ] Garbage collection
- [ ] Struct and instances
- [ ] Methods and initializer
- [ ] Inheritance
- [ ] Optimization

## Minimum target for realease candidate

- [ ] Array and Map values (generic types)
- [ ] Tuples (need variadic generic types)
- [ ] Traits
- [ ] Pattern matching
- [ ] Type checker
- [ ] Modules
- [ ] Final syntax/grammar
- [ ] Resereve all future keywords

## Later versions
- [ ] Fibers
- [ ] Threads
- [ ] Async and await

