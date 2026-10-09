import datetime as dt, importlib.util, pathlib, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("noaa_notices", ROOT / "scripts" / "noaa_notices.py")
nn = importlib.util.module_from_spec(spec); spec.loader.exec_module(nn)

TODAY = dt.date(2026, 10, 9)
# File names as published on weather.gov/notification/ (fetched 2026-10-09); the markup is a stand-in.
PAGE = """
<li>Apr 15: <a href="/media/notification/pdf_2026/pns26-29_Science_for_GFSv17.pdf"><b>PNS26-29</b>:
  Soliciting Comments on Proposed Upgrade of the Global Forecast System</a></li>
<li>Sep 11: <a href="https://www.weather.gov/media/notification/pdf_2026/pns26-67_September-PACIFIC_Monthly_Comms_Test_09172026.pdf">PNS26-67: Pacific tsunami test</a></li>
<li><a href="https://www.weather.gov/media/notification/pdf_2026/SCN26-47_Updated_Retire_NAM_SREF_HREF_HiresW_NAM_MOS.aab.pdf">SCN26-47 &amp; NAM</a></li>
<li><a href="https://www.weather.gov/media/notification/tins/tin10-44wave_model_aab.pdf">TIN10-44 wave</a></li>
<li><a href="https://www.weather.gov/media/notification/pdf_2025/psn25-12_HAFSv2.1.pdf">PNS25-12: HAFS v2.1</a></li>
<li><a href="https://www.weather.gov/media/notification/pdf_2025/pns25-38_Add_Mountain_Wave_Turb_G-AIRMET.pdf">PNS25-38: Mountain Wave Turbulence</a></li>
<li><a href="https://www.weather.gov/media/notification/pdf_2026/pns26-68_Discontinue_High_Wind_and_Associated_Seas_Graphic_for_Dissem.pdf">PNS26-68: High Wind and Associated Seas Graphics</a></li>
<li><a href="/media/notification/pdf_2026/notice_index.html">not a notice</a></li>
"""
WW3 = {"tags": ["6.07"], "production": {"production/GFS.v17": "7a23cd7"}}


class NoaaNotices(unittest.TestCase):
    def setUp(self):
        self.found = nn.notices(PAGE, "https://www.weather.gov/notification/")

    def test_notices_are_keyed_by_pdf_name_with_absolute_url_and_plain_title(self):
        self.assertEqual(set(self.found), {"pns26-29_Science_for_GFSv17",
                                           "pns26-67_September-PACIFIC_Monthly_Comms_Test_09172026",
                                           "SCN26-47_Updated_Retire_NAM_SREF_HREF_HiresW_NAM_MOS.aab",
                                           "tin10-44wave_model_aab",
                                           "psn25-12_HAFSv2.1",  # NOAA's own typo for PNS 25-12
                                           "pns25-38_Add_Mountain_Wave_Turb_G-AIRMET",
                                           "pns26-68_Discontinue_High_Wind_and_Associated_Seas_Graphic_for_Dissem"})
        n = self.found["pns26-29_Science_for_GFSv17"]
        self.assertEqual(n["url"], "https://www.weather.gov/media/notification/pdf_2026/pns26-29_Science_for_GFSv17.pdf")
        self.assertTrue(n["title"].startswith("PNS26-29 : Soliciting"))
        self.assertEqual(self.found["SCN26-47_Updated_Retire_NAM_SREF_HREF_HiresW_NAM_MOS.aab"]["title"], "SCN26-47 & NAM")

    def test_first_run_reports_recent_relevant_notices_only(self):
        out = nn.report({}, self.found, WW3, False, TODAY)
        self.assertIn("pns26-29_Science_for_GFSv17", out)
        self.assertNotIn("tin10-44", out)                 # 2010: history
        self.assertIn("psn25-12_HAFSv2.1", out)
        self.assertIn("Associated_Seas", out)
        self.assertNotIn("PACIFIC_Monthly", out)          # filtered, counted
        self.assertNotIn("Mountain_Wave", out)            # aviation, not ocean waves
        self.assertIn("3 other new notice(s)", out)
        self.assertIn("production/GFS.v17` (new) -> `7a23cd7`", out)

    def test_nothing_new_says_so(self):
        old = {"checked": "2026-10-01", "notices": list(self.found), "ww3": WW3}
        self.assertTrue(nn.report(old, self.found, WW3, False, TODAY).endswith("Nothing relevant since the snapshot."))

    def test_moved_production_branch_and_new_tag_are_reported(self):
        old = {"checked": "2026-10-01", "notices": list(self.found), "ww3": WW3}
        now = {"tags": ["6.07", "7.14"], "production": {"production/GFS.v17": "abcdef0"}}
        out = nn.report(old, self.found, now, False, TODAY)
        self.assertIn("new tags `7.14`", out)
        self.assertIn("production/GFS.v17` 7a23cd7 -> `abcdef0`", out)

    def test_all_lists_the_filtered_notices(self):
        self.assertIn("PACIFIC_Monthly", nn.report({}, self.found, WW3, True, TODAY))


if __name__ == "__main__":
    unittest.main()
