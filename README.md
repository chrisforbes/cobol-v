# COBOL-V
## Version 1.0 - "The Anachronism"

COBOL-V (Common Business-Oriented Language, for Vulkan) is a high-performance, anachronistic dialect of COBOL designed for the development of Vulkan SPIR-V compute and graphics shaders. It provides the rigorous structure of 1960s enterprise software engineering with the parallel processing capabilities of modern GPU architectures.

---

## Building:
- Prerequisites: CMake, A C++20 compiler, Vulkan SDK (for spirv-tools), Git
    - On Windows, we build a minimal 'amber' as part of the cmake build.
    - On Linux, please ensure that you have a 2026.x version of 'amber', 'spirv-dis', and 'spirv-val' in your PATH. Older versions may cause issues with some edge cases in SPIRV.
- `cmake -B build -DCMAKE_BUILD_TYPE=Debug`
- `cmake --build build --config Debug`


## Basic Example:
Here is an example of a simple compute shader that sums two vectors:

```cobol
IDENTIFICATION DIVISION.
PROGRAM-ID. VECTOR-SUM.
ENVIRONMENT DIVISION.
CONFIGURATION SECTION.
OBJECT-COMPUTER. VULKAN-COMPUTE-SHADER.
DATA DIVISION.
LOCAL-STORAGE SECTION.
01  VECTOR-A   PIC V(4) VALUE (1.0, 2.0, 3.0, 4.0).
01  VECTOR-B   PIC V(4) VALUE (5.0, 6.0, 7.0, 8.0).
01  VECTOR-R   PIC V(4) VALUE (0.0, 0.0, 0.0, 0.0).
PROCEDURE DIVISION.
    MAIN-LOGIC.
        COMPUTE VECTOR-R = VECTOR-A + VECTOR-B.
        GOBACK.
```

Many other small sample programs can be found in `tests/cases/*.cob`.

For more details of the full language supported, see `docs/language_spec.md`.

## Usage:
```bash
./cobolv [-g] [--dump-ast] [--dump-symbols] <input file> [output file]
```

- `-g`: Optional. Enables emission of full debug information using the modern `NonSemantic.Shader.DebugInfo.100` instruction set (with embedded source code, compilation units, scopes, and lines) alongside basic reflection names (`OpString`, `OpName`, `OpMemberName`). This allows graphics debuggers (like RenderDoc) to map GPU disassembly back to your original COBOL source code for high-level step-by-step debugging and state inspection.
- `--dump-ast`: Prints the Abstract Syntax Tree (AST) to standard output after parsing.
- `--dump-symbols`: Prints the symbol table (variables, resources, and types) to standard output after semantic analysis.

The compiler writes binary .spv files, ready to be consumed by a Vulkan application. If the output file name is omitted, it defaults to `output.spv` in the current directory.

The target SPIR-V dialect is SPIRV 1.3 for the Vulkan 1.1 environment. Some features (relaxed block layouts) will require a Vulkan implementation supporting version 1.2, or the relaxed layout extensions.