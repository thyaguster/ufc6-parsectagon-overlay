// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace UFC6OverlaySetup {
    public static class ConfigProgram {
        [STAThread]
        public static int Main(string[] args) {
            string version = Assembly.GetExecutingAssembly().GetName().Version.ToString(3);
            try {
                if (args.Length == 2 && args[0] == "--self-test") {
                    string tmp = Path.Combine(Path.GetTempPath(), "ufc6-cfg-self-" + Guid.NewGuid().ToString("N") + ".txt");
                    ConfiguratorTests.Run(tmp);
                    string cfgResult = File.ReadAllText(tmp);
                    try { File.Delete(tmp); } catch { }
                    File.WriteAllText(args[1], UpdateTests.Run() + "\r\n" + DiagnosticsTests.Run() + "\r\n" + cfgResult);
                    return 0;
                }
                if (args.Length == 2 && args[0] == "--check-only") {
                    var result = UpdateClient.Latest(version);
                    File.WriteAllText(args[1], new JavaScriptSerializer().Serialize(new { installed = version, latest = result.Version, available = result.IsNewer, page = result.Page }));
                    return 0;
                }
                if (args.Length == 2 && args[0] == "--download-only") {
                    var result = UpdateClient.Latest(version);
                    string path = UpdateClient.Download(result, args[1]);
                    File.WriteAllText(Path.Combine(args[1], "download-result.txt"), path);
                    return 0;
                }
                if (args.Length == 3 && args[0] == "--export-support") {
                    Console.WriteLine(Diagnostics.Export(args[1], args[2], version));
                    return 0;
                }

                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);

                if ((args.Length == 2 || args.Length == 4) && args[0] == "--diagnostics") {
                    int pid = 0;
                    string renderer = "unknown";
                    if (args.Length == 4) {
                        renderer = args[2];
                        if (!int.TryParse(args[3], out pid) || pid < 0) throw new ArgumentException("Invalid game process.");
                    }
                    using (var form = new DiagnosticsForm(version, Diagnostics.Folder(args[1]), renderer, pid)) Application.Run(form);
                    return 0;
                }
                if (args.Length >= 1 && args[0] == "--update") {
                    string target = args.Length == 3 && args[1] == "--folder" ? Path.GetFullPath(args[2]) : "";
                    using (var form = new UpdateForm(version, target)) Application.Run(form);
                    return 0;
                }
                if (args.Length == 2 && args[0] == "--preview") {
                    using (var form = new UpdateForm(version, "", false)) {
                        form.Show(); Application.DoEvents();
                        using (var image = new Bitmap(form.Width, form.Height)) { form.DrawToBitmap(image, new Rectangle(Point.Empty, image.Size)); image.Save(args[1]); }
                    }
                    return 0;
                }
                if (args.Length == 2 && args[0] == "--preview-config") {
                    string tmpDir = Path.Combine(Path.GetTempPath(), "ufc6-cfg-preview-" + Guid.NewGuid().ToString("N"));
                    Directory.CreateDirectory(tmpDir);
                    try {
                        File.WriteAllBytes(Path.Combine(tmpDir, "UFC 6 novo.exe"), new byte[] { 1 });
                        using (var form = new ConfiguratorForm(tmpDir)) {
                            form.Show(); Application.DoEvents();
                            using (var image = new Bitmap(form.Width, form.Height)) { form.DrawToBitmap(image, new Rectangle(Point.Empty, image.Size)); image.Save(args[1]); }
                        }
                    } finally { try { Directory.Delete(tmpDir, true); } catch { } }
                    return 0;
                }

                string folder = args.Length == 2 && args[0] == "--folder" ? args[1] : null;
                if (string.IsNullOrEmpty(folder)) {
                    string exeDir = Path.GetDirectoryName(Application.ExecutablePath);
                    try { folder = OverlayConfig.NormalizeFolder(exeDir); } catch { folder = FolderPicker.Select(IntPtr.Zero, exeDir); }
                }
                if (string.IsNullOrEmpty(folder)) return 0;
                using (var form = new ConfiguratorForm(folder)) Application.Run(form);
                return 0;
            } catch (Exception ex) {
                if (args.Length == 2 && (args[0] == "--self-test" || args[0] == "--check-only")) File.WriteAllText(args[1], ex.ToString());
                else if (args.Length == 2 && args[0] == "--download-only") { Directory.CreateDirectory(args[1]); File.WriteAllText(Path.Combine(args[1], "download-error.txt"), ex.ToString()); }
                else if (args.Length > 0 && args[0] == "--export-support") Console.Error.WriteLine(ex.Message);
                else MessageBox.Show(ex.Message, "UFC 6 Parsectagon Overlay Configurator", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return 1;
            }
        }
    }
}
