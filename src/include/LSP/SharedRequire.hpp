#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Luau/Ast.h"
#include "Luau/FileResolver.h"
#include "Luau/GlobalTypes.h"

#include "LSP/Uri.hpp"

struct WorkspaceFileResolver;

/// Support for the fork-custom `shared("ModuleName")` string require.
///
/// Sawhorse Roblox frameworks give the `shared` global a `__call` metamethod, making
/// `shared("Foo")` equivalent to requiring the module whose file is named `Foo.luau`.
/// This namespace makes the language server resolve it exactly as it resolves `require`.
///
/// The dependency-graph half of the feature lives in the luau submodule fork
/// (`Analysis/include/Luau/RequireLikeGlobals.h`, branch `carpenter`). Everything else is here.
namespace LSP::SharedRequire
{

/// The global name treated as a string require. Must stay in sync with
/// `Luau::isRequireLikeGlobal` in the luau fork; if the two disagree, `shared()` calls
/// will resolve for type checking but never enter the module dependency graph.
inline constexpr const char* kGlobalName = "shared";

/// Returns the string literal argument of a `shared("Name")` call, if `call` is one.
/// Calls with a non-literal argument are deliberately not matched: there is nothing to resolve.
std::optional<Luau::AstExprConstantString*> matchSharedCall(const Luau::AstExprCall& call);

enum class ResolveStatus
{
    Found,
    NotFound,
    Ambiguous,
};

struct ResolveResult
{
    ResolveStatus status = ResolveStatus::NotFound;
    Luau::ModuleName moduleName{};
    /// Populated only when status == Ambiguous: every candidate that tied for nearest.
    std::vector<Luau::ModuleName> candidates{};
};

/// Case-insensitive index of the workspace's Luau files, keyed by file stem.
///
/// Entries hold a Uri rather than a resolved ModuleName. Module names change every time a
/// Rojo sourcemap is loaded (real path -> virtual `game/...` path), whereas the Uri never
/// does. Resolving names lazily at query time means the index does not have to be rebuilt
/// on sourcemap updates, which removes a whole class of stale-resolution bugs.
class Index
{
public:
    void clear();

    /// Indexes a single `.lua`/`.luau` file. Ignores anything else. Idempotent per Uri.
    void add(const Uri& uri, const Uri& rootUri);
    void remove(const Uri& uri, const Uri& rootUri);

    [[nodiscard]] size_t fileCount() const;

    /// `name` is a bare stem ("Foo") or a partial path ("jobs/Foo"), case-insensitive.
    /// When several files match, the one sharing the longest leading path with
    /// `requiringModule` wins; a genuine tie reports Ambiguous.
    [[nodiscard]] ResolveResult resolve(
        const std::string& name, const Luau::ModuleName& requiringModule, const WorkspaceFileResolver& fileResolver) const;

private:
    struct Entry
    {
        Uri uri;
        /// Root-relative, extension-stripped, forward-slashed, lowercased. Used for
        /// partial-path matching. Deliberately independent of any sourcemap.
        std::string relativePath;
    };

    /// Lowercased file stem -> files carrying that stem.
    std::unordered_map<std::string, std::vector<Entry>> entries;
};

/// Binds `shared` to a type that is both callable (a string require, via Luau's own
/// MagicRequire) and indexable (the Roblox `shared` table).
///
/// Must run *after* definition files are loaded: `globalTypes.d.luau` declares
/// `shared: any`, which would otherwise clobber this binding.
void registerGlobal(Luau::GlobalTypes& globals);

} // namespace LSP::SharedRequire
