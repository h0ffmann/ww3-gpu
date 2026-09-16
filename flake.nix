{
  description = "ww3-lab publications: course book and UFRJ/DEL proposal, built with nix-config's labs/publisher toolchain";

  inputs = {
    # The markdown -> document toolchain and the mkPdf / mkDocx helpers live in nix-config so other
    # repositories can reuse them.
    publisher.url = "github:h0ffmann/nix-config?dir=labs/publisher";
    nixpkgs.follows = "publisher/nixpkgs"; # only for symlinkJoin; same pin as the toolchain
  };

  outputs = { self, publisher, nixpkgs }:
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
      devShells = forAll (system: { default = publisher.devShells.${system}.default; });

      packages = forAll (system:
        let
          inherit (publisher.lib.${system}) mkPdf mkDocx;
          pkgs = nixpkgs.legacyPackages.${system};
          book = mkPdf { name = "ww3-lab-course"; inherit src; command = "bash scripts/build_pdf.sh book"; };
          # Optional docx of the same course, for readers who want to comment or edit it in Word.
          # Kept out of `all` so the publisher Action keeps committing PDFs and nothing else.
          bookDocx = mkDocx { name = "ww3-lab-course-docx"; inherit src; command = "bash scripts/build_docx.sh"; };
          proposalPt = mkPdf { name = "proposal-pt"; inherit src; command = "bash scripts/build_pdf.sh proposal pt"; };
          proposalEn = mkPdf { name = "proposal-en"; inherit src; command = "bash scripts/build_pdf.sh proposal en"; };
          all = pkgs.symlinkJoin { name = "ww3-lab-pubs"; paths = [ book proposalPt proposalEn ]; };
        in
        { inherit book all; book-docx = bookDocx; proposal-pt = proposalPt; proposal-en = proposalEn; default = all; });

      checks = forAll (system: {
        pubs = self.packages.${system}.default;
        # The docx path is optional for readers, not untested in CI.
        docx = self.packages.${system}.book-docx;
      });
    };
}
