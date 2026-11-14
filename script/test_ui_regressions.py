"""Guards for interface fixes that a merge from upstream could quietly undo."""
import html
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def translations(context):
    ts = read("res/translations/ru_RU.ts")
    block = re.search(r"<context>\s*<name>%s</name>(.*?)</context>" % re.escape(context), ts, re.S)
    found = {}
    if block:
        for m in re.finditer(r"<source>(.*?)</source>\s*<translation([^>]*)>(.*?)</translation>", block.group(1), re.S):
            done = "unfinished" not in m.group(2) and m.group(3).strip()
            found[html.unescape(m.group(1))] = bool(done)
    return found


def tr_strings(path):
    text = read(path)
    out = {"n": "\n", "t": "\t", '"': '"', "\\": "\\"}
    result = set()
    for m in re.finditer(r'(?<![\w:])tr\(\s*((?:"(?:[^"\\\n]|\\.)*"\s*)+)[,)]', text):
        parts = re.findall(r'"((?:[^"\\\n]|\\.)*)"', m.group(1))
        result.add("".join(re.sub(r"\\(.)", lambda e: out.get(e.group(1), e.group(0)), p) for p in parts))
    return result


class InterfaceRegressions(unittest.TestCase):
    def test_advanced_interface_is_the_default(self):
        self.assertNotRegex(read("src/database/SettingsRepo.cpp"), r"simple_mode\s*=\s*true")

    def test_process_lookup_is_forced_only_on_windows(self):
        text = read("src/configs/generate.cpp")
        at = text.index('route["find_process"] = true')
        guard = text.rfind("#ifdef Q_OS_WIN", 0, at)
        self.assertNotEqual(guard, -1)
        self.assertNotIn("#endif", text[guard:at])

    def test_frame_waits_only_for_dialog_windows(self):
        body = read("src/ui/widget/NewHome.cpp").split("void MainWindow::syncWindowFrame()", 1)[1].split("\n}\n", 1)[0]
        self.assertIn("dialog->isWindow()", body)

    def test_tab_close_button_is_drawn_by_the_new_style(self):
        self.assertIn("case PE_IndicatorTabClose:", read("src/ui/widget/NewHome.cpp"))

    def test_arranging_lives_in_the_theme_settings(self):
        self.assertNotIn('addAction(T("Arrange the window"))', read("src/ui/widget/NewHome.cpp"))
        self.assertIn('"Arrange the window"', read("src/ui/setting/dialog_basic_settings.cpp"))

    def test_new_screens_are_translated_to_russian(self):
        # Names of protocols and the like stay as they are.
        keep = {"example.com", "AdBlock", "TCP Brutal", "UDP/443"}
        for contexts, path in [
            (["RouteDnsEditor"], "src/ui/setting/RouteDnsEditor.cpp"),
            (["ProcessPicker"], "src/ui/setting/ProcessPicker.cpp"),
            (["DialogRuleTester"], "src/ui/setting/dialog_rule_tester.cpp"),
            (["MuxSettingsWidget", "XrayMuxSettingsWidget"], "src/ui/widget/MuxSettingsWidget.cpp"),
        ]:
            done = {}
            for context in contexts:
                for source, ok in translations(context).items():
                    done[source] = done.get(source) or ok
            for source in tr_strings(path):
                if not re.search(r"[A-Za-z]{3}", source) or source in keep:
                    continue
                with self.subTest(path=path, source=source):
                    self.assertTrue(done.get(source), "no Russian translation")


if __name__ == "__main__":
    unittest.main()
