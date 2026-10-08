# Publishing the book on Leanpub, free to read, from CI

How the book (*Without Changing the Answer*, working title; [WFIP-0002](WFIPs/WFIP-0002-lab-as-a-book.md) §5.7)
reaches Leanpub as a free edition, what the repository does on its own, and the steps only the
author can take. Everything about Leanpub below was read on Leanpub's own pages on 2026-10-08 and
is marked `(v)`; what was not found is marked `⚠`.

## What "free" means here

- **Free for the reader.** The minimum price of a Leanpub book can be set to zero; the lowest
  non-zero minimum is 0.99 USD `(v)` [Leanpub Author Manual, "Sell, Pricing"](https://leanpub.com/help/manual).
  The default is a free edition with a suggested price the reader may pay or not.
- **Not free for the author to create.** Leanpub charges per new book: 99 USD once, 49 USD after
  1,000 USD of lifetime royalties, free after 10,000 USD; books created before the change keep every
  feature free `(v)` [leanpub.com/pricing](https://leanpub.com/pricing), 2026-10-08. The price
  applies "regardless of what writing mode you choose" and whatever price, "even free", the book
  has in the store `(v)` same page. An author who already has a Leanpub book from before the change
  is grandfathered.
- **The API costs more.** `preview.json` and `publish.json` need a Pro author membership
  `(v)` [leanpub.com/help/api](https://leanpub.com/help/api): "API access requires a Pro Author
  membership or above." Webhooks (below) are documented without a plan requirement ⚠.

So the free path is: pay the one-time creation fee (or use an existing book), set the minimum price
to 0, let this repository keep the manuscript current, and click *Publish* on leanpub.com or let a
webhook click it.

## What the repository does

`.github/workflows/leanpub.yml` runs on every push to `main` that changes a lesson, a figure
render, `pubs/book/parts.json` or the exporter, and on every published release:

1. `python3 scripts/leanpub_manuscript.py` exports `course/*.md` as a Markua manuscript
   (`just leanpub` locally, into `build/leanpub/manuscript/`): `Book.txt` lists `about.txt`, a part
   heading per part of `parts.json`, then the lessons in order; `Sample.txt` lists the free sample;
   `images/` holds the committed renders of the Mermaid figures. Lesson links become chapter links,
   links to repository files become GitHub URLs at `main`, `$…$` math becomes Markua math, and the
   reading card under each figure becomes an aside. `about.txt` names the commit the edition is
   from and the WW3 pin. The layout follows Leanpub's `(v)`
   [Book.txt, Sample.txt and manuscript files](https://leanpub.com/read/lfm/leanpub-auto-booktxt-sampletxt-and-manuscript-files),
   [Images](https://leanpub.com/read/lfm/leanpub-auto-images), [Markua resources](https://leanpub.com/read/markua/resources).
2. The manuscript is pushed as the only commit of the `leanpub` branch (force-pushed, generated,
   never edited by hand). Leanpub reads `manuscript/` from the branch the book names; Leanpub
   "does not write to your GitHub repository" `(v)`
   [Getting started with the Git and GitHub writing mode](http://help.leanpub.com/en/articles/2916385-getting-started-using-leanpub-s-git-and-github-writing-mode-to-write-and-publish-a-book).
3. If the repository secret `LEANPUB_API_KEY` exists (Pro plan), the job asks Leanpub for a preview
   on a push and publishes on a release, with the release URL as the release notes to readers.
   Without the secret the job ends at step 2.

`scripts/leanpub_manuscript.py --self-test` and `tests/test_leanpub_manuscript.py` are the gates;
`ci.yml` runs the export on every pull request so a chapter that breaks it fails there.

## Steps for the author

1. **Create the book** at [leanpub.com/create/book](https://leanpub.com/create/book): title
   *Without Changing the Answer* (or the one chosen in WFIP-0002 §11), book URL
   `without-changing-the-answer` (if another, set the repository variable `LEANPUB_BOOK_SLUG`),
   language English. Writing mode: *Cloud*, then *Using Git and GitHub*. Repository:
   `h0ffmann/ww3-gpu`; branch for previews and for publishing: `leanpub` (the form offers `main`
   by default) `(v)` same article. Plan: the one the pricing page names for a new book.
2. **Give Leanpub read access**: GitHub → repository → *Settings* → *Collaborators* → *Add people*
   → search `leanpub` → add. "Your invitation will be accepted automatically in the next few
   minutes" `(v)` same article. The repository is public, and Leanpub only reads it.
3. **Run the workflow once**: *Actions* → *Leanpub* → *Run workflow*, so the `leanpub` branch
   exists before Leanpub looks for it. From then on it refreshes itself.
4. **Price and page**: *Sell* → *Pricing*: minimum price 0.00, a suggested price of your choice;
   *Settings*: cover (`docs/media/course-book-title-page.png` is a start), description
   (`DESCRIPTION.md`), categories. The sample is `Sample.txt`: *About this book*, lesson 00 and
   lesson 13.
5. **Preview and publish**, one of two ways:
   - by hand: *Versions* → *Preview* → *Create Preview*, read the PDF, then *Publish*;
   - automatically: open `https://leanpub.com/<book>/help/webhooks`, copy the *preview* payload
     URL and add it on GitHub under *Settings* → *Webhooks* (push events). Every push the workflow
     makes to `leanpub` then triggers a preview. The *publish* payload URL publishes on every push,
     "so you should not set up a publish webhook until you're actually ready" `(v)`
     [Setting up webhooks](http://help.leanpub.com/en/articles/5243991-setting-up-webhooks-if-you-re-using-leanpub-s-github-writing-mode-for-your-book).
     The payload URLs carry the API key: they are secrets.
   - with a Pro plan: add the API key as the repository secret `LEANPUB_API_KEY`; the workflow
     then previews on each push and publishes on each release on its own.
6. **Say so in the README** once the book page exists: the Leanpub URL goes next to the PDF link
   in "The book", and the `pubs/README.md` paragraph on editions names it.

## Caveats

- Markua's part syntax: the manual shows `# Part #` today and announces `{class: part}` as the
  coming syntax `(v)` [Markua headings](https://leanpub.com/read/markua/headings). The exporter
  writes `{class: part}`; if a preview shows the parts as ordinary chapters, change one line in
  `scripts/leanpub_manuscript.py` (`export()`, the `part-<n>.txt` writer) to `# <title> #`.
- Tables, footnotes and HTML other than the figure cards are passed through as GitHub-flavoured
  Markdown; Markua reads tables, and the course has no footnotes. Anything Leanpub's preview
  flags is fixed in `course/`, never in the generated branch.
- The preview is where the book is read before anyone else sees it: a `⚠` in a chapter is as
  visible on Leanpub as on GitHub, by design.
