// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace UFC6OverlaySetup {
    public sealed class OverlayConfig {
        public const uint PadBack = 0x0004, PadMenu = 0x0008, PadLS = 0x0100, PadRS = 0x0200, PadLB = 0x0400, PadRB = 0x0800, PadY = 0x1000, PadB = 0x2000, PadA = 0x4000, PadX = 0x8000;
        public const uint ShortcutMask = 0x0000FCFE;
        public const uint OpenChordDefault = PadLB | PadRB | PadMenu;
        public const uint KeyCtrl = 0x100, KeyAlt = 0x200, KeyShift = 0x400;

        public static readonly string[] BindingNames = {
            "Open overlay", "Take screenshot", "Cheats", "Settings",
            "Savegame backup", "Restart game", "Quit game", "Restart PC",
            "Shut down PC", "Photo Mode", "Search features"
        };
        public static readonly string[] BindingIds = {
            "Open", "Screenshot", "Cheats", "Settings",
            "Backup", "RestartGame", "QuitGame", "RestartPC",
            "ShutdownPC", "PhotoMode", "Search"
        };
        public static readonly string[] MasterPresetNames = {
            "Custom", "Parsectagon Apex", "Octagon Pill", "Championship Solid",
            "Stealth Island", "Classic Box", "Side Blade", "Bottom Dock",
            "Bento Deck", "Broadcast Card", "Octagon Hub"
        };
        public static readonly string[] LayoutStyleNames = {
            "Classic Box", "Top Bar", "Floating Island", "Side Blade",
            "Bottom Dock", "Bento Deck", "Broadcast Card", "Octagon Hub"
        };
        public static readonly string[] BackdropStyleNames = { "Glass", "Opaque" };
        public static readonly string[] ThemeChoiceNames = { "From config", "Green", "Blue", "Red", "Purple", "Amber", "Parsectagon" };
        public static readonly string[] BoxStyleNames = { "Rounded", "Square", "Outline", "Pill", "Octagon" };
        public static readonly string[] WindowSizeNames = { "Compact", "Expanded", "Tall" };
        public static readonly string[] ButtonIconNames = { "Xbox", "PlayStation", "Keyboard" };

        // Switches & Gameplay
        public bool SkipStartupSplashes = true;
        public bool SuppressOnlineNotices = true;
        public bool StartupSkipPopups = false;
        public bool MuteWhenUnfocused = false;
        public bool InstantReplayInFight = false;
        public bool OfferDecisionReplays = false;
        public bool IncludeOverlayInScreenshots = true;
        public bool AutoLaunchAfterRestart = true;
        public bool OpenFavoritesFirst = false;
        public bool LongevityProtection = false;
        public int TrainingRewardMultiplier = 1;

        // Controls & Shortcuts
        public string ButtonIcons = "Xbox";
        public uint OpeningShortcut = OpenChordDefault;
        public bool GuideButtonOpensMenu = true;
        public bool FirstPlayerOnly = false;
        public uint[] Keybinds = new uint[BindingIds.Length];

        // Appearance
        public int MasterPreset = 1;
        public int LayoutStyle = 1;
        public int BackdropStyle = 0;
        public int ThemeChoice = 6;
        public int UIScale = 100;
        public int FontSize = 18;
        public int BoxStyle = 4;
        public int WindowSize = 2;

        // FPV Camera (ufc6_fpv.ini)
        public float FpvPitchDeg = 0f;
        public float FpvHeightM = 0f;
        public float FpvFwdM = 0.24f;
        public int FpvFovDeg = 90;
        public float FpvGrappleDist = 0.65f;

        public OverlayConfig() {
            Keybinds[9] = 0x78;   // F9 -> Photo Mode
            Keybinds[10] = 0x0146; // Ctrl+F -> Search features
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
        static extern uint GetPrivateProfileStringW(string section, string key, string def, StringBuilder retVal, int size, string filePath);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
        static extern int GetPrivateProfileIntW(string section, string key, int def, string filePath);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        static extern bool WritePrivateProfileStringW(string section, string key, string val, string filePath);

        static string ReadStr(string ini, string section, string key, string def) {
            var sb = new StringBuilder(256);
            GetPrivateProfileStringW(section, key, def, sb, sb.Capacity, ini);
            return sb.ToString();
        }
        static uint ParseHexOrDec(string raw, uint fallback) {
            if (string.IsNullOrWhiteSpace(raw)) return fallback;
            raw = raw.Trim();
            uint v;
            if (raw.StartsWith("0x", StringComparison.OrdinalIgnoreCase)) {
                if (uint.TryParse(raw.Substring(2), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out v)) return v;
            } else if (uint.TryParse(raw, NumberStyles.Integer, CultureInfo.InvariantCulture, out v)) {
                return v;
            }
            return fallback;
        }
        public static int PopCount(uint x) {
            int n = 0;
            while (x != 0) { x &= x - 1; n++; }
            return n;
        }
        public static bool IsPadChordValid(uint pad) {
            if ((pad & ~ShortcutMask) != 0) return false;
            int n = PopCount(pad);
            return n >= 2 && n <= 4;
        }
        public static bool IsKeyValid(uint key) {
            if (key == 0) return true;
            uint vk = key & 0xFF;
            if ((key & ~0x7FFu) != 0 || vk < 8 || vk == 0x1B || vk == 0x79 || vk == 0x5B || vk == 0x5C || vk == 0x5D) return false;
            return vk != 0x10 && vk != 0x11 && vk != 0x12 && !(vk >= 0xA0 && vk <= 0xA5);
        }

        public static string NormalizeFolder(string folder) {
            folder = Path.GetFullPath(folder.Trim().Trim('"'));
            if (!Directory.Exists(folder)) throw new IOException("Choose PORTHEON\\games\\UFC6 (bin / novo) or a Clean Kyty folder (kyty_emulator.exe).");
            if (!File.Exists(Path.Combine(folder, "UFC 6.exe")) && File.Exists(Path.Combine(folder, "bin", "UFC 6.exe"))) folder = Path.Combine(folder, "bin");
            else if (!File.Exists(Path.Combine(folder, "UFC 6.exe")) && !File.Exists(Path.Combine(folder, "UFC 6 novo.exe")) && File.Exists(Path.Combine(folder, "novo", "UFC 6 novo.exe"))) folder = Path.Combine(folder, "novo");
            if (!File.Exists(Path.Combine(folder, "UFC 6.exe")) && !File.Exists(Path.Combine(folder, "UFC 6 novo.exe")) && !File.Exists(Path.Combine(folder, "kyty_emulator.exe")))
                throw new IOException("Game or Kyty engine was not found.");
            return folder;
        }
        public static List<string> TargetFolders(string folder) {
            string norm = NormalizeFolder(folder);
            var list = new List<string> { norm };
            DirectoryInfo parent = Directory.GetParent(norm);
            if (parent != null) {
                string leaf = Path.GetFileName(norm);
                string sibling = string.Equals(leaf, "bin", StringComparison.OrdinalIgnoreCase)
                    ? Path.Combine(parent.FullName, "novo")
                    : string.Equals(leaf, "novo", StringComparison.OrdinalIgnoreCase)
                        ? Path.Combine(parent.FullName, "bin")
                        : null;
                if (sibling != null && (File.Exists(Path.Combine(sibling, "UFC 6.exe")) || File.Exists(Path.Combine(sibling, "UFC 6 novo.exe"))))
                    list.Add(sibling);
            }
            return list;
        }

        public static OverlayConfig Load(string folder) {
            var cfg = new OverlayConfig();
            string ini = Path.Combine(folder, "ufc6-quit-menu.ini");
            if (File.Exists(ini)) {
                cfg.SkipStartupSplashes = GetPrivateProfileIntW("Menu", "SkipStartupSplashes", 1, ini) != 0;
                cfg.SuppressOnlineNotices = GetPrivateProfileIntW("Menu", "SuppressOnlineNotices", 1, ini) != 0;
                cfg.StartupSkipPopups = GetPrivateProfileIntW("Menu", "StartupSkipPopups", 0, ini) != 0;
                cfg.MuteWhenUnfocused = GetPrivateProfileIntW("Menu", "MuteWhenUnfocused", 0, ini) != 0;
                cfg.InstantReplayInFight = GetPrivateProfileIntW("Menu", "InstantReplayInFight", 0, ini) != 0;
                cfg.OfferDecisionReplays = GetPrivateProfileIntW("Menu", "OfferDecisionReplays", 0, ini) != 0;
                cfg.IncludeOverlayInScreenshots = GetPrivateProfileIntW("Menu", "IncludeOverlayInScreenshots", 1, ini) != 0;
                cfg.AutoLaunchAfterRestart = GetPrivateProfileIntW("Menu", "AutoLaunchAfterRestart", 1, ini) != 0;
                cfg.OpenFavoritesFirst = GetPrivateProfileIntW("Menu", "OpenFavoritesFirst", 0, ini) != 0;
                cfg.LongevityProtection = GetPrivateProfileIntW("Menu", "LongevityProtection", 0, ini) != 0;
                int mult = GetPrivateProfileIntW("Menu", "TrainingRewardMultiplier", 1, ini);
                cfg.TrainingRewardMultiplier = (mult == 2 || mult == 5 || mult == 10) ? mult : 1;

                string icons = ReadStr(ini, "Menu", "ButtonIcons", "Xbox");
                cfg.ButtonIcons = Array.Find(ButtonIconNames, n => string.Equals(n, icons, StringComparison.OrdinalIgnoreCase)) ?? "Xbox";
                uint chord = ParseHexOrDec(ReadStr(ini, "Menu", "OpeningShortcut", "0x00000c08"), OpenChordDefault);
                cfg.OpeningShortcut = IsPadChordValid(chord) ? chord : OpenChordDefault;
                cfg.GuideButtonOpensMenu = GetPrivateProfileIntW("Menu", "GuideButtonOpensMenu", 1, ini) != 0;
                string access = ReadStr(ini, "Menu", "ControllerAccess", "Either");
                cfg.FirstPlayerOnly = string.Equals(access, "Player1", StringComparison.OrdinalIgnoreCase);

                for (int i = 0; i < BindingIds.Length; i++) {
                    string def = i == 9 ? "0x78" : i == 10 ? "0x0146" : "0";
                    uint k = ParseHexOrDec(ReadStr(ini, "Keybinds", BindingIds[i] + "Key", def), 0);
                    cfg.Keybinds[i] = IsKeyValid(k) ? k : 0;
                }

                cfg.MasterPreset = Clamp(GetPrivateProfileIntW("Menu", "MasterPreset", 1, ini), 0, MasterPresetNames.Length - 1);
                cfg.LayoutStyle = Clamp(GetPrivateProfileIntW("Menu", "LayoutStyle", 1, ini), 0, LayoutStyleNames.Length - 1);
                cfg.BackdropStyle = Clamp(GetPrivateProfileIntW("Menu", "BackdropStyle", 0, ini), 0, 1);
                cfg.ThemeChoice = Clamp(GetPrivateProfileIntW("Menu", "ThemeChoice", 6, ini), 0, ThemeChoiceNames.Length - 1);
                int scale = GetPrivateProfileIntW("Menu", "UIScale", 100, ini);
                cfg.UIScale = (scale >= 50 && scale <= 300 && scale % 25 == 0) ? scale : 100;
                int font = GetPrivateProfileIntW("Menu", "FontSize", 18, ini);
                cfg.FontSize = (font == 16 || font == 18 || font == 20) ? font : 18;
                cfg.BoxStyle = Clamp(GetPrivateProfileIntW("Menu", "BoxStyle", 4, ini), 0, 4);
                cfg.WindowSize = Clamp(GetPrivateProfileIntW("Menu", "WindowSize", 2, ini), 0, 2);
            }

            string fpv = Path.Combine(folder, "ufc6_fpv.ini");
            if (File.Exists(fpv)) {
                try {
                    string text = File.ReadAllText(fpv, Encoding.ASCII);
                    foreach (string part in text.Split(new[] { ' ', '\t', '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries)) {
                        int eq = part.IndexOf('=');
                        if (eq <= 0) continue;
                        string k = part.Substring(0, eq), v = part.Substring(eq + 1);
                        float fv; int iv;
                        if (k == "pitch" && float.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out fv)) cfg.FpvPitchDeg = Math.Max(-85f, Math.Min(85f, fv));
                        else if (k == "height" && float.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out fv)) cfg.FpvHeightM = Math.Max(-1.5f, Math.Min(1.5f, fv));
                        else if (k == "fwd" && float.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out fv)) cfg.FpvFwdM = Math.Max(-0.5f, Math.Min(0.8f, fv));
                        else if (k == "fov" && int.TryParse(v, NumberStyles.Integer, CultureInfo.InvariantCulture, out iv)) cfg.FpvFovDeg = Clamp(iv, 60, 110);
                        else if (k == "grapple" && float.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out fv)) cfg.FpvGrappleDist = Math.Max(0f, Math.Min(1.2f, fv));
                    }
                } catch { }
            }
            return cfg;
        }

        public void SaveToFolder(string folder) {
            if (!IsPadChordValid(OpeningShortcut)) throw new InvalidDataException("Controller opening shortcut must use 2 to 4 buttons.");
            for (int i = 0; i < Keybinds.Length; i++) {
                if (!IsKeyValid(Keybinds[i])) throw new InvalidDataException("Invalid keybind for " + BindingNames[i]);
                if (Keybinds[i] == 0) continue;
                for (int j = i + 1; j < Keybinds.Length; j++)
                    if (Keybinds[j] == Keybinds[i]) throw new InvalidDataException("Duplicate keybind between " + BindingNames[i] + " and " + BindingNames[j]);
            }

            string ini = Path.Combine(folder, "ufc6-quit-menu.ini");
            Write(ini, "Menu", "SkipStartupSplashes", SkipStartupSplashes ? "1" : "0");
            Write(ini, "Menu", "SuppressOnlineNotices", SuppressOnlineNotices ? "1" : "0");
            Write(ini, "Menu", "StartupSkipPopups", StartupSkipPopups ? "1" : "0");
            Write(ini, "Menu", "MuteWhenUnfocused", MuteWhenUnfocused ? "1" : "0");
            Write(ini, "Menu", "InstantReplayInFight", InstantReplayInFight ? "1" : "0");
            Write(ini, "Menu", "OfferDecisionReplays", OfferDecisionReplays ? "1" : "0");
            Write(ini, "Menu", "IncludeOverlayInScreenshots", IncludeOverlayInScreenshots ? "1" : "0");
            Write(ini, "Menu", "AutoLaunchAfterRestart", AutoLaunchAfterRestart ? "1" : "0");
            Write(ini, "Menu", "OpenFavoritesFirst", OpenFavoritesFirst ? "1" : "0");
            Write(ini, "Menu", "LongevityProtection", LongevityProtection ? "1" : "0");
            Write(ini, "Menu", "TrainingRewardMultiplier", TrainingRewardMultiplier.ToString(CultureInfo.InvariantCulture));
            Write(ini, "Menu", "ButtonIcons", ButtonIcons);
            Write(ini, "Menu", "OpeningShortcut", string.Format(CultureInfo.InvariantCulture, "0x{0:x8}", OpeningShortcut));
            Write(ini, "Menu", "GuideButtonOpensMenu", GuideButtonOpensMenu ? "1" : "0");
            Write(ini, "Menu", "ControllerAccess", FirstPlayerOnly ? "Player1" : "Either");
            for (int i = 0; i < BindingIds.Length; i++)
                Write(ini, "Keybinds", BindingIds[i] + "Key", string.Format(CultureInfo.InvariantCulture, "0x{0:x8}", Keybinds[i]));
            Write(ini, "Menu", "MasterPreset", MasterPreset.ToString(CultureInfo.InvariantCulture));
            Write(ini, "Menu", "LayoutStyle", LayoutStyle.ToString(CultureInfo.InvariantCulture));
            Write(ini, "Menu", "BackdropStyle", BackdropStyle.ToString(CultureInfo.InvariantCulture));
            Write(ini, "Menu", "ThemeChoice", ThemeChoice.ToString(CultureInfo.InvariantCulture));
            Write(ini, "Menu", "UIScale", UIScale.ToString(CultureInfo.InvariantCulture));
            Write(ini, "Menu", "FontSize", FontSize.ToString(CultureInfo.InvariantCulture));
            Write(ini, "Menu", "BoxStyle", BoxStyle.ToString(CultureInfo.InvariantCulture));
            Write(ini, "Menu", "WindowSize", WindowSize.ToString(CultureInfo.InvariantCulture));

            string fpv = Path.Combine(folder, "ufc6_fpv.ini");
            string fpvLine = string.Format(CultureInfo.InvariantCulture, "pitch={0:0.00} height={1:0.000} fwd={2:0.000} fov={3} grapple={4:0.00}\n",
                FpvPitchDeg, FpvHeightM, FpvFwdM, FpvFovDeg, FpvGrappleDist);
            File.WriteAllText(fpv, fpvLine, Encoding.ASCII);
        }

        static void Write(string ini, string section, string key, string val) {
            if (!WritePrivateProfileStringW(section, key, val, ini))
                throw new IOException("Failed writing " + key + " to " + ini);
        }
        static int Clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
    }

    public static class ConfiguratorTests {
        public static void Run(string reportPath) {
            string root = Path.Combine(Path.GetTempPath(), "ufc6-config-test-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            try {
                string bin = Path.Combine(root, "bin"), novo = Path.Combine(root, "novo");
                Directory.CreateDirectory(bin); Directory.CreateDirectory(novo);
                File.WriteAllBytes(Path.Combine(bin, "UFC 6.exe"), new byte[] { 1 });
                File.WriteAllBytes(Path.Combine(novo, "UFC 6 novo.exe"), new byte[] { 1 });

                var cfg = new OverlayConfig {
                    SkipStartupSplashes = false,
                    MuteWhenUnfocused = true,
                    InstantReplayInFight = true,
                    TrainingRewardMultiplier = 5,
                    ButtonIcons = "PlayStation",
                    OpeningShortcut = OverlayConfig.PadLB | OverlayConfig.PadRB | OverlayConfig.PadBack,
                    FirstPlayerOnly = true,
                    ThemeChoice = 2,
                    UIScale = 125,
                    FpvFovDeg = 105,
                    FpvPitchDeg = -4.5f,
                    FpvGrappleDist = 0.80f
                };
                cfg.Keybinds[0] = 0x76; // F7
                foreach (string target in OverlayConfig.TargetFolders(root)) cfg.SaveToFolder(target);

                var loadedBin = OverlayConfig.Load(bin);
                var loadedNovo = OverlayConfig.Load(novo);
                if (loadedBin.MuteWhenUnfocused != true || loadedBin.InstantReplayInFight != true || loadedBin.TrainingRewardMultiplier != 5)
                    throw new InvalidOperationException("Switch round-trip failed.");
                if (loadedNovo.ButtonIcons != "PlayStation" || loadedNovo.OpeningShortcut != cfg.OpeningShortcut || loadedNovo.Keybinds[0] != 0x76)
                    throw new InvalidOperationException("Sibling novo sync failed.");
                if (loadedBin.FpvFovDeg != 105 || Math.Abs(loadedBin.FpvPitchDeg - (-4.5f)) > 0.05f)
                    throw new InvalidOperationException("FPV INI round-trip failed.");
                File.WriteAllText(reportPath, "PASS: configurator INI/FPV round-trip and dual-engine sync.");
            } finally {
                try { Directory.Delete(root, true); } catch { }
            }
        }
    }
}
