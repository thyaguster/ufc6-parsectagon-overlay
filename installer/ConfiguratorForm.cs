// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Windows.Forms;

namespace UFC6OverlaySetup {
    public sealed class ConfiguratorForm : ModernForm {
        readonly List<string> targets;
        OverlayConfig config;

        readonly TabControl tabs = new TabControl();
        readonly Label folderLabel = ModernUi.Label("", 32, 130, 696, 22, 9, false, true);
        readonly Label feedback = ModernUi.Label("", 32, 578, 430, 36, 9, false, true);
        readonly ModernButton saveBtn = new ModernButton { Text = "Save & Apply", Tone = ButtonTone.Primary };
        readonly ModernButton resetBtn = new ModernButton { Text = "Reset Defaults" };
        readonly ModernButton closeBtn = new ModernButton { Text = "Close" };

        // Switches
        readonly CheckBox chkSkipSplashes = MkCheck("Skip startup intros & splashes");
        readonly CheckBox chkSuppressNotices = MkCheck("Suppress offline post-fight notices");
        readonly CheckBox chkSkipPopups = MkCheck("Skip title & network warnings (Experimental)");
        readonly CheckBox chkMuteUnfocused = MkCheck("Mute game audio when unfocused / minimized");
        readonly CheckBox chkInstantReplay = MkCheck("Instant Replay available in offline fights");
        readonly CheckBox chkDecisionReplays = MkCheck("Offer replay after decisions");
        readonly CheckBox chkScreenshotUi = MkCheck("Include overlay inside screenshots");
        readonly CheckBox chkAutoLaunch = MkCheck("Auto-launch UFC 6 after PC restart");
        readonly CheckBox chkFavFirst = MkCheck("Open overlay on Favorites tab first");
        readonly CheckBox chkLongevity = MkCheck("Career: Longevity protection");
        readonly ComboBox cmbTraining = MkCombo(new object[] { "1x (Off)", "2x", "5x", "10x" });

        // Controls & Shortcuts
        readonly ComboBox cmbIcons = MkCombo(OverlayConfig.ButtonIconNames);
        readonly CheckBox chkGuide = MkCheck("Guide / Home button opens overlay");
        readonly CheckBox chkP1Only = MkCheck("Restrict controller access to Player 1 only");
        readonly Dictionary<uint, CheckBox> chordBoxes = new Dictionary<uint, CheckBox>();
        readonly Label[] keyLabels = new Label[OverlayConfig.BindingIds.Length];
        int capturingIndex = -1;

        // Appearance
        readonly ComboBox cmbPreset = MkCombo(OverlayConfig.MasterPresetNames);
        readonly ComboBox cmbLayout = MkCombo(OverlayConfig.LayoutStyleNames);
        readonly ComboBox cmbBackdrop = MkCombo(OverlayConfig.BackdropStyleNames);
        readonly ComboBox cmbTheme = MkCombo(OverlayConfig.ThemeChoiceNames);
        readonly ComboBox cmbScale = MkCombo(new object[] { "50%", "75%", "100%", "125%", "150%", "175%", "200%", "225%", "250%", "275%", "300%" });
        readonly ComboBox cmbFont = MkCombo(new object[] { "16 px", "18 px", "20 px" });
        readonly ComboBox cmbBox = MkCombo(OverlayConfig.BoxStyleNames);
        readonly ComboBox cmbWindow = MkCombo(OverlayConfig.WindowSizeNames);

        // FPV
        readonly NumericUpDown numFov = new NumericUpDown { Minimum = 60, Maximum = 110, Increment = 5, BackColor = ModernUi.Card, ForeColor = ModernUi.Text };
        readonly NumericUpDown numPitch = new NumericUpDown { Minimum = -85, Maximum = 85, DecimalPlaces = 1, Increment = 2.5M, BackColor = ModernUi.Card, ForeColor = ModernUi.Text };
        readonly NumericUpDown numHeight = new NumericUpDown { Minimum = -1.50M, Maximum = 1.50M, DecimalPlaces = 2, Increment = 0.05M, BackColor = ModernUi.Card, ForeColor = ModernUi.Text };
        readonly NumericUpDown numFwd = new NumericUpDown { Minimum = -0.50M, Maximum = 0.80M, DecimalPlaces = 2, Increment = 0.02M, BackColor = ModernUi.Card, ForeColor = ModernUi.Text };
        readonly NumericUpDown numGrapple = new NumericUpDown { Minimum = 0.00M, Maximum = 1.20M, DecimalPlaces = 2, Increment = 0.05M, BackColor = ModernUi.Card, ForeColor = ModernUi.Text };

        public ConfiguratorForm(string folder) {
            targets = OverlayConfig.TargetFolders(folder);
            config = OverlayConfig.Load(targets[0]);

            Text = "UFC 6 Parsectagon Overlay — Configurator";
            ClientSize = new Size(760, 640);
            KeyPreview = true;

            var title = ModernUi.Label("Overlay Configurator", 112, 66, 610, 34, 18, true);
            var sub = ModernUi.Label("Configure switches, shortcuts, appearance, and FPV defaults out of the game", 114, 102, 610, 22, 9, false, true);
            folderLabel.Text = targets.Count > 1
                ? "Target folders (synced): " + string.Join("  +  ", targets)
                : "Target folder: " + targets[0];

            tabs.SetBounds(32, 158, 696, 404);
            tabs.Font = new Font("Segoe UI", 9.5f);
            tabs.TabPages.Add(BuildSwitchesTab());
            tabs.TabPages.Add(BuildControlsTab());
            tabs.TabPages.Add(BuildAppearanceTab());
            tabs.TabPages.Add(BuildFpvTab());

            saveBtn.SetBounds(472, 574, 134, 42);
            resetBtn.SetBounds(328, 574, 134, 42);
            closeBtn.SetBounds(616, 574, 112, 42);

            saveBtn.Click += delegate { Save(); };
            resetBtn.Click += delegate { config = new OverlayConfig(); Populate(); feedback.Text = "Defaults loaded. Click Save & Apply to write."; };
            closeBtn.Click += delegate { Close(); };

            Controls.AddRange(new Control[] { ModernUi.Logo(32, 62, 62), title, sub, folderLabel, tabs, feedback, resetBtn, saveBtn, closeBtn });
            Populate();
        }

        static CheckBox MkCheck(string text) {
            return new CheckBox { Text = text, AutoSize = true, ForeColor = ModernUi.Text, BackColor = ModernUi.Card, Font = new Font("Segoe UI", 10) };
        }
        static ComboBox MkCombo(object[] items) {
            var c = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, BackColor = ModernUi.Card, ForeColor = ModernUi.Text, Font = new Font("Segoe UI", 9.5f), Width = 190 };
            c.Items.AddRange(items);
            return c;
        }
        static TabPage MkTab(string title) {
            return new TabPage(title) { BackColor = ModernUi.Card, ForeColor = ModernUi.Text, Padding = new Padding(16), AutoScroll = true };
        }

        TabPage BuildSwitchesTab() {
            var page = MkTab("Switches & Gameplay");
            var checks = new[] { chkSkipSplashes, chkSuppressNotices, chkSkipPopups, chkMuteUnfocused, chkInstantReplay, chkDecisionReplays, chkScreenshotUi, chkAutoLaunch, chkFavFirst, chkLongevity };
            int y = 14;
            foreach (var c in checks) {
                c.Location = new Point(20, y);
                page.Controls.Add(c);
                y += 30;
            }
            var lbl = ModernUi.Label("Career training reward multiplier:", 20, y + 6, 260, 24, 10);
            cmbTraining.Location = new Point(290, y + 4);
            page.Controls.AddRange(new Control[] { lbl, cmbTraining });
            return page;
        }

        TabPage BuildControlsTab() {
            var page = MkTab("Controls & Shortcuts");
            int y = 14;
            page.Controls.Add(ModernUi.Label("Overlay button icon style:", 20, y + 2, 200, 24, 10));
            cmbIcons.Location = new Point(225, y);
            page.Controls.Add(cmbIcons);
            y += 34;
            chkGuide.Location = new Point(20, y); page.Controls.Add(chkGuide); y += 28;
            chkP1Only.Location = new Point(20, y); page.Controls.Add(chkP1Only); y += 34;

            page.Controls.Add(ModernUi.Label("Controller opening chord (pick 2–4 buttons):", 20, y, 360, 22, 10, true));
            y += 26;
            var buttons = new[] {
                new KeyValuePair<uint, string>(OverlayConfig.PadLB, "LB"),
                new KeyValuePair<uint, string>(OverlayConfig.PadRB, "RB"),
                new KeyValuePair<uint, string>(OverlayConfig.PadMenu, "Menu / Start"),
                new KeyValuePair<uint, string>(OverlayConfig.PadBack, "Back / View"),
                new KeyValuePair<uint, string>(OverlayConfig.PadLS, "LS"),
                new KeyValuePair<uint, string>(OverlayConfig.PadRS, "RS"),
                new KeyValuePair<uint, string>(OverlayConfig.PadA, "A"),
                new KeyValuePair<uint, string>(OverlayConfig.PadB, "B"),
                new KeyValuePair<uint, string>(OverlayConfig.PadX, "X"),
                new KeyValuePair<uint, string>(OverlayConfig.PadY, "Y")
            };
            int x = 20;
            for (int i = 0; i < buttons.Length; i++) {
                var box = MkCheck(buttons[i].Value);
                box.Location = new Point(x, y);
                chordBoxes[buttons[i].Key] = box;
                page.Controls.Add(box);
                x += 120;
                if ((i + 1) % 5 == 0) { x = 20; y += 28; }
            }
            y += 14;
            page.Controls.Add(ModernUi.Label("Keyboard shortcuts (click Bind, then press key/combo):", 20, y, 480, 22, 10, true));
            y += 28;
            for (int i = 0; i < OverlayConfig.BindingNames.Length; i++) {
                int idx = i;
                page.Controls.Add(ModernUi.Label(OverlayConfig.BindingNames[i], 20, y + 3, 180, 22, 9.5f));
                var kl = ModernUi.Label("", 210, y + 3, 200, 22, 9.5f, false, true);
                keyLabels[i] = kl;
                var bind = new ModernButton { Text = "Bind", Bounds = new Rectangle(420, y, 76, 26), Font = new Font("Segoe UI", 8.5f, FontStyle.Bold) };
                var clr = new ModernButton { Text = "Clear", Bounds = new Rectangle(504, y, 70, 26), Font = new Font("Segoe UI", 8.5f, FontStyle.Bold) };
                bind.Click += delegate { capturingIndex = idx; kl.Text = "Press key combo (Esc cancels)..."; kl.ForeColor = ModernUi.Accent; };
                clr.Click += delegate { config.Keybinds[idx] = 0; UpdateKeyLabels(); };
                page.Controls.AddRange(new Control[] { kl, bind, clr });
                y += 32;
            }
            return page;
        }

        TabPage BuildAppearanceTab() {
            var page = MkTab("Appearance");
            var rows = new KeyValuePair<string, ComboBox>[] {
                new KeyValuePair<string, ComboBox>("Master preset:", cmbPreset),
                new KeyValuePair<string, ComboBox>("Layout style:", cmbLayout),
                new KeyValuePair<string, ComboBox>("Backdrop:", cmbBackdrop),
                new KeyValuePair<string, ComboBox>("Theme:", cmbTheme),
                new KeyValuePair<string, ComboBox>("UI scale:", cmbScale),
                new KeyValuePair<string, ComboBox>("Font size:", cmbFont),
                new KeyValuePair<string, ComboBox>("Box style:", cmbBox),
                new KeyValuePair<string, ComboBox>("Window size:", cmbWindow)
            };
            int y = 18;
            foreach (var r in rows) {
                page.Controls.Add(ModernUi.Label(r.Key, 20, y + 3, 180, 24, 10));
                r.Value.Location = new Point(215, y);
                page.Controls.Add(r.Value);
                y += 38;
            }
            return page;
        }

        TabPage BuildFpvTab() {
            var page = MkTab("FPV Camera");
            var rows = new KeyValuePair<string, NumericUpDown>[] {
                new KeyValuePair<string, NumericUpDown>("Field of View (60–110°):", numFov),
                new KeyValuePair<string, NumericUpDown>("Pitch Angle (-85 to +85°):", numPitch),
                new KeyValuePair<string, NumericUpDown>("Eye Height Offset (m):", numHeight),
                new KeyValuePair<string, NumericUpDown>("Forward / Back Offset (m):", numFwd),
                new KeyValuePair<string, NumericUpDown>("Grapple Auto-Exit (m, 0=Off):", numGrapple)
            };
            int y = 20;
            foreach (var r in rows) {
                page.Controls.Add(ModernUi.Label(r.Key, 20, y + 3, 250, 24, 10));
                r.Value.SetBounds(280, y, 120, 28);
                page.Controls.Add(r.Value);
                y += 42;
            }
            return page;
        }

        protected override bool ProcessCmdKey(ref Message msg, Keys keyData) {
            if (capturingIndex >= 0) {
                Keys code = keyData & Keys.KeyCode;
                if (code == Keys.Escape) { capturingIndex = -1; UpdateKeyLabels(); return true; }
                if (code == Keys.ShiftKey || code == Keys.ControlKey || code == Keys.Menu || code == Keys.LShiftKey || code == Keys.RShiftKey || code == Keys.LControlKey || code == Keys.RControlKey || code == Keys.LMenu || code == Keys.RMenu)
                    return true;
                uint val = (uint)code & 0xFF;
                if ((keyData & Keys.Control) != 0) val |= OverlayConfig.KeyCtrl;
                if ((keyData & Keys.Alt) != 0) val |= OverlayConfig.KeyAlt;
                if ((keyData & Keys.Shift) != 0) val |= OverlayConfig.KeyShift;
                if (!OverlayConfig.IsKeyValid(val)) { feedback.Text = "Key reserved (Esc/F10/Win). Pick another."; return true; }
                config.Keybinds[capturingIndex] = val;
                capturingIndex = -1;
                UpdateKeyLabels();
                return true;
            }
            return base.ProcessCmdKey(ref msg, keyData);
        }

        void Populate() {
            chkSkipSplashes.Checked = config.SkipStartupSplashes;
            chkSuppressNotices.Checked = config.SuppressOnlineNotices;
            chkSkipPopups.Checked = config.StartupSkipPopups;
            chkMuteUnfocused.Checked = config.MuteWhenUnfocused;
            chkInstantReplay.Checked = config.InstantReplayInFight;
            chkDecisionReplays.Checked = config.OfferDecisionReplays;
            chkScreenshotUi.Checked = config.IncludeOverlayInScreenshots;
            chkAutoLaunch.Checked = config.AutoLaunchAfterRestart;
            chkFavFirst.Checked = config.OpenFavoritesFirst;
            chkLongevity.Checked = config.LongevityProtection;
            cmbTraining.SelectedIndex = config.TrainingRewardMultiplier == 2 ? 1 : config.TrainingRewardMultiplier == 5 ? 2 : config.TrainingRewardMultiplier == 10 ? 3 : 0;

            cmbIcons.SelectedIndex = Math.Max(0, Array.IndexOf(OverlayConfig.ButtonIconNames, config.ButtonIcons));
            chkGuide.Checked = config.GuideButtonOpensMenu;
            chkP1Only.Checked = config.FirstPlayerOnly;
            foreach (var kv in chordBoxes) kv.Value.Checked = (config.OpeningShortcut & kv.Key) != 0;
            UpdateKeyLabels();

            cmbPreset.SelectedIndex = config.MasterPreset;
            cmbLayout.SelectedIndex = config.LayoutStyle;
            cmbBackdrop.SelectedIndex = config.BackdropStyle;
            cmbTheme.SelectedIndex = config.ThemeChoice;
            cmbScale.SelectedIndex = Math.Max(0, (config.UIScale - 50) / 25);
            cmbFont.SelectedIndex = config.FontSize == 16 ? 0 : config.FontSize == 20 ? 2 : 1;
            cmbBox.SelectedIndex = config.BoxStyle;
            cmbWindow.SelectedIndex = config.WindowSize;

            numFov.Value = config.FpvFovDeg;
            numPitch.Value = (decimal)config.FpvPitchDeg;
            numHeight.Value = (decimal)config.FpvHeightM;
            numFwd.Value = (decimal)config.FpvFwdM;
            numGrapple.Value = (decimal)config.FpvGrappleDist;
        }

        void UpdateKeyLabels() {
            for (int i = 0; i < keyLabels.Length; i++) {
                uint k = config.Keybinds[i];
                if (k == 0) { keyLabels[i].Text = "Not assigned"; keyLabels[i].ForeColor = ModernUi.Muted; continue; }
                var parts = new List<string>();
                if ((k & OverlayConfig.KeyCtrl) != 0) parts.Add("Ctrl");
                if ((k & OverlayConfig.KeyAlt) != 0) parts.Add("Alt");
                if ((k & OverlayConfig.KeyShift) != 0) parts.Add("Shift");
                parts.Add(((Keys)(k & 0xFF)).ToString());
                keyLabels[i].Text = string.Join(" + ", parts);
                keyLabels[i].ForeColor = ModernUi.Text;
            }
        }

        void Save() {
            try {
                config.SkipStartupSplashes = chkSkipSplashes.Checked;
                config.SuppressOnlineNotices = chkSuppressNotices.Checked;
                config.StartupSkipPopups = chkSkipPopups.Checked;
                config.MuteWhenUnfocused = chkMuteUnfocused.Checked;
                config.InstantReplayInFight = chkInstantReplay.Checked;
                config.OfferDecisionReplays = chkDecisionReplays.Checked;
                config.IncludeOverlayInScreenshots = chkScreenshotUi.Checked;
                config.AutoLaunchAfterRestart = chkAutoLaunch.Checked;
                config.OpenFavoritesFirst = chkFavFirst.Checked;
                config.LongevityProtection = chkLongevity.Checked;
                int[] mults = { 1, 2, 5, 10 };
                config.TrainingRewardMultiplier = mults[Math.Max(0, cmbTraining.SelectedIndex)];

                config.ButtonIcons = OverlayConfig.ButtonIconNames[Math.Max(0, cmbIcons.SelectedIndex)];
                config.GuideButtonOpensMenu = chkGuide.Checked;
                config.FirstPlayerOnly = chkP1Only.Checked;
                uint chord = 0;
                foreach (var kv in chordBoxes) if (kv.Value.Checked) chord |= kv.Key;
                config.OpeningShortcut = chord;

                config.MasterPreset = Math.Max(0, cmbPreset.SelectedIndex);
                config.LayoutStyle = Math.Max(0, cmbLayout.SelectedIndex);
                config.BackdropStyle = Math.Max(0, cmbBackdrop.SelectedIndex);
                config.ThemeChoice = Math.Max(0, cmbTheme.SelectedIndex);
                config.UIScale = 50 + Math.Max(0, cmbScale.SelectedIndex) * 25;
                config.FontSize = cmbFont.SelectedIndex == 0 ? 16 : cmbFont.SelectedIndex == 2 ? 20 : 18;
                config.BoxStyle = Math.Max(0, cmbBox.SelectedIndex);
                config.WindowSize = Math.Max(0, cmbWindow.SelectedIndex);

                config.FpvFovDeg = (int)numFov.Value;
                config.FpvPitchDeg = (float)numPitch.Value;
                config.FpvHeightM = (float)numHeight.Value;
                config.FpvFwdM = (float)numFwd.Value;
                config.FpvGrappleDist = (float)numGrapple.Value;

                foreach (string dir in targets) config.SaveToFolder(dir);
                feedback.ForeColor = ModernUi.Accent;
                feedback.Text = "Saved to " + targets.Count + (targets.Count == 1 ? " folder." : " engine folders (bin + novo).");
            } catch (Exception ex) {
                feedback.ForeColor = Color.FromArgb(242, 161, 171);
                feedback.Text = ex.Message;
            }
        }
    }
}
