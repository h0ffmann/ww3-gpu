import gleam/string
import gleeunit
import ww3_gpu_content as c

pub fn main() -> Nil {
  gleeunit.main()
}

const adr =
  "# ADR-0007: Graphify is adopted as an offline locator

| | |
|---|---|
| **Status** | Accepted (2026-10-12, #97) |
| **Date** | 2026-10-11 |
"

pub fn heading_test() {
  assert c.heading(adr) == "ADR-0007: Graphify is adopted as an offline locator"
  assert c.heading("no heading here") == ""
  assert c.heading("# 12. Porting a kernel: `W3SNL1`")
    == "12. Porting a kernel: W3SNL1"
}

pub fn field_test() {
  assert c.field(adr, "Date") == "2026-10-11"
  assert c.field(adr, "Status") == "Accepted (2026-10-12, #97)"
  assert c.field(adr, "Missing") == ""
}

pub fn status_word_test() {
  assert c.status_word("Accepted (2026-10-12, #97)") == "Accepted"
  assert c.status_word("Draft.") == "Draft"
  assert c.status_word("") == ""
}

pub fn split_id_test() {
  assert c.split_id("ADR-0007: the decision: with a colon")
    == #("ADR-0007", "the decision: with a colon")
  assert c.split_id("no id") == #("", "no id")
}

pub fn chapter_test() {
  let ch =
    c.chapter(
      "course/00-orientation.md",
      "# 00. Orientation: what WW3 computes\n",
    )
  assert ch
    == c.Chapter(
      "00",
      "Orientation: what WW3 computes",
      "course/00-orientation.md",
    )
}

pub fn yaml_test() {
  let note = "---\nid: \"202610141200\"\ntitle: A note\ntype: permanent\n---\n"
  assert c.yaml(note, "id") == "202610141200"
  assert c.yaml(note, "title") == "A note"
  assert c.yaml(note, "status") == ""
}

pub fn latest_release_test() {
  assert c.latest_release(
      "# Releases\n\n## v0.1.4: what changed\n\n## v0.1.3\n",
    )
    == "v0.1.4: what changed"
  assert c.latest_release("") == ""
}

pub fn row_test() {
  assert c.link("# a comment\tx\ty\tz\tw") == Error(Nil)
  assert c.link(
      "author\tFabio Akita\thttps://github.com/akitaonrails\twhat\tsource",
    )
    == Ok(c.Link(
      "author",
      "Fabio Akita",
      "https://github.com/akitaonrails",
      "what",
      "source",
    ))
  assert c.person(
      "contributor\tGuimarães, P.V.\t0000-0002-1686-4722\tUFSC\tSupervisor",
    )
    == Ok(c.Person(
      "contributor",
      "Guimarães, P.V.",
      "0000-0002-1686-4722",
      "UFSC",
      "Supervisor",
    ))
}

pub fn lit_test() {
  assert c.lit("a \"quote\" and \\ back\nslash")
    == "\"a \\\"quote\\\" and \\\\ back\\nslash\""
}

pub fn empty_list_test() {
  let text =
    c.render(#("t", "s"), "d", "r", "", "x", [], [], [], [], [], [], [], [])
  assert text |> contains("pub const notes: List(Note) = []")
}

fn contains(haystack: String, needle: String) -> Bool {
  string.contains(haystack, needle)
}

const report =
  "# W3SNL1_KOKKOS

**Status:** L2 (inside WW3, on the CPU). Not run on a GPU inside WW3 yet.

<!-- bench:start -->
| step | date | what | backend | value | baseline | unit | source |
|---|---|---|---|---|---|---|---|
| 1 | 2026-09-15 | the shim | Serial | 25.88 | 25.88 | ms | bench/x.json |
| 2 | 2026-10-14 | ww3_shel | OpenMP | 217.9 | 152.7 | s | just full-hoff |
| 3 | too | few | cells |
<!-- bench:end -->

| 9 | outside | the | markers | 1 | 1 | s | x |
"

pub fn bold_line_test() {
  assert c.bold_line(report, "Status") == "L2 (inside WW3, on the CPU)"
  assert c.bold_line(report, "Missing") == ""
  assert c.bold_line("**Status:** L2, with `W3SNL1`.", "Status")
    == "L2, with W3SNL1"
}

pub fn benches_test() {
  let rows = c.benches("W3SNL1_KOKKOS", report)
  assert rows
    == [
      c.Bench(
        "W3SNL1_KOKKOS",
        "1",
        "2026-09-15",
        "the shim",
        "Serial",
        "25.88",
        "25.88",
        "ms",
        "bench/x.json",
      ),
      c.Bench(
        "W3SNL1_KOKKOS",
        "2",
        "2026-10-14",
        "ww3_shel",
        "OpenMP",
        "217.9",
        "152.7",
        "s",
        "just full-hoff",
      ),
    ]
  assert c.benches("x", "no table") == []
}
