"""Verify that the UFC 4 watcher saves once per completed fight."""

import pathlib
import os
import struct
import sys
import tempfile
import unittest
from unittest import mock

TOOLS = pathlib.Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
from watch_fight_reports import CaptureState, DEFAULT_LOBBY_URL, configured_lobby_url
import export_fight_report


class WatcherStateTest(unittest.TestCase):
    def test_result_and_stats_screen_do_not_create_two_reports(self):
        tracker = CaptureState()
        self.assertIsNone(tracker.observe((123, 1)))
        first = tracker.observe((123, 3))
        self.assertIsNotNone(first)
        tracker.captured = True
        self.assertIsNone(tracker.observe(None))
        self.assertIsNone(tracker.observe((123, 3)))
        self.assertIsNone(tracker.observe((123, 1)))
        second = tracker.observe((123, 3))
        self.assertIsNotNone(second)
        self.assertNotEqual(first, second)

    def test_lobby_url_reuses_default_and_environment_override(self):
        with mock.patch.dict(os.environ, {}, clear=True), \
                mock.patch("watch_fight_reports.install_folder",
                           return_value=pathlib.Path(tempfile.gettempdir()) / "missing-ufc4"):
            self.assertEqual(configured_lobby_url(), DEFAULT_LOBBY_URL)
        with mock.patch.dict(os.environ,
                             {"PARSECTAGON_LOBBY_URL": "https://test.parsectagon.invalid"},
                             clear=True):
            self.assertEqual(configured_lobby_url(),
                             "https://test.parsectagon.invalid")

    def test_explicit_lobby_url_has_priority(self):
        with mock.patch.dict(os.environ,
                             {"PARSECTAGON_LOBBY_URL": "https://environment.invalid"}):
            self.assertEqual(configured_lobby_url("https://explicit.invalid"),
                             "https://explicit.invalid")

    def test_ufc4_reports_migrate_without_moving_ufc6_reports(self):
        with tempfile.TemporaryDirectory() as temporary:
            documents = pathlib.Path(temporary)
            old = documents / "UFC 6 Parsectagon Overlay" / "Fight Reports"
            old.mkdir(parents=True)
            ufc4 = old / "ufc4-test.json"
            sidecar = old / "ufc4-test.json.upload.json"
            ufc6 = old / "fight-test.json"
            for path in (ufc4, sidecar, ufc6):
                path.write_text("{}", encoding="utf-8")
            with mock.patch.object(export_fight_report, "documents_folder",
                                   return_value=documents):
                target = export_fight_report.fight_reports_folder()
            self.assertEqual(target, documents / "UFC Parsectagon Overlay" /
                             "UFC4" / "Fight Reports")
            self.assertTrue((target / ufc4.name).is_file())
            self.assertTrue((target / sidecar.name).is_file())
            self.assertTrue(ufc6.is_file())

    def test_report_from_fields_includes_stuns_control_time_and_tko(self):
        def fbits(val):
            return struct.unpack("<I", struct.pack("<f", val))[0]
        fields = {
            "Global_MatchResultType": (1, 2, (1, 0), (0, 0)),
            "Global_RoundNum": (1, 2, (0, 0), (0, 0)),
            "Global_RoundLength": (1, 2, (fbits(300.0), 0), (0, 0)),
            "Online_MatchTimeSecs": (1, 2, (31, 0), (0, 0)),
            "Fighter_MatchResult": (1, 2, (0, 2), (1, 0)),
            "Online_MatchResultTypeString": (1, 2, (export_fight_report.fnv_key("TKO"), 0), (0, 0)),
            "Online_KnockdownsLanded": (1, 2, (0, 2), (0, 0)),
            "Online_StunsLanded": (1, 2, (1, 3), (0, 0)),
            "Online_AdvanceStrikesLanded": (1, 2, (5, 18), (0, 0)),
            "Online_SignificantStrikesThrown": (1, 2, (12, 24), (0, 0)),
            "Online_StrikesLanded": (1, 2, (7, 22), (0, 0)),
            "Online_StrikesThrown": (1, 2, (15, 29), (0, 0)),
            "Online_TakedownsAttempted": (1, 2, (1, 2), (0, 0)),
            "Online_TakedownsLanded": (1, 2, (0, 1), (0, 0)),
            "Online_Control_Duration": (1, 2, (fbits(12.4), fbits(65.6)), (0, 0)),
        }
        report = export_fight_report.report_from_fields(fields, "Red", "Blue")
        self.assertEqual(report["game"], "ufc4")
        self.assertFalse(report["fightStatisticsComplete"])
        self.assertEqual(report["reportStage"], "complete")
        self.assertEqual(report["result"]["winnerCorner"], "red")
        self.assertEqual(report["result"]["method"], "TKO")
        self.assertEqual(report["result"]["gameMethodCode"], 1)
        self.assertEqual(report["result"]["round"], 1)
        self.assertEqual(report["result"]["clockSeconds"], 31)
        self.assertEqual(report["fighters"][0]["stats"]["stuns"], 3)
        self.assertEqual(report["fighters"][0]["stats"]["controlTimeSeconds"], 66)
        self.assertEqual(report["fighters"][1]["stats"]["controlTimeSeconds"], 12)


if __name__ == "__main__":
    unittest.main()
