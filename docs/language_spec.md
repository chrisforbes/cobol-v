# COBOL-V Language Reference Manual

This document describes the COBOL language dialect accepted by COBOL-V. The language is intentionally minimal, and does not attempt to be a full COBOL.

---

## 1. Division Structure

A COBOL-V source program is organized into four mandatory divisions, ensuring modularity and compliance with the Large-Scale Graphics Processing Standards.

### 1.1 IDENTIFICATION DIVISION
Provides metadata regarding the program and its origin.
```cobol
IDENTIFICATION DIVISION.
PROGRAM-ID. BRIGHTNESS-FILTER.
```

### 1.2 ENVIRONMENT DIVISION
Defines the mapping between COBOL logic and GPU hardware resources.

#### CONFIGURATION SECTION
- **OBJECT-COMPUTER**: Specifies the target shader stage.
  - `VULKAN-COMPUTE-SHADER` (Default)
  - `VULKAN-VERTEX-SHADER`
  - `VULKAN-FRAGMENT-SHADER`
  - *Note: Geometry and Tessellation stages are explicitly unsupported as they are considered "Bad Ideas" for modern hardware performance.*
- **LOCAL-SIZE IS (X, Y, Z)**: Sets the workgroup dimensions (LocalSize) for compute shaders.
- **SPECIAL-NAMES**: Maps hardware registers to COBOL identifiers.
  
| COBOL Name | GLSL Equivalent | Description |
|:---|:---|:---|
| `GPU-GLOBAL-ID` | `gl_GlobalInvocationID` | Global coordinates of the thread. |
| `GPU-LOCAL-ID` | `gl_LocalInvocationID` | Coordinates within the workgroup. |
| `GPU-WORKGROUP-ID` | `gl_WorkGroupID` | ID of the current workgroup. |
| `GPU-NUM-WORKGROUPS` | `gl_NumWorkGroups` | Total number of workgroups. |
| `GPU-LOCAL-INDEX` | `gl_LocalInvocationIndex` | Flattened index within workgroup. |
| `GPU-WORKGROUP-SIZE` | `gl_WorkGroupSize` | Dimensions of the workgroup. |
| `GPU-SUBGROUP-ID` | `gl_SubgroupId` | ID of the current subgroup. |
| `GPU-SUBGROUP-SIZE` | `gl_SubgroupSize` | Number of threads in a subgroup. |
| `GPU-SUBGROUP-INVOCATION-ID` | `gl_SubgroupInvocationID` | Index within the subgroup. |
| `GPU-NUM-SUBGROUPS` | `gl_NumSubgroups` | Number of subgroups in the workgroup. |
| `GPU-SUBGROUP-EQ-MASK` | `gl_SubgroupEqMask` | Subgroup mask (Equal). |
| `GPU-SUBGROUP-GE-MASK` | `gl_SubgroupGeMask` | Subgroup mask (Greater/Equal). |
| `GPU-SUBGROUP-GT-MASK` | `gl_SubgroupGtMask` | Subgroup mask (Greater). |
| `GPU-SUBGROUP-LE-MASK` | `gl_SubgroupLeMask` | Subgroup mask (Less/Equal). |
| `GPU-SUBGROUP-LT-MASK` | `gl_SubgroupLtMask` | Subgroup mask (Less). |
| `GPU-POSITION` | `gl_Position` | Vertex output position (Vertex) or input fragment position (Fragment). |
| `GPU-POINT-SIZE` | `gl_PointSize` | Point size decoration (Vertex). |
| `GPU-VERTEX-INDEX` | `gl_VertexIndex` | Index of the current vertex (Vertex). |
| `GPU-INSTANCE-INDEX` | `gl_InstanceIndex` | Index of the current instance (Vertex). |
| `GPU-DRAW-ID` | `gl_DrawID` | Index of the draw call (Vertex). |
| `GPU-BASE-VERTEX` | `gl_BaseVertex` | Base vertex value (Vertex). |
| `GPU-BASE-INSTANCE` | `gl_BaseInstance` | Base instance value (Vertex). |
| `GPU-FRAG-COORD` | `gl_FragCoord` | Coordinates of the fragment (Fragment). |
| `GPU-FRAG-DEPTH` | `gl_FragDepth` | Depth value of the fragment (Fragment). |
| `GPU-FRONT-FACING` | `gl_FrontFacing` | Whether the fragment is front-facing (Fragment). |
| `GPU-POINT-COORD` | `gl_PointCoord` | Coordinate within a point (Fragment). |
| `GPU-SAMPLE-ID` | `gl_SampleID` | ID of the current sample (Fragment). |
| `GPU-SAMPLE-POSITION` | `gl_SamplePosition` | Position of the current sample (Fragment). |
| `GPU-SAMPLE-MASK-IN` | `gl_SampleMaskIn` | Input sample mask (Fragment). Wrapped in array[1] in SPIR-V. |
| `GPU-SAMPLE-MASK` | `gl_SampleMask` | Output sample mask (Fragment). Wrapped in array[1] in SPIR-V. |
| `GPU-CLIP-DISTANCE` | `gl_ClipDistance` | Array of distances to user-defined clip planes (Vertex Output, Fragment Input). |
| `GPU-CULL-DISTANCE` | `gl_CullDistance` | Array of distances to user-defined cull planes (Vertex Output, Fragment Input). |
| `GPU-HELPER-INVOCATION` | `gl_HelperInvocation` | True if the current fragment is a helper invocation (Fragment). |

> [!NOTE]
> Built-in variables mapped in `SPECIAL-NAMES` must be declared in the `DATA DIVISION` with the `IS BUILT-IN` clause. 

### 1.2.1 Special Built-in Handling
To ensure Vulkan compliance and SPIR-V validity, the COBOL-V compiler performs the following automatic transformations:

- **Automatic Flat Interpolation**: In the `VULKAN-FRAGMENT-SHADER` stage, any `INPUT` variable of type integer or unsigned integer (including built-ins like `GPU-SAMPLE-ID`) is automatically decorated with `OpDecorate Flat`.
- **Sample Mask and Distance Wrapping**: The `GPU-SAMPLE-MASK-IN`, `GPU-SAMPLE-MASK`, `GPU-CLIP-DISTANCE`, and `GPU-CULL-DISTANCE` variables are surfaced as scalar `PIC I` or `PIC V` types in COBOL by default. If declared without an `OCCURS` clause, they are automatically emitted as single-element arrays in SPIR-V to satisfy Vulkan requirements. 

**Stage Validation:** Hardware registers are validated against the current `OBJECT-COMPUTER` stage. Using a compute-only built-in (e.g., `GPU-GLOBAL-ID`) in a graphics stage, or vice-versa, will result in a semantic error. 

**Interface Validation:** Built-ins must be declared in the appropriate interface section (`INPUT SECTION` vs `OUTPUT SECTION`) or the `WORKING-STORAGE` / `LINKAGE` sections. For example, `GPU-VERTEX-INDEX` must be an input.

In `COMPUTE` shaders, all built-in variables are **read-only**. In graphics stages, output built-ins (such as `GPU-POSITION`) are writable.

### 1.2.2 Screen Space Derivatives
Screen space derivatives are available in the `VULKAN-FRAGMENT-SHADER` stage via the `CALL` statement. These instructions calculate the rate of change of a variable relative to screen coordinates.

**Syntax:**
```cobol
CALL "DERIVATIVE-X" USING [source] GIVING [dest].
CALL "DERIVATIVE-Y" USING [source] GIVING [dest].
CALL "TOTAL-DERIVATIVE" USING [source] GIVING [dest].
```

**Variants:**
The compiler also supports fine and coarse variants which map to `OpDPdxFine`, `OpDPdyCoarse`, etc.:
- `DERIVATIVE-X-FINE`, `DERIVATIVE-Y-FINE`, `TOTAL-DERIVATIVE-FINE`
- `DERIVATIVE-X-COARSE`, `DERIVATIVE-Y-COARSE`, `TOTAL-DERIVATIVE-COARSE`

**Requirements:**
- Must be used in the `VULKAN-FRAGMENT-SHADER` stage.
- `source` and `dest` must be floating-point types (`FLOAT`, `V(2)`, `V(3)`, or `V(4)`).
- Fine/Coarse variants require the `DerivativeControl` SPIR-V capability (automatically enabled).

#### INPUT-OUTPUT SECTION
Used for mapping GPU descriptors to COBOL "Files" via the **FILE-CONTROL** paragraph.

**SELECT Statement Syntax:**
`SELECT [file-name] ASSIGN TO "[descriptor-target]" ORGANIZATION IS [type] [ACCESS MODE IS [mode]].`

- **descriptor-target**: A string in the format `"GPU-[IMAGE|SAMPLER|BUFFER]-[SET]-[BINDING]"`.
- **ORGANIZATION types**:
  - `IMAGE-1D`, `IMAGE-2D`, `IMAGE-3D`, `IMAGE-CUBE`, `IMAGE-1D-ARRAY`, `IMAGE-2D-ARRAY`, `IMAGE-CUBE-ARRAY`: Combined Image Sampler (`VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`).
  - `TEXTURE-1D`, `TEXTURE-2D`, `TEXTURE-3D`, `TEXTURE-CUBE`, `TEXTURE-1D-ARRAY`, `TEXTURE-2D-ARRAY`, `TEXTURE-CUBE-ARRAY`: Pure Texture (`VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE`).
  - `SAMPLER`: Pure Sampler (`VK_DESCRIPTOR_TYPE_SAMPLER`).
  - `STORAGE-1D`, `STORAGE-2D`, `STORAGE-3D`, `STORAGE-CUBE`, `STORAGE-1D-ARRAY`, `STORAGE-2D-ARRAY`, `STORAGE-CUBE-ARRAY`: Random-access image (`VK_DESCRIPTOR_TYPE_STORAGE_IMAGE`).
  - `ACCESS-PUSH`: Push Constants. Passes small amounts of data directly to the shader. All `FD` records are grouped into a single SPIR-V `PushConstant` block.
  - `ACCESS-UNIFORM`: Uniform Buffer (`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`). Read-only structured data block. Always uses `STD140` layout. The compiler automatically enforces strict alignment (e.g., 16-byte alignment for arrays) and emits `ArrayStride` decorations.
  - `ACCESS-STORAGE`: Storage Buffer (`VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`). Read-write structured data block. Defaults to `STD430` layout; `STD140` and `SCALAR` are also valid.
- **ACCESS MODE**:
  - `READ-ONLY`: Emits `NonWritable` decoration.
  - `WRITE-ONLY`: Emits `NonReadable` decoration.
  - `READ-WRITE`: Default for storage images.

### 1.3 DATA DIVISION
Defines the memory layout and data types required for GPU execution.

#### FILE SECTION
Defines the structure and metadata for resources declared in the `INPUT-OUTPUT SECTION`.

**FD (File Description) Entry:**
```cobol
FD  [file-name]
    [FORMAT IS [image-format]]
    [LAYOUT IS [STD140 | STD430 | SCALAR]]
    [USAGE IS [COHERENT | VOLATILE]].
01  [record-name] PIC [sampled-type].
```
- **FORMAT IS**: Required for `STORAGE-*` images if `Unknown` is not intended. Supports `RGBA32F`, `R32F`, `RGBA8`, `R32I`, `R32UI`, `RGBA16`, etc.
- **LAYOUT IS**: Specifies the memory layout standard for buffer blocks (`ACCESS-UNIFORM` or `ACCESS-STORAGE`).
  - `STD140`: Mandated for `ACCESS-UNIFORM` blocks. Also valid for `ACCESS-STORAGE`. Scalars 4-byte aligned; `vec2` 8-byte; `vec3`/`vec4` 16-byte. **Note**: Array elements and nested structures are always rounded up to 16-byte alignment slots, and the compiler automatically emits the `ArrayStride 16` decoration for scalar/vector arrays.
  - `STD430`: Default for `ACCESS-STORAGE` blocks. More compact: `vec2` is 8-byte aligned, `vec3` is 16-byte aligned (size 12). Arrays of scalars are tightly packed (stride 4).
  - `SCALAR`: Only valid for `ACCESS-STORAGE` blocks. Most compact: all types align to 4 bytes. A `{float, vec3}` struct uses offsets 0 and 4 (vs 0 and 16 under STD430/STD140). Requires `VK_EXT_scalar_block_layout` (Vulkan 1.2+ core feature) on the device.
  - If omitted, `ACCESS-UNIFORM` defaults to `STD140` and `ACCESS-STORAGE` defaults to `STD430`.
- **PIC Clause**: Determines the sampled type (e.g., `PIC V(4)` for `float4`).

#### WORKING-STORAGE SECTION
Defines variables stored in per-invocation memory (SPIR-V `Private` storage class).

#### LOCAL-STORAGE SECTION
Defines variables shared across a workgroup (SPIR-V `Workgroup` storage class). Variables in this section **cannot** have initial values (`VALUE` clause).

#### INPUT SECTION
Defines vertex attributes or varying inputs for graphics stages. Items in this section must include a `LOCATION` attribute.

#### OUTPUT SECTION
Defines varying outputs for graphics stages. Items in this section must include a `LOCATION` attribute or be mapped to a built-in.

**Data Item Format:**
`[level-number] [data-name] PIC [picture-string] [LOCATION [id]] [INTERPOLATION IS [FLAT | NOPERSPECTIVE | SMOOTH]] [VALUE literal] [IS BUILT-IN].`

- **LOCATION [id]**: Mandatory for non-built-in items in `INPUT` and `OUTPUT` sections. Maps the variable to a specific hardware location index.
- **INTERPOLATION IS**: Specifies the interpolation qualifier for fragment shader inputs. 
  - `FLAT`: No interpolation.
  - `NOPERSPECTIVE`: Linear interpolation in screen space.
  - `SMOOTH`: Perspective-correct linear interpolation (Default).
  Interpolation qualifiers are only valid on `INPUT SECTION` variables within the `VULKAN-FRAGMENT-SHADER` stage.

**Picture Strings (The Type System):**
COBOL-V does away with all of the existing picture string formats you might be familiar with from other COBOLs. Instead, it uses a clean and simple system that maps directly to SPIR-V:

| Picture String | GLSL Type Equivalent | Description |
|:---|:---|:---|
| `PIC FLOAT` | `float` | 32-bit floating point scalar |
| `PIC I` | `int` | 32-bit signed integer |
| `PIC U` | `uint` | 32-bit unsigned integer |
| `PIC B` | `bool` | Boolean scalar |
| `PIC V(N)` | `vecN` | Float vector of size N (2-4) |
| `PIC IV(N)` | `ivecN` | Integer vector of size N (2-4) |
| `PIC UV(N)` | `uvecN` | Unsigned int vector of size N (2-4) |
| `PIC BV(N)` | `bvecN` | Boolean vector of size N (2-4) |
| `PIC M(R, C)` | `matRxC` | Matrix with R rows and C columns |

#### LINKAGE SECTION
Used for **BASED** records, typically mapped to Buffer Device Address (BDA) pointers or Descriptor Sets.

### 1.4 PROCEDURE DIVISION
Contains the executable logic of the shader.

**Structure Requirements:**
- All statements must be contained within a named **paragraph** or **section**.
- "Loose" statements directly under the `PROCEDURE DIVISION` header are not permitted and will result in a parser error.
- Paragraph names must end with a period (`.`).

**Example:**
```cobol
PROCEDURE DIVISION.
    MAIN-LOGIC.
        MOVE 1.0 TO BRIGHTNESS.
        GOBACK.
```

---

## 2. Expressions and Swizzling

COBOL-V utilizes the `OF` qualification syntax for both vector component addressing and record member access, maintaining the English-like readability of the language.

### 2.1 Qualification
When accessing members of a record (such as those in a `PushConstant` block), use the `OF` keyword.
```cobol
MOVE THRESHOLD OF SETTINGS-DATA TO LOCAL-LIMIT.
```

### 2.1 Component Keywords
*   **Coordinates**: `X`, `Y`, `Z`, `W`
*   **Colors**: `RED`, `GREEN`, `BLUE`, `ALPHA` (Aliases for X, Y, Z, W)

### 2.2 Swizzle Syntax
Components are listed as a sequence before the `OF` keyword.
```cobol
MOVE X Y OF POSITION TO VELOCITY.
MOVE 1.0 TO ALPHA OF FRAG-COLOR.
```
*   **Extraction**: Accessing one or more components (e.g., `X Y OF foo`) extracts those components into a scalar or sub-vector.
*   **Masked Assignment**: Assigning to a swizzle (e.g., `MOVE source TO X Y OF dest`) updates only the specified components, leaving others unchanged.

### 2.3 Vector Literals
Vectors can be initialized or constructed inline using parentheses and comma-separated values.
```cobol
01 V1 PIC V(3) VALUE (1.0, 2.0, 3.0).
MOVE (0.0, 0.0, 0.0, 1.0) TO COLOR.
```

### 2.4 Array Subscripting
Array elements are accessed using parentheses following the identifier. 

> [!IMPORTANT]
> COBOL-V uses **0-based indexing** for all array subscripts, aligning with SPIR-V and GLSL conventions. This is a departure from traditional 1-based COBOL indexing.

```cobol
MOVE DATA(0) TO RESULT. *> Accesses the first element
MOVE MEMBER(2) OF BLOCK-RECORD TO LOCAL-V. *> Accesses third element of an array member
MOVE 3.14 TO ARRAY-MEMBER(1) OF STORAGE-BUFFER. *> Writes to second element of a storage buffer array
```

#### 2.4.1 Subscript Qualification
When an array is a member of a record, the subscript must follow the member name directly for unambiguous parsing:
`[member-name] ([subscript]) OF [record-name]`

**Example:**
`MOVE ARRAY-DATA (5) OF UNIFORM-BLOCK TO TARGET-VAR.`

### 2.5 Relational and Logical Expressions
COBOL-V supports standard relational and logical operators for use in `COMPUTE` statements and future control flow.

**Relational Operators:**
- `=` or `EQUAL TO`: Equality
- `<>` or `NOT EQUAL TO`: Inequality
- `>` or `GREATER THAN`: Greater than
- `<` or `LESS THAN`: Less than
- `>=` or `GREATER THAN OR EQUAL TO`: Greater than or equal to
- `<=` or `LESS THAN OR EQUAL TO`: Less than or equal to

**Logical Operators:**
- `AND`: Logical Conjunction
- `OR`: Logical Disjunction
- `NOT`: Logical Negation

**Expression Precedence:**
1.  **Bitwise Unary**: `BIT-NOT`
2.  **Arithmetic**: `+`, `-`, `*`, `/`
3.  **Bitwise Shift**: `BIT-LSHIFT`, `BIT-RSHIFT`
4.  **Bitwise Binary**: `BIT-AND`, `BIT-OR`, `BIT-XOR`
5.  **Relational**: Comparison operators (`EQUALS`, `GREATER`, etc.)
6.  **Logical NOT**: `NOT`
7.  **Logical AND**: `AND`
8.  **Logical OR**: `OR`

### 2.6 Explicit Type Conversions
COBOL-V requires explicit conversion when assigning between incompatible base types (e.g., Float to Integer). These operators perform element-wise conversion for vector types.

| Operator | Source | Target | Opcode |
|:---|:---|:---|:---|
| `FLOAT-TO-INT(x)` | Float | Int | `OpConvertFToS` |
| `FLOAT-TO-UINT(x)` | Float | Uint | `OpConvertFToU` |
| `INT-TO-FLOAT(x)` | Int | Float | `OpConvertSToF` |
| `UINT-TO-FLOAT(x)` | Uint | Float | `OpConvertUToF` |
| `INT-TO-UINT(x)` | Int | Uint | `OpBitcast` |
| `UINT-TO-INT(x)` | Uint | Int | `OpBitcast` |

**Example:**
```cobol
COMPUTE MY-INT = FLOAT-TO-INT(MY-FLOAT).
COMPUTE MY-V3FLOAT = INT-TO-FLOAT(MY-V3INT).
```

---

## 3. Core Statements

### 3.1 MOVE and COMPUTE
The primary means of data transfer and arithmetic.
- `MOVE [expression] TO [variable | swizzle].`
- `COMPUTE [variable | swizzle] = [expression].`
  - Supports arithmetic, relational, and logical expressions.
  - When a relational/logical expression is assigned, the target must be a boolean type (`PIC B` or `PIC BV`).
  - Supports advanced linear algebra operations via the `*` operator (Matrix * Vector, Vector * Matrix, and Matrix * Matrix multiplications).
  - Both `COMPUTE` and `MOVE` may assign to output built-in variables (such as `GPU-POSITION`).

### 3.2 READ Statement
Used for texture sampling and storage image random-access reading.
- **Combined Sampler**: `READ [image-name] AT [coords] [WITH PROJECTION] [COMPARING WITH [ref-val]] [WITH GRADIENTS ([dx], [dy])] [AT LOD [lod-expr]] INTO [target].`
- **Split Sampler**: `READ [texture-name] WITH [sampler-name] AT [coords] [WITH PROJECTION] [COMPARING WITH [ref-val]] [WITH GRADIENTS ([dx], [dy])] [AT LOD [lod-expr]] INTO [target].`
- **FETCH (texelFetch)**: `READ [image-name] FETCH AT [int-coords] [AT LOD [lod-expr]] INTO [target].`
  - Skips filtering and uses integer coordinates. Supports combined samplers (auto-extracts image portion).
- **GATHER (textureGather)**: `READ [resource-name] GATHER [COMPONENT [comp-expr]] AT [coords] INTO [target].`
  - Gathers 4 texels from the specified component (default 0). Target must be a `vec4` (`PIC V(4)`).

The optional clauses (`WITH PROJECTION`, `COMPARING WITH`, `WITH GRADIENTS`, `AT LOD`, `WITH BIAS`) may appear in any order after the coordinate.

The `WITH PROJECTION` clause enables **projective texture sampling** (equivalent to `textureProj` in GLSL). The final component of the coordinate vector is used as the homogeneous divisor, enabling perspective-correct sampling from rasterized clip-space coordinates.
- The coordinate vector must be one dimension larger than the image's base dimensionality: `vec3` for 2D images, `vec4` for 3D images.
- `WITH PROJECTION` is **not supported** for `CUBE` images.
- `WITH PROJECTION` is compatible with `COMPARING WITH` (emits `OpImageSampleProjDrefImplicitLod`/`ExplicitLod`), `AT LOD`, `WITH BIAS`, and `WITH GRADIENTS`.

The `COMPARING WITH` clause enables depth-reference sampling (shadow mapping). Unlike GLSL, COBOL-V does not use separate sampler types for comparison; it is specified at the call site. The reference value must be a scalar float, and the `INTO` target must also be a scalar float.

The `WITH GRADIENTS` clause allows providing explicit x and y derivative vectors for LOD calculation (equivalent to `textureGrad` in GLSL). The dimensionality of the gradients must match the image's base dimensionality (e.g., `vec2` for 2D, `scalar` for 1D Array).

The `WITH BIAS` clause allows specifying an LOD bias for implicit sampling operations.
- **Requirements**: Must be used in the `VULKAN-FRAGMENT-SHADER` stage.
- **Exclusivity**: Cannot be used with `AT LOD` or `WITH GRADIENTS`.
- **Type**: Requires a scalar float expression.

The `AT LOD` clause is optional. Its behavior when omitted depends on the shader stage:
- **VULKAN-FRAGMENT-SHADER**: Uses **implicit LOD** selection based on screen-space derivatives.
- Other stages: Defaults to an explicit **LOD 0**.

If the `AT LOD` clause is provided, it specifies an explicit Level of Detail. It requires a scalar float expression for sampled images (`READ`, `READ WITH`) or a scalar integer for storage images (`READ` on storage or `FETCH`).

**Note**: The `WITH GRADIENTS`, `AT LOD`, and `WITH BIAS` clauses are mutually exclusive and cannot be used in the same `READ` statement.

### 3.3 WRITE Statement
Used for random-access writes to storage images.
- `WRITE [image-name] FROM [source] AT [coords].`

### 3.4 GOBACK Statement
Indicates the completion of execution. Maps to SPIR-V `OpReturn`.

### 3.5 DISCARD Statement
Immediately terminates the current fragment invocation.
- **Requirements**: Must be used in the `VULKAN-FRAGMENT-SHADER` stage.
- **Effect**: The fragment is discarded; no values are written to the framebuffer. Maps to SPIR-V `OpKill`.

### 3.6 INTERPOLATE Statement
Performs custom interpolation of fragment shader input variables. This statement maps to the GLSL `interpolateAt*` family of functions.

- **Syntax**: `INTERPOLATE [interpolant] AT [CENTROID | SAMPLE [index] | OFFSET [vec2]] GIVING [target].`
- **Variants**:
  - **CENTROID**: Interpolates the variable at the centroid of the fragment.
  - **SAMPLE**: Interpolates the variable at a specific sample location (`index` must be a scalar integer).
  - **OFFSET**: Interpolates the variable with a sub-pixel offset (`vec2` must be a `FLOATV2`).
- **Requirements**:
  - Must be used in the `VULKAN-FRAGMENT-SHADER` stage.
  - `interpolant` must be a variable declared in the `INPUT SECTION`.
  - `target` must be compatible with the interpolant's type.
- **Capability**: Automatically enables the `InterpolationFunction` SPIR-V capability.

### 3.7 SYNC Statement
Used for workgroup-level synchronization.
- **SYNC WORKGROUP**: Emits a SPIR-V `OpControlBarrier` with workgroup scope and appropriate memory semantics to ensure all invocations in the workgroup reach the barrier and all memory accesses are visible.

### 3.7 IF Statement
Conditional branching based on boolean expressions. Supports optional `ELSE` and nested blocks.
- **Syntax**:
  ```cobol
  IF [condition]
      [statements...]
  [ELSE
      [statements...]]
  END-IF.
  ```
- **Note**: The `END-IF` terminator is strongly recommended, though traditional COBOL periods (`.`) can also terminate an active `IF` block.
- **Example**:
  ```cobol
  IF A < B
      MOVE 10.0 TO RES
  ELSE
      MOVE 20.0 TO RES
  END-IF.
  ```

### 3.8 PERFORM Statement
Structured loop construct for repetitive execution. Supports count-based, condition-based, and iterator-based loops.

#### 3.8.1 TIMES Variant
Executes a block of statements a fixed number of times.
- **Syntax**:
  ```cobol
  PERFORM [expression] TIMES
      [statements...]
  END-PERFORM.
  ```
- **Example**:
  ```cobol
  PERFORM 10 TIMES
      COMPUTE SUM = SUM + 1.0
  END-PERFORM.
  ```

#### 3.8.2 UNTIL Variant
Executes a block of statements while a condition is false (executes "until" true).
- **Syntax**:
  ```cobol
  PERFORM UNTIL [condition]
      [statements...]
  END-PERFORM.
  ```

#### 3.8.3 VARYING Variant
Executes a block of statements while incrementing an iterator variable.
- **Syntax**:
  ```cobol
  PERFORM VARYING [var] FROM [start] BY [step] UNTIL [condition]
      [statements...]
  END-PERFORM.
  ```
- **Example**:
  ```cobol
  PERFORM VARYING I FROM 0 BY 1 UNTIL I >= 10
      MOVE 0.0 TO DATA(I)
  END-PERFORM.
  ```

---

## 4. Documentation and Comments

COBOL-V supports both traditional and modern commenting styles.

### 4.1 Traditional Comments
Any line with an asterisk (`*`) in the first column.
```cobol
* THIS IS A TRADITIONAL COMMENT LINE
```

### 4.2 Inline Comments
The `*>` sequence introduces an inline comment.
```cobol
MOVE 1.0 TO BRIGHTNESS. *> Set default exposure
```

---

## 5. Specialized GPU Operations

### 5.1 CONSULT COHORT
Subgroup (Cohort) operations allow for high-performance parallel reductions and queries across GPU invocations in the same subgroup. These operations require SPIR-V 1.3+.

#### 5.1.1 Reductions and Scans
Performs a parallel reduction or scan across the cohort.
```cobol
CONSULT COHORT FOR [OP] [MODE] OF [expression] GIVING [target].
```
- **Operations ([OP])**: `SUM`, `PRODUCT`, `MIN`, `MAX`, `AND`, `OR`, `XOR`.
- **Modes ([MODE])** (Optional):
  - `REDUCE` (Default): Single result across the cohort.
  - `INCLUSIVE-SCAN`: Prefix sum including current invocation.
  - `EXCLUSIVE-SCAN`: Prefix sum excluding current invocation.

#### 5.1.2 Subgroup Queries
Queries state or broadcasts values across the cohort.
- **Election**: `CONSULT COHORT FOR ELECT GIVING [target].` (True for exactly one invocation).
- **Vote**: `CONSULT COHORT FOR [ANY|ALL] OF [condition] GIVING [target].`
- **Broadcast**:
  - `CONSULT COHORT FOR BROADCAST [expression] FROM [invocation-id] GIVING [target].`
  - `CONSULT COHORT FOR BROADCAST-FIRST [expression] GIVING [target].`
- **Ballot**: `CONSULT COHORT FOR BALLOT [condition] GIVING [target-pic-uv4].`


### 5.2 Atomic Operations
Atomic memory operations for Storage Images and variables in Storage Buffers. All operations support an optional `GIVING` clause to capture the original value.

- **Targeting**:
  - For **Storage Images**, the `AT [coords]` clause is mandatory.
  - For **Storage Buffer variables**, the `AT [coords]` clause must be omitted.

- **Arithmetic**:
  - `ATOMIC-ADD [expression] TO [target] [AT [coords]] [GIVING [result]].`
  - `ATOMIC-SUB [expression] FROM [target] [AT [coords]] [GIVING [result]].`
- **Bitwise**:
  - `ATOMIC-AND [expression] WITH [target] [AT [coords]] [GIVING [result]].`
  - `ATOMIC-OR [expression] WITH [target] [AT [coords]] [GIVING [result]].`
  - `ATOMIC-XOR [expression] WITH [target] [AT [coords]] [GIVING [result]].`
- **Comparison**:
  - `ATOMIC-MIN [expression] WITH [target] [AT [coords]] [GIVING [result]].`
  - `ATOMIC-MAX [expression] WITH [target] [AT [coords]] [GIVING [result]].`
- **Exchange**:
  - `ATOMIC-EXCHANGE [expression] WITH [target] [AT [coords]] [GIVING [result]].`
  - `ATOMIC-COMPARE-EXCHANGE [value] AND [comparator] WITH [target] [AT [coords]] [GIVING [result]].`

### 5.3 Resource Inquiries
Queries metadata regarding GPU resources.

#### 5.3.1 Image Dimensions and Mipmaps
Queries the dimensions or mipmap levels of an image or texture.
```cobol
INQUIRE [resource-name] FOR [SIZE | LEVELS] [AT LOD [lod-expr]] GIVING [target].
```
- **SIZE**:
  - `1D`: Scalar `PIC I`.
  - `2D / Cube / 1D Array`: Vector `PIC IV(2)`.
  - `3D / 2D Array / Cube Array`: Vector `PIC IV(3)`.
  - `AT LOD`: Optional for sampled images.
- **LEVELS**:
  - Returns a scalar `PIC I` representing the total number of mipmap levels.
  - Does not support `AT LOD`.

---

## 6. Implementation Status

The COBOL-V compiler currently supports:
- **Architecture**: IDENTIFICATION, ENVIRONMENT, DATA, and PROCEDURE divisions.
- **Resource Management**: Fully functional `FILE-CONTROL` and `FD` system for images, samplers, and buffers.
- **I/O**: Procedural `READ` (with `FETCH` and `GATHER` extensions), `WRITE`, and full suite of **Image Atomic Operations**.
- **Interpolation**: Support for custom fragment interpolation via the `INTERPOLATE` statement (`CENTROID`, `SAMPLE`, and `OFFSET`).
- **Inquiry**: `INQUIRE ... FOR SIZE` and `INQUIRE ... FOR LEVELS` for resource metadata.
- **Types**: Scalars, Vectors (V, IV, UV, BV), and Matrices (M).
- **Logic**: `MOVE`, `COMPUTE`, `DISCARD`, **conditional branching** (`IF`), and **structured loops** (`PERFORM` with TIMES, UNTIL, and VARYING).
- **Expressions**: Arithmetic, **Relational** (EQUAL, GREATER, etc.), **Logical** (AND, OR, NOT), **Bitwise** (`BIT-AND`, `BIT-OR`, etc.), and **Explicit Conversions** (`FLOAT-TO-INT`, etc.).
- **Qualification**: Support for `member OF parent` for record and push constant access.
- **Meta**: Workgroup size configuration, Built-in mappings, and **Push Constants / Uniform Blocks / Storage Buffers**.
- **Shared Memory**: `LOCAL-STORAGE SECTION` for workgroup-shared variables and `SYNC WORKGROUP` for barrier synchronization.
- **Graphics Stages**: Support for **Vertex and Fragment Shaders** including `INPUT SECTION`, `OUTPUT SECTION`, and explicit `LOCATION` attributes.
- **Arrays**: Full support for fixed-size arrays via the `OCCURS n TIMES` clause, including automatic SPIR-V type generation, ArrayStride decorations, and bounds checking.
- **Diagnostics**: The compiler includes an integrated AST dumper reachable via the `--dump-ast` flag and **detailed semantic type-mismatch reporting**.
- **Descope**: Multisampled textures (MS) are not planned for implementation.
