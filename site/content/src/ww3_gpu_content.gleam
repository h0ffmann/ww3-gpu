//// `gleam run` in site/content: reads the files of this repository and writes
//// ../src/ww3_gpu_site/content.gleam, the only place the site takes its facts from. Nothing in
//// site/src is typed by hand: a title, a status, a date or a DOI that the page shows is read here.
//// The pattern is h0ffmann/h0ffmann's `site/content`, itself from marola-research's `tools/content`.

import gleam/int
import gleam/list
import gleam/string

/// The repository's root, seen from site/content.
const root = "../../"

pub type Chapter {
  Chapter(number: String, title: String, file: String)
}

/// An ADR or a WFIP: its number, the title its heading states, its status and its date.
pub type Record {
  Record(id: String, title: String, status: String, date: String, file: String)
}

pub type Note {
  Note(id: String, title: String, kind: String, status: String, file: String)
}

pub type Person {
  Person(
    role: String,
    name: String,
    orcid: String,
    affiliation: String,
    kind: String,
  )
}

pub type Link {
  Link(kind: String, name: String, url: String, what: String, source: String)
}

/// A port report of kokkos/port_report/: its name, its status line and its file.
pub type Report {
  Report(name: String, status: String, file: String)
}

/// One row of a report's bench table. The speed-up is baseline / value, computed by the site.
pub type Bench {
  Bench(
    report: String,
    step: String,
    date: String,
    what: String,
    backend: String,
    value: String,
    baseline: String,
    unit: String,
    source: String,
  )
}

@external(javascript, "./ww3_gpu_content_ffi.mjs", "read")
fn read(path: String) -> String

@external(javascript, "./ww3_gpu_content_ffi.mjs", "write")
fn write(path: String, text: String) -> Nil

@external(javascript, "./ww3_gpu_content_ffi.mjs", "list")
fn list_dir(dir: String) -> List(String)

@external(javascript, "./ww3_gpu_content_ffi.mjs", "zenodo_people")
fn zenodo_people(path: String) -> String

@external(javascript, "./ww3_gpu_content_ffi.mjs", "revision")
fn revision() -> String

// ---- parsing, pure and tested ------------------------------------------------------------------

/// The text of the first `# ` heading, or "".
pub fn heading(text: String) -> String {
  text
  |> string.split("\n")
  |> list.find(fn(line) { string.starts_with(line, "# ") })
  |> fn(found) {
    case found {
      Ok(line) ->
        line |> string.drop_start(2) |> string.replace("`", "") |> string.trim
      Error(_) -> ""
    }
  }
}

/// The value of a header-table row `| **Name** | value |`, or "".
pub fn field(text: String, name: String) -> String {
  let prefix = "| **" <> name <> "** |"
  text
  |> string.split("\n")
  |> list.find(fn(line) { string.starts_with(line, prefix) })
  |> fn(found) {
    case found {
      Ok(line) ->
        line
        |> string.drop_start(string.length(prefix))
        |> string.trim
        |> drop_suffix("|")
        |> string.trim
      Error(_) -> ""
    }
  }
}

/// The value of a `key: value` line (YAML front matter or a flat YAML file), unquoted, or "".
pub fn yaml(text: String, key: String) -> String {
  let prefix = key <> ":"
  text
  |> string.split("\n")
  |> list.find(fn(line) { string.starts_with(line, prefix) })
  |> fn(found) {
    case found {
      Ok(line) ->
        line
        |> string.drop_start(string.length(prefix))
        |> string.trim
        |> unquote
      Error(_) -> ""
    }
  }
}

/// The first word of a status cell: "Accepted" from "Accepted (2026-10-08, #97)".
pub fn status_word(cell: String) -> String {
  case string.split(string.trim(cell), " ") {
    [first, ..] -> drop_suffix(drop_suffix(first, "."), ",")
    [] -> ""
  }
}

/// "ADR-0007" and the rest from "ADR-0007: the decision".
pub fn split_id(title: String) -> #(String, String) {
  case string.split_once(title, ": ") {
    Ok(#(id, rest)) -> #(id, rest)
    Error(_) -> #("", title)
  }
}

/// The text after a bold label at the start of a line, up to its first full stop:
/// "L2 (inside WW3, on the CPU)" from "**Status:** L2 (inside WW3, on the CPU). Not run …".
pub fn bold_line(text: String, label: String) -> String {
  let prefix = "**" <> label <> ":** "
  text
  |> string.split("\n")
  |> list.find(fn(line) { string.starts_with(line, prefix) })
  |> fn(found) {
    case found {
      Ok(line) -> {
        let rest =
          string.drop_start(line, string.length(prefix))
          |> string.replace("`", "")
        case string.split_once(rest, ". ") {
          Ok(#(first, _)) -> first
          Error(_) -> drop_suffix(string.trim(rest), ".")
        }
      }
      Error(_) -> ""
    }
  }
}

/// The rows of the table between `<!-- bench:start -->` and `<!-- bench:end -->`, as Bench
/// values: the header and the separator are not rows, and a row of another width is skipped.
pub fn benches(report: String, text: String) -> List(Bench) {
  case string.split_once(text, "<!-- bench:start -->") {
    Error(_) -> []
    Ok(#(_, rest)) -> {
      let block = case string.split_once(rest, "<!-- bench:end -->") {
        Ok(#(b, _)) -> b
        Error(_) -> ""
      }
      block
      |> string.split("\n")
      |> list.filter(fn(line) { string.starts_with(line, "| ") })
      |> list.filter_map(fn(line) {
        let cells =
          line
          |> string.trim
          |> drop_suffix("|")
          |> string.drop_start(1)
          |> string.split("|")
          |> list.map(string.trim)
        case cells {
          ["step", ..] -> Error(Nil)
          [step, date, what, backend, value, baseline, unit, source] ->
            Ok(Bench(
              report,
              step,
              date,
              what,
              backend,
              value,
              baseline,
              unit,
              source,
            ))
          _ -> Error(Nil)
        }
      })
    }
  }
}

/// "00" and "Orientation: what WW3 actually computes" from "00. Orientation: …".
pub fn chapter(file: String, text: String) -> Chapter {
  let title = heading(text)
  case string.split_once(title, ". ") {
    Ok(#(number, rest)) -> Chapter(number, rest, file)
    Error(_) -> Chapter("", title, file)
  }
}

/// The first `## ` heading of RELEASES.md, or "" when the file has none.
pub fn latest_release(text: String) -> String {
  text
  |> string.split("\n")
  |> list.find(fn(line) { string.starts_with(line, "## ") })
  |> fn(found) {
    case found {
      Ok(line) -> string.trim(string.drop_start(line, 3))
      Error(_) -> ""
    }
  }
}

pub fn person(line: String) -> Result(Person, Nil) {
  case string.split(line, "\t") {
    [role, name, orcid, affiliation, kind] ->
      Ok(Person(role, name, orcid, affiliation, kind))
    _ -> Error(Nil)
  }
}

/// A row of site/data/*.tsv; a line starting with `#` is a comment.
pub fn link(line: String) -> Result(Link, Nil) {
  case string.starts_with(line, "#"), string.split(line, "\t") {
    False, [kind, name, url, what, source] ->
      Ok(Link(kind, name, url, what, source))
    _, _ -> Error(Nil)
  }
}

fn drop_suffix(s: String, suffix: String) -> String {
  case string.ends_with(s, suffix) {
    True -> string.drop_end(s, string.length(suffix))
    False -> s
  }
}

fn unquote(s: String) -> String {
  case string.starts_with(s, "\""), string.ends_with(s, "\"") {
    True, True -> s |> string.drop_start(1) |> string.drop_end(1)
    _, _ -> s
  }
}

// ---- rendering ---------------------------------------------------------------------------------

/// A Gleam string literal.
pub fn lit(s: String) -> String {
  "\""
  <> s
  |> string.replace("\\", "\\\\")
  |> string.replace("\"", "\\\"")
  |> string.replace("\n", "\\n")
  <> "\""
}

fn call(name: String, args: List(String)) -> String {
  name <> "(" <> string.join(list.map(args, lit), ", ") <> ")"
}

fn items(name: String, type_: String, rows: List(String)) -> String {
  let body = case rows {
    [] -> "[]"
    _ -> "[\n  " <> string.join(rows, ",\n  ") <> ",\n]"
  }
  "pub const " <> name <> ": List(" <> type_ <> ") = " <> body <> "\n"
}

const types =
  "pub type Chapter {
  Chapter(number: String, title: String, file: String)
}

pub type Record {
  Record(id: String, title: String, status: String, date: String, file: String)
}

pub type Note {
  Note(id: String, title: String, kind: String, status: String, file: String)
}

pub type Person {
  Person(role: String, name: String, orcid: String, affiliation: String, kind: String)
}

pub type Link {
  Link(kind: String, name: String, url: String, what: String, source: String)
}

pub type Report {
  Report(name: String, status: String, file: String)
}

pub type Bench {
  Bench(report: String, step: String, date: String, what: String, backend: String, value: String, baseline: String, unit: String, source: String)
}
"

pub fn render(
  book: #(String, String),
  doi: String,
  record_title: String,
  release: String,
  rev: String,
  chapters: List(Chapter),
  adrs: List(Record),
  wfips: List(Record),
  notes: List(Note),
  people: List(Person),
  links: List(Link),
  reports: List(Report),
  bench_rows: List(Bench),
) -> String {
  let record = fn(r: Record) {
    call("Record", [r.id, r.title, r.status, r.date, r.file])
  }
  string.join(
    [
      "//// Generated by site/content from the files of this repository (`just site`). Do not edit:\n//// change the file it was read from.\n",
      types,
      "pub const book_title = " <> lit(book.0) <> "\n",
      "pub const book_subtitle = " <> lit(book.1) <> "\n",
      "pub const doi = " <> lit(doi) <> "\n",
      "pub const record_title = " <> lit(record_title) <> "\n",
      "pub const latest_release = " <> lit(release) <> "\n",
      "pub const revision = " <> lit(rev) <> "\n",
      items(
        "chapters",
        "Chapter",
        list.map(chapters, fn(c) {
          call("Chapter", [c.number, c.title, c.file])
        }),
      ),
      items("adrs", "Record", list.map(adrs, record)),
      items("wfips", "Record", list.map(wfips, record)),
      items(
        "notes",
        "Note",
        list.map(notes, fn(n) {
          call("Note", [n.id, n.title, n.kind, n.status, n.file])
        }),
      ),
      items(
        "people",
        "Person",
        list.map(people, fn(p) {
          call("Person", [p.role, p.name, p.orcid, p.affiliation, p.kind])
        }),
      ),
      items(
        "links",
        "Link",
        list.map(links, fn(l) {
          call("Link", [l.kind, l.name, l.url, l.what, l.source])
        }),
      ),
      items(
        "reports",
        "Report",
        list.map(reports, fn(r) { call("Report", [r.name, r.status, r.file]) }),
      ),
      items(
        "benches",
        "Bench",
        list.map(bench_rows, fn(b) {
          call("Bench", [
            b.report,
            b.step,
            b.date,
            b.what,
            b.backend,
            b.value,
            b.baseline,
            b.unit,
            b.source,
          ])
        }),
      ),
    ],
    "\n",
  )
}

// ---- reading the repository --------------------------------------------------------------------

fn matching(dir: String, prefix: String) -> List(String) {
  list_dir(root <> dir)
  |> list.filter(fn(name) {
    string.starts_with(name, prefix) && string.ends_with(name, ".md")
  })
}

fn records(dir: String, prefix: String, date_field: String) -> List(Record) {
  matching(dir, prefix)
  |> list.filter(fn(name) { !string.contains(name, ".tasks.") })
  |> list.map(fn(name) {
    let text = read(root <> dir <> name)
    let #(id, title) = split_id(heading(text))
    Record(
      id,
      title,
      status_word(field(text, "Status")),
      field(text, date_field),
      dir <> name,
    )
  })
}

fn is_number(s: String) -> Bool {
  case int.parse(s) {
    Ok(_) -> True
    Error(_) -> False
  }
}

pub fn main() -> Nil {
  let chapters =
    list_dir(root <> "course")
    |> list.filter(fn(name) {
      string.ends_with(name, ".md") && is_number(string.slice(name, 0, 2))
    })
    |> list.map(fn(name) {
      chapter("course/" <> name, read(root <> "course/" <> name))
    })
  let notes =
    matching("notes/", "")
    |> list.filter(fn(name) { name != "index.md" && name != "README.md" })
    |> list.map(fn(name) {
      let text = read(root <> "notes/" <> name)
      Note(
        yaml(text, "id"),
        yaml(text, "title"),
        yaml(text, "type"),
        yaml(text, "status"),
        "notes/" <> name,
      )
    })
    |> list.filter(fn(n) { n.id != "" && n.title != "" })
    |> list.reverse
  let meta = read(root <> "pubs/site/meta.yaml")
  let cff = read(root <> "CITATION.cff")
  let people =
    zenodo_people(root <> ".zenodo.json")
    |> string.split("\n")
    |> list.filter_map(person)
  let links =
    read("../data/related.tsv")
    |> string.split("\n")
    |> list.filter_map(link)
  let report_files =
    list_dir(root <> "kokkos/port_report")
    |> list.filter(fn(name) { string.ends_with(name, "_KOKKOS.md") })
  let reports =
    list.map(report_files, fn(name) {
      let text = read(root <> "kokkos/port_report/" <> name)
      Report(
        heading(text),
        bold_line(text, "Status"),
        "kokkos/port_report/" <> name,
      )
    })
  let bench_rows =
    list.flat_map(report_files, fn(name) {
      let text = read(root <> "kokkos/port_report/" <> name)
      benches(heading(text), text)
    })
  let text =
    render(
      #(yaml(meta, "title"), yaml(meta, "subtitle")),
      yaml(cff, "doi"),
      yaml(cff, "title"),
      latest_release(read(root <> "RELEASES.md")),
      revision(),
      chapters,
      records("docs/ADRs/", "ADR-", "Date"),
      records("docs/WFIPs/", "WFIP-", "Created"),
      notes,
      people,
      links,
      reports,
      bench_rows,
    )
  write("../src/ww3_gpu_site/content.gleam", text)
}
