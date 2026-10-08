#!/usr/bin/env python3
"""Generate codemeta.json from CITATION.cff and .zenodo.json; --check fails when it is stale.

codemeta.json is what Software Heritage, OpenAIRE and other software indexes read. It is derived,
never edited: CITATION.cff owns the authors, title, abstract, licences, keywords and DOI;
.zenodo.json adds the supervisor as a contributor. cffconvert 2.0's codemeta output is not used
because it renders a list of licences as one broken URL ("spdx.org/licenses/['MIT', ...]").

    scripts/codemeta.py           # rewrite codemeta.json
    scripts/codemeta.py --check   # exit 1 if codemeta.json differs from what would be written
"""
import json
import pathlib
import sys

import yaml

ROOT = pathlib.Path(__file__).resolve().parents[1]
LANGUAGES = ["C++", "Fortran", "Shell"]  # lab code only; Python is repo tooling (AGENTS.md)


def person(p, affiliation_key="affiliation"):
    out = {"@type": "Person"}
    if p.get("orcid"):
        orcid = p["orcid"]
        out["@id"] = orcid if orcid.startswith("https://") else f"https://orcid.org/{orcid}"
    if "given-names" in p:
        out["givenName"], out["familyName"] = p["given-names"], p["family-names"]
    else:  # Zenodo's "Family, Given"
        family, _, given = p["name"].partition(", ")
        out["givenName"], out["familyName"] = given, family
    if p.get(affiliation_key):
        out["affiliation"] = {"@type": "Organization", "name": p[affiliation_key]}
    return out


def build(cff, zenodo):
    concept = next(i["value"] for i in cff["identifiers"] if "Concept" in i.get("description", ""))
    licences = cff["license"] if isinstance(cff["license"], list) else [cff["license"]]
    return {
        "@context": "https://w3id.org/codemeta/3.0",
        "@type": "SoftwareSourceCode",
        "name": cff["title"],
        "description": " ".join(cff["abstract"].split()),
        "author": [person(a) for a in cff["authors"]],
        "contributor": [person(c) for c in zenodo.get("contributors", [])],
        "identifier": f"https://doi.org/{concept}",
        "codeRepository": cff["repository-code"],
        "license": [f"https://spdx.org/licenses/{l}" for l in licences],
        "keywords": cff["keywords"],
        "programmingLanguage": LANGUAGES,
        "readme": f"{cff['repository-code']}/blob/main/README.md",
    }


def render(root=ROOT):
    cff = yaml.safe_load((root / "CITATION.cff").read_text(encoding="utf-8"))
    zenodo = json.loads((root / ".zenodo.json").read_text(encoding="utf-8"))
    return json.dumps(build(cff, zenodo), indent=2, ensure_ascii=False) + "\n"


def main(argv):
    out, text = ROOT / "codemeta.json", render()
    if "--check" in argv:
        if not out.exists() or out.read_text(encoding="utf-8") != text:
            print("codemeta.json is stale: run `python3 scripts/codemeta.py` and commit it", file=sys.stderr)
            return 1
        print("codemeta: current")
        return 0
    out.write_text(text, encoding="utf-8")
    print(f"wrote {out.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
