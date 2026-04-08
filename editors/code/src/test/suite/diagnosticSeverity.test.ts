/* eslint-disable @typescript-eslint/naming-convention */
import * as assert from "assert";
import * as vscode from "vscode";
import {
  extractLintName,
  applyOverrides,
} from "../../diagnosticSeverityMiddleware";

function makeLintDiagnostic(
  lintName: string,
  code: number | { value: number; target: vscode.Uri },
  severity = vscode.DiagnosticSeverity.Warning,
): vscode.Diagnostic {
  const diag = new vscode.Diagnostic(
    new vscode.Range(0, 0, 0, 5),
    `${lintName}: some message`,
    severity,
  );
  diag.source = "Luau";
  diag.code = code;
  return diag;
}

function makeTypeErrorDiagnostic(): vscode.Diagnostic {
  const diag = new vscode.Diagnostic(
    new vscode.Range(0, 0, 0, 5),
    "TypeError: some type error",
    vscode.DiagnosticSeverity.Error,
  );
  diag.source = "Luau";
  diag.code = { value: "TypeError", target: vscode.Uri.parse("https://luau.org/types") };
  return diag;
}

function makeParseErrorDiagnostic(): vscode.Diagnostic {
  const diag = new vscode.Diagnostic(
    new vscode.Range(0, 0, 0, 5),
    "SyntaxError: unexpected symbol",
    vscode.DiagnosticSeverity.Error,
  );
  diag.source = "Luau";
  diag.code = { value: "SyntaxError", target: vscode.Uri.parse("https://luau.org/syntax") };
  return diag;
}

function makeNonLuauDiagnostic(): vscode.Diagnostic {
  const diag = new vscode.Diagnostic(
    new vscode.Range(0, 0, 0, 5),
    "Some other tool error",
    vscode.DiagnosticSeverity.Warning,
  );
  diag.source = "eslint";
  diag.code = 42;
  return diag;
}

suite("Diagnostic Severity Middleware Test Suite", () => {
  suite("extractLintName", () => {
    test("extracts lint name from message prefix with compound code", () => {
      const diag = makeLintDiagnostic("LocalUnused", {
        value: 7,
        target: vscode.Uri.parse("https://luau.org/lint#localunused-7"),
      });
      assert.strictEqual(extractLintName(diag), "LocalUnused");
    });

    test("extracts lint name from message prefix with plain numeric code", () => {
      const diag = makeLintDiagnostic("ImportUnused", 9);
      assert.strictEqual(extractLintName(diag), "ImportUnused");
    });

    test("returns undefined for type error diagnostic (string code)", () => {
      const diag = makeTypeErrorDiagnostic();
      assert.strictEqual(extractLintName(diag), undefined);
    });

    test("returns undefined for parse error diagnostic", () => {
      const diag = makeParseErrorDiagnostic();
      assert.strictEqual(extractLintName(diag), undefined);
    });

    test("returns undefined for non-Luau source diagnostic", () => {
      const diag = makeNonLuauDiagnostic();
      assert.strictEqual(extractLintName(diag), undefined);
    });

    test("returns undefined when message has no colon and code is non-numeric", () => {
      const diag = new vscode.Diagnostic(
        new vscode.Range(0, 0, 0, 5),
        "no colon here",
        vscode.DiagnosticSeverity.Error,
      );
      diag.source = "Luau";
      diag.code = { value: "SomeStringCode", target: vscode.Uri.parse("https://luau.org") };
      assert.strictEqual(extractLintName(diag), undefined);
    });
  });

  suite("applyOverrides", () => {
    test("remaps Warning to Information", () => {
      const diag = makeLintDiagnostic("LocalUnused", { value: 7, target: vscode.Uri.parse("https://luau.org/lint#localunused-7") });
      const result = applyOverrides([diag], { LocalUnused: "Information" });
      assert.strictEqual(result.length, 1);
      assert.strictEqual(result[0].severity, vscode.DiagnosticSeverity.Information);
    });

    test("remaps Warning to Hint", () => {
      const diag = makeLintDiagnostic("DeprecatedGlobal", { value: 2, target: vscode.Uri.parse("https://luau.org/lint#deprecatedglobal-2") });
      const result = applyOverrides([diag], { DeprecatedGlobal: "Hint" });
      assert.strictEqual(result.length, 1);
      assert.strictEqual(result[0].severity, vscode.DiagnosticSeverity.Hint);
    });

    test("remaps Warning to Error", () => {
      const diag = makeLintDiagnostic("UnknownGlobal", { value: 1, target: vscode.Uri.parse("https://luau.org/lint#unknownglobal-1") });
      const result = applyOverrides([diag], { UnknownGlobal: "Error" });
      assert.strictEqual(result.length, 1);
      assert.strictEqual(result[0].severity, vscode.DiagnosticSeverity.Error);
    });

    test("filters out diagnostics with Off override", () => {
      const diag = makeLintDiagnostic("ImportUnused", { value: 9, target: vscode.Uri.parse("https://luau.org/lint#importunused-9") });
      const result = applyOverrides([diag], { ImportUnused: "Off" });
      assert.strictEqual(result.length, 0);
    });

    test("leaves non-lint diagnostics untouched", () => {
      const typeError = makeTypeErrorDiagnostic();
      const parseError = makeParseErrorDiagnostic();
      const nonLuau = makeNonLuauDiagnostic();
      const result = applyOverrides(
        [typeError, parseError, nonLuau],
        { LocalUnused: "Information" },
      );
      assert.strictEqual(result.length, 3);
      assert.strictEqual(result[0].severity, vscode.DiagnosticSeverity.Error);
      assert.strictEqual(result[1].severity, vscode.DiagnosticSeverity.Error);
      assert.strictEqual(result[2].severity, vscode.DiagnosticSeverity.Warning);
    });

    test("leaves all diagnostics untouched when overrides is empty", () => {
      const diag = makeLintDiagnostic("LocalUnused", 7);
      const result = applyOverrides([diag], {});
      assert.strictEqual(result.length, 1);
      assert.strictEqual(result[0].severity, vscode.DiagnosticSeverity.Warning);
    });

    test("leaves diagnostics untouched when lint name is not in overrides", () => {
      const diag = makeLintDiagnostic("LocalUnused", 7);
      const result = applyOverrides([diag], { ImportUnused: "Off" });
      assert.strictEqual(result.length, 1);
      assert.strictEqual(result[0].severity, vscode.DiagnosticSeverity.Warning);
    });

    test("handles multiple diagnostics with mixed overrides", () => {
      const local = makeLintDiagnostic("LocalUnused", 7);
      const imported = makeLintDiagnostic("ImportUnused", 9);
      const typeErr = makeTypeErrorDiagnostic();
      const result = applyOverrides(
        [local, imported, typeErr],
        { LocalUnused: "Information", ImportUnused: "Off" },
      );
      assert.strictEqual(result.length, 2);
      assert.strictEqual(result[0].severity, vscode.DiagnosticSeverity.Information);
      assert.strictEqual(result[1].severity, vscode.DiagnosticSeverity.Error);
    });

    test("ignores unknown lint names in overrides gracefully", () => {
      const diag = makeLintDiagnostic("LocalUnused", 7);
      // "NotARealLint" is not a real lint - should be a no-op
      const result = applyOverrides([diag], { NotARealLint: "Information" });
      assert.strictEqual(result.length, 1);
      assert.strictEqual(result[0].severity, vscode.DiagnosticSeverity.Warning);
    });
  });
});
