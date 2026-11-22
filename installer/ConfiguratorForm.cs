// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Reflection;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace UFC6OverlaySetup {
    public sealed class PillToggle : Control {
        bool isOn;
        public bool Checked { get { return isOn; } set { if (isOn != value) { isOn = value; Invalidate(); if (CheckedChanged != null) CheckedChanged(this, EventArgs.Empty); } } }
        public event EventHandler CheckedChanged;
        public PillToggle() {
            Size = new Size(82, 26);
            Cursor = Cursors.Hand;
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true);
        }
        protected override void OnClick(EventArgs e) { Checked = !Checked; base.OnClick(e); }
        protected override void OnPaint(PaintEventArgs e) {
            e.Graphics.Clear(Parent != null ? Parent.BackColor : ModernUi.Card);
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            string label = isOn ? "On" : "Off";
            Color labelColor = isOn ? ModernUi.Accent : ModernUi.Muted;
            TextRenderer.DrawText(e.Graphics, label, new Font("Segoe UI", 9.5f, FontStyle.Bold), new Rectangle(0, 0, 32, Height), labelColor, TextFormatFlags.Right | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix);
            var track = new RectangleF(38, 3, 42, 20);
            Color trackColor = isOn ? ModernUi.Accent : Color.FromArgb(46, 58, 74);
            using (var path = ModernUi.Rounded(track, 10))
            using (var brush = new SolidBrush(trackColor))
                e.Graphics.FillPath(brush, path);
            float thumbX = isOn ? track.Right - 17 : track.X + 3;
            using (var thumb = new SolidBrush(isOn ? Color.FromArgb(14, 24, 20) : Color.FromArgb(225, 232, 240)))
                e.Graphics.FillEllipse(thumb, thumbX, track.Y + 3, 14, 14);
        }
    }

    public sealed class PillSelector : Control {
        readonly string[] options;
        int idx;
        public int SelectedIndex { get { return idx; } set { int c = Math.Max(0, Math.Min(options.Length - 1, value)); if (idx != c) { idx = c; Invalidate(); if (SelectedIndexChanged != null) SelectedIndexChanged(this, EventArgs.Empty); } } }
        public string SelectedText { get { return options[idx]; } }
        public event EventHandler SelectedIndexChanged;
        public PillSelector(string[] items) {
            options = items;
            Size = new Size(216, 28);
            Cursor = Cursors.Hand;
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true);
        }
        protected override void OnMouseDown(MouseEventArgs e) {
            if (e.X < 34) SelectedIndex = (idx + options.Length - 1) % options.Length;
            else SelectedIndex = (idx + 1) % options.Length;
            base.OnMouseDown(e);
        }
        protected override void OnPaint(PaintEventArgs e) {
            e.Graphics.Clear(Parent != null ? Parent.BackColor : ModernUi.Card);
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            var box = new RectangleF(0.5f, 0.5f, Width - 1, Height - 1);
            using (var path = ModernUi.Rounded(box, 12))
            using (var fill = new SolidBrush(Color.FromArgb(18, 26, 37)))
            using (var pen = new Pen(ModernUi.Border)) {
                e.Graphics.FillPath(fill, path);
                e.Graphics.DrawPath(pen, path);
            }
            var f = new Font("Segoe UI", 9.5f, FontStyle.Bold);
            TextRenderer.DrawText(e.Graphics, "‹", f, new Rectangle(4, 0, 24, Height), ModernUi.Muted, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix);
            TextRenderer.DrawText(e.Graphics, options[idx], new Font("Segoe UI", 9.5f), new Rectangle(28, 0, Width - 56, Height), ModernUi.Text, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
            TextRenderer.DrawText(e.Graphics, "›", f, new Rectangle(Width - 28, 0, 24, Height), ModernUi.Muted, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix);
        }
    }

    public sealed class RowCard : Panel {
        public RowCard(int y, int width) {
            Bounds = new Rectangle(12, y, width, 40);
            BackColor = Color.FromArgb(20, 29, 41);
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true);
        }
        protected override void OnPaint(PaintEventArgs e) {
            e.Graphics.Clear(Parent != null ? Parent.BackColor : ModernUi.Card);
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            using (var p = ModernUi.Rounded(new RectangleF(0.5f, 0.5f, Width - 1, Height - 1), 10))
            using (var fill = new SolidBrush(BackColor))
            using (var pen = new Pen(Color.FromArgb(40, 56, 74))) {
                e.Graphics.FillPath(fill, p);
                e.Graphics.DrawPath(pen, p);
            }
        }
    }

    public sealed class ConfiguratorForm : ModernForm {
        readonly List<string> targets;
        readonly string appVersion;
        OverlayConfig config;

        readonly ModernButton[] tabButtons = new ModernButton[5];
        readonly Panel[] pages = new Panel[5];
        readonly ModernCard bodyCard = new ModernCard();
        readonly Label folderLabel = new Label { AutoEllipsis = true, UseMnemonic = false, ForeColor = ModernUi.Muted, BackColor = Color.Transparent, Font = new Font("Segoe UI", 9) };
        readonly Label feedback = new Label { UseMnemonic = false, ForeColor = ModernUi.Muted, BackColor = Color.Transparent, Font = new Font("Segoe UI", 9) };
        readonly ModernButton supportBtn = new ModernButton { Text = "Support report..." };
        readonly ModernButton updateBtn = new ModernButton { Text = "Check updates" };
        readonly ModernButton resetBtn = new ModernButton { Text = "Reset Defaults" };
        readonly ModernButton saveBtn = new ModernButton { Text = "Save & Apply", Tone = ButtonTone.Primary };
        readonly ModernButton closeBtn = new ModernButton { Text = "Close" };

        // Switches
        readonly PillToggle chkSkipSplashes = new PillToggle();
        readonly PillToggle chkSuppressNotices = new PillToggle();
        readonly PillToggle chkSkipPopups = new PillToggle();
        readonly PillToggle chkMuteUnfocused = new PillToggle();
        readonly PillToggle chkInstantReplay = new PillToggle();
        readonly PillToggle chkDecisionReplays = new PillToggle();
        readonly PillToggle chkScreenshotUi = new PillToggle();
        readonly PillToggle chkAutoLaunch = new PillToggle();
        readonly PillToggle chkFavFirst = new PillToggle();
        readonly PillToggle chkLongevity = new PillToggle();
        readonly PillSelector selTraining = new PillSelector(new[] { "1x (Off)", "2x", "5x", "10x" });

        // Controls & Shortcuts
        readonly PillSelector selIcons = new PillSelector(OverlayConfig.ButtonIconNames);
        readonly PillToggle chkGuide = new PillToggle();
        readonly PillToggle chkP1Only = new PillToggle();
        readonly Dictionary<uint, CheckBox> chordBoxes = new Dictionary<uint, CheckBox>();
        readonly Label[] keyLabels = new Label[OverlayConfig.BindingIds.Length];
        int capturingIndex = -1;

        // Appearance
        readonly PillSelector selPreset = new PillSelector(OverlayConfig.MasterPresetNames);
        readonly PillSelector selLayout = new PillSelector(OverlayConfig.LayoutStyleNames);
        readonly PillSelector selBackdrop = new PillSelector(OverlayConfig.BackdropStyleNames);
        readonly PillSelector selTheme = new PillSelector(OverlayConfig.ThemeChoiceNames);
        readonly PillSelector selScale = new PillSelector(new[] { "50%", "75%", "100%", "125%", "150%", "175%", "200%", "225%", "250%", "275%", "300%" });
        readonly PillSelector selFont = new PillSelector(new[] { "16 px", "18 px", "20 px" });
        readonly PillSelector selBox = new PillSelector(OverlayConfig.BoxStyleNames);
        readonly PillSelector selWindow = new PillSelector(OverlayConfig.WindowSizeNames);

        // FPV
        readonly ComboBox cmbFpvPreset = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, BackColor = Color.FromArgb(18, 26, 37), ForeColor = ModernUi.Text, FlatStyle = FlatStyle.Flat, Font = new Font("Segoe UI", 9.5f) };
        readonly ModernButton btnApplyPreset = new ModernButton { Text = "Apply", Tone = ButtonTone.Primary, UseMnemonic = false };
        readonly ModernButton btnSavePreset = new ModernButton { Text = "Save As...", UseMnemonic = false };
        readonly ModernButton btnDelPreset = new ModernButton { Text = "Delete", Tone = ButtonTone.Danger, UseMnemonic = false };
        readonly NumericUpDown numFov = new NumericUpDown { Minimum = 60, Maximum = 110, Increment = 5, BackColor = Color.FromArgb(18, 26, 37), ForeColor = ModernUi.Text, BorderStyle = BorderStyle.FixedSingle };
        readonly NumericUpDown numPitch = new NumericUpDown { Minimum = -85, Maximum = 85, DecimalPlaces = 1, Increment = 2.5M, BackColor = Color.FromArgb(18, 26, 37), ForeColor = ModernUi.Text, BorderStyle = BorderStyle.FixedSingle };
        readonly NumericUpDown numHeight = new NumericUpDown { Minimum = -1.50M, Maximum = 1.50M, DecimalPlaces = 2, Increment = 0.05M, BackColor = Color.FromArgb(18, 26, 37), ForeColor = ModernUi.Text, BorderStyle = BorderStyle.FixedSingle };
        readonly NumericUpDown numFwd = new NumericUpDown { Minimum = -0.50M, Maximum = 0.80M, DecimalPlaces = 2, Increment = 0.02M, BackColor = Color.FromArgb(18, 26, 37), ForeColor = ModernUi.Text, BorderStyle = BorderStyle.FixedSingle };
        readonly NumericUpDown numGrapple = new NumericUpDown { Minimum = 0.00M, Maximum = 1.20M, DecimalPlaces = 2, Increment = 0.05M, BackColor = Color.FromArgb(18, 26, 37), ForeColor = ModernUi.Text, BorderStyle = BorderStyle.FixedSingle };
        readonly PillSelector selFpvWobble = new PillSelector(OverlayConfig.FpvWobbleNames);

        // Parsectagon Account
        readonly Label accountStatusLbl = new Label { UseMnemonic = false, ForeColor = ModernUi.Text, BackColor = Color.Transparent, Font = new Font("Segoe UI", 10.5f, FontStyle.Bold) };
        readonly Label accountDetailLbl = new Label { UseMnemonic = false, ForeColor = ModernUi.Muted, BackColor = Color.Transparent, Font = new Font("Segoe UI", 9.5f) };
        readonly ModernButton linkBtn = new ModernButton { Text = "Log in / Link Parsectagon", Tone = ButtonTone.Primary, UseMnemonic = false };
        readonly ModernButton unlinkBtn = new ModernButton { Text = "Unlink PC", Tone = ButtonTone.Danger, UseMnemonic = false };
        readonly ModernButton refreshAcctBtn = new ModernButton { Text = "Refresh Status", UseMnemonic = false };
        readonly Timer accountPoll = new Timer { Interval = 700 };

        public ConfiguratorForm(string folder) {
            targets = OverlayConfig.TargetFolders(folder);
            config = OverlayConfig.Load(targets[0]);
            appVersion = Assembly.GetExecutingAssembly().GetName().Version.ToString(3);

            Text = "UFC 6 Parsectagon Overlay — Configurator";
            ClientSize = new Size(980, 780);
            KeyPreview = true;

            var title = ModernUi.Label("Overlay Configurator", 112, 56, 820, 40, 18, true);
            title.UseMnemonic = false;
            var sub = ModernUi.Label("Configure switches, shortcuts, appearance, and FPV defaults out of the game", 114, 98, 820, 22, 9.5f, false, true);
            sub.UseMnemonic = false;
            folderLabel.SetBounds(114, 122, 834, 22);
            folderLabel.Text = targets.Count > 1
                ? "Synced: bin + novo (" + Path.GetDirectoryName(targets[0]) + ")"
                : "Folder: " + targets[0];

            string[] tabTitles = { "Switches", "Shortcuts", "Appearance", "FPV Camera", "Parsectagon" };
            int tx = 32;
            for (int i = 0; i < 5; i++) {
                int tabIdx = i;
                var btn = new ModernButton { Text = tabTitles[i], Bounds = new Rectangle(tx, 152, 176, 34), Font = new Font("Segoe UI", 9.5f, FontStyle.Bold) };
                btn.UseMnemonic = false;
                btn.Click += delegate { SelectTab(tabIdx); };
                tabButtons[i] = btn;
                Controls.Add(btn);
                tx += 185;
            }

            bodyCard.SetBounds(32, 196, 916, 492);
            pages[0] = BuildSwitchesTab();
            pages[1] = BuildControlsTab();
            pages[2] = BuildAppearanceTab();
            pages[3] = BuildFpvTab();
            pages[4] = BuildParsectagonTab();
            foreach (var p in pages) {
                p.SetBounds(8, 8, 900, 476);
                p.BackColor = ModernUi.Card;
                p.AutoScroll = true;
                bodyCard.Controls.Add(p);
            }

            feedback.SetBounds(32, 694, 916, 20);
            supportBtn.SetBounds(32, 722, 146, 40); supportBtn.Font = new Font("Segoe UI", 9f, FontStyle.Bold);
            updateBtn.SetBounds(186, 722, 140, 40); updateBtn.Font = new Font("Segoe UI", 9f, FontStyle.Bold);
            resetBtn.SetBounds(540, 722, 134, 40); resetBtn.Font = new Font("Segoe UI", 9f, FontStyle.Bold);
            saveBtn.SetBounds(682, 722, 144, 40);
            closeBtn.SetBounds(834, 722, 114, 40);
            foreach (var b in new[] { supportBtn, updateBtn, resetBtn, saveBtn, closeBtn }) b.UseMnemonic = false;

            supportBtn.Click += delegate {
                try { using (var d = new DiagnosticsForm(appVersion, Diagnostics.Folder(targets[0]))) d.ShowDialog(this); }
                catch (Exception ex) { feedback.ForeColor = Color.FromArgb(242, 161, 171); feedback.Text = ex.Message; }
            };
            updateBtn.Click += delegate {
                try { using (var u = new UpdateForm(appVersion, targets[0])) { u.ShowDialog(this); if (u.InstallerOpened) Close(); } }
                catch (Exception ex) { feedback.ForeColor = Color.FromArgb(242, 161, 171); feedback.Text = ex.Message; }
            };
            saveBtn.Click += delegate { Save(); };
            resetBtn.Click += delegate { config = new OverlayConfig(); Populate(); feedback.ForeColor = ModernUi.Muted; feedback.Text = "Defaults loaded. Click Save & Apply to write."; };
            closeBtn.Click += delegate { Close(); };

            Controls.AddRange(new Control[] { ModernUi.Logo(32, 60, 64), title, sub, folderLabel, bodyCard, feedback, supportBtn, updateBtn, resetBtn, saveBtn, closeBtn });
            accountPoll.Tick += delegate { RefreshAccountUi(); };
            FormClosed += delegate { accountPoll.Stop(); accountPoll.Dispose(); };
            SelectTab(0);
            Populate();
            RefreshAccountUi();
        }

        void SelectTab(int index) {
            for (int i = 0; i < 5; i++) {
                pages[i].Visible = (i == index);
                tabButtons[i].Tone = (i == index) ? ButtonTone.Primary : ButtonTone.Secondary;
                tabButtons[i].Invalidate();
            }
            if (index == 4) { RefreshAccountUi(); accountPoll.Start(); }
            else accountPoll.Stop();
        }

        static RowCard AddRowAt(Panel page, int x, int y, string title, Control rightControl, int width) {
            var row = new RowCard(y, width) { Left = x };
            var lbl = new Label { Text = title, UseMnemonic = false, Bounds = new Rectangle(12, 0, width - rightControl.Width - 26, 40), Font = new Font("Segoe UI", 9.5f), ForeColor = ModernUi.Text, BackColor = Color.Transparent, TextAlign = ContentAlignment.MiddleLeft };
            rightControl.Location = new Point(width - rightControl.Width - 10, (40 - rightControl.Height) / 2);
            row.Controls.Add(lbl);
            row.Controls.Add(rightControl);
            page.Controls.Add(row);
            return row;
        }
        static RowCard AddRow(Panel page, ref int y, string title, Control rightControl, int width = 876) {
            var r = AddRowAt(page, 10, y, title, rightControl, width);
            y += 46;
            return r;
        }

        Panel BuildSwitchesTab() {
            var page = new Panel();
            var left = new KeyValuePair<string, Control>[] {
                new KeyValuePair<string, Control>("Skip startup intros & splashes", chkSkipSplashes),
                new KeyValuePair<string, Control>("Suppress offline post-fight notices", chkSuppressNotices),
                new KeyValuePair<string, Control>("Skip title & network warnings (Exp.)", chkSkipPopups),
                new KeyValuePair<string, Control>("Mute audio when unfocused / minimized", chkMuteUnfocused),
                new KeyValuePair<string, Control>("Instant Replay in offline fights", chkInstantReplay),
                new KeyValuePair<string, Control>("Offer replay after decisions", chkDecisionReplays)
            };
            var right = new KeyValuePair<string, Control>[] {
                new KeyValuePair<string, Control>("Include overlay inside screenshots", chkScreenshotUi),
                new KeyValuePair<string, Control>("Auto-launch UFC 6 after PC restart", chkAutoLaunch),
                new KeyValuePair<string, Control>("Open overlay on Favorites tab first", chkFavFirst),
                new KeyValuePair<string, Control>("Career: Longevity protection", chkLongevity),
                new KeyValuePair<string, Control>("Training reward multiplier", selTraining)
            };
            for (int i = 0; i < left.Length; i++) AddRowAt(page, 10, 8 + i * 48, left[i].Key, left[i].Value, 432);
            for (int i = 0; i < right.Length; i++) AddRowAt(page, 454, 8 + i * 48, right[i].Key, right[i].Value, 432);
            return page;
        }

        Panel BuildControlsTab() {
            var page = new Panel();
            AddRowAt(page, 10, 8, "Overlay button icon style", selIcons, 432);
            AddRowAt(page, 10, 56, "Guide / Home button opens overlay", chkGuide, 432);
            AddRowAt(page, 10, 104, "Restrict controller access to P1 only", chkP1Only, 432);

            var chordCard = new RowCard(8, 432) { Left = 454, Height = 136 };
            var chordTitle = new Label { Text = "Controller opening chord (pick 2–4 buttons):", UseMnemonic = false, Bounds = new Rectangle(14, 8, 400, 22), Font = new Font("Segoe UI", 9.5f, FontStyle.Bold), ForeColor = ModernUi.Text, BackColor = Color.Transparent };
            chordCard.Controls.Add(chordTitle);
            var buttons = new[] {
                new KeyValuePair<uint, string>(OverlayConfig.PadLB, "LB"),
                new KeyValuePair<uint, string>(OverlayConfig.PadRB, "RB"),
                new KeyValuePair<uint, string>(OverlayConfig.PadMenu, "Menu"),
                new KeyValuePair<uint, string>(OverlayConfig.PadBack, "Back"),
                new KeyValuePair<uint, string>(OverlayConfig.PadLS, "LS"),
                new KeyValuePair<uint, string>(OverlayConfig.PadRS, "RS"),
                new KeyValuePair<uint, string>(OverlayConfig.PadA, "A"),
                new KeyValuePair<uint, string>(OverlayConfig.PadB, "B"),
                new KeyValuePair<uint, string>(OverlayConfig.PadX, "X"),
                new KeyValuePair<uint, string>(OverlayConfig.PadY, "Y")
            };
            int cx = 14, cy = 36;
            for (int i = 0; i < buttons.Length; i++) {
                var box = new CheckBox { Text = buttons[i].Value, UseMnemonic = false, AutoSize = true, ForeColor = ModernUi.Text, BackColor = Color.Transparent, Font = new Font("Segoe UI", 9.5f), Location = new Point(cx, cy) };
                chordBoxes[buttons[i].Key] = box;
                chordCard.Controls.Add(box);
                cx += 82;
                if ((i + 1) % 5 == 0) { cx = 14; cy += 34; }
            }
            page.Controls.Add(chordCard);

            int y = 156;
            var kbHdr = new Label { Text = "Keyboard shortcuts (click Bind, then press key/combo):", UseMnemonic = false, Bounds = new Rectangle(12, y, 540, 22), Font = new Font("Segoe UI", 9.5f, FontStyle.Bold), ForeColor = ModernUi.Text, BackColor = Color.Transparent };
            page.Controls.Add(kbHdr);
            y += 26;

            for (int i = 0; i < OverlayConfig.BindingNames.Length; i++) {
                int idx = i;
                int col = i % 2, rowIdx = i / 2;
                int rx = col == 0 ? 10 : 454, ry = y + rowIdx * 46;
                var row = new RowCard(ry, 432) { Left = rx };
                var nameLbl = new Label { Text = OverlayConfig.BindingNames[i], UseMnemonic = false, Bounds = new Rectangle(12, 0, 140, 40), Font = new Font("Segoe UI", 9.2f), ForeColor = ModernUi.Text, BackColor = Color.Transparent, TextAlign = ContentAlignment.MiddleLeft };
                var kl = new Label { UseMnemonic = false, Bounds = new Rectangle(154, 0, 134, 40), Font = new Font("Segoe UI", 9f), ForeColor = ModernUi.Muted, BackColor = Color.Transparent, TextAlign = ContentAlignment.MiddleLeft };
                keyLabels[i] = kl;
                var bind = new ModernButton { Text = "Bind", Bounds = new Rectangle(292, 7, 64, 26), Font = new Font("Segoe UI", 8.5f, FontStyle.Bold), UseMnemonic = false };
                var clr = new ModernButton { Text = "Clear", Bounds = new Rectangle(360, 7, 62, 26), Font = new Font("Segoe UI", 8.5f, FontStyle.Bold), UseMnemonic = false };
                bind.Click += delegate { capturingIndex = idx; kl.Text = "Press key..."; kl.ForeColor = ModernUi.Accent; };
                clr.Click += delegate { config.Keybinds[idx] = 0; UpdateKeyLabels(); };
                row.Controls.AddRange(new Control[] { nameLbl, kl, bind, clr });
                page.Controls.Add(row);
            }
            return page;
        }

        Panel BuildAppearanceTab() {
            var page = new Panel();
            var left = new KeyValuePair<string, Control>[] {
                new KeyValuePair<string, Control>("Master preset", selPreset),
                new KeyValuePair<string, Control>("Layout style", selLayout),
                new KeyValuePair<string, Control>("Backdrop style", selBackdrop),
                new KeyValuePair<string, Control>("Color theme", selTheme)
            };
            var right = new KeyValuePair<string, Control>[] {
                new KeyValuePair<string, Control>("UI scale", selScale),
                new KeyValuePair<string, Control>("Font size", selFont),
                new KeyValuePair<string, Control>("Row box style", selBox),
                new KeyValuePair<string, Control>("Window size", selWindow)
            };
            for (int i = 0; i < left.Length; i++) AddRowAt(page, 10, 8 + i * 48, left[i].Key, left[i].Value, 432);
            for (int i = 0; i < right.Length; i++) AddRowAt(page, 454, 8 + i * 48, right[i].Key, right[i].Value, 432);
            return page;
        }

        Panel BuildFpvTab() {
            var page = new Panel();
            int y = 6;
            var presetCard = new RowCard(12, 656) { Location = new Point(12, y), Height = 58 };
            var presetLbl = ModernUi.Label("Camera Preset", 16, 17, 130, 24, 9.8f, false, false);
            cmbFpvPreset.SetBounds(150, 14, 242, 28);
            btnApplyPreset.SetBounds(402, 13, 72, 30);
            btnSavePreset.SetBounds(480, 13, 88, 30);
            btnDelPreset.SetBounds(574, 13, 68, 30);
            cmbFpvPreset.SelectedIndexChanged += delegate {
                var p = cmbFpvPreset.SelectedItem as FpvPreset;
                btnDelPreset.Enabled = (p != null && !p.BuiltIn);
            };
            btnApplyPreset.Click += delegate { ApplySelectedFpvPreset(); };
            btnSavePreset.Click += delegate { SaveCurrentAsFpvPreset(); };
            btnDelPreset.Click += delegate { DeleteSelectedFpvPreset(); };
            presetCard.Controls.AddRange(new Control[] { presetLbl, cmbFpvPreset, btnApplyPreset, btnSavePreset, btnDelPreset });
            page.Controls.Add(presetCard);
            y += 66;
            AddRow(page, ref y, "Field of View (60–110°)", numFov);
            AddRow(page, ref y, "Pitch Angle (-85 to +85°)", numPitch);
            AddRow(page, ref y, "Eye Height Offset (m)", numHeight);
            AddRow(page, ref y, "Forward / Back Offset (m)", numFwd);
            AddRow(page, ref y, "Grapple Auto-Exit (m, 0 = Off)", numGrapple);
            AddRow(page, ref y, "Head Wobble / Feel", selFpvWobble);
            return page;
        }

        void RefreshFpvPresets(string selectName) {
            cmbFpvPreset.Items.Clear();
            foreach (var p in OverlayConfig.BuiltInFpvPresets()) cmbFpvPreset.Items.Add(p);
            foreach (var p in config.FpvPresets) cmbFpvPreset.Items.Add(p);
            int idx = 0;
            if (!string.IsNullOrEmpty(selectName)) {
                for (int i = 0; i < cmbFpvPreset.Items.Count; i++) {
                    var p = (FpvPreset)cmbFpvPreset.Items[i];
                    if (string.Equals(p.Name, selectName, StringComparison.OrdinalIgnoreCase)) { idx = i; break; }
                }
            }
            if (cmbFpvPreset.Items.Count > 0) cmbFpvPreset.SelectedIndex = idx;
            var sel = cmbFpvPreset.SelectedItem as FpvPreset;
            btnDelPreset.Enabled = (sel != null && !sel.BuiltIn);
        }

        void ApplySelectedFpvPreset() {
            var p = cmbFpvPreset.SelectedItem as FpvPreset;
            if (p == null) return;
            numFov.Value = Math.Max(numFov.Minimum, Math.Min(numFov.Maximum, p.FovDeg));
            numPitch.Value = Math.Max(numPitch.Minimum, Math.Min(numPitch.Maximum, (decimal)p.PitchDeg));
            numHeight.Value = Math.Max(numHeight.Minimum, Math.Min(numHeight.Maximum, (decimal)p.HeightM));
            numFwd.Value = Math.Max(numFwd.Minimum, Math.Min(numFwd.Maximum, (decimal)p.FwdM));
            numGrapple.Value = Math.Max(numGrapple.Minimum, Math.Min(numGrapple.Maximum, (decimal)p.GrappleDist));
            selFpvWobble.SelectedIndex = Math.Max(0, Math.Min(OverlayConfig.FpvWobbleNames.Length - 1, p.Wobble));
            feedback.ForeColor = ModernUi.Accent;
            feedback.Text = "Applied preset \"" + p.Name + "\". Click Save to write to game folders.";
        }

        void SaveCurrentAsFpvPreset() {
            using (var dlg = new Form {
                Text = "Save FPV Preset",
                ClientSize = new Size(360, 128),
                FormBorderStyle = FormBorderStyle.FixedDialog,
                StartPosition = FormStartPosition.CenterParent,
                MaximizeBox = false, MinimizeBox = false,
                BackColor = ModernUi.Background, ForeColor = ModernUi.Text
            }) {
                var lbl = ModernUi.Label("Preset name:", 16, 14, 320, 22, 9.5f, false, false);
                var box = new TextBox {
                    Location = new Point(16, 40), Size = new Size(328, 26),
                    BackColor = Color.FromArgb(18, 26, 37), ForeColor = ModernUi.Text,
                    Font = new Font("Segoe UI", 10f), MaxLength = 40
                };
                var cur = cmbFpvPreset.SelectedItem as FpvPreset;
                box.Text = (cur != null && !cur.BuiltIn) ? cur.Name : ("Custom " + (config.FpvPresets.Count + 1));
                var ok = new ModernButton { Text = "Save Preset", Tone = ButtonTone.Primary, Location = new Point(162, 80), Size = new Size(104, 32), DialogResult = DialogResult.OK };
                var cancel = new ModernButton { Text = "Cancel", Location = new Point(272, 80), Size = new Size(72, 32), DialogResult = DialogResult.Cancel };
                dlg.Controls.AddRange(new Control[] { lbl, box, ok, cancel });
                dlg.AcceptButton = ok; dlg.CancelButton = cancel;
                if (dlg.ShowDialog(this) != DialogResult.OK) return;
                string name = (box.Text ?? "").Replace("|", "-").Trim();
                if (name.Length == 0) return;
                var existing = config.FpvPresets.Find(x => string.Equals(x.Name, name, StringComparison.OrdinalIgnoreCase));
                if (existing == null) { existing = new FpvPreset { Name = name }; config.FpvPresets.Add(existing); }
                existing.FovDeg = (int)numFov.Value;
                existing.PitchDeg = (float)numPitch.Value;
                existing.HeightM = (float)numHeight.Value;
                existing.FwdM = (float)numFwd.Value;
                existing.GrappleDist = (float)numGrapple.Value;
                existing.Wobble = selFpvWobble.SelectedIndex;
                RefreshFpvPresets(name);
                feedback.ForeColor = ModernUi.Accent;
                feedback.Text = "Saved preset \"" + name + "\". Click Save to sync to game folders.";
            }
        }

        void DeleteSelectedFpvPreset() {
            var p = cmbFpvPreset.SelectedItem as FpvPreset;
            if (p == null || p.BuiltIn) return;
            config.FpvPresets.Remove(p);
            RefreshFpvPresets(null);
            feedback.ForeColor = ModernUi.Accent;
            feedback.Text = "Removed preset \"" + p.Name + "\". Click Save to sync.";
        }

        Panel BuildParsectagonTab() {
            var page = new Panel();
            var card = new RowCard(12, 656) { Height = 110 };
            accountStatusLbl.SetBounds(16, 14, 624, 28);
            accountDetailLbl.SetBounds(16, 46, 624, 48);
            card.Controls.AddRange(new Control[] { accountStatusLbl, accountDetailLbl });

            linkBtn.SetBounds(12, 138, 224, 42);
            refreshAcctBtn.SetBounds(246, 138, 144, 42);
            unlinkBtn.SetBounds(400, 138, 124, 42);

            linkBtn.Click += delegate { RunAccountHelper("--link-lobby", "Opening browser to link Parsectagon..."); };
            refreshAcctBtn.Click += delegate { RunAccountHelper("--lobby-account-status", "Checking Parsectagon account..."); };
            unlinkBtn.Click += delegate { RunAccountHelper("--unlink-lobby", "Unlinking this PC from Parsectagon..."); };

            page.Controls.AddRange(new Control[] { card, linkBtn, refreshAcctBtn, unlinkBtn });
            return page;
        }

        string FindIconsHelper() {
            foreach (string dir in targets) {
                string p = Path.Combine(dir, "ufc6_menu_icons.exe");
                if (File.Exists(p)) return p;
            }
            string local = Path.Combine(Path.GetDirectoryName(Application.ExecutablePath), "ufc6_menu_icons.exe");
            return File.Exists(local) ? local : null;
        }

        void RunAccountHelper(string op, string msg) {
            string helper = FindIconsHelper();
            if (helper == null) {
                feedback.ForeColor = Color.FromArgb(242, 161, 171);
                feedback.Text = "ufc6_menu_icons.exe not found in game folder.";
                return;
            }
            try {
                var psi = new System.Diagnostics.ProcessStartInfo(helper, op + " --lobby-url \"https://parsectagon.cc\"") {
                    WorkingDirectory = Path.GetDirectoryName(helper),
                    CreateNoWindow = true,
                    UseShellExecute = false
                };
                System.Diagnostics.Process.Start(psi);
                feedback.ForeColor = ModernUi.Accent;
                feedback.Text = msg;
                accountPoll.Start();
            } catch (Exception ex) {
                feedback.ForeColor = Color.FromArgb(242, 161, 171);
                feedback.Text = ex.Message;
            }
        }

        void RefreshAccountUi() {
            try {
                string home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
                string stateFile = Path.Combine(home, ".parsectagon-overlay-link.json");
                string tokenFile = Path.Combine(home, ".parsectagon-overlay-token");
                if (File.Exists(stateFile)) {
                    var dict = new JavaScriptSerializer().Deserialize<Dictionary<string, object>>(File.ReadAllText(stateFile));
                    string st = dict != null && dict.ContainsKey("status") ? Convert.ToString(dict["status"]) : "unlinked";
                    if (st == "linked") {
                        string name = dict.ContainsKey("name") ? Convert.ToString(dict["name"]) : "Fighter";
                        string rank = dict.ContainsKey("rank") ? Convert.ToString(dict["rank"]) : "";
                        string rating = dict.ContainsKey("rating") ? Convert.ToString(dict["rating"]) : "";
                        accountStatusLbl.ForeColor = ModernUi.Accent;
                        accountStatusLbl.Text = "Linked to Parsectagon as " + name;
                        accountDetailLbl.Text = string.IsNullOrEmpty(rank) ? "This PC is authorized for Parsectagon." : ("Rank: " + rank + "  •  Rating: " + rating);
                        unlinkBtn.Enabled = true;
                        return;
                    }
                    if (st == "pending") {
                        string code = dict.ContainsKey("code") ? Convert.ToString(dict["code"]) : "";
                        accountStatusLbl.ForeColor = Color.FromArgb(245, 190, 88);
                        accountStatusLbl.Text = "Waiting for browser approval (Code: " + code + ")";
                        accountDetailLbl.Text = "Approve the link in your browser window. This status updates automatically.";
                        unlinkBtn.Enabled = false;
                        return;
                    }
                    if (st == "error") {
                        string err = dict.ContainsKey("message") ? Convert.ToString(dict["message"]) : "Unknown error";
                        accountStatusLbl.ForeColor = Color.FromArgb(242, 161, 171);
                        accountStatusLbl.Text = "Parsectagon link error";
                        accountDetailLbl.Text = err;
                        unlinkBtn.Enabled = File.Exists(tokenFile);
                        return;
                    }
                }
                if (File.Exists(tokenFile)) {
                    accountStatusLbl.ForeColor = ModernUi.Text;
                    accountStatusLbl.Text = "Token saved on this PC";
                    accountDetailLbl.Text = "Click Refresh Status to verify your Parsectagon fighter profile.";
                    unlinkBtn.Enabled = true;
                } else {
                    accountStatusLbl.ForeColor = ModernUi.Muted;
                    accountStatusLbl.Text = "Not linked to Parsectagon";
                    accountDetailLbl.Text = "Click Log in / Link Parsectagon to authorize this PC in your browser.";
                    unlinkBtn.Enabled = false;
                }
            } catch {
                accountStatusLbl.ForeColor = ModernUi.Muted;
                accountStatusLbl.Text = "Not linked to Parsectagon";
                accountDetailLbl.Text = "Click Log in / Link Parsectagon to authorize this PC in your browser.";
                unlinkBtn.Enabled = false;
            }
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
                if (!OverlayConfig.IsKeyValid(val)) { feedback.ForeColor = Color.FromArgb(242, 161, 171); feedback.Text = "Key reserved (Esc/F10/Win). Pick another."; return true; }
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
            selTraining.SelectedIndex = config.TrainingRewardMultiplier == 2 ? 1 : config.TrainingRewardMultiplier == 5 ? 2 : config.TrainingRewardMultiplier == 10 ? 3 : 0;

            selIcons.SelectedIndex = Math.Max(0, Array.IndexOf(OverlayConfig.ButtonIconNames, config.ButtonIcons));
            chkGuide.Checked = config.GuideButtonOpensMenu;
            chkP1Only.Checked = config.FirstPlayerOnly;
            foreach (var kv in chordBoxes) kv.Value.Checked = (config.OpeningShortcut & kv.Key) != 0;
            UpdateKeyLabels();

            selPreset.SelectedIndex = config.MasterPreset;
            selLayout.SelectedIndex = config.LayoutStyle;
            selBackdrop.SelectedIndex = config.BackdropStyle;
            selTheme.SelectedIndex = config.ThemeChoice;
            selScale.SelectedIndex = Math.Max(0, (config.UIScale - 50) / 25);
            selFont.SelectedIndex = config.FontSize == 16 ? 0 : config.FontSize == 20 ? 2 : 1;
            selBox.SelectedIndex = config.BoxStyle;
            selWindow.SelectedIndex = config.WindowSize;

            numFov.Value = config.FpvFovDeg;
            numPitch.Value = (decimal)config.FpvPitchDeg;
            numHeight.Value = (decimal)config.FpvHeightM;
            numFwd.Value = (decimal)config.FpvFwdM;
            numGrapple.Value = (decimal)config.FpvGrappleDist;
            selFpvWobble.SelectedIndex = Math.Max(0, Math.Min(OverlayConfig.FpvWobbleNames.Length - 1, config.FpvWobble));
            RefreshFpvPresets(null);
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
                config.TrainingRewardMultiplier = mults[selTraining.SelectedIndex];

                config.ButtonIcons = OverlayConfig.ButtonIconNames[selIcons.SelectedIndex];
                config.GuideButtonOpensMenu = chkGuide.Checked;
                config.FirstPlayerOnly = chkP1Only.Checked;
                uint chord = 0;
                foreach (var kv in chordBoxes) if (kv.Value.Checked) chord |= kv.Key;
                config.OpeningShortcut = chord;

                config.MasterPreset = selPreset.SelectedIndex;
                config.LayoutStyle = selLayout.SelectedIndex;
                config.BackdropStyle = selBackdrop.SelectedIndex;
                config.ThemeChoice = selTheme.SelectedIndex;
                config.UIScale = 50 + selScale.SelectedIndex * 25;
                config.FontSize = selFont.SelectedIndex == 0 ? 16 : selFont.SelectedIndex == 2 ? 20 : 18;
                config.BoxStyle = selBox.SelectedIndex;
                config.WindowSize = selWindow.SelectedIndex;

                config.FpvFovDeg = (int)numFov.Value;
                config.FpvPitchDeg = (float)numPitch.Value;
                config.FpvHeightM = (float)numHeight.Value;
                config.FpvFwdM = (float)numFwd.Value;
                config.FpvGrappleDist = (float)numGrapple.Value;
                config.FpvWobble = selFpvWobble.SelectedIndex;

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
