import importlib.util, json, pathlib, unittest

try:
    import yaml  # noqa: F401  (the publisher shell that runs tests/ in pubs.yml may lack it)
except ImportError:
    yaml = None

ROOT = pathlib.Path(__file__).resolve().parents[1]
if yaml:
    spec = importlib.util.spec_from_file_location("codemeta", ROOT / "scripts" / "codemeta.py")
    codemeta = importlib.util.module_from_spec(spec); spec.loader.exec_module(codemeta)


@unittest.skipUnless(yaml, "PyYAML not installed; citation.yml runs this check with it")
class Codemeta(unittest.TestCase):
    def test_checked_in_file_is_current(self):
        self.assertEqual((ROOT / "codemeta.json").read_text(encoding="utf-8"), codemeta.render())

    def test_each_licence_is_its_own_spdx_url(self):
        meta = json.loads(codemeta.render())
        self.assertEqual(meta["license"], ["https://spdx.org/licenses/MIT",
                                           "https://spdx.org/licenses/LGPL-3.0-or-later"])

    def test_authors_carry_orcid_and_the_concept_doi_is_the_identifier(self):
        meta = json.loads(codemeta.render())
        self.assertTrue(meta["author"][0]["@id"].startswith("https://orcid.org/"))
        self.assertEqual(meta["identifier"], "https://doi.org/10.5281/zenodo.23221351")

    def test_zenodo_contributor_name_is_split(self):
        p = codemeta.person({"name": "Guimarães, Pedro Veras", "affiliation": "LabECO"})
        self.assertEqual((p["givenName"], p["familyName"]), ("Pedro Veras", "Guimarães"))
