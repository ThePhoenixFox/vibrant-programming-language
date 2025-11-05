# Vibrant Programming Language

Vibrant is a programming language that is designed to learn [LLVM](https://llvm.org/). 

## Features

Vibrant has these following features: 

- Strong Static Types\*
- Ahead of Time Compilation\*
- Multiparadigm\*
  - Procedural\*
  - Functional\*
  - Object Oriented\*
 
*\*These features have been scheduled for a future release of the Vibrant specification*

## Version Specification

The Vibrant language specification version is displayed in the following fashion: vM.m.s

`M` is the current version of the specification, included with every publication.

`m` is the current implementation version of the specification, included with every publication of this repository,  (starts at 1).

`s` is the current minor implementation version of the implementation, only included in this repository, (starts at 0). May have -rc`n` extention which indicates unreadiness, (starts at 1).

Examples: v1.1.0 v3.2.3 v2.2.1 v1.6.7 v2.1.0-rc1

The Vibrant language has specification guideline that updates on every **version** with the following changes allowed:

### Syntax
- Change existing syntax
- Add new syntax
- Deprecated previous syntax
### Standard Library
- Change existing specification on the Standard Library
- Add new functions, objects, and libaries to the Standard Library
- Deprecate previous functions, objects, and libaries from the Standard Library

## Build

Compiler Requirement

| Compiler | Minimum Version |
|----------|-----------------|
| Clang    | 5.0+ (10+ on macOS) |
| GCC      | 7.4+             |
| MSVC     | 2019 (16.8+)     |

*These are LLVM minimums, if you can't compile these sources files, you should not compile a project that depends on them.*

Project Standard Requirement

| Item     | Required Standard |
|----------|-------------------|
| C++      | C++20             |
| LLVM     | 21.1.0            |
| CMake    | 3.23              |

1. Clone the repository

`git clone <path> https://github.com/ThePhoenixFox/vibrant-language.git`

2. Install [LLVM](https://releases.llvm.org/)
3. Use [CMake](https://cmake.org/download/) for the Vibrant compiler,

```
cd <path>
mkdir build && cd build
cmake .. 
  -DVIBRANT_BUILD_STRICT=ON // Enables -Wall -Werror or /W4 /permissive- (Default: ON)
  -DVIVRANT_BUILD_OPTIONS="" // Extra build options you can put in like -fno-rtti (Default: None)
  -DCMAKE_BUILD_TYPE=RELEASE // Debug Release RelWithDebInfo MinSizeRel (Default: Release)
```

4. Build the Vibrant compiler

`cmake --build .`

5. Install the Vibrant compiler

This project uses GNU Install, the resulting binary will be called `vbc` and stored in the respective folder for your binary installations. 

`cmake --install .`

## Specification 

You can read the specification for Vibrant [here](https://ThePhoenixFox.github.io/Vibrant-Programming-Language/index.html). 

## License

LLVM's license is include in the root directory of this repository and is the license this repository uses for its own source code. You can read more about the specifics of the license [here](LICENSE.txt).

Any third party software included in this project will have a `NOTICE` file included for their respective licenses.

## Sources

These are documentation or articles that have helped in the development of Vibrant

[LLVM](https://llvm.org/)

[Crafting Interpreters](https://craftinginterpreters.com/)

[Creating the Bolt Compiler](https://mukulrathi.com/create-your-own-programming-language/llvm-ir-cpp-api-tutorial/)

