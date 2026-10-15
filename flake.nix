{
  description = "ww3-lab publications: course book and UFRJ/DEL proposal, built with nix-config's labs/publisher toolchain";

  inputs = {
    # The markdown -> document toolchain and the mkPdf / mkDocx helpers live in picasso (h0ffmann/
    # picasso, formerly nix-config; the submodule directory keeps that name) so other
    # repositories can reuse them.
    publisher.url = "github:h0ffmann/picasso?dir=labs/publisher";
    nixpkgs.follows = "publisher/nixpkgs"; # only for symlinkJoin; same pin as the toolchain
    # The site's toolchain (Gleam on JavaScript, Lustre, node, esbuild, just), as the sister sites
    # h0ffmann/h0ffmann and h0ffmann/closed-source take it: `nix develop .#site`, `just site`.
    gleam-website.url = "github:h0ffmann/picasso?dir=labs/gleam-website";
  };

  outputs = { self, publisher, nixpkgs, gleam-website }:
    let
      systems = builtins.attrNames publisher.devShells;
      forAll = f: nixpkgs.lib.genAttrs systems f;
      # Only what scripts/build_pdf.sh reads, so unrelated edits don't rebuild the PDFs.
      src = nixpkgs.lib.cleanSourceWith {
        src = ./.;
        filter = path: _type:
          let p = toString path; r = toString ./.;
          in nixpkgs.lib.any (d: p == "${r}/${d}" || nixpkgs.lib.hasPrefix "${r}/${d}/" p) [ "course" "pubs" "scripts" ];
      };
    in
    {
      devShells = forAll (system: { default = publisher.devShells.${system}.default; }
        // nixpkgs.lib.optionalAttrs (gleam-website.devShells ? ${system}) {
          site = gleam-website.devShells.${system}.default;
        });

      packages = forAll (system:
        let
          inherit (publisher.lib.${system}) mkPdf mkDocx;
          pkgs = nixpkgs.legacyPackages.${system};
          book = mkPdf { name = "ww3-lab-course"; inherit src; command = "bash scripts/build_pdf.sh book"; };
          # The same course as a Word document, for readers who comment or edit rather than read.
          # Part of `all`, so it is built, uploaded and committed next to the PDFs.
          bookDocx = mkDocx { name = "ww3-lab-course-docx"; inherit src; command = "bash scripts/build_docx.sh book"; };
          # The proposal in Word is what the advisors comment on; it has no DEL cover page.
          proposalPtDocx = mkDocx { name = "proposal-pt-docx"; inherit src; command = "bash scripts/build_docx.sh proposal pt"; };
          proposalEnDocx = mkDocx { name = "proposal-en-docx"; inherit src; command = "bash scripts/build_docx.sh proposal en"; };
          proposalPt = mkPdf { name = "proposal-pt"; inherit src; command = "bash scripts/build_pdf.sh proposal pt"; };
          proposalEn = mkPdf { name = "proposal-en"; inherit src; command = "bash scripts/build_pdf.sh proposal en"; };
          all = pkgs.symlinkJoin {
            name = "ww3-lab-pubs";
            paths = [ book bookDocx proposalPt proposalEn proposalPtDocx proposalEnDocx ];
          };
        in
        {
          inherit book all;
          book-docx = bookDocx;
          proposal-pt = proposalPt;
          proposal-en = proposalEn;
          proposal-pt-docx = proposalPtDocx;
          proposal-en-docx = proposalEnDocx;
          default = all;
        });

      checks = forAll (system: {
        pubs = self.packages.${system}.default;
      });
    };
}
