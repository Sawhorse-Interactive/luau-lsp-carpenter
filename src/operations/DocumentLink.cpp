#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"
#include "LSP/LuauExt.hpp"
#include "Platform/RobloxPlatform.hpp"

struct RequireInfo
{
    Luau::AstExpr* require = nullptr;
    Luau::Location location;
    bool isShared = false;
};

struct FindRequireVisitor : public Luau::AstVisitor
{
    std::vector<RequireInfo> requireInfos{};

    bool visit(Luau::AstExprCall* call) override
    {
        if (auto maybeRequire = types::matchRequire(*call))
            requireInfos.emplace_back(RequireInfo{*maybeRequire, call->argLocation, false});
        else if (auto maybeShared = types::matchShared(*call))
            requireInfos.emplace_back(RequireInfo{*maybeShared, call->argLocation, true});
        return true;
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        for (Luau::AstStat* stat : block->body)
        {
            stat->visit(this);
        }

        return false;
    }
};

std::vector<lsp::DocumentLink> WorkspaceFolder::documentLink(const lsp::DocumentLinkParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    std::vector<lsp::DocumentLink> result{};

    frontend.parse(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule || !sourceModule->root)
        return {};

    FindRequireVisitor visitor;
    visitor.visit(sourceModule->root);

    for (auto& require : visitor.requireInfos)
    {
        std::optional<Uri> resolvedUri;

        if (require.isShared)
        {
            // Resolve shared("FileName") by filename lookup
            if (auto* str = require.require->as<Luau::AstExprConstantString>())
            {
                std::string fileName(str->value.data, str->value.size);
                if (auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(platform.get()))
                {
                    auto sharedResult = robloxPlatform->resolveSharedModuleName(fileName);
                    if (sharedResult.status == SharedModuleResult::Found)
                        resolvedUri = platform->resolveToRealPath(sharedResult.moduleName);
                }
            }
        }
        else
        {
            if (auto moduleInfo = frontend.moduleResolver.resolveModuleInfo(moduleName, *require.require))
                resolvedUri = platform->resolveToRealPath(moduleInfo->name);
        }

        if (resolvedUri)
        {
            lsp::DocumentLink link;
            link.target = *resolvedUri;
            link.range = lsp::Range{
                {require.location.begin.line, require.location.begin.column}, {require.location.end.line, require.location.end.column - 1}};
            result.push_back(link);
        }
    }

    return result;
}

std::vector<lsp::DocumentLink> LanguageServer::documentLink(const lsp::DocumentLinkParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->documentLink(params);
}
