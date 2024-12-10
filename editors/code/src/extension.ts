import * as vscode from "vscode";
import * as os from "os";
import { fetch } from "undici";
import {
  Executable,
  ServerOptions,
  LanguageClient,
  LanguageClientOptions,
} from "vscode-languageclient/node";

import {
  registerComputeBytecode,
  registerComputeCompilerRemarks,
} from "./bytecode";

import * as roblox from "./roblox";
import * as utils from "./utils";

export type PlatformContext = { client: LanguageClient | undefined };
export type AddArgCallback = (
  argument: string,
  mode?: "All" | "Prod" | "Debug",
) => void;

let foundFiles: vscode.Uri[] = [];
let client: LanguageClient | undefined = undefined;
let platformContext: PlatformContext = { client: undefined };
const clientDisposables: vscode.Disposable[] = [];

const CURRENT_FFLAGS =
  "https://clientsettingscdn.roblox.com/v1/settings/application?applicationName=PCDesktopClient";
const FFLAG_KINDS = ["FFlag", "FInt", "DFFlag", "DFInt"];

type FFlags = Record<string, string>;
type FFlagsEndpoint = { applicationSettings: FFlags };

const getFFlags = async () => {
  return vscode.window.withProgress(
    {
      location: vscode.ProgressLocation.Window,
      title: "Luau: Fetching FFlags",
      cancellable: false,
    },
    () =>
      fetch(CURRENT_FFLAGS)
        .then((r) => r.json() as Promise<FFlagsEndpoint>)
        .then((r) => r.applicationSettings),
  );
};

const isAlphanumericUnderscore = (str: string) => {
  return /^[a-zA-Z0-9_]+$/.test(str);
};

const startLanguageServer = async (context: vscode.ExtensionContext) => {
  for (const disposable of clientDisposables) {
    disposable.dispose();
  }
  clientDisposables.splice(0, clientDisposables.length); // empty the list
  if (client) {
    await client.stop();
  }

  console.log("Starting Luau Language Server");

  const args = ["lsp"];
  const debugArgs = ["lsp"];

  const addArg = (argument: string, mode: "All" | "Prod" | "Debug" = "All") => {
    if (mode === "All" || mode === "Prod") {
      args.push(argument);
    }
    if (mode === "All" || mode === "Debug") {
      debugArgs.push(argument);
    }
  };

  await roblox.preLanguageServerStart(platformContext, context, addArg);

  const typesConfig = vscode.workspace.getConfiguration("luau-lsp.types");

  // Load extra type definitions
  const definitionFiles = typesConfig.get<string[]>("definitionFiles");
  if (definitionFiles) {
    for (let definitionPath of definitionFiles) {
      definitionPath = utils.resolvePath(definitionPath);
      let uri;
      if (vscode.workspace.workspaceFolders) {
        uri = utils.resolveUri(
          vscode.workspace.workspaceFolders[0].uri,
          definitionPath,
        );
      } else {
        uri = vscode.Uri.file(definitionPath);
      }
      if (await utils.exists(uri)) {
        addArg(`--definitions=${uri.fsPath}`);
      } else {
        vscode.window.showWarningMessage(
          `Definitions file at ${definitionPath} does not exist, types will not be provided from this file`,
        );
      }
    }
  }

  // Load extra documentation files
  const documentationFiles = typesConfig.get<string[]>("documentationFiles");
  if (documentationFiles) {
    for (let documentationPath of documentationFiles) {
      documentationPath = utils.resolvePath(documentationPath);
      let uri;
      if (vscode.workspace.workspaceFolders) {
        uri = utils.resolveUri(
          vscode.workspace.workspaceFolders[0].uri,
          documentationPath,
        );
      } else {
        uri = vscode.Uri.file(documentationPath);
      }
      if (await utils.exists(uri)) {
        addArg(`--docs=${uri.fsPath}`);
      } else {
        vscode.window.showWarningMessage(
          `Documentations file at ${documentationPath} does not exist`,
        );
      }
    }
  }

  // Handle FFlags
  const fflags: FFlags = {};
  const fflagsConfig = vscode.workspace.getConfiguration("luau-lsp.fflags");

  if (!fflagsConfig.get<boolean>("enableByDefault")) {
    addArg("--no-flags-enabled");
  }

  // Sync FFlags with upstream
  if (fflagsConfig.get<boolean>("sync")) {
    try {
      const currentFlags = await getFFlags();
      if (currentFlags) {
        for (const [name, value] of Object.entries(currentFlags)) {
          for (const kind of FFLAG_KINDS) {
            if (name.startsWith(`${kind}Luau`)) {
              // Remove the "FFlag" part from the name
              fflags[name.substring(kind.length)] = value;
            }
          }
        }
      }
    } catch (err) {
      vscode.window.showWarningMessage(
        "Failed to fetch current Luau FFlags: " + err,
      );
    }
  }

  // Enable new solver
  if (fflagsConfig.get<boolean>("enableNewSolver")) {
    fflags["LuauSolverV2"] = "true";
    fflags["LuauNewSolverPopulateTableLocations"] = "true";
    fflags["LuauNewSolverPrePopulateClasses"] = "true";
  }

  // Handle overrides
  const overridenFFlags = fflagsConfig.get<FFlags>("override");
  if (overridenFFlags) {
    for (let [name, value] of Object.entries(overridenFFlags)) {
      if (!isAlphanumericUnderscore(name)) {
        vscode.window.showWarningMessage(
          `Invalid FFlag name: '${name}'. It can only contain alphanumeric characters`,
        );
      }

      name = name.trim();
      value = value.trim();
      // Validate that the name and value is valid
      if (name.length > 0 && value.length > 0) {
        fflags[name] = value;
      }
    }
  }

  const run: Executable = {
    command: vscode.Uri.joinPath(
      context.extensionUri,
      "bin",
      os.platform() === "win32" ? "server.exe" : "server",
    ).fsPath,
    args,
  };

  // If debugging, run the locally build extension, with local type definitions file
  const debug: Executable = {
    command: vscode.Uri.joinPath(
      context.extensionUri,
      "..",
      "..",
      process.env["SERVER_PATH"] ?? "unknown.exe",
    ).fsPath,
    args: debugArgs,
  };

  const serverOptions: ServerOptions = { run, debug };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [
      { language: "lua", scheme: "file" },
      { language: "luau", scheme: "file" },
      { language: "lua", scheme: "untitled" },
      { language: "luau", scheme: "untitled" },
    ],
    diagnosticPullOptions: {
      onChange: vscode.workspace
        .getConfiguration("luau-lsp.diagnostics")
        .get("pullOnChange", true),
      onSave: vscode.workspace
        .getConfiguration("luau-lsp.diagnostics")
        .get("pullOnSave", true),
    },
    initializationOptions: {
      fflags,
    },
  };

  client = new LanguageClient(
    "luau",
    "Luau Language Server",
    serverOptions,
    clientOptions,
  );

  platformContext.client = client;

  // Register commands
  client.onNotification("$/command", (params) => {
    vscode.commands.executeCommand(params.command, params.data);
  });

  clientDisposables.push(...registerComputeBytecode(context, client));
  clientDisposables.push(...registerComputeCompilerRemarks(context, client));

  console.log("LSP Setup");
  await client.start();
};

function createAliasEntry(file: vscode.Uri) {
  var fsPath = file.fsPath.replace(/\\init(\.server|\.client)*(\.lua|\.luau)$/, '');
  var filename = fsPath.replace(/^.*[\\/]/, '').replace(/(\.server|\.client)*(\.lua|\.luau)$/, '');
      
  let relativePath = vscode.workspace.asRelativePath(file.fsPath);
  filename = filename.replace(" ", "");
  return [filename, `\t\t"${filename}": "./${relativePath}",\n`];
}

function generateJsonAliasesContents(files: vscode.Uri[]) {
  let json = "{\n\t\"aliases\": {\n\t\t\"src\": \"\./\",\n";
  let aliasEntries: string[] = [];

  for (const file of files) {
    let [filename, entryJson] = createAliasEntry(file); 

    if (aliasEntries.includes(filename)) {
      continue;
    }

    aliasEntries.push(filename);
    json += entryJson;
  }

  if (json.endsWith(",\n")) {
    json = json.slice(0, -2);
  }

  json += `\n\t}\n}`;
  return json;
}

function writeAliasesFile(contentsJson: string) {
  let workspaces = vscode.workspace.workspaceFolders;

  if (!workspaces) {
    console.warn("No workspaces");
    return;
  }

  let aliasesUri = workspaces[0].uri.with({ path: workspaces[0].uri.path + "/.luaurc" });
  vscode.workspace.fs.writeFile(aliasesUri, Buffer.from(contentsJson));
}

async function createAliasesFile() {
  await vscode.workspace.findFiles("{**/*.lua,**/*.luau}").then((files) => {
    foundFiles = files;
  });

  let contentsJson = generateJsonAliasesContents(foundFiles);
  writeAliasesFile(contentsJson);
}

function addToAliasesFile(file: vscode.Uri) {
  foundFiles.push(file);
  let contentsJson = generateJsonAliasesContents(foundFiles);
  writeAliasesFile(contentsJson);
}

function removeFromAliasesFile(file: vscode.Uri) {
  foundFiles = foundFiles.filter((element) => element.fsPath !== file.fsPath);
  let contentsJson = generateJsonAliasesContents(foundFiles);
  writeAliasesFile(contentsJson);
}

export async function activate(context: vscode.ExtensionContext) {
  console.log("Luau LSP activated");

  await roblox.onActivate(platformContext, context);

  createAliasesFile();

  context.subscriptions.push(
    vscode.commands.registerCommand("luau-lsp.reloadServer", async () => {
      vscode.window.showInformationMessage("Reloading Language Server");
      await startLanguageServer(context);
    }),
    vscode.commands.registerCommand("luau-lsp.flushTimeTrace", async () => {
      if (client) {
        client.sendNotification("$/flushTimeTrace");
      }
    }),
    vscode.workspace.onDidCreateFiles((e) => {
      for (const file of e.files) {
        if (file.fsPath.endsWith(".lua") || file.fsPath.endsWith(".luau")) {
          addToAliasesFile(file);
        }
        // this would help with finding a file which shares the same name as a child file
        // but introduces its own problems which im not going to address right now
        // else {
        //   vscode.workspace.fs.stat(file).then((stat) => {
        //     if (stat.type === vscode.FileType.Directory) {
        //       addToAliasesFile(file);
        //     }
        //   });
        // }
      }
    }),
    vscode.workspace.onDidDeleteFiles((e) => {
      for (const file of e.files) {
        removeFromAliasesFile(file);
      }
    }),
    vscode.workspace.onDidRenameFiles((e) => {
      for (const file of e.files) {
        removeFromAliasesFile(file.oldUri)
        addToAliasesFile(file.newUri)
      }
    }),
  );

  context.subscriptions.push(
    vscode.workspace.onDidChangeConfiguration((e) => {
      if (e.affectsConfiguration("luau-lsp.fflags")) {
        vscode.window
          .showInformationMessage(
            "Luau FFlags have been changed, reload server for this to take effect.",
            "Reload Language Server",
          )
          .then((command) => {
            if (command === "Reload Language Server") {
              vscode.commands.executeCommand("luau-lsp.reloadServer");
            }
          });
      } else if (
        e.affectsConfiguration("luau-lsp.types") ||
        e.affectsConfiguration("luau-lsp.platform.type")
      ) {
        vscode.window
          .showInformationMessage(
            "Luau type definitions have been changed, reload server for this to take effect.",
            "Reload Language Server",
          )
          .then((command) => {
            if (command === "Reload Language Server") {
              vscode.commands.executeCommand("luau-lsp.reloadServer");
            }
          });
      }
    }),
  );

  await startLanguageServer(context);

  await roblox.postLanguageServerStart(platformContext, context);
}

export async function deactivate() {
  return Promise.allSettled([
    ...roblox.onDeactivate(),
    client?.stop(),
    clientDisposables.map((disposable) => disposable.dispose()),
  ]);
}
