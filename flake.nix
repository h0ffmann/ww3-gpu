{
  description = "ww3-lab publications: the UFRJ/DEL proposal, built with picasso's labs/publisher toolchain";

  inputs = {
    # The markdown -> document toolchain and the mkPdf / mkDocx helpers live in picasso (h0ffmann/
    # picasso, formerly nix-config; the submodule directory has the same name) so other
    # repositories can reuse them.
    publisher.url = "github:h0ffmann/picasso?dir=labs/publisher";
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
          in nixpkgs.lib.any (d: p == "${r}/${d}" || nixpkgs.lib.hasPrefix "${r}/${d}/" p) [ "pubs" "scripts" ];
      };
    in
    {
      devShells = forAll (system: { default = publisher.devShells.${system}.default; });

      packages = forAll (system:
        let
          inherit (publisher.lib.${system}) mkPdf mkDocx;
          pkgs = nixpkgs.legacyPackages.${system};
          # The proposal in Word is what the advisors comment on; it has no DEL cover page.
          proposalPtDocx = mkDocx { name = "proposal-pt-docx"; inherit src; command = "bash scripts/build_docx.sh proposal pt"; };
          proposalEnDocx = mkDocx { name = "proposal-en-docx"; inherit src; command = "bash scripts/build_docx.sh proposal en"; };
          proposalPt = mkPdf { name = "proposal-pt"; inherit src; command = "bash scripts/build_pdf.sh proposal pt"; };
          proposalEn = mkPdf { name = "proposal-en"; inherit src; command = "bash scripts/build_pdf.sh proposal en"; };
          all = pkgs.symlinkJoin {
            name = "ww3-lab-pubs";
            paths = [ proposalPt proposalEn proposalPtDocx proposalEnDocx ];
          };
        in
        {
          inherit all;
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
