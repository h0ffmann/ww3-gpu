import importlib.util, json, pathlib, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("results", ROOT / "scripts" / "results.py")
results = importlib.util.module_from_spec(spec); spec.loader.exec_module(results)

RECORD = {"study": "s", "date": "2026-01-01", "routine": "R", "what": "w", "machine": "m",
          "toolchain": "t", "commit": "abc1234", "regtest": "passed: just smoke-ww3-gpu", "statistic": "median of three",
          "runs": [{"backend": "Serial", "command": "cmd", "shim_ms": 2.5, "kernel_ms": 2.0}]}


class Results(unittest.TestCase):
    def setUp(self):
        self.root = pathlib.Path(tempfile.mkdtemp())
        (self.root / "bench" / "results").mkdir(parents=True)

    def write(self, record, name="s.json"):
        p = self.root / "bench" / "results" / name
        p.write_text(json.dumps(record), encoding="utf-8")
        return results.load(self.root / "bench" / "results")

    def test_a_valid_record_has_no_problems(self):
        self.assertEqual(results.problems(self.write(RECORD)), [])

    def test_a_timing_without_passed_regtest_is_rejected(self):
        errs = results.problems(self.write(dict(RECORD, regtest="not run")))
        self.assertTrue(any("without parity" in e for e in errs))

    def test_a_new_record_with_only_l1_is_rejected(self):
        rec = {k: v for k, v in RECORD.items() if k != "regtest"}
        errs = results.problems(self.write(dict(rec, date="2026-10-14", l1="passed: L1_x")))
        self.assertTrue(any("without parity" in e for e in errs))

    def test_an_older_record_keeps_its_l1_as_history(self):
        rec = {k: v for k, v in RECORD.items() if k != "regtest"}
        self.assertEqual(results.problems(self.write(dict(rec, date="2026-09-15", l1="passed: L1_x"))), [])

    def test_a_run_without_its_command_is_rejected(self):
        bad = dict(RECORD, runs=[{"backend": "Serial", "shim_ms": 1, "kernel_ms": 1}])
        self.assertIn("s.json: run 0 missing 'command'", results.problems(self.write(bad)))

    def test_marked_block_is_regenerated_with_a_relative_link(self):
        records = self.write(RECORD)
        page = self.root / "docs" / "p.md"
        page.parent.mkdir()
        page.write_text("x\n<!-- results:s:start -->\nold\n<!-- results:s:end -->\ny\n", encoding="utf-8")
        new, missing = results.render(page, records, self.root)
        self.assertEqual(missing, [])
        self.assertIn("| Serial | 2.5 | 2 | — | — |", new)
        self.assertIn("(../bench/results/s.json)", new)
        self.assertNotIn("old", new)

    def test_a_record_with_columns_renders_them_and_needs_their_keys(self):
        rec = dict(RECORD, columns=[["backend", "backend"], ["Fortran ms", "fortran_ms"], ["ratio", "r"]],
                   runs=[{"backend": "Serial", "command": "cmd", "fortran_ms": 1.5, "r": "2.00x"}])
        records = self.write(rec)
        self.assertEqual(results.problems(records), [])
        page = self.root / "p.md"
        page.write_text("<!-- results:s:start -->\n<!-- results:s:end -->\n", encoding="utf-8")
        new, _ = results.render(page, records, self.root)
        self.assertIn("| backend | Fortran ms | ratio |", new)
        self.assertIn("| Serial | 1.5 | 2.00x |", new)
        bad = dict(rec, runs=[{"backend": "Serial", "command": "cmd", "fortran_ms": 1.5}])
        self.assertIn("s.json: run 0 missing 'r'", results.problems(self.write(bad)))

    PERF = ("case\tfull\nbackend\tcuda\nparts\tKokkos --kokkos SRCE\nrepeat\t3\n"
            "ww3_s\t20.1 20.3 20.2\nww3gpu_s\t10.0 10.2 10.1\nww3_median_s\t20.2\nww3gpu_median_s\t10.1\n"
            "baseline\tCPU, OpenMP, 32 threads\nswitch\tswitch_lab_ompg\nomp_num_threads\t\n"
            "cpu\tIntel(R) Core(TM) i9-14900\nthreads\t32\ngpu\tGPU 0: RTX 4090\ndriver\t595.84\n"
            "toolchain\tGNU Fortran 16.2.0\nkokkos\tkokkos-cuda-5.2.2\ncommit\tabc1234\nww3_commit\tdef5678\n"
            "dirty\t\ndate\t2026-10-17\ncommand\tbash scripts/ww3-gpu.sh --ww3-gpu full --gpu --kokkos SRCE --repeat 3\n")

    def perf(self, text):
        p = self.root / "perf.tsv"
        p.write_text(text, encoding="utf-8")
        return p

    def test_a_perf_summary_becomes_a_valid_record(self):
        r = results.from_perf(self.perf(self.PERF))
        self.assertEqual(r["statistic"], "median of 3 runs")
        self.assertEqual(r["runs"][1]["ratio"], "2.00x")
        self.assertTrue(r["regtest"].startswith("passed: "))
        self.assertEqual(results.problems(self.write(r, name=r["study"] + ".json")), [])

    def test_the_omp_environment_and_load_reach_the_machine_line(self):
        env = "omp_env\tOMP_PROC_BIND=false OMP_PLACES=unset\nload_1min\t0.40 1.10\n"
        r = results.from_perf(self.perf(self.PERF + env))
        self.assertIn("OMP_PROC_BIND=false", r["machine"])
        self.assertIn("load average (1 min) 0.40 1.10", r["machine"])
        self.assertNotIn("load", results.from_perf(self.perf(self.PERF))["machine"])

    def test_a_perf_summary_of_fewer_than_three_runs_is_refused(self):
        with self.assertRaises(ValueError):
            results.from_perf(self.perf(self.PERF.replace("repeat\t3", "repeat\t2")))

    def test_a_record_is_never_overwritten(self):
        p = self.perf(self.PERF)
        res = self.root / "bench" / "results"
        results.record_cmd(p, res)
        with self.assertRaises(FileExistsError):
            results.record_cmd(p, res)

    def test_a_study_name_cannot_leave_the_results_directory(self):
        with self.assertRaises(ValueError):
            results.record_cmd(self.perf(self.PERF.replace("case\tfull", "case\t../../x")),
                               self.root / "bench" / "results")

    def test_a_gpu_uuid_never_reaches_the_record(self):
        text = self.PERF.replace("gpu\tGPU 0: RTX 4090", "gpu\tGPU 0: RTX 4090 (UUID: GPU-0000-1111)")
        r = results.from_perf(self.perf(text))
        self.assertNotIn("UUID", r["machine"])
        self.assertIn("RTX 4090", r["machine"])

    def test_a_gpu_record_says_it_was_judged_by_tolerance(self):
        self.assertIn("not bits", results.from_perf(self.perf(self.PERF))["regtest"])
        cpu = results.from_perf(self.perf(self.PERF.replace("backend\tcuda", "backend\topenmp")))
        self.assertNotIn("not bits", cpu["regtest"])

    def test_a_dirty_tree_is_said_in_the_record(self):
        r = results.from_perf(self.perf(self.PERF.replace("dirty\t\n", "dirty\tyes\n")))
        self.assertIn("uncommitted", r["commit_note"])

    def test_checked_in_tables_are_current(self):
        self.assertEqual(results.main(["check"]), 0)
