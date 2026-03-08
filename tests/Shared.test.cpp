#include "doctest.h"
#include "Fixture.h"
#include "Platform/RobloxPlatform.hpp"
#include "LSP/LuauExt.hpp"
#include "Luau/Parser.h"

TEST_SUITE_BEGIN("SharedModuleResolution");

TEST_CASE_FIXTURE(Fixture, "shared_resolves_module_by_filename")
{
    auto moduleUri = newDocument("MyModule.luau", R"(
        return { value = 42 }
    )");

    // Manually add the file to the index (newDocument creates in-memory docs, not on disk)
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto myModuleName = workspace.fileResolver.getModuleName(moduleUri);
    robloxPlatform->addFileToIndex(moduleUri, myModuleName);

    // Pre-check the shared module so it's available during type checking
    workspace.frontend.check(myModuleName);

    auto result = check(R"(
        local mod = shared("MyModule")
    )");

    auto ty = requireType("mod");
    auto ttv = Luau::get<Luau::TableType>(Luau::follow(ty));
    CHECK(ttv != nullptr);
}

TEST_CASE_FIXTURE(Fixture, "shared_error_when_not_found")
{
    auto result = check(R"(
        local mod = shared("NonExistent")
    )");

    LUAU_LSP_REQUIRE_ERRORS(result);
}

TEST_CASE_FIXTURE(Fixture, "shared_error_when_ambiguous")
{
    auto uriA = newDocument("src/ModuleA/Common.luau", R"(
        return { a = 1 }
    )");
    auto uriB = newDocument("src/ModuleB/Common.luau", R"(
        return { b = 2 }
    )");

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    robloxPlatform->addFileToIndex(uriA, workspace.fileResolver.getModuleName(uriA));
    robloxPlatform->addFileToIndex(uriB, workspace.fileResolver.getModuleName(uriB));

    auto result = check(R"(
        local mod = shared("Common")
    )");

    LUAU_LSP_REQUIRE_ERRORS(result);
}

TEST_CASE_FIXTURE(Fixture, "shared_partial_path_resolves_ambiguity")
{
    auto commonAUri = newDocument("src/ModuleA/Common.luau", R"(
        return { a = 1 }
    )");
    auto commonBUri = newDocument("src/ModuleB/Common.luau", R"(
        return { b = 2 }
    )");

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    robloxPlatform->addFileToIndex(commonAUri, workspace.fileResolver.getModuleName(commonAUri));
    robloxPlatform->addFileToIndex(commonBUri, workspace.fileResolver.getModuleName(commonBUri));

    // Pre-check the target module
    workspace.frontend.check(workspace.fileResolver.getModuleName(commonAUri));

    auto result = check(R"(
        local mod = shared("ModuleA/Common")
    )");

    auto ty = requireType("mod");
    auto ttv = Luau::get<Luau::TableType>(Luau::follow(ty));
    CHECK(ttv != nullptr);
}

TEST_CASE_FIXTURE(Fixture, "shared_filename_index_resolution")
{
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);

    auto uri = newDocument("src/Objects/Shop/ClientLimitedDragonStand.luau", R"(
        return { price = 100 }
    )");

    auto moduleName = workspace.fileResolver.getModuleName(uri);
    robloxPlatform->addFileToIndex(uri, moduleName);

    // Simple name
    auto result1 = robloxPlatform->resolveSharedModuleName("ClientLimitedDragonStand");
    CHECK(result1.status == SharedModuleResult::Found);

    // Partial path
    auto result2 = robloxPlatform->resolveSharedModuleName("Shop/ClientLimitedDragonStand");
    CHECK(result2.status == SharedModuleResult::Found);

    // Full path from root
    auto result3 = robloxPlatform->resolveSharedModuleName("src/Objects/Shop/ClientLimitedDragonStand");
    CHECK(result3.status == SharedModuleResult::Found);

    // Non-existent
    auto result4 = robloxPlatform->resolveSharedModuleName("DoesNotExist");
    CHECK(result4.status == SharedModuleResult::NotFound);
}

TEST_CASE_FIXTURE(Fixture, "shared_init_luau_uses_parent_directory_name")
{
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);

    auto uri = newDocument("src/MyModule/init.luau", R"(
        return { value = 1 }
    )");

    auto moduleName = workspace.fileResolver.getModuleName(uri);
    robloxPlatform->addFileToIndex(uri, moduleName);

    // Should resolve by parent directory name, not "init"
    auto result1 = robloxPlatform->resolveSharedModuleName("MyModule");
    CHECK(result1.status == SharedModuleResult::Found);

    // "init" should NOT match
    auto result2 = robloxPlatform->resolveSharedModuleName("init");
    CHECK(result2.status == SharedModuleResult::NotFound);

    // Partial path should also work
    auto result3 = robloxPlatform->resolveSharedModuleName("src/MyModule");
    CHECK(result3.status == SharedModuleResult::Found);
}

TEST_CASE_FIXTURE(Fixture, "shared_exports_type_bindings")
{
    auto moduleUri = newDocument("Dumpster.luau", R"(
        export type Dumpster = {
            new: () -> Dumpster,
            destroy: (self: Dumpster) -> (),
        }
        local Dumpster = {}
        Dumpster.__index = Dumpster
        function Dumpster.new(): Dumpster
            return setmetatable({}, Dumpster) :: any
        end
        function Dumpster:destroy()
        end
        return Dumpster
    )");

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto moduleName = workspace.fileResolver.getModuleName(moduleUri);
    robloxPlatform->addFileToIndex(moduleUri, moduleName);

    // Pre-check the shared module
    workspace.frontend.check(moduleName);

    auto result = check(R"(
        local Dumpster = shared("Dumpster")
        type Dumpster = Dumpster.Dumpster
    )");

    // The type alias should resolve without errors
    LUAU_LSP_REQUIRE_NO_ERRORS(result);
}

TEST_CASE_FIXTURE(Fixture, "shared_exports_type_bindings_autocomplete")
{
    auto moduleUri = newDocument("Dumpster.luau", R"(
        export type Dumpster = {
            new: () -> Dumpster,
            destroy: (self: Dumpster) -> (),
        }
        local Dumpster = {}
        Dumpster.__index = Dumpster
        function Dumpster.new(): Dumpster
            return setmetatable({}, Dumpster) :: any
        end
        function Dumpster:destroy()
        end
        return Dumpster
    )");

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto moduleName = workspace.fileResolver.getModuleName(moduleUri);
    robloxPlatform->addFileToIndex(moduleUri, moduleName);

    // Pre-check the shared module with BOTH resolvers (like ensureSharedDependenciesChecked does)
    workspace.frontend.check(moduleName);
    Luau::FrontendOptions acOptions{/* retainFullTypeGraphs: */ true, /* forAutocomplete: */ true, /* runLintChecks: */ false};
    workspace.frontend.check(moduleName, acOptions);

    // Check the main module with forAutocomplete (like checkStrict does for hover)
    auto mainUri = newDocument("test.luau", R"(
        local Dumpster = shared("Dumpster")
        type Dumpster = Dumpster.Dumpster
    )");

    auto mainModName = workspace.fileResolver.getModuleName(mainUri);
    Luau::FrontendOptions mainOptions{/* retainFullTypeGraphs: */ true, /* forAutocomplete: */ true, /* runLintChecks: */ false};
    auto result = workspace.frontend.check(mainModName, mainOptions);

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
}

TEST_CASE_FIXTURE(Fixture, "shared_exports_type_bindings_real_lsp_sequence")
{
    // Mimics the real LSP: checkSimple (diagnostics) then checkStrict (hover)
    auto moduleUri = newDocument("Dumpster.luau", R"(
        export type Dumpster = {
            new: () -> Dumpster,
            destroy: (self: Dumpster) -> (),
        }
        local Dumpster = {}
        Dumpster.__index = Dumpster
        function Dumpster.new(): Dumpster
            return setmetatable({}, Dumpster) :: any
        end
        function Dumpster:destroy()
        end
        return Dumpster
    )");

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto depModuleName = workspace.fileResolver.getModuleName(moduleUri);
    robloxPlatform->addFileToIndex(moduleUri, depModuleName);

    auto mainUri = newDocument("test.luau", R"(
        local Dumpster = shared("Dumpster")
        type Dumpster = Dumpster.Dumpster
        local d = Dumpster.new()
    )");
    auto mainModName = workspace.fileResolver.getModuleName(mainUri);

    // Step 1: checkSimple (diagnostics path) - forAutocomplete: false, retainFullTypeGraphs: false
    {
        // ensureSharedDependenciesChecked
        Luau::FrontendOptions depOpts{/* retainFullTypeGraphs: */ true, /* forAutocomplete: */ false, /* runLintChecks: */ false};
        workspace.frontend.check(depModuleName, depOpts);
        Luau::FrontendOptions depAcOpts{/* retainFullTypeGraphs: */ true, /* forAutocomplete: */ true, /* runLintChecks: */ false};
        workspace.frontend.check(depModuleName, depAcOpts);

        // main check
        Luau::FrontendOptions mainOpts{/* retainFullTypeGraphs: */ false, /* forAutocomplete: */ false, /* runLintChecks: */ true};
        auto result1 = workspace.frontend.check(mainModName, mainOpts);
        LUAU_LSP_REQUIRE_NO_ERRORS(result1);
    }

    // Step 2: checkStrict (hover path) - forAutocomplete: true, retainFullTypeGraphs: true
    {
        // ensureSharedDependenciesChecked again
        Luau::FrontendOptions depOpts{/* retainFullTypeGraphs: */ true, /* forAutocomplete: */ false, /* runLintChecks: */ false};
        workspace.frontend.check(depModuleName, depOpts);
        Luau::FrontendOptions depAcOpts{/* retainFullTypeGraphs: */ true, /* forAutocomplete: */ true, /* runLintChecks: */ false};
        workspace.frontend.check(depModuleName, depAcOpts);

        // Mark dirty if type graph was discarded (like checkStrict does)
        auto module = workspace.frontend.moduleResolverForAutocomplete.getModule(mainModName);
        if (module && module->internalTypes.types.empty())
            workspace.frontend.markDirty(mainModName);

        // main check
        Luau::FrontendOptions mainOpts{/* retainFullTypeGraphs: */ true, /* forAutocomplete: */ true, /* runLintChecks: */ true};
        auto result2 = workspace.frontend.check(mainModName, mainOpts);
        LUAU_LSP_REQUIRE_NO_ERRORS(result2);
    }
}

TEST_CASE_FIXTURE(Fixture, "shared_typeof_annotation_exports_type_bindings")
{
    auto moduleUri = newDocument("Dumpster.luau", R"(
        export type Dumpster = {
            new: () -> Dumpster,
            destroy: (self: Dumpster) -> (),
        }
        local Dumpster = {}
        Dumpster.__index = Dumpster
        function Dumpster.new(): Dumpster
            return setmetatable({}, Dumpster) :: any
        end
        function Dumpster:destroy()
        end
        return Dumpster
    )");

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto moduleName = workspace.fileResolver.getModuleName(moduleUri);
    robloxPlatform->addFileToIndex(moduleUri, moduleName);

    // Pre-check the shared module
    workspace.frontend.check(moduleName);

    auto result = check(R"(
        local Dumpster: typeof(shared("Dumpster"))
        type Dumpster = Dumpster.Dumpster
    )");

    // typeof(shared(...)) should populate type bindings just like direct shared() calls
    LUAU_LSP_REQUIRE_NO_ERRORS(result);
}

TEST_CASE("matchShared_matches_shared_calls")
{
    Luau::Allocator allocator;
    Luau::AstNameTable names(allocator);

    Luau::ParseOptions options;
    auto parseResult = Luau::Parser::parse("shared('test')", strlen("shared('test')"), names, allocator, options);
    REQUIRE(parseResult.root);

    auto* block = parseResult.root;
    REQUIRE(block->body.size > 0);
    auto* exprStat = block->body.data[0]->as<Luau::AstStatExpr>();
    REQUIRE(exprStat);
    auto* call = exprStat->expr->as<Luau::AstExprCall>();
    REQUIRE(call);

    auto result = types::matchShared(*call);
    CHECK(result.has_value());
}

TEST_CASE("matchShared_does_not_match_require_calls")
{
    Luau::Allocator allocator;
    Luau::AstNameTable names(allocator);

    Luau::ParseOptions options;
    auto parseResult = Luau::Parser::parse("require('test')", strlen("require('test')"), names, allocator, options);
    REQUIRE(parseResult.root);

    auto* block = parseResult.root;
    REQUIRE(block->body.size > 0);
    auto* exprStat = block->body.data[0]->as<Luau::AstStatExpr>();
    REQUIRE(exprStat);
    auto* call = exprStat->expr->as<Luau::AstExprCall>();
    REQUIRE(call);

    auto result = types::matchShared(*call);
    CHECK(!result.has_value());
}

// ============================================================================
// Module retention / dangling TypeId crash tests
//
// These tests simulate the real LSP lifecycle that previously caused a crash:
// VisitType.h assertion "GenericTypeVisitor::traverse(TypeId) is not exhaustive!"
// The crash occurred when a dependency module was destroyed while the consumer
// still held TypeIds pointing into the dependency's interfaceTypes arena.
// ============================================================================

static void setupSharedModule(Fixture& f, const std::string& filename, const std::string& source)
{
    auto uri = f.newDocument(filename, source);
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(f.workspace.platform.get());
    REQUIRE(robloxPlatform);
    robloxPlatform->addFileToIndex(uri, f.workspace.fileResolver.getModuleName(uri));
}

TEST_CASE_FIXTURE(Fixture, "shared_checkSimple_then_checkStrict_no_crash")
{
    // Simulates: workspace diagnostics (checkSimple) then hover (checkStrict).
    // checkSimple discards internalTypes. checkStrict marks dirty and rechecks.
    // The consumer module's types reference the dependency's interfaceTypes.
    // After the recheck, toString traverses the type graph — this is where
    // the crash used to occur.
    setupSharedModule(*this, "Types.luau", R"(
        export type Config = {
            name: string,
            value: number,
        }
        return { defaultConfig = { name = "test", value = 0 } }
    )");

    auto mainUri = newDocument("Consumer.luau", R"(
        local Types = shared("Types")
        type Config = Types.Config
        local cfg: Config = Types.defaultConfig
    )");
    auto mainMod = workspace.fileResolver.getModuleName(mainUri);

    // Step 1: checkSimple — discards type graph (like workspace diagnostics)
    Luau::FrontendOptions simpleOpts{false, false, true};
    workspace.frontend.check(mainMod, simpleOpts);

    // Step 2: checkStrict — retains type graph (like hover)
    // This is the checkStrict hack: mark dirty if internalTypes was cleared
    auto module = workspace.frontend.moduleResolver.getModule(mainMod);
    if (module && module->internalTypes.types.empty())
        workspace.frontend.markDirty(mainMod);

    Luau::FrontendOptions strictOpts{true, false, true};
    auto result = workspace.frontend.check(mainMod, strictOpts);
    LUAU_LSP_REQUIRE_NO_ERRORS(result);

    // Step 3: Access types — this triggers the VisitType traversal that would crash
    auto mod = workspace.frontend.moduleResolver.getModule(mainMod);
    REQUIRE(mod);

    Luau::ToStringOptions toStringOpts;
    toStringOpts.exhaustive = true;
    // Traverse the return type — follows into dependency's interfaceTypes
    auto retStr = Luau::toString(mod->returnType, toStringOpts);
    CHECK(!retStr.empty());

    // Also traverse exported type bindings if any
    for (const auto& [name, tf] : mod->exportedTypeBindings)
    {
        auto tyStr = Luau::toString(tf.type, toStringOpts);
        CHECK(!tyStr.empty());
    }
}

TEST_CASE_FIXTURE(Fixture, "shared_dependency_recheck_consumer_types_survive")
{
    // Simulates: dependency B is edited and rechecked. Consumer A was checked
    // before but NOT marked dirty yet. Verify A's types are still accessible
    // (the retained module keeps old B alive).
    auto depUri = newDocument("Dep.luau", R"(
        export type Item = { id: number, label: string }
        return { create = function(): Item return { id = 1, label = "a" } end }
    )");
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto depMod = workspace.fileResolver.getModuleName(depUri);
    robloxPlatform->addFileToIndex(depUri, depMod);

    auto consumerUri = newDocument("Consumer.luau", R"(
        local Dep = shared("Dep")
        type Item = Dep.Item
        local item = Dep.create()
    )");
    auto consumerMod = workspace.fileResolver.getModuleName(consumerUri);

    // Check consumer (which also checks Dep as a dependency)
    Luau::FrontendOptions opts{true, false, true};
    auto result1 = workspace.frontend.check(consumerMod, opts);
    LUAU_LSP_REQUIRE_NO_ERRORS(result1);

    // Get the consumer module — it holds TypeIds into Dep's interfaceTypes
    auto mod1 = workspace.frontend.moduleResolver.getModule(consumerMod);
    REQUIRE(mod1);

    // Now simulate editing the dependency: markDirty propagates to consumer
    workspace.frontend.markDirty(depMod);

    // Update the dependency source
    updateDocument(depUri, R"(
        export type Item = { id: number, label: string, extra: boolean }
        return { create = function(): Item return { id = 1, label = "a", extra = true } end }
    )");

    // Consumer is now dirty (markDirty propagated), so recheck it too
    auto result2 = workspace.frontend.check(consumerMod, opts);
    LUAU_LSP_REQUIRE_NO_ERRORS(result2);

    // Access the new consumer module's types — should reference new Dep
    auto mod2 = workspace.frontend.moduleResolver.getModule(consumerMod);
    REQUIRE(mod2);
    Luau::ToStringOptions toStringOpts;
    toStringOpts.exhaustive = true;
    auto retStr = Luau::toString(mod2->returnType, toStringOpts);
    CHECK(!retStr.empty());
}

TEST_CASE_FIXTURE(Fixture, "shared_multiple_consumers_same_dependency")
{
    // Multiple modules use shared("Dep"). When Dep is rechecked, both
    // consumers must survive without dangling TypeIds.
    setupSharedModule(*this, "Dep.luau", R"(
        export type Vec2 = { x: number, y: number }
        return { zero = { x = 0, y = 0 } }
    )");

    auto uriA = newDocument("ConsumerA.luau", R"(
        local Dep = shared("Dep")
        type Vec2 = Dep.Vec2
        local v: Vec2 = Dep.zero
    )");
    auto uriB = newDocument("ConsumerB.luau", R"(
        local Dep = shared("Dep")
        local pos: Dep.Vec2 = Dep.zero
    )");
    auto modA = workspace.fileResolver.getModuleName(uriA);
    auto modB = workspace.fileResolver.getModuleName(uriB);

    Luau::FrontendOptions opts{true, false, true};
    auto resultA = workspace.frontend.check(modA, opts);
    auto resultB = workspace.frontend.check(modB, opts);
    LUAU_LSP_REQUIRE_NO_ERRORS(resultA);
    LUAU_LSP_REQUIRE_NO_ERRORS(resultB);

    // Access both modules' types via toString
    Luau::ToStringOptions toStringOpts;
    toStringOpts.exhaustive = true;
    auto moduleA = workspace.frontend.moduleResolver.getModule(modA);
    auto moduleB = workspace.frontend.moduleResolver.getModule(modB);
    REQUIRE(moduleA);
    REQUIRE(moduleB);
    CHECK(!Luau::toString(moduleA->returnType, toStringOpts).empty());
    CHECK(!Luau::toString(moduleB->returnType, toStringOpts).empty());
}

TEST_CASE_FIXTURE(Fixture, "shared_hover_after_checkSimple_no_crash")
{
    // End-to-end: simulates the actual LSP flow that caused the crash.
    // 1. checkSimple for diagnostics (discards type graph)
    // 2. hover() call which internally calls checkStrict then toString
    setupSharedModule(*this, "Utils.luau", R"(
        export type Options = { verbose: boolean, retries: number }
        local function run(opts: Options) end
        return { run = run }
    )");

    auto mainUri = newDocument("Main.luau", R"(
        local Utils = shared("Utils")
        local opts: Utils.Options = { verbose = true, retries = 3 }
        Utils.run(opts)
    )");
    auto mainMod = workspace.fileResolver.getModuleName(mainUri);

    // Step 1: checkSimple (workspace diagnostics)
    workspace.checkSimple(mainMod, nullptr);

    // Step 2: hover — calls checkStrict internally, then accesses types
    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{mainUri};
    params.position = lsp::Position{2, 14}; // hover over "opts"

    // This must not crash with VisitType.h assertion
    auto hoverResult = workspace.hover(params, nullptr);
    // We don't assert the exact content, just that it didn't crash
    CHECK(hoverResult.has_value());
}

TEST_CASE_FIXTURE(Fixture, "shared_frontend_clear_then_recheck_no_crash")
{
    // Simulates sourcemap update: frontend.clear() destroys all modules,
    // then everything is rechecked from scratch.
    setupSharedModule(*this, "Data.luau", R"(
        export type Record = { key: string, val: number }
        return { empty = { key = "", val = 0 } }
    )");

    auto mainUri = newDocument("Main.luau", R"(
        local Data = shared("Data")
        type Record = Data.Record
        local r: Record = Data.empty
    )");
    auto mainMod = workspace.fileResolver.getModuleName(mainUri);

    // Initial check
    Luau::FrontendOptions opts{true, false, true};
    auto result1 = workspace.frontend.check(mainMod, opts);
    LUAU_LSP_REQUIRE_NO_ERRORS(result1);

    // Simulate sourcemap update: clear all modules
    workspace.frontend.clear();

    // Recheck from scratch — all modules need reparsing and rechecking
    auto result2 = workspace.frontend.check(mainMod, opts);
    LUAU_LSP_REQUIRE_NO_ERRORS(result2);

    // Access types after clear + recheck
    auto mod = workspace.frontend.moduleResolver.getModule(mainMod);
    REQUIRE(mod);
    Luau::ToStringOptions toStringOpts;
    toStringOpts.exhaustive = true;
    CHECK(!Luau::toString(mod->returnType, toStringOpts).empty());
}

TEST_CASE_FIXTURE(Fixture, "shared_retained_modules_prevent_dangling_typeids")
{
    ENABLE_NEW_SOLVER();

    // Directly verify that retainedModules is populated and keeps deps alive.
    auto libUri = newDocument("Lib.luau", R"(
        export type Foo = { x: number }
        return { make = function(): Foo return { x = 1 } end }
    )");
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto libMod = workspace.fileResolver.getModuleName(libUri);
    robloxPlatform->addFileToIndex(libUri, libMod);

    auto mainUri = newDocument("Main.luau", R"(
        local Lib = shared("Lib")
        local f = Lib.make()
    )");
    auto mainMod = workspace.fileResolver.getModuleName(mainUri);

    Luau::FrontendOptions opts{true, false, true};
    workspace.frontend.check(mainMod, opts);

    auto mod = workspace.frontend.moduleResolver.getModule(mainMod);
    REQUIRE(mod);

    // The consumer module should retain its dependency
    CHECK(!mod->retainedModules.empty());

    // The retained module should be the Lib module
    bool foundLib = false;
    for (const auto& retained : mod->retainedModules)
    {
        if (retained->name == libMod)
            foundLib = true;
    }
    CHECK(foundLib);
}

TEST_CASE_FIXTURE(Fixture, "shared_checkSimple_then_checkStrict_with_new_solver")
{
    ENABLE_NEW_SOLVER();

    // Same as shared_checkSimple_then_checkStrict_no_crash but with new solver.
    // The new solver always uses moduleResolver (not moduleResolverForAutocomplete).
    setupSharedModule(*this, "Types.luau", R"(
        export type Config = {
            name: string,
            value: number,
        }
        return { defaultConfig = { name = "test", value = 0 } }
    )");

    auto mainUri = newDocument("Consumer.luau", R"(
        local Types = shared("Types")
        type Config = Types.Config
        local cfg: Config = Types.defaultConfig
    )");
    auto mainMod = workspace.fileResolver.getModuleName(mainUri);

    // checkSimple — discards type graph
    Luau::FrontendOptions simpleOpts{false, false, true};
    workspace.frontend.check(mainMod, simpleOpts);

    // checkStrict hack
    auto module = workspace.frontend.moduleResolver.getModule(mainMod);
    if (module && module->internalTypes.types.empty())
        workspace.frontend.markDirty(mainMod);

    Luau::FrontendOptions strictOpts{true, false, true};
    auto result = workspace.frontend.check(mainMod, strictOpts);
    LUAU_LSP_REQUIRE_NO_ERRORS(result);

    // Traverse types — must not crash
    auto mod = workspace.frontend.moduleResolver.getModule(mainMod);
    REQUIRE(mod);
    Luau::ToStringOptions toStringOpts;
    toStringOpts.exhaustive = true;
    CHECK(!Luau::toString(mod->returnType, toStringOpts).empty());
}

TEST_CASE_FIXTURE(Fixture, "shared_repeated_checkSimple_checkStrict_cycles")
{
    // Simulate repeated diagnostics + hover cycles, which is the normal
    // LSP usage pattern. Each cycle: checkSimple clears internalTypes,
    // then checkStrict rechecks with full type graph.
    setupSharedModule(*this, "Svc.luau", R"(
        export type Request = { url: string, method: string }
        export type Response = { status: number, body: string }
        return {
            fetch = function(req: Request): Response
                return { status = 200, body = "" }
            end,
        }
    )");

    auto mainUri = newDocument("Client.luau", R"(
        local Svc = shared("Svc")
        type Request = Svc.Request
        type Response = Svc.Response
        local resp = Svc.fetch({ url = "http://test", method = "GET" })
    )");
    auto mainMod = workspace.fileResolver.getModuleName(mainUri);

    for (int cycle = 0; cycle < 5; ++cycle)
    {
        // checkSimple (workspace diagnostics)
        Luau::FrontendOptions simpleOpts{false, false, true};
        workspace.frontend.check(mainMod, simpleOpts);

        // checkStrict (hover)
        auto module = workspace.frontend.moduleResolver.getModule(mainMod);
        if (module && module->internalTypes.types.empty())
            workspace.frontend.markDirty(mainMod);

        Luau::FrontendOptions strictOpts{true, false, true};
        auto result = workspace.frontend.check(mainMod, strictOpts);
        LUAU_LSP_REQUIRE_NO_ERRORS(result);

        // Access types
        auto mod = workspace.frontend.moduleResolver.getModule(mainMod);
        REQUIRE(mod);
        Luau::ToStringOptions toStringOpts;
        toStringOpts.exhaustive = true;
        CHECK(!Luau::toString(mod->returnType, toStringOpts).empty());
    }
}

// ============================================================================
// Compound LSP usage scenarios
//
// These tests simulate realistic multi-operation LSP sessions spanning the kind
// of sequences that happen during a real editing session: opening files, getting
// diagnostics, hovering, editing dependencies, go-to-definition, completion,
// sourcemap updates, etc.  The goal is to exercise the module retention and
// type-graph lifecycle under realistic interleaved operations.
// ============================================================================

TEST_CASE_FIXTURE(Fixture, "shared_session_open_diagnostics_hover_edit_cycle")
{
    // Simulates a typical editing session:
    //   1. Open consumer file -> documentDiagnostics (checkSimple on old solver, checkStrict on new)
    //   2. Hover over shared() variable -> checkStrict + toString
    //   3. Edit the dependency module -> markDirty propagates
    //   4. documentDiagnostics again on consumer (re-checks with new dependency)
    //   5. Hover again on consumer -> must not crash
    //   6. Repeat cycle with another edit

    auto depUri = newDocument("PlayerData.luau", R"(
        export type PlayerData = {
            name: string,
            score: number,
            level: number,
        }
        local function create(name: string): PlayerData
            return { name = name, score = 0, level = 1 }
        end
        return { create = create }
    )");
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto depMod = workspace.fileResolver.getModuleName(depUri);
    robloxPlatform->addFileToIndex(depUri, depMod);

    auto consumerUri = newDocument("Game.luau", R"(
        local PlayerData = shared("PlayerData")
        type PlayerData = PlayerData.PlayerData
        local p: PlayerData = PlayerData.create("Alice")
        local name = p.name
        local score = p.score
    )");
    auto consumerMod = workspace.fileResolver.getModuleName(consumerUri);

    // --- Cycle 1 ---

    // 1a. documentDiagnostics (checkSimple path for old solver)
    workspace.checkSimple(consumerMod, nullptr);

    // 1b. Hover over "p" at line 3 col 14
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{3, 14};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }

    // 1c. Edit the dependency — add a new field
    workspace.frontend.markDirty(depMod);
    updateDocument(depUri, R"(
        export type PlayerData = {
            name: string,
            score: number,
            level: number,
            rank: string,
        }
        local function create(name: string): PlayerData
            return { name = name, score = 0, level = 1, rank = "bronze" }
        end
        return { create = create }
    )");

    // 1d. documentDiagnostics on consumer again
    workspace.checkSimple(consumerMod, nullptr);

    // 1e. Hover again — checkStrict rechecks with updated dependency
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{3, 14};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }

    // --- Cycle 2: edit dependency again ---

    workspace.frontend.markDirty(depMod);
    updateDocument(depUri, R"(
        export type PlayerData = {
            name: string,
            score: number,
            level: number,
            rank: string,
            isActive: boolean,
        }
        local function create(name: string): PlayerData
            return { name = name, score = 0, level = 1, rank = "bronze", isActive = true }
        end
        return { create = create }
    )");

    workspace.checkSimple(consumerMod, nullptr);

    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{4, 14}; // hover over "name"
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }
}

TEST_CASE_FIXTURE(Fixture, "shared_session_multi_file_diamond_dependency")
{
    // Diamond dependency pattern:
    //   Types.luau (shared types)
    //       |          |
    //   Producer.luau  Consumer.luau
    //       |          |
    //   App.luau (uses both Producer and Consumer via shared())
    //
    // Tests: diagnostics on all files, hover on App, edit Types, re-diagnostics, hover again.

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);

    auto typesUri = newDocument("Types.luau", R"(
        export type Item = { id: number, name: string, price: number }
        return {}
    )");
    auto typesMod = workspace.fileResolver.getModuleName(typesUri);
    robloxPlatform->addFileToIndex(typesUri, typesMod);

    auto producerUri = newDocument("Producer.luau", R"(
        local Types = shared("Types")
        type Item = Types.Item
        local function makeItem(id: number, name: string, price: number): Item
            return { id = id, name = name, price = price }
        end
        return { makeItem = makeItem }
    )");
    auto producerMod = workspace.fileResolver.getModuleName(producerUri);
    robloxPlatform->addFileToIndex(producerUri, producerMod);

    auto consumerUri = newDocument("Consumer.luau", R"LUAU(
        local Types = shared("Types")
        type Item = Types.Item
        local function formatItem(item: Item): string
            return item.name .. " ($" .. tostring(item.price) .. ")"
        end
        return { formatItem = formatItem }
    )LUAU");
    auto consumerMod = workspace.fileResolver.getModuleName(consumerUri);
    robloxPlatform->addFileToIndex(consumerUri, consumerMod);

    auto appUri = newDocument("App.luau", R"(
        local Producer = shared("Producer")
        local Consumer = shared("Consumer")
        local item = Producer.makeItem(1, "Sword", 100)
        local formatted = Consumer.formatItem(item)
    )");
    auto appMod = workspace.fileResolver.getModuleName(appUri);

    // Workspace diagnostics: checkSimple on all files
    workspace.checkSimple(typesMod, nullptr);
    workspace.checkSimple(producerMod, nullptr);
    workspace.checkSimple(consumerMod, nullptr);
    workspace.checkSimple(appMod, nullptr);

    // Hover on App.luau — "item" at line 3
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{appUri};
        params.position = lsp::Position{3, 14};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }

    // Edit Types.luau — add a field
    workspace.frontend.markDirty(typesMod);
    updateDocument(typesUri, R"(
        export type Item = { id: number, name: string, price: number, quantity: number }
        return {}
    )");

    // Re-diagnostics on the full chain
    workspace.checkSimple(typesMod, nullptr);
    workspace.checkSimple(producerMod, nullptr);
    workspace.checkSimple(consumerMod, nullptr);
    workspace.checkSimple(appMod, nullptr);

    // Hover on App.luau again after dependency chain changed
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{appUri};
        params.position = lsp::Position{3, 14};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }

    // Hover on Consumer.luau — "item" parameter
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{3, 37};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }
}

TEST_CASE_FIXTURE(Fixture, "shared_session_clear_and_recheck_all_files")
{
    // Simulates a sourcemap update that triggers frontend.clear(),
    // followed by diagnostics on all open files and then hover operations.
    // This is the most aggressive lifecycle event — all modules destroyed at once.

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);

    auto configUri = newDocument("Config.luau", R"(
        export type Settings = { debug: boolean, verbose: boolean, maxRetries: number }
        return { defaults = { debug = false, verbose = false, maxRetries = 3 } }
    )");
    auto configMod = workspace.fileResolver.getModuleName(configUri);
    robloxPlatform->addFileToIndex(configUri, configMod);

    auto loggerUri = newDocument("Logger.luau", R"(
        local Config = shared("Config")
        type Settings = Config.Settings
        local function log(settings: Settings, msg: string)
            if settings.verbose then print(msg) end
        end
        return { log = log }
    )");
    auto loggerMod = workspace.fileResolver.getModuleName(loggerUri);
    robloxPlatform->addFileToIndex(loggerUri, loggerMod);

    auto appUri = newDocument("App.luau", R"(
        local Config = shared("Config")
        local Logger = shared("Logger")
        local settings: Config.Settings = Config.defaults
        Logger.log(settings, "hello")
    )");
    auto appMod = workspace.fileResolver.getModuleName(appUri);

    // Initial check of all modules
    Luau::FrontendOptions opts{true, false, true};
    workspace.frontend.check(appMod, opts);

    // Hover on App to populate type graph
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{appUri};
        params.position = lsp::Position{3, 14};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }

    // --- Sourcemap update: clear everything ---
    workspace.frontend.clear();

    // Re-check all open files (like the LSP does after a sourcemap change)
    workspace.checkSimple(configMod, nullptr);
    workspace.checkSimple(loggerMod, nullptr);
    workspace.checkSimple(appMod, nullptr);

    // Hover on App.luau after full clear + recheck
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{appUri};
        params.position = lsp::Position{3, 14};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }

    // Hover on Logger.luau after full clear + recheck
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{loggerUri};
        params.position = lsp::Position{3, 28};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }

    // --- Second clear cycle to stress-test ---
    workspace.frontend.clear();

    workspace.checkSimple(appMod, nullptr);

    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{appUri};
        params.position = lsp::Position{4, 14};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }
}

TEST_CASE_FIXTURE(Fixture, "shared_session_interleaved_edit_hover_completion")
{
    // Simulates rapid user interaction: typing in a consumer file while
    // hovering and completing on shared() types. Each edit triggers markDirty
    // and re-diagnostics, interleaved with hover requests.

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);

    auto mathUri = newDocument("MathUtils.luau", R"(
        export type Vector3 = { x: number, y: number, z: number }
        local function add(a: Vector3, b: Vector3): Vector3
            return { x = a.x + b.x, y = a.y + b.y, z = a.z + b.z }
        end
        local function scale(v: Vector3, s: number): Vector3
            return { x = v.x * s, y = v.y * s, z = v.z * s }
        end
        return { add = add, scale = scale }
    )");
    auto mathMod = workspace.fileResolver.getModuleName(mathUri);
    robloxPlatform->addFileToIndex(mathUri, mathMod);

    // Version 1 of consumer: just imports
    auto consumerUri = newDocument("Physics.luau", R"(
        local MathUtils = shared("MathUtils")
        type Vector3 = MathUtils.Vector3
    )");
    auto consumerMod = workspace.fileResolver.getModuleName(consumerUri);

    // Diagnostics
    workspace.checkSimple(consumerMod, nullptr);

    // Version 2: user adds a variable
    workspace.frontend.markDirty(consumerMod);
    updateDocument(consumerUri, R"(
        local MathUtils = shared("MathUtils")
        type Vector3 = MathUtils.Vector3
        local pos: Vector3 = { x = 0, y = 0, z = 0 }
    )");
    workspace.checkSimple(consumerMod, nullptr);

    // Hover on "pos"
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{3, 14};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }

    // Version 3: user adds a function call
    workspace.frontend.markDirty(consumerMod);
    updateDocument(consumerUri, R"(
        local MathUtils = shared("MathUtils")
        type Vector3 = MathUtils.Vector3
        local pos: Vector3 = { x = 0, y = 0, z = 0 }
        local vel: Vector3 = { x = 1, y = 0, z = 0 }
        local newPos = MathUtils.add(pos, vel)
    )");
    workspace.checkSimple(consumerMod, nullptr);

    // Hover on "newPos"
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{5, 14};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }

    // Version 4: user adds another function call
    workspace.frontend.markDirty(consumerMod);
    updateDocument(consumerUri, R"(
        local MathUtils = shared("MathUtils")
        type Vector3 = MathUtils.Vector3
        local pos: Vector3 = { x = 0, y = 0, z = 0 }
        local vel: Vector3 = { x = 1, y = 0, z = 0 }
        local newPos = MathUtils.add(pos, vel)
        local scaled = MathUtils.scale(newPos, 2)
    )");
    workspace.checkSimple(consumerMod, nullptr);

    // Hover on "scaled"
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{6, 14};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }

    // Hover on "MathUtils" itself
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{1, 14};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }
}

TEST_CASE_FIXTURE(Fixture, "shared_session_dependency_edited_while_consumers_open")
{
    // Simulates editing a shared dependency while multiple consumers are open.
    // After each edit, all consumers get re-diagnostics and hover.
    // This stresses module replacement + retention across the dependency graph.

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);

    auto apiUri = newDocument("API.luau", R"(
        export type User = { id: number, name: string }
        export type Post = { id: number, title: string, author: User }
        local function getUser(id: number): User
            return { id = id, name = "user" }
        end
        local function getPost(id: number): Post
            return { id = id, title = "post", author = getUser(1) }
        end
        return { getUser = getUser, getPost = getPost }
    )");
    auto apiMod = workspace.fileResolver.getModuleName(apiUri);
    robloxPlatform->addFileToIndex(apiUri, apiMod);

    auto profileUri = newDocument("Profile.luau", R"(
        local API = shared("API")
        type User = API.User
        local function showProfile(user: User): string
            return user.name
        end
        return { showProfile = showProfile }
    )");
    auto profileMod = workspace.fileResolver.getModuleName(profileUri);

    auto feedUri = newDocument("Feed.luau", R"(
        local API = shared("API")
        type Post = API.Post
        local function showPost(post: Post): string
            return post.title .. " by " .. post.author.name
        end
        return { showPost = showPost }
    )");
    auto feedMod = workspace.fileResolver.getModuleName(feedUri);

    auto dashboardUri = newDocument("Dashboard.luau", R"(
        local API = shared("API")
        local user = API.getUser(1)
        local post = API.getPost(1)
    )");
    auto dashboardMod = workspace.fileResolver.getModuleName(dashboardUri);

    // Initial diagnostics + hover on all consumers
    workspace.checkSimple(profileMod, nullptr);
    workspace.checkSimple(feedMod, nullptr);
    workspace.checkSimple(dashboardMod, nullptr);

    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{dashboardUri};
        params.position = lsp::Position{2, 14};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }

    // --- Edit 1: Add email to User ---
    workspace.frontend.markDirty(apiMod);
    updateDocument(apiUri, R"(
        export type User = { id: number, name: string, email: string }
        export type Post = { id: number, title: string, author: User }
        local function getUser(id: number): User
            return { id = id, name = "user", email = "user@test.com" }
        end
        local function getPost(id: number): Post
            return { id = id, title = "post", author = getUser(1) }
        end
        return { getUser = getUser, getPost = getPost }
    )");

    // Re-diagnostics on all consumers
    workspace.checkSimple(profileMod, nullptr);
    workspace.checkSimple(feedMod, nullptr);
    workspace.checkSimple(dashboardMod, nullptr);

    // Hover on each consumer
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{profileUri};
        params.position = lsp::Position{3, 40};
        CHECK(workspace.hover(params, nullptr).has_value());
    }
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{feedUri};
        params.position = lsp::Position{3, 37};
        CHECK(workspace.hover(params, nullptr).has_value());
    }
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{dashboardUri};
        params.position = lsp::Position{3, 14};
        CHECK(workspace.hover(params, nullptr).has_value());
    }

    // --- Edit 2: Add a new function to API ---
    workspace.frontend.markDirty(apiMod);
    updateDocument(apiUri, R"(
        export type User = { id: number, name: string, email: string }
        export type Post = { id: number, title: string, author: User }
        local function getUser(id: number): User
            return { id = id, name = "user", email = "user@test.com" }
        end
        local function getPost(id: number): Post
            return { id = id, title = "post", author = getUser(1) }
        end
        local function deletePost(id: number): boolean
            return true
        end
        return { getUser = getUser, getPost = getPost, deletePost = deletePost }
    )");

    workspace.checkSimple(profileMod, nullptr);
    workspace.checkSimple(feedMod, nullptr);
    workspace.checkSimple(dashboardMod, nullptr);

    // Hover across all files after second edit
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{dashboardUri};
        params.position = lsp::Position{2, 14};
        CHECK(workspace.hover(params, nullptr).has_value());
    }
}

TEST_CASE_FIXTURE(Fixture, "shared_session_goto_definition_across_shared_boundary")
{
    // Simulates: diagnostics -> goto definition on a shared() import -> hover
    // on the definition site -> back to consumer -> hover.
    // This exercises cross-module type access from both sides.

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);

    auto libUri = newDocument("EventLib.luau", R"(
        export type Event = { name: string, timestamp: number }
        local function fire(name: string): Event
            return { name = name, timestamp = 0 }
        end
        return { fire = fire }
    )");
    auto libMod = workspace.fileResolver.getModuleName(libUri);
    robloxPlatform->addFileToIndex(libUri, libMod);

    auto handlerUri = newDocument("Handler.luau", R"(
        local EventLib = shared("EventLib")
        type Event = EventLib.Event
        local evt = EventLib.fire("click")
        local name = evt.name
    )");
    auto handlerMod = workspace.fileResolver.getModuleName(handlerUri);

    // Diagnostics
    workspace.checkSimple(handlerMod, nullptr);

    // Hover on "evt" — triggers checkStrict
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{handlerUri};
        params.position = lsp::Position{3, 14};
        CHECK(workspace.hover(params, nullptr).has_value());
    }

    // Goto definition on "EventLib" variable — would navigate to EventLib.luau
    {
        lsp::DefinitionParams params;
        params.textDocument = lsp::TextDocumentIdentifier{handlerUri};
        params.position = lsp::Position{1, 14};
        auto defResult = workspace.gotoDefinition(params, nullptr);
        // We don't assert the exact location, just that it doesn't crash
        (void)defResult;
    }

    // Hover on the dependency file itself
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{libUri};
        params.position = lsp::Position{2, 20};
        CHECK(workspace.hover(params, nullptr).has_value());
    }

    // Back to consumer — hover on "name"
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{handlerUri};
        params.position = lsp::Position{4, 14};
        CHECK(workspace.hover(params, nullptr).has_value());
    }
}

TEST_CASE_FIXTURE(Fixture, "shared_session_rapid_dependency_edits_with_hover")
{
    // Simulates a user rapidly editing a shared dependency (e.g., fixing a type)
    // while hovering on a consumer file between each save. Each edit replaces
    // the dependency module, and the consumer's retained reference must keep
    // the old types alive until the consumer is rechecked.

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);

    auto depUri = newDocument("Schema.luau", R"(
        export type Schema = { version: number }
        return { current = { version = 1 } }
    )");
    auto depMod = workspace.fileResolver.getModuleName(depUri);
    robloxPlatform->addFileToIndex(depUri, depMod);

    auto consumerUri = newDocument("Validator.luau", R"(
        local Schema = shared("Schema")
        type Schema = Schema.Schema
        local s: Schema = Schema.current
    )");
    auto consumerMod = workspace.fileResolver.getModuleName(consumerUri);

    // Initial check
    workspace.checkSimple(consumerMod, nullptr);

    for (int i = 2; i <= 6; ++i)
    {
        // Edit dependency
        workspace.frontend.markDirty(depMod);
        std::string src = R"(
            export type Schema = { version: number, rev: number }
            return { current = { version = )" +
                           std::to_string(i) + R"(, rev = )" + std::to_string(i * 10) + R"( } }
        )";
        updateDocument(depUri, src);

        // Diagnostics on consumer
        workspace.checkSimple(consumerMod, nullptr);

        // Hover on consumer — checkStrict + toString
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{3, 14};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }
}

TEST_CASE_FIXTURE(Fixture, "shared_session_multiple_clears_interleaved_with_operations")
{
    // Simulates multiple sourcemap updates (frontend.clear()) interleaved with
    // various LSP operations. Each clear destroys all modules; subsequent
    // operations must rebuild everything from scratch.

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);

    auto baseUri = newDocument("Base.luau", R"(
        export type Entity = { id: number, active: boolean }
        return { create = function(id: number): Entity return { id = id, active = true } end }
    )");
    auto baseMod = workspace.fileResolver.getModuleName(baseUri);
    robloxPlatform->addFileToIndex(baseUri, baseMod);

    auto gameUri = newDocument("Game.luau", R"(
        local Base = shared("Base")
        type Entity = Base.Entity
        local e = Base.create(1)
        local isActive = e.active
    )");
    auto gameMod = workspace.fileResolver.getModuleName(gameUri);

    for (int clearCycle = 0; clearCycle < 3; ++clearCycle)
    {
        // Clear all modules (simulates sourcemap update)
        workspace.frontend.clear();

        // checkSimple on game (triggers dependency check of Base too)
        workspace.checkSimple(gameMod, nullptr);

        // Hover on "e"
        {
            lsp::HoverParams params;
            params.textDocument = lsp::TextDocumentIdentifier{gameUri};
            params.position = lsp::Position{3, 14};
            CHECK(workspace.hover(params, nullptr).has_value());
        }

        // Hover on "isActive"
        {
            lsp::HoverParams params;
            params.textDocument = lsp::TextDocumentIdentifier{gameUri};
            params.position = lsp::Position{4, 14};
            CHECK(workspace.hover(params, nullptr).has_value());
        }

        // Goto definition on "Base"
        {
            lsp::DefinitionParams params;
            params.textDocument = lsp::TextDocumentIdentifier{gameUri};
            params.position = lsp::Position{1, 14};
            auto defResult = workspace.gotoDefinition(params, nullptr);
            (void)defResult;
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "shared_session_consumer_and_dependency_both_edited")
{
    // Simulates both the consumer and dependency being edited in quick succession,
    // with hover operations between each edit. This is the most common real
    // scenario: user has both files open and switches between them.

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);

    auto depUri = newDocument("Store.luau", R"(
        export type State = { count: number }
        local function getState(): State
            return { count = 0 }
        end
        return { getState = getState }
    )");
    auto depMod = workspace.fileResolver.getModuleName(depUri);
    robloxPlatform->addFileToIndex(depUri, depMod);

    auto viewUri = newDocument("View.luau", R"(
        local Store = shared("Store")
        type State = Store.State
        local state = Store.getState()
    )");
    auto viewMod = workspace.fileResolver.getModuleName(viewUri);

    // Initial check + hover
    workspace.checkSimple(viewMod, nullptr);
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{viewUri};
        params.position = lsp::Position{3, 14};
        CHECK(workspace.hover(params, nullptr).has_value());
    }

    // Edit dependency: add field
    workspace.frontend.markDirty(depMod);
    updateDocument(depUri, R"(
        export type State = { count: number, loading: boolean }
        local function getState(): State
            return { count = 0, loading = false }
        end
        return { getState = getState }
    )");

    // Hover on consumer before re-checking (checkStrict will pick up dirty dep)
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{viewUri};
        params.position = lsp::Position{3, 14};
        CHECK(workspace.hover(params, nullptr).has_value());
    }

    // Edit consumer: add usage of new field
    workspace.frontend.markDirty(viewMod);
    updateDocument(viewUri, R"(
        local Store = shared("Store")
        type State = Store.State
        local state = Store.getState()
        local isLoading = state.loading
    )");

    // Diagnostics on consumer
    workspace.checkSimple(viewMod, nullptr);

    // Hover on new variable
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{viewUri};
        params.position = lsp::Position{4, 14};
        CHECK(workspace.hover(params, nullptr).has_value());
    }

    // Edit dependency again
    workspace.frontend.markDirty(depMod);
    updateDocument(depUri, R"(
        export type State = { count: number, loading: boolean, error: string? }
        local function getState(): State
            return { count = 0, loading = false, error = nil }
        end
        return { getState = getState }
    )");

    // Edit consumer to use error field
    workspace.frontend.markDirty(viewMod);
    updateDocument(viewUri, R"(
        local Store = shared("Store")
        type State = Store.State
        local state = Store.getState()
        local isLoading = state.loading
        local err = state.error
    )");

    workspace.checkSimple(viewMod, nullptr);

    // Final hover on err
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{viewUri};
        params.position = lsp::Position{5, 14};
        CHECK(workspace.hover(params, nullptr).has_value());
    }
}

TEST_CASE_FIXTURE(Fixture, "shared_session_full_lifecycle_with_new_solver")
{
    ENABLE_NEW_SOLVER();

    // Full lifecycle under the new solver: diagnostics (which use checkStrict
    // in new solver), hover, dependency edits, clear, recheck.
    // The new solver uses moduleResolver (not moduleResolverForAutocomplete).

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);

    auto typesUri = newDocument("NetTypes.luau", R"(
        export type Packet = { kind: string, data: string, seq: number }
        return {}
    )");
    auto typesMod = workspace.fileResolver.getModuleName(typesUri);
    robloxPlatform->addFileToIndex(typesUri, typesMod);

    auto clientUri = newDocument("NetClient.luau", R"(
        local NetTypes = shared("NetTypes")
        type Packet = NetTypes.Packet
        local function send(pkt: Packet) end
        local function recv(): Packet
            return { kind = "data", data = "", seq = 0 }
        end
        return { send = send, recv = recv }
    )");
    auto clientMod = workspace.fileResolver.getModuleName(clientUri);
    robloxPlatform->addFileToIndex(clientUri, clientMod);

    auto appUri = newDocument("NetApp.luau", R"(
        local NetClient = shared("NetClient")
        local NetTypes = shared("NetTypes")
        type Packet = NetTypes.Packet
        local pkt: Packet = NetClient.recv()
        NetClient.send(pkt)
    )");
    auto appMod = workspace.fileResolver.getModuleName(appUri);

    // documentDiagnostics path (new solver uses checkStrict)
    workspace.checkStrict(appMod, nullptr, false);

    // Hover
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{appUri};
        params.position = lsp::Position{4, 14};
        CHECK(workspace.hover(params, nullptr).has_value());
    }

    // Edit dependency type
    workspace.frontend.markDirty(typesMod);
    updateDocument(typesUri, R"(
        export type Packet = { kind: string, data: string, seq: number, ack: boolean }
        return {}
    )");

    // Re-diagnostics (new solver path)
    workspace.checkStrict(appMod, nullptr, false);

    // Hover after edit
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{appUri};
        params.position = lsp::Position{4, 14};
        CHECK(workspace.hover(params, nullptr).has_value());
    }

    // Clear and recheck
    workspace.frontend.clear();
    workspace.checkStrict(appMod, nullptr, false);

    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{appUri};
        params.position = lsp::Position{4, 14};
        CHECK(workspace.hover(params, nullptr).has_value());
    }

    // Verify retainedModules on consumer
    auto mod = workspace.frontend.moduleResolver.getModule(appMod);
    REQUIRE(mod);
    CHECK(!mod->retainedModules.empty());
}

TEST_CASE_FIXTURE(Fixture, "shared_goto_definition_on_type_from_transitive_dependency")
{
    // Reproduces the real crash: ServerSmallBoat uses shared("ServerEnemy"),
    // ServerEnemy itself uses many shared() imports (Dumpster, Signal, etc.),
    // and go-to-definition on a type reference like ServerEnemy.ServerEnemyOptions
    // crashes with VisitType.h(439) assertion.
    //
    // The key pattern is:
    //   1. Multiple modules checked via checkSimple (workspace diagnostics / indexing)
    //   2. Go-to-definition triggers checkStrict with forAutocomplete=true
    //   3. The dependency (ServerEnemy) has transitive shared() dependencies
    //   4. Type traversal hits a dangling TypeId from a transitive dependency

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);

    // Transitive dependency: Dumpster (used by ServerEnemy)
    auto dumpsterUri = newDocument("Dumpster.luau", R"(
        export type Dumpster = {
            new: () -> Dumpster,
            Add: (self: Dumpster, item: any) -> (),
            Destroy: (self: Dumpster) -> (),
        }
        local Dumpster = {}
        Dumpster.__index = Dumpster
        function Dumpster.new(): Dumpster
            return setmetatable({}, Dumpster) :: any
        end
        function Dumpster:Add(item: any) end
        function Dumpster:Destroy() end
        return Dumpster
    )");
    auto dumpsterMod = workspace.fileResolver.getModuleName(dumpsterUri);
    robloxPlatform->addFileToIndex(dumpsterUri, dumpsterMod);

    // Transitive dependency: Signal (used by ServerEnemy)
    auto signalUri = newDocument("Signal.luau", R"(
        export type Signal<T...> = {
            Fire: (self: Signal<T...>, T...) -> (),
            Connect: (self: Signal<T...>, fn: (T...) -> ()) -> (),
        }
        local Signal = {}
        Signal.__index = Signal
        function Signal.new()
            return setmetatable({}, Signal)
        end
        function Signal:Fire(...) end
        function Signal:Connect(fn) end
        return Signal
    )");
    auto signalMod = workspace.fileResolver.getModuleName(signalUri);
    robloxPlatform->addFileToIndex(signalUri, signalMod);

    // Transitive dependency: GetRemote (used by ServerEnemy)
    auto getRemoteUri = newDocument("GetRemote.luau", R"(
        export type Remote = {
            FireClient: (self: Remote, player: any, ...any) -> (),
            FireAllClients: (self: Remote, ...any) -> (),
        }
        local function GetRemote(name: string): Remote
            return {} :: any
        end
        return GetRemote
    )");
    auto getRemoteMod = workspace.fileResolver.getModuleName(getRemoteUri);
    robloxPlatform->addFileToIndex(getRemoteUri, getRemoteMod);

    // Mid-level dependency: ServerEnemy (uses Dumpster, Signal, GetRemote via shared())
    auto serverEnemyUri = newDocument("ServerEnemy.luau", R"(
        local Dumpster = shared("Dumpster")
        local Signal = shared("Signal")
        local GetRemote = shared("GetRemote")

        type Dumpster = Dumpster.Dumpster
        type Signal<T...> = Signal.Signal<T...>
        type Remote = GetRemote.Remote

        export type ServerEnemyDifficultyData = {
            NamePrefix: string?,
            Level: NumberRange,
        }

        export type ServerEnemyOptions = {
            Model: Model?,
            Parent: Instance,
            CFrame: CFrame?,
            HitboxSize: Vector3?,
            Scale: number?,
            DoInit: boolean?,
            DifficultyData: ServerEnemyDifficultyData?,
        }

        local ServerEnemy = {}
        ServerEnemy.__index = ServerEnemy

        function ServerEnemy.new(options: ServerEnemyOptions): ServerEnemy
            local self = setmetatable({
                _dumpster = Dumpster.new() :: Dumpster,
                Died = Signal.new() :: Signal<any>,
                Destroyed = Signal.new() :: Signal<nil>,
            }, ServerEnemy)
            return self
        end

        function ServerEnemy:Destroy()
            self._dumpster:Destroy()
        end

        export type ServerEnemy = typeof(ServerEnemy)
        return ServerEnemy
    )");
    auto serverEnemyMod = workspace.fileResolver.getModuleName(serverEnemyUri);
    robloxPlatform->addFileToIndex(serverEnemyUri, serverEnemyMod);

    // Mid-level dependency: ServerBoat (uses ServerEnemy via shared())
    auto serverBoatUri = newDocument("ServerBoat.luau", R"(
        local ServerEnemy = shared("ServerEnemy")
        type ServerEnemyOptions = ServerEnemy.ServerEnemyOptions

        local ServerBoat = setmetatable({}, ServerEnemy)
        ServerBoat.__index = ServerBoat

        function ServerBoat.new(options: ServerEnemyOptions): ServerBoat
            local self = setmetatable(ServerEnemy.new(options), ServerBoat)
            return self
        end

        export type ServerBoat = typeof(ServerBoat)
        return ServerBoat
    )");
    auto serverBoatMod = workspace.fileResolver.getModuleName(serverBoatUri);
    robloxPlatform->addFileToIndex(serverBoatUri, serverBoatMod);

    // Consumer: ServerSmallBoat (uses ServerBoat and ServerEnemy via shared())
    auto consumerUri = newDocument("ServerSmallBoat.luau", R"(
        local ServerBoat = shared("ServerBoat")
        local ServerEnemy = shared("ServerEnemy")

        type ServerEnemyOptions = ServerEnemy.ServerEnemyOptions

        local ServerSmallBoat = setmetatable({}, ServerBoat)
        ServerSmallBoat.__index = ServerSmallBoat

        function ServerSmallBoat.new(options: ServerEnemyOptions): ServerSmallBoat
            local self = setmetatable(ServerBoat.new(options), ServerSmallBoat)
            return self
        end

        export type ServerSmallBoat = typeof(ServerSmallBoat)
        return ServerSmallBoat
    )");
    auto consumerMod = workspace.fileResolver.getModuleName(consumerUri);

    // Step 1: checkSimple on ALL modules (simulates workspace indexing + diagnostics)
    // This is what happens when the LSP starts: it indexes and checks all 1654 files
    workspace.checkSimple(dumpsterMod, nullptr);
    workspace.checkSimple(signalMod, nullptr);
    workspace.checkSimple(getRemoteMod, nullptr);
    workspace.checkSimple(serverEnemyMod, nullptr);
    workspace.checkSimple(serverBoatMod, nullptr);
    workspace.checkSimple(consumerMod, nullptr);

    // Step 2: Go-to-definition on "ServerEnemyOptions" in the type reference
    // `type ServerEnemyOptions = ServerEnemy.ServerEnemyOptions` (line 4, col 40)
    // This calls checkStrict (forAutocomplete=true) then accesses the imported module
    {
        lsp::DefinitionParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{4, 40}; // on "ServerEnemyOptions" after the dot
        auto defResult = workspace.gotoDefinition(params, nullptr);
        // Should not crash — and should find the definition
        (void)defResult;
    }

    // Step 3: Hover on the same type reference
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{4, 40};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }

    // Step 4: Hover on the consumer's function parameter that uses the type
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{9, 40}; // "options" parameter
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }
}

TEST_CASE_FIXTURE(Fixture, "shared_goto_definition_type_ref_with_many_transitive_deps")
{
    // Even more aggressive: the dependency has MANY shared() imports,
    // all with exported types. This stresses the module retention under
    // the old solver where retainedModules is not populated.

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);

    // Create 6 transitive dependencies, each with exported types
    std::vector<std::pair<Uri, Luau::ModuleName>> transDeps;
    for (int i = 0; i < 6; ++i)
    {
        std::string name = "TransDep" + std::to_string(i);
        std::string src = "export type T" + std::to_string(i) + " = { val" + std::to_string(i) + ": number }\n"
                          "return { create = function(): T" + std::to_string(i) + " return { val" + std::to_string(i) + " = " + std::to_string(i) +
                          " } end }";
        auto uri = newDocument(name + ".luau", src);
        auto mod = workspace.fileResolver.getModuleName(uri);
        robloxPlatform->addFileToIndex(uri, mod);
        transDeps.push_back({uri, mod});
    }

    // Hub module that imports all transitive deps via shared()
    std::string hubSrc;
    for (int i = 0; i < 6; ++i)
    {
        hubSrc += "local TransDep" + std::to_string(i) + " = shared(\"TransDep" + std::to_string(i) + "\")\n";
    }
    for (int i = 0; i < 6; ++i)
    {
        hubSrc += "type T" + std::to_string(i) + " = TransDep" + std::to_string(i) + ".T" + std::to_string(i) + "\n";
    }
    hubSrc += "export type HubConfig = {\n";
    for (int i = 0; i < 6; ++i)
    {
        hubSrc += "    dep" + std::to_string(i) + ": T" + std::to_string(i) + ",\n";
    }
    hubSrc += "}\n";
    hubSrc += "return {}\n";

    auto hubUri = newDocument("Hub.luau", hubSrc);
    auto hubMod = workspace.fileResolver.getModuleName(hubUri);
    robloxPlatform->addFileToIndex(hubUri, hubMod);

    // Consumer that uses Hub's exported type
    auto consumerUri = newDocument("Consumer.luau", R"(
        local Hub = shared("Hub")
        type HubConfig = Hub.HubConfig
    )");
    auto consumerMod = workspace.fileResolver.getModuleName(consumerUri);

    // checkSimple on ALL modules (like workspace indexing)
    for (auto& [uri, mod] : transDeps)
        workspace.checkSimple(mod, nullptr);
    workspace.checkSimple(hubMod, nullptr);
    workspace.checkSimple(consumerMod, nullptr);

    // Go-to-definition on "HubConfig" type reference
    {
        lsp::DefinitionParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{2, 30};
        auto defResult = workspace.gotoDefinition(params, nullptr);
        (void)defResult;
    }

    // Hover on the type
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{2, 14};
        auto hoverResult = workspace.hover(params, nullptr);
        CHECK(hoverResult.has_value());
    }

    // Now edit a transitive dep and repeat
    workspace.frontend.markDirty(transDeps[0].second);
    updateDocument(transDeps[0].first, R"(
        export type T0 = { val0: number, extra: string }
        return { create = function(): T0 return { val0 = 0, extra = "new" } end }
    )");

    workspace.checkSimple(consumerMod, nullptr);

    // Go-to-definition again after transitive dep changed
    {
        lsp::DefinitionParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{2, 30};
        auto defResult = workspace.gotoDefinition(params, nullptr);
        (void)defResult;
    }
}

TEST_CASE_FIXTURE(Fixture, "shared_checkSimple_all_then_goto_def_type_alias")
{
    // Minimal reproduction of the crash pattern:
    // 1. Dependency exports a complex type
    // 2. Consumer creates a type alias from the dependency
    // 3. checkSimple on both (discards type graphs)
    // 4. go-to-definition on the type alias (triggers checkStrict forAutocomplete=true)
    // This is the exact sequence that happens in real LSP usage.

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);

    auto depUri = newDocument("EnemyTypes.luau", R"(
        export type DifficultyData = {
            NamePrefix: string?,
            Level: NumberRange,
            Experience: NumberRange,
            MaxHealth: NumberRange,
            Coins: NumberRange,
        }
        export type EnemyOptions = {
            Model: Model?,
            Parent: Instance,
            CFrame: CFrame?,
            HitboxSize: Vector3?,
            Scale: number?,
            DoInit: boolean?,
            DifficultyData: DifficultyData?,
        }
        return {}
    )");
    auto depMod = workspace.fileResolver.getModuleName(depUri);
    robloxPlatform->addFileToIndex(depUri, depMod);

    auto consumerUri = newDocument("EnemyConsumer.luau", R"(
        local EnemyTypes = shared("EnemyTypes")
        type EnemyOptions = EnemyTypes.EnemyOptions
        local function spawn(opts: EnemyOptions) end
        return { spawn = spawn }
    )");
    auto consumerMod = workspace.fileResolver.getModuleName(consumerUri);

    // checkSimple on both (like workspace diagnostics)
    workspace.checkSimple(depMod, nullptr);
    workspace.checkSimple(consumerMod, nullptr);

    // go-to-definition on "EnemyOptions" after the dot (type reference)
    {
        lsp::DefinitionParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{2, 38};
        auto defResult = workspace.gotoDefinition(params, nullptr);
        (void)defResult;
    }

    // Hover on the type alias
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{2, 14};
        CHECK(workspace.hover(params, nullptr).has_value());
    }

    // Hover on the function parameter that uses the type
    {
        lsp::HoverParams params;
        params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
        params.position = lsp::Position{3, 30};
        CHECK(workspace.hover(params, nullptr).has_value());
    }
}

TEST_CASE_FIXTURE(Fixture, "shared_old_solver_retains_dependency_modules")
{
    // The old solver must retain shared() dependency modules to prevent their
    // interfaceTypes arenas from being freed while the consumer still holds
    // TypeIds pointing into them. Without retention, replacing a dependency
    // module (e.g., after markDirty + recheck) causes a use-after-free crash
    // in VisitType.h(439): "GenericTypeVisitor::traverse(TypeId) is not exhaustive!"

    auto depUri = newDocument("RetainDep.luau", R"(
        export type Config = { name: string, value: number }
        return { defaultConfig = { name = "test", value = 0 } }
    )");
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto depMod = workspace.fileResolver.getModuleName(depUri);
    robloxPlatform->addFileToIndex(depUri, depMod);

    auto consumerUri = newDocument("RetainConsumer.luau", R"(
        local Dep = shared("RetainDep")
        type Config = Dep.Config
        local cfg: Config = Dep.defaultConfig
    )");
    auto consumerMod = workspace.fileResolver.getModuleName(consumerUri);

    // Check consumer (this also checks the dependency as part of the build queue)
    Luau::FrontendOptions opts{true, false, true};
    workspace.frontend.check(consumerMod, opts);

    auto consumerModule = workspace.frontend.moduleResolver.getModule(consumerMod);
    REQUIRE(consumerModule);

    // The consumer must retain its shared() dependency module so that TypeIds
    // from the dependency's interfaceTypes remain valid even if the dependency
    // is later replaced in the resolver.
    CHECK(!consumerModule->retainedModules.empty());

    // Verify the retained module is actually the dependency
    bool foundDep = false;
    for (const auto& retained : consumerModule->retainedModules)
    {
        if (retained->name == depMod)
            foundDep = true;
    }
    CHECK(foundDep);
}

TEST_CASE_FIXTURE(Fixture, "shared_old_solver_dependency_replacement_survives")
{
    // End-to-end test: after checking consumer for autocomplete, replace the
    // dependency by marking dirty + rechecking. Then access the consumer's
    // stale types. Without retention, the dependency's interfaceTypes would be
    // freed and type traversal would crash.

    auto depUri = newDocument("ReplaceDep.luau", R"(
        export type Item = { id: number, label: string }
        return { create = function(): Item return { id = 1, label = "a" } end }
    )");
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto depMod = workspace.fileResolver.getModuleName(depUri);
    robloxPlatform->addFileToIndex(depUri, depMod);

    auto consumerUri = newDocument("ReplaceConsumer.luau", R"(
        local Dep = shared("ReplaceDep")
        type Item = Dep.Item
        local item = Dep.create()
        return item
    )");
    auto consumerMod = workspace.fileResolver.getModuleName(consumerUri);

    // Check consumer for autocomplete
    Luau::FrontendOptions acOpts{true, true, false};
    workspace.frontend.check(consumerMod, acOpts);

    // The new solver uses moduleResolver (not moduleResolverForAutocomplete).
    auto& resolver = FFlag::LuauSolverV2 ? workspace.frontend.moduleResolver : workspace.frontend.moduleResolverForAutocomplete;
    auto consumerModule = resolver.getModule(consumerMod);
    REQUIRE(consumerModule);

    // The consumer must retain the dependency
    CHECK(!consumerModule->retainedModules.empty());

    // Now mark the dependency dirty and recheck it (simulates file edit)
    workspace.frontend.markDirty(depMod);
    updateDocument(depUri, R"(
        export type Item = { id: number, label: string, extra: boolean }
        return { create = function(): Item return { id = 1, label = "a", extra = true } end }
    )");

    // Recheck ONLY the dependency for autocomplete — this replaces the old
    // dependency module in the resolver. Without retention, the old module's
    // interfaceTypes arena would be freed.
    Luau::FrontendOptions depOpts{true, true, false};
    workspace.frontend.check(depMod, depOpts);

    // The consumer module is stale (dirtyModuleForAutocomplete=true) but
    // we access its types without rechecking. The retained module prevents
    // the old dependency's interfaceTypes from being freed.
    Luau::ToStringOptions toStringOpts;
    toStringOpts.exhaustive = true;

    // This traverses the consumer's return type, which references TypeIds
    // from the old dependency's interfaceTypes. Without retention, this
    // would crash with VisitType.h(439) assertion or segfault.
    auto retStr = Luau::toString(consumerModule->returnType, toStringOpts);
    CHECK(!retStr.empty());
}

TEST_SUITE_END();
