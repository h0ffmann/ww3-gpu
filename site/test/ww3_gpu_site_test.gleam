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
    [
      "book",
      "chapters",
      "decisions",
      "designs",
      "port",
      "brain",
      "cite",
      "related",
    ],
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

pub fn every_benchmark_number_test() {
  let page = html()
  assert content.reports != []
  assert content.benches != []
  list.each(content.reports, fn(r) {
    assert string.contains(page, r.file)
  })
  list.each(content.benches, fn(b) {
    assert string.contains(
      page,
      b.value <> " " <> b.unit <> " against " <> b.baseline,
    )
    assert string.contains(page, app.times(app.speedup(b)))
  })
}

pub fn a_slower_step_is_never_green_test() {
  assert app.shade(0.7) == "slower"
  assert app.shade(1.0) == "g1"
  assert app.shade(4.35) == "g2"
  assert app.shade(34.5) == "g3"
  assert app.shade(531.0) == "g4"
  assert app.times(34.5066) == "34.5x"
  assert app.times(0.7008) == "0.7x"
  assert app.times(531.06) == "531x"
}

pub fn the_global_numbers_and_each_record_are_on_the_page_test() {
  let page = html()
  assert list.length(content.stats) == 5
  list.each(content.stats, fn(s) {
    assert string.contains(page, s.name)
    assert string.contains(page, ">" <> s.share <> "<")
    assert string.contains(page, ">" <> s.count <> "<")
  })
  list.each(content.reports, fn(r) {
    assert r.regtested != ""
    assert string.contains(page, string.replace(r.regtested, "'", "&#39;"))
  })
}
