import * as vscode from "vscode";
import { CancellationToken, TextDocument, Uri } from "vscode";
import {
  ProvideDiagnosticSignature,
  ProvideWorkspaceDiagnosticSignature,
  vsdiag,
} from "vscode-languageclient/node";

const SEVERITY_MAP: Readonly<Record<string, vscode.DiagnosticSeverity>> = {
  Error: vscode.DiagnosticSeverity.Error,
  Warning: vscode.DiagnosticSeverity.Warning,
  Information: vscode.DiagnosticSeverity.Information,
  Hint: vscode.DiagnosticSeverity.Hint,
};

export type SeverityOverrides = Record<string, string>;

/**
 * Extracts the lint rule name from a Luau lint diagnostic.
 * Returns undefined if the diagnostic is not a Luau lint warning.
 *
 * Lint diagnostics have:
 *   - source === "Luau"
 *   - numeric code (type/parse errors use string codes)
 *   - message starting with "LintName: ..."
 *
 * Note: because the server always sets codeDescription, the protocol converter
 * merges code + codeDescription into a compound { value, target } object.
 * We handle both forms.
 */
export function extractLintName(
  diagnostic: vscode.Diagnostic,
): string | undefined {
  if (diagnostic.source !== "Luau") {
    return undefined;
  }

  const code = diagnostic.code;
  let codeValue: string | number | undefined;
  if (typeof code === "object" && code !== null && "value" in code) {
    codeValue = (code as { value: string | number }).value;
  } else {
    codeValue = code as string | number | undefined;
  }

  // Lint warnings have numeric codes; type errors and parse errors use strings
  if (typeof codeValue !== "number") {
    return undefined;
  }

  // Extract name from the message prefix (authoritative, set by the server)
  const colonIndex = diagnostic.message.indexOf(":");
  if (colonIndex > 0) {
    return diagnostic.message.substring(0, colonIndex);
  }

  return undefined;
}

/**
 * Reads the current severity overrides from VS Code configuration.
 * Called fresh on every diagnostic response so changes take effect
 * without restarting the server.
 */
export function getSeverityOverrides(): SeverityOverrides {
  return vscode.workspace
    .getConfiguration("luau-lsp.diagnostics")
    .get<SeverityOverrides>("severityOverrides", {});
}

/**
 * Applies severity overrides to an array of diagnostics.
 * Mutates severity in-place and filters out diagnostics marked "Off".
 * Returns the (possibly shorter) filtered array.
 */
export function applyOverrides(
  diagnostics: vscode.Diagnostic[],
  overrides: SeverityOverrides,
): vscode.Diagnostic[] {
  if (Object.keys(overrides).length === 0) {
    return diagnostics;
  }

  const result: vscode.Diagnostic[] = [];
  for (const diag of diagnostics) {
    const lintName = extractLintName(diag);
    if (lintName === undefined || !(lintName in overrides)) {
      result.push(diag);
      continue;
    }

    const overrideValue = overrides[lintName];

    if (overrideValue === "Off") {
      continue;
    }

    const mappedSeverity = SEVERITY_MAP[overrideValue];
    if (mappedSeverity !== undefined) {
      diag.severity = mappedSeverity;
    }
    result.push(diag);
  }
  return result;
}

/**
 * Middleware for document (pull) diagnostics.
 * Intercepts textDocument/diagnostic responses and applies severity overrides.
 */
export async function provideDiagnosticsMiddleware(
  document: TextDocument | Uri,
  previousResultId: string | undefined,
  token: CancellationToken,
  next: ProvideDiagnosticSignature,
): Promise<vsdiag.DocumentDiagnosticReport | undefined | null> {
  const report = await next(document, previousResultId, token);
  if (!report || report.kind !== vsdiag.DocumentDiagnosticReportKind.full) {
    return report;
  }

  const overrides = getSeverityOverrides();
  report.items = applyOverrides(report.items, overrides);

  // Process related documents (e.g., dependents included in the response)
  const relatedReport = report as vsdiag.RelatedFullDocumentDiagnosticReport;
  if (relatedReport.relatedDocuments) {
    for (const subReport of Object.values(relatedReport.relatedDocuments)) {
      if (subReport.kind === vsdiag.DocumentDiagnosticReportKind.full) {
        subReport.items = applyOverrides(subReport.items, overrides);
      }
    }
  }

  return report;
}

/**
 * Middleware for workspace diagnostics.
 * Wraps the resultReporter to intercept streamed partial results,
 * and processes the final report.
 */
export async function provideWorkspaceDiagnosticsMiddleware(
  resultIds: vsdiag.PreviousResultId[],
  token: CancellationToken,
  resultReporter: vsdiag.ResultReporter,
  next: ProvideWorkspaceDiagnosticSignature,
): Promise<vsdiag.WorkspaceDiagnosticReport | undefined | null> {
  const overrides = getSeverityOverrides();

  const wrappedReporter: vsdiag.ResultReporter = (chunk) => {
    if (chunk?.items) {
      for (const docReport of chunk.items) {
        if (docReport.kind === vsdiag.DocumentDiagnosticReportKind.full) {
          (docReport as vsdiag.WorkspaceFullDocumentDiagnosticReport).items =
            applyOverrides(
              (docReport as vsdiag.WorkspaceFullDocumentDiagnosticReport).items,
              overrides,
            );
        }
      }
    }
    resultReporter(chunk);
  };

  const report = await next(resultIds, token, wrappedReporter);
  if (!report) {
    return report;
  }

  for (const docReport of report.items) {
    if (docReport.kind === vsdiag.DocumentDiagnosticReportKind.full) {
      (docReport as vsdiag.WorkspaceFullDocumentDiagnosticReport).items =
        applyOverrides(
          (docReport as vsdiag.WorkspaceFullDocumentDiagnosticReport).items,
          overrides,
        );
    }
  }

  return report;
}
