//// The HTML document around the page, in language `l` (app.t).

import lustre/attribute
import lustre/element.{type Element}
import lustre/element/html
import ww3_gpu_site/app

/// Where the site is published. A link preview needs whole addresses.
pub const address = "https://h0ffmann.github.io/ww3-gpu/"

/// The address of the page in language `l`: the English one at the root, the other under its
/// language's name.
pub fn address_of(l: String) -> String {
  case l {
    "en" -> address
    _ -> address <> l <> "/"
  }
}

pub fn document(l: String, body: Element(msg)) -> Element(msg) {
  let meta = fn(name, value) {
    html.meta([attribute.name(name), attribute.content(value)])
  }
  let property = fn(name, value) {
    html.meta([attribute.attribute("property", name), attribute.content(value)])
  }
  let stylesheet = fn(href) {
    html.link([attribute.rel("stylesheet"), attribute.href(href)])
  }
  let alternate = fn(lang) {
    html.link([
      attribute.rel("alternate"),
      attribute.attribute("hreflang", lang),
      attribute.href(address_of(lang)),
    ])
  }
  let title = "ww3-gpu: " <> app.t(l, "without changing the answer")
  let description =
    app.t(
      l,
      "a book in progress on agentic coding and agentic research, around a bit-for-bit GPU port of WAVEWATCH III.",
    )
  html.html([attribute.lang(l)], [
    html.head([], [
      html.meta([attribute.charset("utf-8")]),
      meta("viewport", "width=device-width, initial-scale=1"),
      html.title([], title),
      meta("description", description),
      meta("author", "Matheus Hoffmann"),
      html.link([attribute.rel("canonical"), attribute.href(address_of(l))]),
      alternate("en"),
      alternate("pt-BR"),
      property("og:title", title),
      property("og:description", description),
      property("og:type", "website"),
      property("og:url", address_of(l)),
      html.link([
        attribute.rel("icon"),
        attribute.href(app.up(l) <> "favicon.svg"),
        attribute.type_("image/svg+xml"),
      ]),
      html.link([
        attribute.rel("preconnect"),
        attribute.href("https://fonts.googleapis.com"),
      ]),
      html.link([
        attribute.rel("preconnect"),
        attribute.href("https://fonts.gstatic.com"),
        attribute.crossorigin(""),
      ]),
      stylesheet(
        "https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:opsz,wght@12..96,600;12..96,800&family=Newsreader:ital,opsz,wght@0,6..72,400;0,6..72,600;1,6..72,400&family=JetBrains+Mono:wght@400;500&display=swap",
      ),
      stylesheet(app.up(l) <> "style.css"),
    ]),
    html.body([], [body]),
  ])
}
