#include "doctest.h"
#include "Fixture.h"

#include "LSP/SharedRequire.hpp"

TEST_SUITE_BEGIN("SharedRequire");

static constexpr const char* kTestModuleSource = R"(
    local M = {}

    --- Does the thing.
    function M.someFunction(foo: string): boolean
        print(foo)
        return true
    end

    export type Options = { verbose: boolean }

    return M
)";

TEST_CASE_FIXTURE(Fixture, "shared_resolves_module_return_type")
{
    newDocument("TestModule.luau", kTestModuleSource);

    auto result = check(R"(
        local TestModule = shared("TestModule")
        local ok: boolean = TestModule.someFunction("hello, world!")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);

    // The blind spot in the previous test suite: indexing or calling `any` produces no error,
    // so a completely unresolved shared() still passed. Assert the resolved shape directly.
    auto ty = Luau::follow(requireType("TestModule"));
    CHECK_MESSAGE(Luau::get<Luau::TableType>(ty) != nullptr, "expected a table, got " << Luau::toString(ty));
}

TEST_CASE_FIXTURE(Fixture, "shared_resolves_to_the_dependency_module_return_type")
{
    auto moduleUri = newDocument("TestModule.luau", kTestModuleSource);

    auto result = check(R"(
        local TestModule = shared("TestModule")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);

    // What `require` would have produced: the dependency module's own return type.
    auto dependency = getModule(workspace.fileResolver.getModuleName(moduleUri));
    REQUIRE(dependency);
    auto expected = Luau::first(dependency->returnType);
    REQUIRE(expected);

    CHECK(Luau::toString(Luau::follow(requireType("TestModule"))) == Luau::toString(Luau::follow(*expected)));
}

TEST_CASE_FIXTURE(Fixture, "shared_imports_exported_type_bindings")
{
    newDocument("TestModule.luau", kTestModuleSource);

    // Exercises the two patched copies of Luau::matchRequire. Without them this reports
    // "Unknown type 'TestModule.Options'".
    auto result = check(R"(
        local TestModule = shared("TestModule")
        local opts: TestModule.Options = { verbose = true }
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
}

TEST_CASE_FIXTURE(Fixture, "shared_resolves_module_return_type_with_new_solver")
{
    ENABLE_NEW_SOLVER();

    // The Fixture registers globals in its constructor, i.e. while still in old-solver mode.
    // Production registers them after the fflags from client configuration are applied, so
    // rebind here to reproduce the real ordering.
    LSP::SharedRequire::registerGlobal(workspace.frontend.globals);

    newDocument("TestModule.luau", kTestModuleSource);

    auto result = check(R"(
        local TestModule = shared("TestModule")
        local ok: boolean = TestModule.someFunction("hello, world!")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);

    auto ty = Luau::follow(requireType("TestModule"));
    CHECK_MESSAGE(Luau::get<Luau::TableType>(ty) != nullptr, "expected a table, got " << Luau::toString(ty));
}

TEST_CASE_FIXTURE(Fixture, "shared_remains_usable_as_a_table")
{
    // Roblox's `shared` is a shared table. Making it callable must not take that away.
    auto result = check(R"(
        shared.someKey = 42
        local value = shared.someKey
        local other = shared["anotherKey"]
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
}

TEST_CASE_FIXTURE(Fixture, "shared_resolves_partial_path")
{
    newDocument("jobs/TestModule.luau", kTestModuleSource);

    auto result = check(R"(
        local TestModule = shared("jobs/TestModule")
        local ok: boolean = TestModule.someFunction("hello, world!")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::get<Luau::TableType>(Luau::follow(requireType("TestModule"))) != nullptr);
}

TEST_CASE_FIXTURE(Fixture, "shared_is_case_insensitive")
{
    newDocument("TestModule.luau", kTestModuleSource);

    auto result = check(R"(
        local TestModule = shared("testmodule")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::get<Luau::TableType>(Luau::follow(requireType("TestModule"))) != nullptr);
}

TEST_CASE_FIXTURE(Fixture, "shared_prefers_nearest_sibling_on_ambiguity")
{
    newDocument("far/away/Target.luau", "return { which = \"far\" }");
    newDocument("near/Target.luau", "return { which = \"near\" }");

    auto consumerUri = newDocument("near/Consumer.luau", R"(
        local Target = shared("Target")
        return Target
    )");

    auto result = workspace.frontend.check(workspace.fileResolver.getModuleName(consumerUri));
    LUAU_LSP_REQUIRE_NO_ERRORS(result);

    auto module = getModule(workspace.fileResolver.getModuleName(consumerUri));
    REQUIRE(module);
    auto ty = requireType(module, "Target");
    CHECK(Luau::get<Luau::TableType>(Luau::follow(ty)) != nullptr);

    // The sibling in `near/` shares more of its path with the consumer than the one in `far/away/`.
    auto resolved = workspace.platform->sharedRequireIndex.resolve(
        "Target", workspace.fileResolver.getModuleName(consumerUri), workspace.fileResolver);
    CHECK(resolved.status == LSP::SharedRequire::ResolveStatus::Found);
    CHECK(resolved.moduleName == workspace.fileResolver.getModuleName(workspace.rootUri.resolvePath("near/Target.luau")));
}

TEST_CASE_FIXTURE(Fixture, "shared_reports_unknown_module")
{
    auto result = check(R"(
        local Missing = shared("NoSuchModule")
    )");

    LUAU_LSP_REQUIRE_ERROR_COUNT(1, result);
    CHECK_MESSAGE(Luau::toString(result.errors[0]).find("NoSuchModule") != std::string::npos,
        "expected the error to name the module, got " << Luau::toString(result.errors[0]));
}

TEST_CASE_FIXTURE(Fixture, "shared_indexes_init_luau_under_parent_directory_name")
{
    newDocument("Packages/Signal/init.luau", kTestModuleSource);

    auto result = check(R"(
        local Signal = shared("Signal")
        local ok: boolean = Signal.someFunction("hello, world!")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::get<Luau::TableType>(Luau::follow(requireType("Signal"))) != nullptr);
}

TEST_CASE_FIXTURE(Fixture, "shared_hover_resolves_member_function_signature")
{
    newDocument("TestModule.luau", kTestModuleSource);

    auto consumerUri = newDocument("Consumer.luau", R"(
local TestModule = shared("TestModule")
TestModule.someFunction("hello, world!")
)");

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
    params.position = lsp::Position{2, 12}; // over `someFunction`

    auto hoverResult = workspace.hover(params, nullptr);
    REQUIRE(hoverResult.has_value());

    // Previously every hover test asserted only has_value(), which passes even when the
    // member resolved to `any`. Assert the content so a regression is actually caught.
    const bool mentionsSignature =
        hoverResult->contents.value.find("string") != std::string::npos && hoverResult->contents.value.find("boolean") != std::string::npos;
    CHECK_MESSAGE(mentionsSignature, "expected a resolved signature, got " << hoverResult->contents.value);
}

TEST_CASE_FIXTURE(Fixture, "shared_supports_goto_definition")
{
    auto moduleUri = newDocument("TestModule.luau", kTestModuleSource);
    auto consumerUri = newDocument("Consumer.luau", R"(
local TestModule = shared("TestModule")
)");

    lsp::DefinitionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{consumerUri};
    params.position = lsp::Position{1, 28}; // inside the "TestModule" string literal

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE(result.size() == 1);
    CHECK(result[0].uri == moduleUri);
}

TEST_CASE_FIXTURE(Fixture, "shared_produces_a_document_link")
{
    auto moduleUri = newDocument("TestModule.luau", kTestModuleSource);
    auto consumerUri = newDocument("Consumer.luau", R"(
local TestModule = shared("TestModule")
)");

    lsp::DocumentLinkParams params;
    params.textDocument = lsp::TextDocumentIdentifier{consumerUri};

    auto links = workspace.documentLink(params);
    REQUIRE(links.size() == 1);
    CHECK(links[0].target == moduleUri);
}

TEST_SUITE_END();
