const { defineConfig } = require("@vscode/test-cli");
const os = require("os");
const path = require("path");

// macOS caps unix socket paths at 104 bytes. VS Code creates its instance socket
// inside the user data dir, which by default lives under the repo checkout - on CI
// that path overflows the cap and startup fails with EINVAL, so keep it short.
const userDataDir = path.join(os.tmpdir(), "vsct-luau");

module.exports = defineConfig({
    files: "out/test/**/*.test.js",
    launchArgs: [`--user-data-dir=${userDataDir}`],
});
