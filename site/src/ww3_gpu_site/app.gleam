//// The page. Every fact it shows comes from `content`, which site/content writes from the files of
//// this repository; the sentences that tell the story are written here, in lower case
//// (DESIGN.md). The page is prerendered and needs no JavaScript.

import gleam/float
import gleam/int
import gleam/list
import gleam/string
import lustre/attribute.{type Attribute}
import lustre/element.{type Element}
import lustre/element/html
import ww3_gpu_site/content.{
  type Bench, type Link, type Note, type Person, type Record, type Report,
}

pub const repo = "https://github.com/h0ffmann/ww3-gpu"

const blob = "https://github.com/h0ffmann/ww3-gpu/blob/main/"

pub fn view() -> Element(msg) {
  html.main([], [
    cover(),
    book(),
    chapters(),
    records(
      "decisions",
      "the decisions",
      "each one a choice the lab made, the context that forced it and what follows. an accepted record is never edited; a new one supersedes it.",
      content.adrs,
    ),
    records(
      "designs",
      "the designs",
      "a change is designed before it is built, tied to a deliverable of the project proposal, with its definition of done.",
      content.wfips,
    ),
    port(),
    brain(),
    cite(),
    related(),
    footer(),
  ])
}

// ---- the port, measured ----------------------------------------------------------------------
// Each bench row of kokkos/port_report/*_KOKKOS.md is one step: a bar as long as the log of its
// speed-up (baseline / value), in a deeper shade of the proof light the larger the speed-up
// (DESIGN.md); a step slower than its baseline is grey and says "slower".

fn number(s: String) -> Float {
  case float.parse(s) {
    Ok(x) -> x
    Error(_) ->
      case int.parse(s) {
        Ok(n) -> int.to_float(n)
        Error(_) -> 0.0
      }
  }
}

pub fn speedup(b: Bench) -> Float {
  let v = number(b.value)
  case v >. 0.0 {
    True -> number(b.baseline) /. v
    False -> 0.0
  }
}

/// "34.5x" or "531x" or "0.70x": two significant decimals below 10, one below 100, none above.
pub fn times(x: Float) -> String {
  let digits = case x <. 10.0, x <. 100.0 {
    True, _ -> 2
    False, True -> 1
    False, False -> 0
  }
  let shown = case digits {
    0 -> int.to_string(float.round(x))
    _ -> float.to_string(float.to_precision(x, digits))
  }
  shown <> "x"
}

/// The shade of a step: g1 (under 2x) to g4 (100x and more); "slower" under 1x.
pub fn shade(x: Float) -> String {
  case x <. 1.0, x <. 2.0, x <. 10.0, x <. 100.0 {
    True, _, _, _ -> "slower"
    _, True, _, _ -> "g1"
    _, _, True, _ -> "g2"
    _, _, _, True -> "g3"
    _, _, _, _ -> "g4"
  }
}

fn log10(x: Float) -> Float {
  case float.logarithm(x), float.logarithm(10.0) {
    Ok(a), Ok(b) -> a /. b
    _, _ -> 0.0
  }
}

fn bar_width(x: Float, top: Float) -> String {
  let w = case x <=. 1.0 || top <=. 1.0 {
    True -> 4.0
    False -> 4.0 +. 96.0 *. log10(x) /. log10(top)
  }
  float.to_string(float.to_precision(w, 1)) <> "%"
}

fn step(b: Bench, top: Float) -> Element(msg) {
  let x = speedup(b)
  html.li([attribute.class("step")], [
    html.span([attribute.class("num")], [html.text(b.step)]),
    html.span([attribute.class("name")], [
      html.text(b.what <> " · " <> b.backend),
    ]),
    html.span([attribute.class("date")], [html.text(b.date)]),
    html.span([attribute.class("bar")], [
      html.span(
        [
          attribute.class("fill " <> shade(x)),
          attribute.style("width", bar_width(x, top)),
        ],
        [],
      ),
    ]),
    html.span([attribute.class("speed " <> shade(x))], [
      html.text(case x <. 1.0 {
        True -> times(x) <> " · slower"
        False -> times(x)
      }),
    ]),
    html.span([attribute.class("aff")], [
      html.text(
        b.value
        <> " "
        <> b.unit
        <> " against "
        <> b.baseline
        <> " "
        <> b.unit
        <> " · "
        <> b.source,
      ),
    ]),
  ])
}

fn report(r: Report) -> Element(msg) {
  let rows = list.filter(content.benches, fn(b) { b.report == r.name })
  let top = list.fold(rows, 1.0, fn(acc, b) { float.max(acc, speedup(b)) })
  html.div([attribute.class("report")], [
    html.h3([], [
      a(blob <> r.file, r.name),
      html.text(" "),
      html.span([attribute.class("chip code")], [html.text(r.status)]),
    ]),
    case rows {
      [] ->
        html.p([attribute.class("empty")], [
          html.text("no benchmark: the report says why."),
        ])
      _ ->
        html.ol(
          [attribute.class("steps")],
          list.map(rows, fn(b) { step(b, top) }),
        )
    },
  ])
}

fn port() -> Element(msg) {
  section("port", "the port, measured", [
    lede(
      "every number of every port report, step by step. a bar is the speed-up over its own baseline, on a log scale; the deeper the green, the larger it is. a step slower than its baseline is grey, and says so.",
    ),
    ..list.map(content.reports, report)
  ])
}

fn a(href: String, text: String) -> Element(msg) {
  html.a([attribute.href(href)], [html.text(text)])
}

fn section(id: String, title: String, children: List(Element(msg))) {
  html.section([attribute.id(id), attribute.class("wrap")], [
    html.h2([], [html.text(title)]),
    ..children
  ])
}

fn lede(text: String) -> Element(msg) {
  html.p([attribute.class("lede")], [html.text(text)])
}

fn light(name: String) -> Attribute(msg) {
  attribute.class("light " <> name)
}

fn cover() -> Element(msg) {
  html.header([attribute.class("cover wrap")], [
    html.p([attribute.class("kicker")], [
      html.text("h0ffmann / ww3-gpu"),
    ]),
    html.h1([], [html.text("ww3-gpu")]),
    html.p([attribute.class("epithet")], [
      html.text(string.lowercase(content.book_title)),
    ]),
    html.p([attribute.class("lede")], [
      html.text(
        "a book in progress on agentic coding and agentic research, compiled from its own repository, around one problem: moving the expensive kernels of WAVEWATCH III to GPUs without changing the answer.",
      ),
    ]),
    html.p([attribute.class("actions")], [
      html.a([attribute.class("button"), attribute.href("book/")], [
        html.text("read the chapters"),
      ]),
      html.a(
        [attribute.class("button quiet"), attribute.href("ww3-lab-course.pdf")],
        [html.text("the book (PDF)")],
      ),
      html.a([attribute.class("button quiet"), attribute.href(repo)], [
        html.text("the repository"),
      ]),
    ]),
    html.nav([attribute.class("jump")], [
      a("#book", "the book"),
      a("#chapters", "chapters"),
      a("#decisions", "decisions"),
      a("#designs", "designs"),
      a("#brain", "second brain"),
      a("#cite", "cite"),
      a("#related", "related"),
    ]),
  ])
}

fn book() -> Element(msg) {
  section("book", "the book", [
    html.p([attribute.class("title")], [html.text(content.book_title)]),
    lede(content.book_subtitle),
    html.p([], [
      html.text("the repository is the book's source and its evidence: "),
      html.text(int.to_string(list.length(content.chapters))),
      html.text(
        " chapters, built into a PDF on every merge, and the code they describe next to them. every claim carries ",
      ),
      html.span([light("proof")], [html.text("(v)")]),
      html.text(" when it was checked and "),
      html.span([light("warn")], [html.text("⚠")]),
      html.text(" when it was not, and every number comes with its command."),
    ]),
    html.p([attribute.class("links")], [
      a("book/", "the chapters on the web"),
      html.text(" · "),
      a("ww3-lab-course.pdf", "the PDF"),
      html.text(" · "),
      a("andamento/", "progress (pt-BR)"),
      html.text(" · "),
      a("adrs/", "decision records (pt-BR)"),
    ]),
  ])
}

fn chapters() -> Element(msg) {
  section("chapters", "the chapters", [
    html.ol(
      [attribute.class("chapters")],
      list.map(content.chapters, fn(c) {
        html.li([], [
          html.span([attribute.class("num")], [html.text(c.number)]),
          html.a([attribute.href(blob <> c.file)], [html.text(c.title)]),
        ])
      }),
    ),
  ])
}

fn status_class(status: String) -> String {
  case string.lowercase(status) {
    "accepted" | "implemented" -> "proof"
    "proposed" | "draft" -> "code"
    "rejected" | "superseded" -> "muted"
    _ -> "wave"
  }
}

fn records(
  id: String,
  title: String,
  about: String,
  rows: List(Record),
) -> Element(msg) {
  section(id, title, [
    lede(about),
    html.ul(
      [attribute.class("records")],
      list.map(rows, fn(r) {
        html.li([], [
          html.a([attribute.href(blob <> r.file)], [
            html.span([attribute.class("num")], [html.text(r.id)]),
            html.text(r.title),
          ]),
          html.span([attribute.class("chip " <> status_class(r.status))], [
            html.text(string.lowercase(r.status)),
          ]),
          html.span([attribute.class("date")], [html.text(r.date)]),
        ])
      }),
    ),
  ])
}

fn brain() -> Element(msg) {
  let notes = case content.notes {
    [] -> [html.p([attribute.class("empty")], [html.text("no note yet.")])]
    notes -> [
      html.ul([attribute.class("records")], list.map(notes, note)),
    ]
  }
  section("brain", "the second brain", [
    lede(
      "one idea in one note, with a permanent id, links between notes, and a status that stays unverified until a source is cited. the notes are what the chapters are written from.",
    ),
    ..notes
  ])
}

fn note(n: Note) -> Element(msg) {
  html.li([], [
    html.a([attribute.href(blob <> n.file)], [
      html.span([attribute.class("num")], [html.text(n.id)]),
      html.text(n.title),
    ]),
    html.span([attribute.class("chip " <> note_class(n.status))], [
      html.text(n.status),
    ]),
    html.span([attribute.class("date")], [html.text(n.kind)]),
  ])
}

fn note_class(status: String) -> String {
  case status {
    "verified" | "approved" -> "proof"
    "unverified" | "open-question" -> "warn"
    _ -> "code"
  }
}

fn orcid(p: Person) -> List(Element(msg)) {
  case p.orcid {
    "" -> []
    id -> [
      html.text(" · "),
      html.a([attribute.href("https://orcid.org/" <> id)], [
        html.text("ORCID " <> id),
      ]),
    ]
  }
}

fn person(p: Person) -> Element(msg) {
  let role = case p.kind {
    "" -> p.role
    kind -> p.role <> ", " <> string.lowercase(kind)
  }
  html.li([], [
    html.span([attribute.class("name")], [html.text(p.name)]),
    html.span([attribute.class("date")], [html.text(role)]),
    html.span([attribute.class("aff")], [html.text(p.affiliation), ..orcid(p)]),
  ])
}

fn cite() -> Element(msg) {
  let release = case content.latest_release {
    "" -> []
    r -> [html.p([], [html.text("latest release: " <> r)])]
  }
  section("cite", "how to cite", [
    lede(
      "cite the concept DOI for the work as a whole: it always resolves to the latest edition. to pin the exact code you ran, cite that release's own DOI.",
    ),
    html.p([attribute.class("doi")], [
      html.a([attribute.href("https://doi.org/" <> content.doi)], [
        html.text(content.doi),
      ]),
    ]),
    html.p([], [html.text(content.record_title)]),
    ..list.append(release, [
      html.ul([attribute.class("people")], list.map(content.people, person)),
    ])
  ])
}

fn link_item(l: Link) -> Element(msg) {
  html.li([], [
    html.a([attribute.href(l.url)], [html.text(l.name)]),
    html.span([attribute.class("aff")], [html.text(l.what)]),
  ])
}

fn related() -> Element(msg) {
  let authors =
    content.people
    |> list.filter(fn(p) { p.role == "contributor" })
    |> list.map(fn(p) {
      html.li([], [
        html.span([attribute.class("name")], [html.text(p.name)]),
        html.span([attribute.class("aff")], [
          html.text(
            string.lowercase(p.kind) <> " of this work · " <> p.affiliation,
          ),
          ..orcid(p)
        ]),
      ])
    })
  let kind = fn(k) { list.filter(content.links, fn(l: Link) { l.kind == k }) }
  section("related", "related authors and works", [
    html.h3([], [html.text("authors")]),
    html.ul(
      [attribute.class("people")],
      list.append(authors, list.map(kind("author"), link_item)),
    ),
    html.h3([], [html.text("sister works, by the same author")]),
    html.ul([attribute.class("people")], list.map(kind("work"), link_item)),
  ])
}

fn footer() -> Element(msg) {
  html.footer([attribute.class("wrap foot")], [
    html.p([], [
      html.text("every fact on this page was read from the repository at "),
      html.a([attribute.href(repo <> "/commit/" <> content.revision)], [
        html.text(content.revision),
      ]),
      html.text(
        ". WAVEWATCH III is a registered trademark of NOAA's National Weather Service; this project is independent and not endorsed by NOAA.",
      ),
    ]),
  ])
}
