//// The HTML document around the page.

import lustre/attribute
import lustre/element.{type Element}
import lustre/element/html

/// Where the site is published. A link preview needs whole addresses.
pub const address = "https://h0ffmann.github.io/ww3-gpu/"

pub fn document(body: Element(msg)) -> Element(msg) {
  let meta = fn(name, value) {
    html.meta([attribute.name(name), attribute.content(value)])
  }
  let property = fn(name, value) {
    html.meta([attribute.attribute("property", name), attribute.content(value)])
  }
  let stylesheet = fn(href) {
    html.link([attribute.rel("stylesheet"), attribute.href(href)])
  }
  let title = "ww3-gpu: without changing the answer"
  let description =
    "a book in progress on agentic coding and agentic research, around a bit-for-bit GPU port of WAVEWATCH III."
  html.html([attribute.lang("en")], [
    html.head([], [
      html.meta([attribute.charset("utf-8")]),
      meta("viewport", "width=device-width, initial-scale=1"),
      html.title([], title),
      meta("description", description),
      meta("author", "Matheus Hoffmann"),
      html.link([attribute.rel("canonical"), attribute.href(address)]),
      property("og:title", title),
      property("og:description", description),
      property("og:type", "website"),
      property("og:url", address),
      html.link([
        attribute.rel("icon"),
        attribute.href("favicon.svg"),
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
      stylesheet("style.css"),
    ]),
    html.body([], [body]),
  ])
}
