// The few things the content generator needs from Node: files, a directory listing, the JSON of
// .zenodo.json and one git command. Paths are relative to site/content, where `gleam run` runs.
import { readFileSync, writeFileSync, existsSync, readdirSync } from "node:fs";
import { execSync } from "node:child_process";
import { toList } from "./gleam.mjs";

// The file's text, or "" when it does not exist.
export function read(path) {
  return existsSync(path) ? readFileSync(path, "utf8") : "";
}

export function write(path, text) {
  writeFileSync(path, text);
}

// The names in a directory, sorted; [] when it does not exist.
export function list(dir) {
  return toList(existsSync(dir) ? readdirSync(dir).sort() : []);
}

// The people of .zenodo.json, one per line: role, name, orcid, affiliation, type, tab-separated.
export function zenodo_people(path) {
  if (!existsSync(path)) return "";
  const meta = JSON.parse(readFileSync(path, "utf8"));
  const row = (role, p) =>
    [role, p.name || "", p.orcid || "", p.affiliation || "", p.type || ""].join("\t");
  return [
    ...(meta.creators || []).map((p) => row("creator", p)),
    ...(meta.contributors || []).map((p) => row("contributor", p)),
  ].join("\n");
}

// The short hash of the commit that is built, or "unversioned" outside a Git checkout.
export function revision() {
  try {
    return execSync("git rev-parse --short HEAD", { stdio: ["ignore", "pipe", "ignore"] })
      .toString()
      .trim();
  } catch (_) {
    return "unversioned";
  }
}
