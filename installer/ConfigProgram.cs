// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.IO;
using System.Windows.Forms;

namespace UFC6OverlaySetup {
    public static class ConfigProgram {
        [STAThread]
        public static int Main(string[] args) {
            try {
                if (args.Length == 2 && args[0] == "--self-test") {
                    ConfiguratorTests.Run(args[1]);
                    return 0;
                }
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                string folder = args.Length == 2 && args[0] == "--folder" ? args[1] : null;
                if (string.IsNullOrEmpty(folder)) {
                    string exeDir = Path.GetDirectoryName(Application.ExecutablePath);
                    try { folder = OverlayConfig.NormalizeFolder(exeDir); } catch { folder = FolderPicker.Select(IntPtr.Zero, exeDir); }
                }
                if (string.IsNullOrEmpty(folder)) return 0;
                using (var form = new ConfiguratorForm(folder)) Application.Run(form);
                return 0;
            } catch (Exception ex) {
                if (args.Length == 2 && args[0] == "--self-test") {
                    File.WriteAllText(args[1], "FAIL: " + ex);
                    return 1;
                }
                MessageBox.Show(ex.Message, "UFC 6 Parsectagon Overlay Configurator", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return 1;
            }
        }
    }
}
