//// `gleam run -m ww3_gpu_site/prerender > dist/index.html`: the whole page, rendered once at
//// build time. It needs no JavaScript.

import gleam/io
import lustre/element
import ww3_gpu_site/app
import ww3_gpu_site/page

pub fn main() -> Nil {
  app.view()
  |> page.document
  |> element.to_document_string
  |> io.println
}
