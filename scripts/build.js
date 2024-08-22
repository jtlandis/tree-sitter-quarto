#!/usr/bin/env node

const { execSync } = require("child_process");
const { join } = require("path");

console.log("building tree-sitter-quarto");
execSync("tree-sitter generate", {
  stdio: "inherit",
  cwd: join(__dirname, "..", dir),
});
/*for (const dir of ["tree-sitter-markdown", "tree-sitter-markdown-inline"]) {
  console.log(`building ${dir}`);
  execSync("tree-sitter generate", {
    stdio: "inherit",
    cwd: join(__dirname, "..", dir)
  });
}*/
