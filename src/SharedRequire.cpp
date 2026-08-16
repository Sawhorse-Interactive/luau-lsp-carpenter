#include "LSP/SharedRequire.hpp"

#include <algorithm>
#include <cctype>
#include <memory>

#include "Luau/BuiltinDefinitions.h"
#include "Luau/Type.h"
#include "Luau/TypeArena.h"

#include "LSP/WorkspaceFileResolver.hpp"


namespace LSP::SharedRequire
{

static std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
    return value;
}

static bool isLuauExtension(const std::string& extension)
{
    return extension == ".lua" || extension == ".luau";
}

static bool isPathSeparator(char c)
{
    return c == '/' || c == '\\';
}

/// Number of leading segments shared by two paths. Module names are '/'-separated virtual
/// paths when a sourcemap is loaded and OS file paths otherwise, so both separators count.
static size_t commonSegmentCount(const std::string& left, const std::string& right)
{
    size_t count = 0;
    size_t leftPos = 0;
    size_t rightPos = 0;

    const auto findSeparator = [](const std::string& value, size_t from)
    {
        for (size_t i = from; i < value.size(); ++i)
            if (isPathSeparator(value[i]))
                return i;
        return std::string::npos;
    };

    while (leftPos <= left.size() && rightPos <= right.size())
    {
        auto leftEnd = findSeparator(left, leftPos);
        auto rightEnd = findSeparator(right, rightPos);

        auto leftSegment = left.substr(leftPos, leftEnd == std::string::npos ? std::string::npos : leftEnd - leftPos);
        auto rightSegment = right.substr(rightPos, rightEnd == std::string::npos ? std::string::npos : rightEnd - rightPos);

        if (leftSegment != rightSegment)
            break;

        count++;

        if (leftEnd == std::string::npos || rightEnd == std::string::npos)
            break;

        leftPos = leftEnd + 1;
        rightPos = rightEnd + 1;
    }

    return count;
}

/// True when `path` ends with `suffix` on a '/' boundary (or is exactly `suffix`).
static bool endsWithPathSuffix(const std::string& path, const std::string& suffix)
{
    if (path == suffix)
        return true;
    if (path.size() <= suffix.size())
        return false;
    return path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0 && path[path.size() - suffix.size() - 1] == '/';
}

/// The index key (lowercased stem) and root-relative path for a file, applying Rojo's
/// `init.luau` folding: `foo/init.luau` is the module named `foo`, not `init`.
static std::optional<std::pair<std::string, std::string>> indexKeyFor(const Uri& uri, const Uri& rootUri)
{
    auto extension = uri.extension();
    if (!isLuauExtension(extension))
        return std::nullopt;

    auto filename = uri.filename();
    if (filename.size() <= extension.size())
        return std::nullopt;
    auto stem = filename.substr(0, filename.size() - extension.size());

    // Root-relative path, extension stripped.
    std::string relativePath = uri.path;
    const std::string& rootPath = rootUri.path;
    if (!rootPath.empty() && relativePath.size() > rootPath.size() && relativePath.compare(0, rootPath.size(), rootPath) == 0)
    {
        relativePath = relativePath.substr(rootPath.size());
        if (!relativePath.empty() && relativePath.front() == '/')
            relativePath.erase(relativePath.begin());
    }
    relativePath = relativePath.substr(0, relativePath.size() - extension.size());

    if (toLower(stem) == "init")
    {
        // Rojo collapses `dir/init.luau` into an instance named `dir`.
        auto parent = uri.parent();
        if (!parent)
            return std::nullopt;
        stem = parent->filename();

        auto lastSlash = relativePath.find_last_of('/');
        relativePath = lastSlash == std::string::npos ? std::string{} : relativePath.substr(0, lastSlash);
    }

    if (stem.empty())
        return std::nullopt;

    return std::make_pair(toLower(stem), toLower(relativePath));
}

std::optional<Luau::AstExprConstantString*> matchSharedCall(const Luau::AstExprCall& call)
{
    if (call.args.size != 1)
        return std::nullopt;

    const auto* global = call.func->as<Luau::AstExprGlobal>();
    if (!global || global->name != kGlobalName)
        return std::nullopt;

    if (auto* literal = call.args.data[0]->as<Luau::AstExprConstantString>())
        return literal;

    return std::nullopt;
}

void Index::clear()
{
    entries.clear();
}

size_t Index::fileCount() const
{
    size_t count = 0;
    for (const auto& [_, bucket] : entries)
        count += bucket.size();
    return count;
}

void Index::add(const Uri& uri, const Uri& rootUri)
{
    auto key = indexKeyFor(uri, rootUri);
    if (!key)
        return;

    auto& bucket = entries[key->first];
    for (const auto& entry : bucket)
        if (entry.uri == uri)
            return;

    bucket.push_back(Entry{uri, key->second});
}

void Index::remove(const Uri& uri, const Uri& rootUri)
{
    auto key = indexKeyFor(uri, rootUri);
    if (!key)
        return;

    auto it = entries.find(key->first);
    if (it == entries.end())
        return;

    auto& bucket = it->second;
    bucket.erase(std::remove_if(bucket.begin(), bucket.end(),
                     [&uri](const Entry& entry)
                     {
                         return entry.uri == uri;
                     }),
        bucket.end());

    if (bucket.empty())
        entries.erase(it);
}

ResolveResult Index::resolve(const std::string& name, const Luau::ModuleName& requiringModule, const WorkspaceFileResolver& fileResolver) const
{
    if (name.empty())
        return {};

    std::string needle = toLower(name);
    std::replace(needle.begin(), needle.end(), '\\', '/');

    // Tolerate a written-out extension (`shared("Foo.luau")`).
    for (const char* extension : {".luau", ".lua"})
    {
        auto length = std::char_traits<char>::length(extension);
        if (needle.size() > length && needle.compare(needle.size() - length, length, extension) == 0)
        {
            needle = needle.substr(0, needle.size() - length);
            break;
        }
    }

    if (needle.empty())
        return {};

    auto lastSlash = needle.find_last_of('/');
    auto stem = lastSlash == std::string::npos ? needle : needle.substr(lastSlash + 1);

    auto it = entries.find(stem);
    if (it == entries.end())
        return {ResolveStatus::NotFound, {}, {}};

    std::vector<Luau::ModuleName> matches;
    for (const auto& entry : it->second)
    {
        if (lastSlash != std::string::npos && !endsWithPathSuffix(entry.relativePath, needle))
            continue;

        auto moduleName = fileResolver.getModuleName(entry.uri);
        if (moduleName == requiringModule)
            continue; // never resolve a module to itself

        matches.push_back(std::move(moduleName));
    }

    if (matches.empty())
        return {ResolveStatus::NotFound, {}, {}};

    if (matches.size() == 1)
        return {ResolveStatus::Found, matches.front(), {}};

    // Prefer the candidate sharing the longest leading path with the requiring module. For a
    // loaded sourcemap these are virtual `game/...` paths, so this is instance-tree proximity;
    // without one they are file paths, so it is directory proximity. A same-folder sibling
    // always wins outright.
    size_t bestScore = 0;
    std::vector<Luau::ModuleName> best;
    for (const auto& candidate : matches)
    {
        auto score = commonSegmentCount(candidate, requiringModule);
        if (score > bestScore)
        {
            bestScore = score;
            best.clear();
        }
        if (score == bestScore)
            best.push_back(candidate);
    }

    if (best.size() == 1)
        return {ResolveStatus::Found, best.front(), {}};

    std::sort(matches.begin(), matches.end());
    return {ResolveStatus::Ambiguous, {}, std::move(matches)};
}

void registerGlobal(Luau::GlobalTypes& globals)
{
    Luau::TypeArena& arena = globals.globalTypes;
    const Luau::TypeId stringType = globals.builtinTypes->stringType;
    const Luau::TypeId anyType = globals.builtinTypes->anyType;

    // The callable half: `(moduleName: string) -> any`, driven by Luau's own MagicRequire.
    // MagicRequire is not specific to the `require` global; it resolves whatever ModuleInfo
    // the RequireTracer recorded for the call site, which the luau fork populates for
    // `shared()` too. Reusing it means there is no bespoke magic function to keep in sync.
    Luau::FunctionType callType{arena.addTypePack({stringType}), arena.addTypePack({anyType})};
    callType.argNames.emplace_back(Luau::FunctionArgument{"moduleName", {}});
    const Luau::TypeId callTypeId = arena.addType(std::move(callType));
    Luau::attachMagicFunction(callTypeId, std::make_shared<Luau::MagicRequire>());

    // The table half: Roblox's `shared` is a plain shared table, so `shared.foo` must keep
    // type checking. Upstream declares it as `any`; an indexer preserves that permissiveness.
    //
    // Both solvers dispatch the magic function through this intersection: the old solver walks
    // the overloads, and the new solver relies on the fork's intersection-aware magic lookup in
    // ConstraintSolver::tryDispatch for FunctionCallConstraint (upstream only checks a callee
    // that is directly a FunctionType).
    Luau::TableType tableType{Luau::TableState::Sealed, Luau::TypeLevel{}, globals.globalScope.get()};
    tableType.indexer = Luau::TableIndexer{anyType, anyType};
    const Luau::TypeId tableTypeId = arena.addType(std::move(tableType));

    const Luau::TypeId sharedType = arena.addType(Luau::IntersectionType{{callTypeId, tableTypeId}});
    Luau::addGlobalBinding(globals, kGlobalName, sharedType, "@luau-lsp/global/shared");
}

} // namespace LSP::SharedRequire
