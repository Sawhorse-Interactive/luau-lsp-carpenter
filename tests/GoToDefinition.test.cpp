#include "doctest.h"
#include "Fixture.h"

#include "LSP/IostreamHelpers.hpp"
#include "Platform/RobloxPlatform.hpp"

TEST_SUITE_BEGIN("GoToDefinition");

TEST_CASE_FIXTURE(Fixture, "local_variable_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local value = 5

        local y = val|ue
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 14});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 19});
}

TEST_CASE_FIXTURE(Fixture, "local_variable_definition_pointing_to_a_table")
{
    auto [source, position] = sourceWithMarker(R"(
        local process = { call = function() end }
        local value = pro|cess.call()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{1, 14});
    CHECK_EQ(result[0].range.end, lsp::Position{1, 21});
}

TEST_CASE_FIXTURE(Fixture, "local_inlined_primitive_table_property_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local T = {
            value = 5
        }

        local y = T.val|ue
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 17});
}

TEST_CASE_FIXTURE(Fixture, "local_separate_primitive_table_property_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local T = {}
        T.value = 5

        local y = T.val|ue
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 10});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 15});
}

TEST_CASE_FIXTURE(Fixture, "local_inlined_function_table_property_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local T = {
            useFunction = function() end
        }

        local y = T.useFu|nction()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 23});
}

TEST_CASE_FIXTURE(Fixture, "local_separate_anonymous_function_table_property_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local T = {}
        T.useFunction = function()
        end

        local y = T.useFu|nction()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 10});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 21});
}

TEST_CASE_FIXTURE(Fixture, "local_named_function_table_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local T = {}
        function T.useFunction()
        end

        local y = T.useFu|nction()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 19});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 30});
}

TEST_CASE_FIXTURE(Fixture, "type_alias_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        type Foo = string

        local y: Fo|o = ""
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 8});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 25});
}

TEST_CASE_FIXTURE(Fixture, "methods_on_explicitly_defined_self")
{
    auto [source, position] = sourceWithMarker(R"(
        local Test = {}
        Test.__index = Test

        function Test.new()
            local self = setmetatable({}, Test)
            return self
        end

        function Test.someFunc(self: Test)
        end

        function Test:anotherFunc()
        end

        function Test.anotherCallingFunc(self: Test)
            self:some|Func()
            self:anotherFunc()
        end

        export type Test = typeof(Test.new())

        return Test
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{9, 22});
    CHECK_EQ(result[0].range.end, lsp::Position{9, 30});

    params.position.line += 1;
    result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{12, 22});
    CHECK_EQ(result[0].range.end, lsp::Position{12, 33});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_inlined_function_table_property_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        return {
            useFunction = function(x: string)
            end,
        }
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 23});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_indirect_variable_inlined_function_table_property_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        local T = {
            useFunction = function(x: string)
            end,
        }
        return T
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 23});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_table_property_referencing_local_variable_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        local function useFunction(x: string)
        end

        return {
            useFunction = useFunction
        }
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{6, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{6, 23});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_separate_function_table_property_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        local T = {}
        T.useFunction = function(x: string)
        end

        return T
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 10});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 21});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_named_function_table_property_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        local T = {}
        function T.useFunction(x: string)
        end

        return T
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 19});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 30});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_imported_type_alias_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        export type Foo = string

        return {}
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y: utilities.Fo|o = ""
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 8});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 32});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_imported_type_alias_definition_after_check_without_retaining_type_graphs")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        export type Foo = string

        return {}
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y: utilities.Fo|o = ""
    )");
    auto document = newDocument("main.luau", source);

    // This test explicitly expects type graphs to not be retained (i.e., the required module scope was cleared)
    // We should still be able to find the type references.
    workspace.checkSimple(workspace.fileResolver.getModuleName(document), /* cancellationToken= */ nullptr);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 8});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 32});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_imported_type_alias_definition_after_check_without_retaining_type_graphs_goto_type_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        export type Foo = string

        return {}
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y: utilities.Fo|o = ""
    )");
    auto document = newDocument("main.luau", source);

    // This test explicitly expects type graphs to not be retained (i.e., the required module scope was cleared)
    // We should still be able to find the type references.
    workspace.checkSimple(workspace.fileResolver.getModuleName(document), /* cancellationToken= */ nullptr);

    auto params = lsp::TypeDefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoTypeDefinition(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->uri, required);
    CHECK_EQ(result->range.start, lsp::Position{2, 8});
    CHECK_EQ(result->range.end, lsp::Position{2, 32});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_of_a_named_function_returns_the_underlying_definition_and_not_the_require_stmt")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        local function useFunction(x: string)
        end

        return useFunction
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local useFunction = require(game.Testing.useFunction)

        local y = useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 23});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 34});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_of_an_anonymous_function_returns_the_underlying_definition_and_not_the_require_stmt")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        return function(x: string)
        end
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local useFunction = require(game.Testing.useFunction)

        local y = useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 15});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 15});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_works_for_a_string_require_path")
{
    auto [source, position] = sourceWithMarker(R"(
        local X = require("./te|st")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, workspace.rootUri.resolvePath("test.lua"));
    CHECK_EQ(result[0].range.start, lsp::Position{0, 0});
    CHECK_EQ(result[0].range.end, lsp::Position{0, 0});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_works_for_a_roblox_require_path")
{
    loadSourcemap(R"({
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [{ "name": "Test", "className": "ModuleScript", "filePaths": ["source.luau"] }]
            }
        ]
    })");

    auto [source, position] = sourceWithMarker(R"(
        local X = require(game.ReplicatedStorage.Te|st)
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, workspace.rootUri.resolvePath("source.luau"));
    CHECK_EQ(result[0].range.start, lsp::Position{0, 0});
    CHECK_EQ(result[0].range.end, lsp::Position{0, 0});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_two_levels_deep_reexport_function_property")
{
    // Module A defines the table with the function
    auto moduleA = newDocument("moduleA.luau", R"(
        --!strict
        local T = {}
        function T.useFunction(x: string)
        end

        return T
    )");
    registerDocumentForVirtualPath(moduleA, "game/Testing/ModuleA");

    // Module B re-exports Module A
    auto moduleB = newDocument("moduleB.luau", R"(
        --!strict
        return require(game.Testing.ModuleA)
    )");
    registerDocumentForVirtualPath(moduleB, "game/Testing/ModuleB");

    // Module C requires Module B and calls the method
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.ModuleB)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, moduleA);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 19});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 30});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_two_levels_deep_reexport_table_with_method")
{
    // Module A defines the table with the method
    auto moduleA = newDocument("moduleA.luau", R"(
        --!strict
        local T = {
            useFunction = function(x: string)
            end,
        }
        return T
    )");
    registerDocumentForVirtualPath(moduleA, "game/Testing/ModuleA");

    // Module B re-exports Module A
    auto moduleB = newDocument("moduleB.luau", R"(
        --!strict
        local A = require(game.Testing.ModuleA)
        return A
    )");
    registerDocumentForVirtualPath(moduleB, "game/Testing/ModuleB");

    // Module C requires Module B and calls the method
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.ModuleB)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, moduleA);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 23});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_three_levels_deep_reexport_function_property")
{
    // Module A defines the table with the function
    auto moduleA = newDocument("moduleA.luau", R"(
        --!strict
        local T = {}
        function T.useFunction(x: string)
        end

        return T
    )");
    registerDocumentForVirtualPath(moduleA, "game/Testing/ModuleA");

    // Module B re-exports Module A
    auto moduleB = newDocument("moduleB.luau", R"(
        --!strict
        return require(game.Testing.ModuleA)
    )");
    registerDocumentForVirtualPath(moduleB, "game/Testing/ModuleB");

    // Module C re-exports Module B
    auto moduleC = newDocument("moduleC.luau", R"(
        --!strict
        return require(game.Testing.ModuleB)
    )");
    registerDocumentForVirtualPath(moduleC, "game/Testing/ModuleC");

    // Module D requires Module C and calls the method
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.ModuleC)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, moduleA);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 19});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 30});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_two_levels_deep_wrapper_table")
{
    // Module A defines a table with a function
    auto moduleA = newDocument("moduleA.luau", R"(
        --!strict
        local T = {}
        function T.useFunction(x: string)
        end

        return T
    )");
    registerDocumentForVirtualPath(moduleA, "game/Testing/ModuleA");

    // Module B wraps Module A's export into a new table
    auto moduleB = newDocument("moduleB.luau", R"(
        --!strict
        local A = require(game.Testing.ModuleA)
        return { tools = A }
    )");
    registerDocumentForVirtualPath(moduleB, "game/Testing/ModuleB");

    // Module C requires Module B and calls the nested method
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local B = require(game.Testing.ModuleB)

        local y = B.tools.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, moduleA);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 19});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 30});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_two_levels_deep_oop_method")
{
    // Module A defines a class with OOP pattern
    auto moduleA = newDocument("moduleA.luau", R"(
        --!strict
        local Class = {}
        Class.__index = Class

        function Class.new()
            return setmetatable({}, Class)
        end

        function Class:method()
        end

        return Class
    )");
    registerDocumentForVirtualPath(moduleA, "game/Testing/ModuleA");

    // Module B re-exports Module A
    auto moduleB = newDocument("moduleB.luau", R"(
        --!strict
        return require(game.Testing.ModuleA)
    )");
    registerDocumentForVirtualPath(moduleB, "game/Testing/ModuleB");

    // Module C requires Module B and calls the method on an instance
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local Class = require(game.Testing.ModuleB)
        local obj = Class.new()

        obj:met|hod()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, moduleA);
}

TEST_CASE_FIXTURE(Fixture, "cross_module_two_levels_deep_type_annotation")
{
    // Module A defines an exported type
    auto moduleA = newDocument("moduleA.luau", R"(
        --!strict
        export type Config = {
            init: (self: Config) -> (),
            update: (self: Config, dt: number) -> (),
        }

        return {}
    )");
    registerDocumentForVirtualPath(moduleA, "game/Testing/ModuleA");

    // Module B requires Module A and re-exports the type
    auto moduleB = newDocument("moduleB.luau", R"(
        --!strict
        local Types = require(game.Testing.ModuleA)

        export type Config = Types.Config

        return {}
    )");
    registerDocumentForVirtualPath(moduleB, "game/Testing/ModuleB");

    // Module C uses Module B's type and tries to go to the property definition
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local B = require(game.Testing.ModuleB)

        local cfg: B.Config = {} :: any
        cfg:in|it()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, moduleA);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 16});
}

TEST_CASE_FIXTURE(Fixture, "shared_go_to_definition_on_method_two_levels_deep")
{
    // Module A defines the table with the function
    auto moduleA = newDocument("ModuleA.luau", R"(
        --!strict
        local T = {}
        function T.useFunction(x: string)
        end

        return T
    )");
    registerDocumentForVirtualPath(moduleA, "game/Testing/ModuleA");

    // Module B requires Module A and re-exports
    auto moduleB = newDocument("ModuleB.luau", R"(
        --!strict
        return require(game.Testing.ModuleA)
    )");
    registerDocumentForVirtualPath(moduleB, "game/Testing/ModuleB");

    // Add Module B to the shared file index
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto moduleBName = workspace.fileResolver.getModuleName(moduleB);
    robloxPlatform->addFileToIndex(moduleB, moduleBName);

    // Pre-check dependencies
    workspace.frontend.check(workspace.fileResolver.getModuleName(moduleA));
    workspace.frontend.check(moduleBName);

    // Module C uses shared("ModuleB") and calls the method
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = shared("ModuleB")

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, moduleA);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 19});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 30});
}

TEST_CASE_FIXTURE(Fixture, "shared_go_to_definition_on_method_direct")
{
    // Module A defines the table with the function
    auto moduleA = newDocument("DirectModule.luau", R"(
        --!strict
        local T = {}
        function T.useFunction(x: string)
        end

        return T
    )");

    // Add to the shared file index
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto moduleAName = workspace.fileResolver.getModuleName(moduleA);
    robloxPlatform->addFileToIndex(moduleA, moduleAName);

    // Pre-check
    workspace.frontend.check(moduleAName);

    // Module C uses shared("DirectModule") and calls the method
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = shared("DirectModule")

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, moduleA);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 19});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 30});
}

TEST_CASE_FIXTURE(Fixture, "shared_go_to_definition_on_method_oop_two_levels_deep")
{
    // Module A defines an OOP class
    auto moduleA = newDocument("ClassModule.luau", R"(
        --!strict
        local Class = {}
        Class.__index = Class

        function Class.new()
            return setmetatable({}, Class)
        end

        function Class:method()
        end

        return Class
    )");
    registerDocumentForVirtualPath(moduleA, "game/Testing/ClassModule");

    // Module B requires and re-exports Module A
    auto moduleB = newDocument("ClassProxy.luau", R"(
        --!strict
        return require(game.Testing.ClassModule)
    )");
    registerDocumentForVirtualPath(moduleB, "game/Testing/ClassProxy");

    // Add Module B to shared index
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto moduleBName = workspace.fileResolver.getModuleName(moduleB);
    robloxPlatform->addFileToIndex(moduleB, moduleBName);

    // Pre-check
    workspace.frontend.check(workspace.fileResolver.getModuleName(moduleA));
    workspace.frontend.check(moduleBName);

    // Module C uses shared, creates instance, calls method
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local Class = shared("ClassProxy")
        local obj = Class.new()

        obj:met|hod()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, moduleA);
}

TEST_CASE_FIXTURE(Fixture, "shared_go_to_definition_on_inlined_table_method_two_levels")
{
    // Module A defines a table with inline methods
    auto moduleA = newDocument("InlineModule.luau", R"(
        --!strict
        local T = {
            doStuff = function(x: string)
            end,
        }
        return T
    )");
    registerDocumentForVirtualPath(moduleA, "game/Testing/InlineModule");

    // Module B re-exports via variable
    auto moduleB = newDocument("InlineProxy.luau", R"(
        --!strict
        local A = require(game.Testing.InlineModule)
        return A
    )");
    registerDocumentForVirtualPath(moduleB, "game/Testing/InlineProxy");

    // Add Module B to shared index
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto moduleBName = workspace.fileResolver.getModuleName(moduleB);
    robloxPlatform->addFileToIndex(moduleB, moduleBName);

    // Pre-check
    workspace.frontend.check(workspace.fileResolver.getModuleName(moduleA));
    workspace.frontend.check(moduleBName);

    // Module C uses shared and calls the method
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utils = shared("InlineProxy")

        utils.doSt|uff("test")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, moduleA);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 19});
}

TEST_CASE_FIXTURE(Fixture, "shared_go_to_definition_on_named_function_two_levels")
{
    // Module A defines named functions on the table
    auto moduleA = newDocument("NamedFnModule.luau", R"(
        --!strict
        local T = {}
        function T.doStuff(x: string)
        end
        return T
    )");
    registerDocumentForVirtualPath(moduleA, "game/Testing/NamedFnModule");

    // Module B re-exports
    auto moduleB = newDocument("NamedFnProxy.luau", R"(
        --!strict
        local A = require(game.Testing.NamedFnModule)
        return A
    )");
    registerDocumentForVirtualPath(moduleB, "game/Testing/NamedFnProxy");

    // Add Module B to shared index
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto moduleBName = workspace.fileResolver.getModuleName(moduleB);
    robloxPlatform->addFileToIndex(moduleB, moduleBName);

    // Pre-check
    workspace.frontend.check(workspace.fileResolver.getModuleName(moduleA));
    workspace.frontend.check(moduleBName);

    // Module C uses shared and calls the method
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utils = shared("NamedFnProxy")

        utils.doSt|uff("test")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, moduleA);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 19});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 26});
}

TEST_CASE_FIXTURE(Fixture, "shared_go_to_definition_no_precheck_two_levels")
{
    // Module A defines the table with the function
    auto moduleA = newDocument("NoPrecheckA.luau", R"(
        --!strict
        local T = {}
        function T.useFunction(x: string)
        end
        return T
    )");
    registerDocumentForVirtualPath(moduleA, "game/Testing/NoPrecheckA");

    // Module B requires and re-exports Module A
    auto moduleB = newDocument("NoPrecheckB.luau", R"(
        --!strict
        return require(game.Testing.NoPrecheckA)
    )");
    registerDocumentForVirtualPath(moduleB, "game/Testing/NoPrecheckB");

    // Add Module B to shared index (but do NOT pre-check anything)
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto moduleBName = workspace.fileResolver.getModuleName(moduleB);
    robloxPlatform->addFileToIndex(moduleB, moduleBName);

    // Module C uses shared (no pre-checking of dependencies)
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utils = shared("NoPrecheckB")

        utils.useFu|nction("test")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, moduleA);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 19});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 30});
}

TEST_CASE_FIXTURE(Fixture, "shared_go_to_definition_after_checkSimple_then_checkStrict")
{
    // Mimics real LSP: diagnostics run checkSimple first, then hover/gotodef runs checkStrict
    auto moduleA = newDocument("SeqCheckA.luau", R"(
        --!strict
        local T = {}
        function T.useFunction(x: string)
        end
        return T
    )");
    registerDocumentForVirtualPath(moduleA, "game/Testing/SeqCheckA");

    auto moduleB = newDocument("SeqCheckB.luau", R"(
        --!strict
        return require(game.Testing.SeqCheckA)
    )");
    registerDocumentForVirtualPath(moduleB, "game/Testing/SeqCheckB");

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto moduleBName = workspace.fileResolver.getModuleName(moduleB);
    robloxPlatform->addFileToIndex(moduleB, moduleBName);

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utils = shared("SeqCheckB")

        utils.useFu|nction("test")
    )");
    auto document = newDocument("main.luau", source);
    auto mainModName = workspace.fileResolver.getModuleName(document);

    // Step 1: checkSimple (diagnostics path, does not retain type graphs)
    workspace.checkSimple(mainModName, nullptr);

    // Step 2: gotoDefinition (calls checkStrict internally)
    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, moduleA);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 19});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 30});
}

TEST_CASE_FIXTURE(Fixture, "shared_go_to_definition_with_sourcemap_two_levels")
{
    loadSourcemap(R"({
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [
                    {
                        "name": "ModuleA",
                        "className": "ModuleScript",
                        "filePaths": ["ModuleA.luau"]
                    },
                    {
                        "name": "ModuleB",
                        "className": "ModuleScript",
                        "filePaths": ["ModuleB.luau"]
                    }
                ]
            },
            {
                "name": "ServerScriptService",
                "className": "ServerScriptService",
                "children": [
                    {
                        "name": "Main",
                        "className": "Script",
                        "filePaths": ["main.luau"]
                    }
                ]
            }
        ]
    })");

    // Module A defines the table with the function
    auto moduleA = newDocument("ModuleA.luau", R"(
        --!strict
        local T = {}
        function T.useFunction(x: string)
        end
        return T
    )");

    // Module B requires and re-exports Module A
    auto moduleB = newDocument("ModuleB.luau", R"(
        --!strict
        return require(game.ReplicatedStorage.ModuleA)
    )");

    // Add Module B to shared file index
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    auto moduleBName = workspace.fileResolver.getModuleName(moduleB);
    robloxPlatform->addFileToIndex(moduleB, moduleBName);

    // Module C uses shared("ModuleB") and calls method
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utils = shared("ModuleB")

        utils.useFu|nction("test")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, moduleA);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 19});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 30});
}

TEST_CASE_FIXTURE(Fixture, "shared_go_to_definition_type_cast_property_two_levels")
{
    // Signal module defines a generic type with a Connect method
    auto signalModule = newDocument("Signal.luau", R"(
        export type Connection = {
            Disconnect: (self: Connection) -> (),
            Connected: boolean,
        }

        export type Signal<T... = ...any> = {
            Fire: (self: Signal<T...>, T...) -> (),
            Connect: (self: Signal<T...>, fn: (T...) -> ()) -> Connection,
            Once: (self: Signal<T...>, fn: (T...) -> ()) -> Connection,
            DisconnectAll: (self: Signal<T...>) -> (),
            Destroy: (self: Signal<T...>) -> (),
            Wait: (self: Signal<T...>) -> T...,
        }

        local Signal = {}

        function Signal.new(): Signal
            return {} :: any
        end

        return Signal
    )");
    registerDocumentForVirtualPath(signalModule, "game/Shared/Signal");

    // DragonAttacksClient uses shared("Signal") and creates Signal instances
    auto dragonModule = newDocument("DragonAttacksClient.luau", R"(
        local Signal = shared("Signal")

        local DragonAttacksClient = {}
        DragonAttacksClient.MeleeStarted = Signal.new() :: Signal.Signal
        DragonAttacksClient.ProjectileShot = Signal.new() :: Signal.Signal

        return DragonAttacksClient
    )");
    registerDocumentForVirtualPath(dragonModule, "game/Client/DragonAttacksClient");

    // Set up shared file index for both modules
    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    robloxPlatform->addFileToIndex(signalModule, workspace.fileResolver.getModuleName(signalModule));
    robloxPlatform->addFileToIndex(dragonModule, workspace.fileResolver.getModuleName(dragonModule));

    // Pre-check
    workspace.frontend.check(workspace.fileResolver.getModuleName(signalModule));
    workspace.frontend.check(workspace.fileResolver.getModuleName(dragonModule));

    // FtuxEnemiesController uses shared("DragonAttacksClient") and calls :Connect
    auto [source, position] = sourceWithMarker(R"(
        local DragonAttacksClient = shared("DragonAttacksClient")

        DragonAttacksClient.ProjectileShot:Conn|ect(function()
        end)
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    // Should navigate to Signal.luau where Connect is defined, NOT DragonAttacksClient.luau
    CHECK_EQ(result[0].uri, signalModule);
}

TEST_CASE_FIXTURE(Fixture, "property_on_table_type_without_actual_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        type Process = {
            spawn: (string) -> string
        }

        local process = {} :: Process
        local value = process.sp|awn()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 17});
}

TEST_CASE_FIXTURE(Fixture, "property_on_the_return_of_a_function_call")
{
    auto [source, position] = sourceWithMarker(R"(
        type Result = {
            unwrap: (Result) -> boolean
        }

        type Process = {
            spawn: (string) -> Result
        }

        local process = {} :: Process
        local value = process.spawn("test"):unwr|ap()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 18});
}

TEST_CASE_FIXTURE(Fixture, "go_to_type_definition_returns_the_table_location")
{
    auto [source, position] = sourceWithMarker(R"(
        type Process = {
            spawn: (string) -> Result
        }

        local process = {} :: Process
        local value = pr|ocess.spawn("test")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::TypeDefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoTypeDefinition(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->uri, document);
    CHECK_EQ(result->range.start, lsp::Position{1, 23});
    CHECK_EQ(result->range.end, lsp::Position{3, 9});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_respects_cancellation")
{
    auto cancellationToken = std::make_shared<Luau::FrontendCancellationToken>();
    cancellationToken->cancel();

    auto document = newDocument("a.luau", "local x = 1");
    CHECK_THROWS_AS(workspace.gotoDefinition(lsp::DefinitionParams{{{document}}}, cancellationToken), RequestCancelledException);
}

TEST_CASE_FIXTURE(Fixture, "go_to_type_definition_respects_cancellation")
{
    auto cancellationToken = std::make_shared<Luau::FrontendCancellationToken>();
    cancellationToken->cancel();

    auto document = newDocument("a.luau", "local x = 1");
    CHECK_THROWS_AS(workspace.gotoTypeDefinition(lsp::TypeDefinitionParams{{{document}}}, cancellationToken), RequestCancelledException);
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_for_property_on_union_type_with_same_definition")
{
    auto document = newDocument("main.luau", R"(
        type BaseNode<HOS> = {
            has_one_supporter: HOS,
        }

        type Node = BaseNode<true> | BaseNode<false>

        local x: Node = {} :: any
        local y = x.has_one_supporter
    )");

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = lsp::Position{8, 20}; // x.has_one_supporter

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range, lsp::Range{{2, 12}, {2, 29}});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_for_property_on_union_type_with_different_definitions")
{
    auto document = newDocument("main.luau", R"(
        type A = {
            prop: number,
        }

        type B = {
            prop: string,
        }

        type Union = A | B

        local x: Union = {} :: any
        local y = x.prop
    )");

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = lsp::Position{12, 20}; // x.prop

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 2);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[1].uri, document);
    CHECK_EQ(result[0].range, lsp::Range{{2, 12}, {2, 16}});
    CHECK_EQ(result[1].range, lsp::Range{{6, 12}, {6, 16}});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_on_original_global_function_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        function global|Function()
        end
    )");

    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range, lsp::Range{{1, 17}, {1, 31}});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_on_original_local_function_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        local function local|Function()
        end
    )");

    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range, lsp::Range{{1, 23}, {1, 36}});
}

TEST_SUITE_END();
