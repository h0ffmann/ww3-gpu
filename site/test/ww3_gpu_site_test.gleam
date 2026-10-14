import gleam/list
import gleam/string
import gleeunit
import lustre/element
import ww3_gpu_site/app
import ww3_gpu_site/content
import ww3_gpu_site/page

pub fn main() -> Nil {
  gleeunit.main()
}

fn html() -> String {
  app.view() |> page.document |> element.to_document_string
}

pub fn every_section_test() {
  let page = html()
  list.each(
    ["book", "chapters", "decisions", "designs", "brain", "cite", "related"],
    fn(id) {
      assert string.contains(page, "id=\"" <> id <> "\"")
    },
  )
}

pub fn every_chapter_test() {
  let page = html()
  assert content.chapters != []
  list.each(content.chapters, fn(c) {
    assert string.contains(page, c.file)
  })
}

pub fn every_record_test() {
  let page = html()
  list.each(list.append(content.adrs, content.wfips), fn(r) {
    assert string.contains(page, r.file)
  })
}

pub fn the_doi_test() {
  assert content.doi == "10.5281/zenodo.23221351"
  assert string.contains(html(), "https://doi.org/10.5281/zenodo.23221351")
}

pub fn related_authors_test() {
  let page = html()
  // Guimarães P.V. comes from .zenodo.json, with his ORCID; Fabio Akita from site/data/related.tsv.
  assert string.contains(page, "https://orcid.org/0000-0002-1686-4722")
  assert string.contains(page, "https://github.com/akitaonrails")
  assert string.contains(page, "legendary br dev")
}

pub fn the_book_links_test() {
  let page = html()
  assert string.contains(page, "href=\"book/\"")
  assert string.contains(page, "href=\"ww3-lab-course.pdf\"")
}

pub fn no_script_test() {
  assert !string.contains(html(), "<script")
}
