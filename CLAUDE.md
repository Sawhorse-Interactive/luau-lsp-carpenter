## Project Overview

Luau Language Server (luau-lsp) is an implementation of the Language Server Protocol (LSP) for the Luau programming language. It provides IDE features like diagnostics, autocompletion, hover, go-to-definition, etc. The project also includes a standalone CLI (`luau-lsp analyze`) for CI type-checking and linting.

## Memories

Always read these memory files at the start of a conversation. After reading each one, output `[MEMORY_NAME]` (e.g. `[MEMORY.md]`, `[building.md]`) to confirm it was loaded.

- [.claude/memory/MEMORY.md](.claude/memory/MEMORY.md) - Index of user preferences and key patterns
- [.claude/memory/building.md](.claude/memory/building.md) - How to build and test
- [.claude/memory/shared-feature.md](.claude/memory/shared-feature.md) - The `shared("FileName")` module resolution feature

## Architecture

### Core Components

- **LanguageServer** (`src/LanguageServer.cpp`, `src/include/LSP/LanguageServer.hpp`): Main LSP message dispatcher. Handles JSON-RPC requests/notifications and routes to appropriate handlers.

- **WorkspaceFolder** (`src/Workspace.cpp`, `src/include/LSP/Workspace.hpp`): Represents a workspace folder. Contains the Luau `Frontend` for type checking. Implements all LSP operations (completion, hover, diagnostics, etc.).

- **WorkspaceFileResolver** (`src/WorkspaceFileResolver.cpp`): Implements Luau's `FileResolver` interface. Handles file reading, module resolution, and configuration loading.

- **LSPPlatform** (`src/include/Platform/LSPPlatform.hpp`): Base class for platform-specific behavior. Factory method `getPlatform()` returns either the base implementation or `RobloxPlatform`.

- **RobloxPlatform** (`src/include/Platform/RobloxPlatform.hpp`): Extends `LSPPlatform` with Roblox-specific features - sourcemap parsing, DataModel types, service auto-imports, Color3/BrickColor handling.

### LSP Operations

Located in `src/operations/`:

- Each file implements a specific LSP feature (Completion, Hover, GotoDefinition, References, Rename, etc.)
- Operations are methods on `WorkspaceFolder` that take LSP params and return LSP results

### Transport Layer

`src/transport/`: Handles JSON-RPC communication

- `StdioTransport`: Standard input/output for primary usage
- `PipeTransport`: Named pipe for alternative IDE integration

### Protocol Types

`src/include/Protocol/`: LSP protocol structures with nlohmann/json serialization

### External Dependencies

Located in `extern/` and `luau/`:

- `luau/`: Luau compiler and type checker (submodule)
- `extern/json/`: nlohmann/json for JSON handling
- `extern/glob/`: Glob pattern matching
- `extern/argparse/`: CLI argument parsing
- `extern/toml/`: TOML parsing
- `extern/doctest/`: Test framework

## Code Style

- C++17 standard
- Uses Allman brace style (configured in `.clang-format`)
- 4-space indentation, no tabs
- 150 column limit
- Luau code uses StyLua for formatting

## Committing Changes

- Do NOT add `Co-Authored-By: Claude ...` lines to commit messages

## Testing Patterns

Tests use the `Fixture` class from `tests/Fixture.h` with doctest's `TEST_CASE_FIXTURE`:

```cpp
TEST_CASE_FIXTURE(Fixture, "FeatureName")
{
    auto uri = newDocument("test.luau", "local x = 1");
    // Test operations using workspace
}
```

- `newDocument()`: Create and register a test document
- `check()`: Type check source code
- `loadDefinition()`: Load type definition files
- `loadSourcemap()`: Load Rojo sourcemap for Roblox tests
- `sourceWithMarker()`: Parse source with `|` cursor position marker

### Testing with the New Type Solver

When writing tests that require the new Luau type solver (`LuauSolverV2`), use the `ENABLE_NEW_SOLVER()` macro at the start of the test:

```cpp
TEST_CASE_FIXTURE(Fixture, "feature_requiring_new_solver")
{
    ENABLE_NEW_SOLVER();

    auto uri = newDocument("test.luau", "local x = 1");
    // Test code...
}
```

**Important:** Do not use `ScopedFastFlag{FFlag::LuauSolverV2, true}` directly. The Frontend caches the solver mode at construction time, so the `ENABLE_NEW_SOLVER()` macro is required to properly update both the FFlag and the Frontend's cached solver mode.

## Key CMake Targets

- `Luau.LanguageServer`: Static library containing LSP implementation
- `Luau.LanguageServer.CLI`: Executable (`luau-lsp`)
- `Luau.LanguageServer.Test`: Test executable

## CMake Options

- `LUAU_ENABLE_TIME_TRACE`: Enable Luau time tracing
- `LSP_BUILD_ASAN`: Build with AddressSanitizer
- `LSP_STATIC_CRT`: Link with static CRT on Windows
- `LSP_BUILD_WITH_SENTRY`: Enable crash reporting (Windows/macOS only)
- `LSP_WERROR`: Treat warnings as errors (default: ON)
