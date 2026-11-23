// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace UFC6OverlaySetup {
    public sealed class RuntimeBridge {
        public string target { get; set; }
        public string original_name { get; set; }
        public string original_sha256 { get; set; }
    }
    public sealed class Release {
        public string version { get; set; }
        public string core_file { get; set; }
        public string engine_sha256 { get; set; }
        public string parsec_engine_sha256 { get; set; }
        public string dll_sha256 { get; set; }
        public string[] previous_dll_sha256 { get; set; }
        public Dictionary<string,string> companion_files { get; set; }
        public Dictionary<string,string[]> previous_companion_sha256 { get; set; }
        public RuntimeBridge runtime_bridge { get; set; }
        public Release new_engine { get; set; }
        public Release ufc4 { get; set; }
    }
    public sealed class Inspection {
        public string Folder;
        public bool Supported;
        public string ParsecStatus;
        public readonly List<string> Conflicts = new List<string>();
        public readonly List<string> HardConflicts = new List<string>();
        public readonly List<string> ReplacedFiles = new List<string>();
        public readonly Dictionary<string,string> Hashes = new Dictionary<string,string>();
        public int Installed;
        public int Current;
        public bool RuntimeAvailable = true;
        public bool PortheonReplaced;
        public bool SiblingPortheonReplaced;
        public int SiblingInstalled;
        public int TotalFiles;
        public string EngineLabel;
        public string SiblingEngineNotice;
        public bool CleanKyty;
        public static string DescribeEngine(string folderPath,string engineLabel) {
            string leaf = string.IsNullOrEmpty(folderPath) ? "" : Path.GetFileName(folderPath.TrimEnd('\\','/'));
            if (engineLabel != null && engineLabel.StartsWith("Clean Kyty",StringComparison.OrdinalIgnoreCase)) return string.IsNullOrEmpty(leaf) ? engineLabel : engineLabel + " (" + leaf + ")";
            if (leaf.Equals("novo",StringComparison.OrdinalIgnoreCase)) return "PS5 Pro / DLSS (novo)";
            if (leaf.Equals("bin",StringComparison.OrdinalIgnoreCase)) return "Original / Parsec (bin)";
            return string.IsNullOrEmpty(leaf) ? engineLabel : engineLabel + " (" + leaf + ")";
        }
        public string Summary(Manager manager) {
            var lines = new List<string>();
            string engineDisplay = DescribeEngine(Folder,EngineLabel);
            lines.Add(Supported ? "Compatible " + engineDisplay + " engine." : "Unsupported engine (" + engineDisplay + "). Installation is disabled.");
            if (CleanKyty && Supported) lines.Add("Clean Kyty mode: installs version.dll and the Vulkan overlay layer without modifying libwinpthread-1.dll. UFC 6 hooks activate automatically when a supported UFC 6 image is loaded.");
            if (ParsecStatus != null) lines.Add(ParsecStatus);
            if (PortheonReplaced) {
                lines.Add("PORTHEON update detected: a game update replaced overlay file(s) (" + string.Join(", ",ReplacedFiles) + ") in " + engineDisplay + ".");
                if (Supported && RuntimeAvailable) lines.Add("Action needed: click Repair / Reinstall to back up the replaced files and reinstall the overlay for " + engineDisplay + ".");
                else if (!Supported) lines.Add("This engine (" + engineDisplay + ") was updated to an unrecognized build. Wait for an overlay update or switch to the other engine.");
            } else {
                lines.Add(Installed == 0 ? "Overlay is not installed." : Current == TotalFiles ? "Overlay v" + manager.Version + " is already installed." : "An existing overlay installation was found for " + engineDisplay + "; it can be updated or removed.");
            }
            if (!RuntimeAvailable) lines.Add("Original threading runtime is missing. Restore libwinpthread-1.dll from your matching PORTHEON installation; do not delete the saved ufc6_pthread_original.dll.");
            if (HardConflicts.Count != 0) lines.Add("Linked or directory conflicts: " + string.Join(", ",HardConflicts) + ". These items cannot be replaced automatically.");
            else if (Conflicts.Count != 0 && !PortheonReplaced) lines.Add("Conflicting or linked files: " + string.Join(", ",Conflicts) + ". These files will not be replaced or removed.");
            if (!string.IsNullOrEmpty(SiblingEngineNotice)) lines.Add(SiblingEngineNotice);
            lines.Add("\r\nSettings and screenshots are kept. Close UFC 6 before making changes.");
            return string.Join("\r\n", lines);
        }
    }
    public sealed class Manager {
        public readonly List<string> Order = new List<string>();
        public readonly Dictionary<string,byte[]> Payload;
        readonly Dictionary<string,string> expected = new Dictionary<string,string>();
        readonly Dictionary<string,HashSet<string>> known = new Dictionary<string,HashSet<string>>();
        readonly string engineHash;
        readonly string parsecHash;
        readonly RuntimeBridge runtime;
        readonly string engineFile;
        readonly string engineLabel;
        readonly string coreFile;
        readonly string layerManifest;
        public Manager NewEngine;
        public Manager KytyEngine;
        public Manager Ufc4Engine;
        public readonly string Version;
        // Tests inject fixture bytes and a fixture engine hash into the same transaction code.
        public Action<int> BeforeCommit;
        public Manager(Release release, Dictionary<string,byte[]> payload) : this(release,payload,"UFC 6.exe","PORTHEON 3309ab23f",false) {}
        public Manager(Release release, Dictionary<string,byte[]> payload,string executable,string label) : this(release,payload,executable,label,false) {}
        public Manager(Release release, Dictionary<string,byte[]> payload,string executable,string label,bool skipRuntimeBridge) {
            engineFile=executable;engineLabel=label;
            coreFile = string.IsNullOrEmpty(release.core_file) ? "version.dll" : release.core_file;
            layerManifest = engineFile.Equals("sacredfire-game.exe", StringComparison.OrdinalIgnoreCase) ? "ufc4_actions_layer.json" : "ufc6_menu_layer.json";
            Version = release.version; engineHash = release.engine_sha256; parsecHash = release.parsec_engine_sha256; Payload = payload; runtime = skipRuntimeBridge ? null : release.runtime_bridge;
            string skippedRuntimeTarget = skipRuntimeBridge && release.runtime_bridge != null ? release.runtime_bridge.target : null;
            foreach (var pair in release.companion_files) {
                if (skippedRuntimeTarget != null && string.Equals(pair.Key,skippedRuntimeTarget,StringComparison.OrdinalIgnoreCase)) continue;
                expected.Add(pair.Key,pair.Value); Order.Add(pair.Key);
            }
            expected.Add(coreFile,release.dll_sha256); Order.Add(coreFile); // Core commits last.
            if (runtime != null) {
                if (!expected.ContainsKey(runtime.target) || runtime.target == coreFile) throw new InvalidDataException("Invalid runtime bridge target.");
                expected.Add(runtime.original_name,runtime.original_sha256); Order.Insert(0,runtime.original_name);
            }
            foreach (string name in Order) {
                if (Path.GetFileName(name) != name || name.IndexOfAny(new [] {'/', '\\', ':'}) >= 0) throw new InvalidDataException("Invalid release filename.");
                string[] old;
                if (name == coreFile) old = release.previous_dll_sha256;
                else if (!release.previous_companion_sha256.TryGetValue(name,out old)) old = new string[0];
                known[name] = new HashSet<string>(old ?? new string[0],StringComparer.OrdinalIgnoreCase);
                known[name].Add(expected[name]);
                if (runtime != null && name == runtime.target) known[name].Add(runtime.original_sha256);
                if (runtime != null && name == runtime.original_name) continue; // User's original; never bundled.
                if (!payload.ContainsKey(name) || Hash(payload[name]) != expected[name]) throw new InvalidDataException("The bundled " + name + " failed verification.");
            }
        }
        public static string Hash(byte[] bytes) { using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-","").ToLowerInvariant(); }
        public static string HashFile(string path) { using (var sha = SHA256.Create()) using (var file = File.OpenRead(path)) return BitConverter.ToString(sha.ComputeHash(file)).Replace("-","").ToLowerInvariant(); }
        static bool Linked(string path) { return (File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0; }
        public static string Normalize(string folder) {
            folder = Path.GetFullPath(folder.Trim().Trim('"'));
            if (!Directory.Exists(folder)) throw new IOException("Choose PORTHEON\\games\\UFC6 (bin / novo) or a Clean Kyty folder (kyty_emulator.exe).");
            if (!File.Exists(Path.Combine(folder,"UFC 6.exe")) && File.Exists(Path.Combine(folder,"bin","UFC 6.exe"))) folder = Path.Combine(folder,"bin");
            else if (!File.Exists(Path.Combine(folder,"UFC 6.exe")) && !File.Exists(Path.Combine(folder,"UFC 6 novo.exe")) && File.Exists(Path.Combine(folder,"novo","UFC 6 novo.exe"))) folder = Path.Combine(folder,"novo");
            if (Linked(folder)) throw new IOException("Choose a real game folder, rather than a linked folder.");
            if (!File.Exists(Path.Combine(folder,"UFC 6.exe")) && !File.Exists(Path.Combine(folder,"UFC 6 novo.exe")) && !File.Exists(Path.Combine(folder,"kyty_emulator.exe"))) throw new IOException("Game or Kyty engine was not found. Select PORTHEON\\games\\UFC6 (bin / novo) or a Clean Kyty folder with kyty_emulator.exe.");
            return folder;
        }
        public static string NormalizeUfc4(string folder) {
            folder = Path.GetFullPath(folder.Trim().Trim('"'));
            if (!Directory.Exists(folder)) throw new IOException("Choose PORTHEON\\games\\UFC4 (or UFC4\\bin containing sacredfire-game.exe).");
            if (!File.Exists(Path.Combine(folder, "sacredfire-game.exe")) && File.Exists(Path.Combine(folder, "bin", "sacredfire-game.exe"))) folder = Path.Combine(folder, "bin");
            if (Linked(folder)) throw new IOException("Choose a real game folder, rather than a linked folder.");
            if (!File.Exists(Path.Combine(folder, "sacredfire-game.exe"))) throw new IOException("UFC 4 native executable (sacredfire-game.exe) was not found in the selected folder.");
            return folder;
        }
        static bool IsCleanKytyFolder(string folder) {
            return File.Exists(Path.Combine(folder,"kyty_emulator.exe")) && !File.Exists(Path.Combine(folder,"UFC 6.exe")) && !File.Exists(Path.Combine(folder,"UFC 6 novo.exe"));
        }
        static bool HasOverlayMarkers(string folder) {
            return File.Exists(Path.Combine(folder,"ufc6-quit-menu.ini")) || File.Exists(Path.Combine(folder,"ufc6-quit-menu.log")) || File.Exists(Path.Combine(folder,"ufc6_pthread_original.dll")) || File.Exists(Path.Combine(folder,"ufc4-quit-menu.ini")) || File.Exists(Path.Combine(folder,"ufc4_actions.ini")) || File.Exists(Path.Combine(folder,"ufc4-quit-menu.log"));
        }
        static void PopulateSiblingState(Manager other,Inspection target,string currentFolder,string siblingFolderName) {
            if (other == null) return;
            DirectoryInfo parent = Directory.GetParent(currentFolder);
            if (parent == null) return;
            string sibling = Path.Combine(parent.FullName,siblingFolderName);
            if (!Directory.Exists(sibling)) return;
            try {
                Inspection otherState = other.InspectCore(sibling);
                string otherDisplay = Inspection.DescribeEngine(sibling,otherState.EngineLabel);
                target.SiblingPortheonReplaced = otherState.PortheonReplaced;
                target.SiblingInstalled = otherState.Installed;
                if (otherState.PortheonReplaced) target.SiblingEngineNotice = "Other engine (" + otherDisplay + ") was also replaced by a PORTHEON update and will be repaired together (click Repair / Reinstall).";
                else if (!otherState.Supported && (otherState.Installed > 0 || otherState.ReplacedFiles.Count > 0)) target.SiblingEngineNotice = "Other engine (" + otherDisplay + ") has an updated game executable and needs attention.";
                else if (otherState.Installed > 0 && otherState.Current < otherState.TotalFiles) target.SiblingEngineNotice = "Other engine (" + otherDisplay + ") has an older or partial overlay installed and will be updated together.";
                else if (otherState.Conflicts.Count != 0) target.SiblingEngineNotice = "Other engine (" + otherDisplay + ") has conflicting files (" + string.Join(", ",otherState.Conflicts) + "); both-folder installation is blocked.";
                else if (otherState.Supported && otherState.RuntimeAvailable) target.SiblingEngineNotice = "Both " + Inspection.DescribeEngine(currentFolder,target.EngineLabel) + " and " + otherDisplay + " were detected and will be managed together.";
            } catch { }
        }
        public Inspection Inspect(string folder) {
            if (engineFile.Equals("sacredfire-game.exe", StringComparison.OrdinalIgnoreCase)) return InspectCore(folder);
            folder = Normalize(folder);
            if (KytyEngine != null && IsCleanKytyFolder(folder)) return KytyEngine.InspectCore(folder);
            if(NewEngine!=null && File.Exists(Path.Combine(folder,"UFC 6 novo.exe"))) {
                Inspection newResult = NewEngine.InspectCore(folder);
                PopulateSiblingState(this,newResult,folder,"bin");
                return newResult;
            }
            Inspection result = InspectCore(folder);
            if (NewEngine != null) PopulateSiblingState(NewEngine,result,folder,"novo");
            return result;
        }
        Inspection InspectCore(string folder) {
            bool isUfc4 = engineFile.Equals("sacredfire-game.exe", StringComparison.OrdinalIgnoreCase);
            folder = isUfc4 ? NormalizeUfc4(folder) : Normalize(folder);
            if (Linked(Path.Combine(folder,engineFile))) throw new IOException("The game executable is a link. Nothing was changed.");
            string executable=Path.Combine(folder,engineFile);
            bool isKyty = engineFile.Equals("kyty_emulator.exe",StringComparison.OrdinalIgnoreCase);
            var result = new Inspection { Folder = folder, CleanKyty = isKyty, Supported = File.Exists(executable) && (isKyty || isUfc4 || HashFile(executable) == engineHash || EngineCompatibility.Matches(executable,engineFile)), TotalFiles=Order.Count, EngineLabel=engineLabel };
            string parsec = Path.Combine(folder,"UFC 6 Parsec.exe");
            if (parsecHash != null && File.Exists(parsec)) result.ParsecStatus = !Linked(parsec) && (HashFile(parsec) == parsecHash || EngineCompatibility.Matches(parsec,"UFC 6 Parsec.exe")) ? "Verified Parsec / two-player engine is supported." : "Unrecognized Parsec engine: that mode will not load the overlay.";
            foreach (string name in Order) {
                string path = Path.Combine(folder,name);
                if (Directory.Exists(path) || (File.Exists(path) && Linked(path))) { result.Conflicts.Add(name); result.HardConflicts.Add(name); continue; }
                string hash = File.Exists(path) ? HashFile(path) : null;
                result.Hashes[name] = hash;
                if (hash == null) continue;
                if (!known[name].Contains(hash)) { result.Conflicts.Add(name); result.ReplacedFiles.Add(name); }
                else {
                    bool original = runtime != null && (name == runtime.original_name || (name == runtime.target && hash == runtime.original_sha256));
                    if (!original) ++result.Installed;
                    if (hash == expected[name]) ++result.Current;
                }
            }
            if (runtime != null) result.RuntimeAvailable = (result.Hashes.ContainsKey(runtime.target) && result.Hashes[runtime.target] == runtime.original_sha256) || (result.Hashes.ContainsKey(runtime.original_name) && result.Hashes[runtime.original_name] == runtime.original_sha256);
            result.PortheonReplaced = result.ReplacedFiles.Count > 0 && result.HardConflicts.Count == 0 && (result.Installed > 0 || HasOverlayMarkers(folder));
            return result;
        }
        public static void RequireClosed(string folder) {
            // Do not terminate anything. A matching process that cannot be inspected is also left alone.
            foreach (var process in Process.GetProcesses()) using (process) {
                string name;
                try { name = process.ProcessName; } catch { continue; }
                if (!string.Equals(name,"UFC 6",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"UFC 6 novo",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"UFC 6 Parsec",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"UFC6",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"kyty_emulator",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"kyty_launcher",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"ufc6_menu_restart",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"ufc6_menu_icons",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"ufc6_menu_config",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"ufc6_menu_update",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"sacredfire-game",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"ufc4_actions_restart",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"ufc4_fight_report_watcher",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"ufc4_keycap_sync",StringComparison.OrdinalIgnoreCase)) continue;
                try {
                    string path = process.MainModule.FileName;
                    if (string.Equals(Path.GetDirectoryName(path),folder,StringComparison.OrdinalIgnoreCase) || string.Equals(path,Path.Combine(Directory.GetParent(folder).FullName,"UFC6.exe"),StringComparison.OrdinalIgnoreCase))
                        throw new InvalidOperationException("Close the game and its launcher before installing, updating, or removing the overlay.");
                } catch (Win32Exception) { throw new InvalidOperationException("A game or overlay process is running. Close the game and launcher, then try again."); }
                catch (InvalidOperationException) { throw; }
            }
        }
        static void Unchanged(string path,string hash) {
            if (Directory.Exists(path) || (File.Exists(path) && Linked(path)) || (File.Exists(path) ? HashFile(path) : null) != hash) throw new IOException("A file changed during setup. No replacement was attempted: " + Path.GetFileName(path));
        }
        public string ApplyAll(string folder,bool remove,bool repair) {
            string primary = Normalize(folder);
            if (KytyEngine != null && IsCleanKytyFolder(primary)) return KytyEngine.Apply(primary,remove,repair);
            if (NewEngine == null) return Apply(primary,remove,repair);
            bool isNovo = File.Exists(Path.Combine(primary,"UFC 6 novo.exe"));
            DirectoryInfo parent = Directory.GetParent(primary);
            if (parent == null) return Apply(primary,remove,repair);
            string sibling = Path.Combine(parent.FullName,isNovo ? "bin" : "novo");
            if (!Directory.Exists(sibling)) return Apply(primary,remove,repair);
            Manager siblingManager = isNovo ? this : NewEngine;
            string siblingExe = isNovo ? "UFC 6.exe" : "UFC 6 novo.exe";
            if (!File.Exists(Path.Combine(sibling,siblingExe))) return Apply(primary,remove,repair);
            Inspection sibState = siblingManager.InspectCore(sibling);
            string priDisplay = Inspection.DescribeEngine(primary,isNovo ? NewEngine.engineLabel : engineLabel);
            string sibDisplay = Inspection.DescribeEngine(sibling,sibState.EngineLabel);
            if (!remove) {
                if (!sibState.Supported || !sibState.RuntimeAvailable || sibState.HardConflicts.Count != 0 || (sibState.Conflicts.Count != 0 && !repair))
                    throw new IOException(sibDisplay + " is not ready for installation. Neither engine folder was changed.\r\n" + sibState.Summary(this));
            } else if (sibState.Installed > 0 && sibState.Conflicts.Count != 0) {
                throw new IOException(sibDisplay + " contains conflicting files. Neither engine folder was changed.");
            }
            RequireClosed(primary);RequireClosed(sibling);
            string message=Apply(primary,remove,repair);
            if(remove && sibState.Installed==0)return message;
            try{siblingManager.Apply(sibling,remove,repair);}
            catch(Exception error){throw new IOException(priDisplay + " was changed, but " + sibDisplay + " failed. Check both engine folders before retrying. " + error.Message,error);}
            return remove ? "Overlay removed from both " + priDisplay + " and " + sibDisplay + ". Settings, screenshots, and upgrade backups were kept." :
                "Overlay v" + Version + " installed for both " + priDisplay + " and " + sibDisplay + ".\r\nSettings and screenshots were kept. Recognized older files were backed up.";
        }
        public string Apply(string folder,bool remove) {
            return Apply(folder,remove,false);
        }
        public string Apply(string folder,bool remove,bool repair) {
            bool isUfc4 = engineFile.Equals("sacredfire-game.exe", StringComparison.OrdinalIgnoreCase);
            folder = isUfc4 ? NormalizeUfc4(folder) : Normalize(folder);
            if(!isUfc4 && KytyEngine!=null && IsCleanKytyFolder(folder)) return KytyEngine.Apply(folder,remove,repair);
            if(!isUfc4 && NewEngine!=null && File.Exists(Path.Combine(folder,"UFC 6 novo.exe"))) return NewEngine.Apply(folder,remove,repair);
            var state = Inspect(folder); folder = state.Folder;
            if (state.HardConflicts.Count != 0) throw new IOException("Linked or directory conflicts were found: " + string.Join(", ",state.HardConflicts) + ". Nothing was changed.");
            if (state.Conflicts.Count != 0 && (remove || !repair)) throw new IOException("Unknown or linked files were found: " + string.Join(", ",state.Conflicts) + ". Nothing was changed.");
            if (!remove && !state.Supported) throw new IOException("This is not the verified " + engineLabel + " build. Nothing was changed.");
            RequireClosed(folder);
            byte[] originalRuntime = null;
            if (runtime != null) {
                string source = state.Hashes[runtime.target] == runtime.original_sha256 ? runtime.target : state.Hashes[runtime.original_name] == runtime.original_sha256 ? runtime.original_name : null;
                if (source == null) throw new IOException("The verified original threading runtime is missing. Nothing was changed. Restore it from your own matching PORTHEON installation.");
                originalRuntime = File.ReadAllBytes(Path.Combine(folder,source));
                if (Hash(originalRuntime) != runtime.original_sha256) throw new IOException("The original runtime changed during setup. Nothing was changed.");
                Payload[runtime.original_name] = originalRuntime;
            }
            var desired = Order.ToDictionary(name => name,name => !remove ? expected[name] : runtime != null && name == runtime.target ? runtime.original_sha256 : null);
            var order = remove ? Order.AsEnumerable().Reverse().ToList() : Order;
            var staged = new Dictionary<string,string>();
            var backups = new Dictionary<string,string>();
            var committed = new List<string>();
            var installed = new HashSet<string>();
            string token = Guid.NewGuid().ToString("N");
            try {
                foreach (string name in order) {
                    string old = state.Hashes[name];
                    if (old == desired[name]) continue;
                    if (desired[name] != null) {
                        string temp = Path.Combine(folder,".ufc6-menu-" + token + "-" + name + ".tmp");
                        using (var file = new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None)) { staged.Add(name,temp); byte[] bytes = remove ? originalRuntime : Payload[name]; file.Write(bytes,0,bytes.Length); file.Flush(true); }
                        if (HashFile(temp) != desired[name]) throw new IOException("Temporary payload verification failed.");
                    }
                    if (old != null) backups[name] = Path.Combine(folder,"ufc6-quit-menu." + (remove ? "removed-" : state.ReplacedFiles.Contains(name) ? "repaired-" : "previous-") + name + "-" + old.Substring(0,16) + "-" + token + ".bak");
                }
                foreach (string name in order) {
                    if (!staged.ContainsKey(name) && !backups.ContainsKey(name)) continue;
                    if (BeforeCommit != null) BeforeCommit(committed.Count);
                    string target = Path.Combine(folder,name);
                    Unchanged(target,state.Hashes[name]);
                    committed.Add(name);
                    if (backups.ContainsKey(name)) File.Move(target,backups[name]);
                    if (staged.ContainsKey(name)) { File.Move(staged[name],target); installed.Add(name); if (HashFile(target) != desired[name]) throw new IOException("Installed file verification failed."); }
                }
            } catch (Exception error) {
                var failures = new List<string>();
                foreach (string name in committed.AsEnumerable().Reverse()) {
                    string target = Path.Combine(folder,name);
                    try {
                        if (installed.Contains(name) && File.Exists(target) && !Linked(target) && HashFile(target) == desired[name]) File.Delete(target);
                        if (backups.ContainsKey(name) && File.Exists(backups[name]) && !File.Exists(target) && !Directory.Exists(target)) File.Move(backups[name],target);
                        if ((File.Exists(target) ? HashFile(target) : null) != state.Hashes[name]) failures.Add(name);
                    } catch { failures.Add(name); }
                }
                if (failures.Count != 0) throw new IOException(error.Message + "\r\nAutomatic rollback needs attention for: " + string.Join(", ",failures) + ". Original files remain in the adjacent backup files.",error);
                throw;
            } finally { foreach (string path in staged.Values) { try { if (File.Exists(path)) File.Delete(path); } catch { } } }
            if (remove) foreach (string path in backups.Values) { try { File.Delete(path); } catch { /* The mod is removed; leave any locked backup in place. */ } }
            try { string legacy = Path.Combine(folder, "ufc6_menu_update.exe"); if (File.Exists(legacy) && !Linked(legacy)) File.Delete(legacy); } catch { }
            if (isUfc4 && !remove) {
                try {
                    string ini = Path.Combine(folder, "ufc4-quit-menu.ini");
                    string legacyIni = Path.Combine(folder, "ufc4_actions.ini");
                    if (!File.Exists(ini) && File.Exists(legacyIni)) File.Copy(legacyIni, ini, false);
                } catch { }
            }
            try {
                string manifest = Path.Combine(folder, layerManifest);
                bool hklmOk = false;
                try {
                    using (var key = Microsoft.Win32.Registry.LocalMachine.CreateSubKey(@"SOFTWARE\Khronos\Vulkan\ImplicitLayers",true)) {
                        if (key != null) {
                            if (remove) key.DeleteValue(manifest,false);
                            else key.SetValue(manifest,0,Microsoft.Win32.RegistryValueKind.DWord);
                            hklmOk = true;
                        }
                    }
                } catch { }
                if (!hklmOk || remove) {
                    using (var key = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(@"SOFTWARE\Khronos\Vulkan\ImplicitLayers",true)) {
                        if (key != null) {
                            if (remove) key.DeleteValue(manifest,false);
                            else key.SetValue(manifest,0,Microsoft.Win32.RegistryValueKind.DWord);
                        }
                    }
                }
            } catch { /* Best-effort; version.dll also sets VK_ADD_IMPLICIT_LAYER_PATH and HKCU at runtime. */ }
            return remove ? "Overlay removed. Settings, screenshots, and upgrade backups were kept." :
                isUfc4 ? "UFC 4 Overlay v" + Version + " installed.\r\nLaunch UFC 4 normally, then hold LB + RB + Menu for about one second, or press F10.\r\n\r\nSettings and screenshots were kept. Recognized older files were backed up." :
                "Overlay v" + Version + " installed.\r\nLaunch UFC 6 normally, then hold LB + RB + Menu for about one second, or press F10.\r\n\r\nSettings and screenshots were kept. Recognized older files were backed up.";
        }
    }
    public sealed class SetupForm : ModernForm {
        readonly Manager manager;
        readonly CheckBox chkUfc6 = new CheckBox { Text = "UFC 6", Checked = true, AutoSize = true, ForeColor = ModernUi.Text, BackColor = ModernUi.Card, Font = new Font("Segoe UI", 10, FontStyle.Bold) };
        readonly CheckBox chkUfc4 = new CheckBox { Text = "UFC 4", Checked = false, AutoSize = true, ForeColor = ModernUi.Text, BackColor = ModernUi.Card, Font = new Font("Segoe UI", 10, FontStyle.Bold) };
        readonly TextBox folder = new TextBox();
        readonly TextBox folder4 = new TextBox();
        readonly TextBox status = new TextBox();
        readonly ModernButton support = new ModernButton(), configure = new ModernButton();
        readonly CheckBox autoConfigure = new CheckBox { Text = "Open configurator after install", Checked = true, AutoSize = true, ForeColor = ModernUi.Muted, BackColor = ModernUi.Background, Font = new Font("Segoe UI", 9) };
        readonly ModernButton browse = new ModernButton(), browse4 = new ModernButton(), check = new ModernButton(), parsectagon = new ModernButton(), discord = new ModernButton(), install = new ModernButton(), remove = new ModernButton(), updates = new ModernButton();
        bool busy;
        bool repairMode;
        public SetupForm(Manager manager) {
            this.manager = manager;
            Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);
            Text = "Parsectagon Overlay Setup — v" + manager.Version;
            ClientSize=new Size(760,710);
            var title=ModernUi.Label("Parsectagon Overlay",116,74,610,38,19,true);
            var hint=ModernUi.Label("INSTALL  /  UPDATE  /  REMOVE  (UFC 6 & UFC 4)",118,116,580,24,9,false,true);
            var version=ModernUi.Label("SETUP  v"+manager.Version,32,145,620,20,9,false,true);
            var pathCard=new ModernCard {Bounds=new Rectangle(32,178,696,232)};
            pathCard.Controls.Add(ModernUi.Label("1   Select games & PORTHEON / Clean Kyty folders",20,16,650,28,12,true));
            pathCard.Controls.Add(ModernUi.Label("Enable UFC 6 and/or UFC 4 below. Selecting PORTHEON\\games auto-detects both.",20,46,650,22,10,false,true));
            chkUfc6.Location=new Point(20,82);
            var input=new ModernCard {Bounds=new Rectangle(96,74,424,40)};
            folder.SetBounds(12,9,400,24);folder.BorderStyle=BorderStyle.None;folder.BackColor=ModernUi.Card;folder.ForeColor=ModernUi.Text;folder.Font=new Font("Segoe UI",10.5f);input.Controls.Add(folder);
            browse.Text="Browse...";browse.SetBounds(536,74,140,40);
            chkUfc4.Location=new Point(20,132);
            var input4=new ModernCard {Bounds=new Rectangle(96,124,424,40)};
            folder4.SetBounds(12,9,400,24);folder4.BorderStyle=BorderStyle.None;folder4.BackColor=ModernUi.Card;folder4.ForeColor=ModernUi.Text;folder4.Font=new Font("Segoe UI",10.5f);input4.Controls.Add(folder4);
            browse4.Text="Browse...";browse4.SetBounds(536,124,140,40);
            if (manager.Ufc4Engine == null) { chkUfc4.Enabled = folder4.Enabled = browse4.Enabled = false; }
            check.Text="Check folders";check.SetBounds(20,182,130,32);
            parsectagon.Text="Parsectagon.cc";parsectagon.SetBounds(160,182,140,32);
            discord.Text="Discord";discord.SetBounds(310,182,110,32);
            updates.Text="Check for updates";updates.SetBounds(506,182,170,32);
            pathCard.Controls.AddRange(new Control[]{chkUfc6,input,browse,chkUfc4,input4,browse4,check,parsectagon,discord,updates});
            var infoCard=new ModernCard {Bounds=new Rectangle(32,426,696,174)};
            infoCard.Controls.Add(ModernUi.Label("2   Check compatibility",20,16,650,27,12,true));
            status.SetBounds(20,52,656,104);ModernUi.Status(status);
            status.Text="Choose your UFC 6 folder (bin / novo / Clean Kyty) and/or UFC 4 folder (sacredfire-game.exe).\r\nUse the checkboxes to include or exclude each game from installation or removal.\r\nClick Check folders to verify.";infoCard.Controls.Add(status);
            support.Text="Support report...";support.SetBounds(32,624,196,44);support.Enabled=false;
            support.Click+=delegate{try{using(var diagnostic=new DiagnosticsForm(manager.Version,Diagnostics.Folder(folder.Text))){diagnostic.ShowDialog(this);}}catch(Exception error){status.Text=error.Message;}};
            configure.Text="Configure...";configure.SetBounds(238,624,124,44);configure.Enabled=false;
            configure.Click+=delegate{try{using(var cfg=new ConfiguratorForm(folder.Text)){cfg.ShowDialog(this);}}catch(Exception error){status.Text=error.Message;}};
            install.Text="Install / Update";install.SetBounds(372,624,176,44);install.Tone=ButtonTone.Primary;install.Enabled=false;
            remove.Text="Remove overlay";remove.SetBounds(558,624,170,44);remove.Tone=ButtonTone.Danger;remove.Enabled=false;
            autoConfigure.Location=new Point(478,680);
            var kept=ModernUi.Label("Settings, screenshots and save backups are kept.",32,681,440,22,9,false,true);
            Controls.AddRange(new Control[]{ModernUi.Logo(32,70,66),title,hint,version,pathCard,infoCard,support,configure,install,remove,autoConfigure,kept});
            parsectagon.Click+=delegate{try{Process.Start(new ProcessStartInfo("https://parsectagon.cc"){UseShellExecute=true});}catch(Exception error){status.Text="Could not open Parsectagon: "+error.Message;}};
            discord.Click+=delegate{try{Process.Start(new ProcessStartInfo("https://discord.gg/a84pBw2Ff"){UseShellExecute=true});}catch(Exception error){status.Text="Could not open Discord: "+error.Message;}};
            updates.Click+=delegate{using(var update=new UpdateForm(manager.Version,folder.Text)){update.ShowDialog(this);if(update.InstallerOpened)Close();}};
            AcceptButton=check;
            browse.Click+=delegate{try{string selected=FolderPicker.Select(Handle,folder.Text);if(selected!=null){AutoDetectFromPath(selected,true);Run(null);}}catch(Exception error){status.Text="Could not open the folder picker: "+error.Message;}};
            browse4.Click+=delegate{try{string selected=FolderPicker.Select(Handle,folder4.Text);if(selected!=null){AutoDetectFromPath(selected,false);Run(null);}}catch(Exception error){status.Text="Could not open the folder picker: "+error.Message;}};
            EventHandler invalidate = delegate { support.Enabled = configure.Enabled = install.Enabled = remove.Enabled = false; };
            folder.TextChanged += invalidate;
            folder4.TextChanged += invalidate;
            chkUfc6.CheckedChanged += delegate { folder.Enabled = browse.Enabled = chkUfc6.Checked; invalidate(null, null); };
            chkUfc4.CheckedChanged += delegate { folder4.Enabled = browse4.Enabled = chkUfc4.Checked && manager.Ufc4Engine != null; invalidate(null, null); };
            check.Click += delegate { Run(null); };
            install.Click += delegate { Run(false, repairMode); };
            remove.Click += delegate { if (MessageBox.Show(this,"Remove the overlay from the selected game folder(s)? Settings and screenshots will be kept.","Remove overlay",MessageBoxButtons.YesNo,MessageBoxIcon.Question,MessageBoxDefaultButton.Button2) == DialogResult.Yes) Run(true, false); };
            FormClosing += delegate(object sender,FormClosingEventArgs e) { if (busy) e.Cancel = true; };
        }
        void AutoDetectFromPath(string selected, bool fromUfc6Row) {
            string p = Path.GetFullPath(selected.Trim().Trim('"'));
            if (File.Exists(Path.Combine(p, "UFC6", "bin", "UFC 6.exe")) || File.Exists(Path.Combine(p, "UFC4", "bin", "sacredfire-game.exe"))) {
                string u6 = Path.Combine(p, "UFC6");
                string u4 = Path.Combine(p, "UFC4", "bin");
                if (Directory.Exists(u6)) { folder.Text = u6; chkUfc6.Checked = true; }
                if (Directory.Exists(u4) && manager.Ufc4Engine != null) { folder4.Text = u4; chkUfc4.Checked = true; }
                return;
            }
            if (fromUfc6Row) {
                folder.Text = p; chkUfc6.Checked = true;
                if (manager.Ufc4Engine != null && string.IsNullOrWhiteSpace(folder4.Text)) {
                    try {
                        string gamesDir = Directory.GetParent(p) != null && (Path.GetFileName(p).Equals("bin", StringComparison.OrdinalIgnoreCase) || Path.GetFileName(p).Equals("novo", StringComparison.OrdinalIgnoreCase)) ? Directory.GetParent(Directory.GetParent(p).FullName).FullName : Directory.GetParent(p).FullName;
                        string sib4 = Path.Combine(gamesDir, "UFC4", "bin");
                        if (File.Exists(Path.Combine(sib4, "sacredfire-game.exe"))) { folder4.Text = sib4; chkUfc4.Checked = true; }
                    } catch { }
                }
            } else {
                folder4.Text = p; chkUfc4.Checked = true;
                if (string.IsNullOrWhiteSpace(folder.Text)) {
                    try {
                        string gamesDir = Path.GetFileName(p).Equals("bin", StringComparison.OrdinalIgnoreCase) ? Directory.GetParent(Directory.GetParent(p).FullName).FullName : Directory.GetParent(p).FullName;
                        string sib6 = Path.Combine(gamesDir, "UFC6");
                        if (Directory.Exists(sib6)) { folder.Text = sib6; chkUfc6.Checked = true; }
                    } catch { }
                }
            }
        }
        public void SelectFolder(string path) { AutoDetectFromPath(path, !File.Exists(Path.Combine(path, "sacredfire-game.exe")) && !File.Exists(Path.Combine(path, "bin", "sacredfire-game.exe"))); Run(null); }
        void Run(bool? removal) { Run(removal,false); }
        void Run(bool? removal,bool repair) {
            if (busy) return;
            bool do6 = chkUfc6.Checked, do4 = chkUfc4.Checked && manager.Ufc4Engine != null;
            if (!do6 && !do4) { status.Text = "Select at least one game (UFC 6 or UFC 4) to check, install, or remove."; return; }
            busy = true; string path6 = folder.Text; string path4 = folder4.Text;
            chkUfc6.Enabled = chkUfc4.Enabled = folder.Enabled = folder4.Enabled = browse.Enabled = browse4.Enabled = check.Enabled = updates.Enabled = support.Enabled = configure.Enabled = install.Enabled = remove.Enabled = false;
            status.Text = removal.HasValue ? (repair ? "Repairing overlay…" : "Applying changes…") : "Checking engines and installed files…"; UseWaitCursor = true;
            var worker = new BackgroundWorker();
            worker.DoWork += delegate(object sender,DoWorkEventArgs e) {
                var msgs = new List<string>();
                Inspection state6 = null, state4 = null;
                if (do6) {
                    if (removal.HasValue) msgs.Add("UFC 6: " + manager.ApplyAll(path6, removal.Value, repair));
                    state6 = manager.Inspect(path6);
                }
                if (do4) {
                    if (removal.HasValue) msgs.Add("UFC 4: " + manager.Ufc4Engine.Apply(path4, removal.Value, repair));
                    state4 = manager.Ufc4Engine.Inspect(path4);
                }
                e.Result = new object[] { state6, state4, msgs.Count > 0 ? string.Join("\r\n\r\n", msgs) : null };
            };
            worker.RunWorkerCompleted += delegate(object sender,RunWorkerCompletedEventArgs e) {
                busy = false; UseWaitCursor = false;
                chkUfc6.Enabled = check.Enabled = updates.Enabled = true;
                chkUfc4.Enabled = manager.Ufc4Engine != null;
                folder.Enabled = browse.Enabled = chkUfc6.Checked;
                folder4.Enabled = browse4.Enabled = chkUfc4.Checked && manager.Ufc4Engine != null;
                if (e.Error != null) { status.Text = e.Error.Message + "\r\n\r\nIf Windows denies access, close the game first. For a protected game folder, right-click this setup and choose Run as administrator."; }
                else {
                    var values = (object[])e.Result;
                    var state6 = (Inspection)values[0];
                    var state4 = (Inspection)values[1];
                    if (state6 != null) folder.Text = state6.Folder;
                    if (state4 != null) folder4.Text = state4.Folder;
                    if (values[2] != null) status.Text = (string)values[2];
                    else {
                        var sum = new List<string>();
                        if (state6 != null) sum.Add("[UFC 6]\r\n" + state6.Summary(manager));
                        if (state4 != null) sum.Add("[UFC 4]\r\n" + state4.Summary(manager.Ufc4Engine));
                        status.Text = string.Join("\r\n\r\n", sum);
                    }
                    support.Enabled = configure.Enabled = (state6 != null);
                    bool rep6 = state6 != null && (state6.PortheonReplaced || state6.SiblingPortheonReplaced) && state6.HardConflicts.Count == 0;
                    bool rep4 = state4 != null && state4.PortheonReplaced && state4.HardConflicts.Count == 0;
                    repairMode = rep6 || rep4;
                    install.Text = repairMode ? "Repair / Reinstall" : "Install / Update";
                    bool canInstall6 = state6 == null || (state6.Supported && state6.RuntimeAvailable && (state6.Conflicts.Count == 0 || state6.PortheonReplaced));
                    bool canInstall4 = state4 == null || (state4.Supported && (state4.Conflicts.Count == 0 || state4.PortheonReplaced));
                    install.Enabled = (state6 != null || state4 != null) && canInstall6 && canInstall4;
                    bool hasInstalled6 = state6 != null && (state6.Installed > 0 || state6.SiblingInstalled > 0) && state6.Conflicts.Count == 0;
                    bool hasInstalled4 = state4 != null && state4.Installed > 0 && state4.Conflicts.Count == 0;
                    remove.Enabled = hasInstalled6 || hasInstalled4;
                    if (removal.HasValue && !removal.Value && autoConfigure.Checked && state6 != null) {
                        try { using (var cfg = new ConfiguratorForm(state6.Folder)) cfg.ShowDialog(this); } catch { }
                    }
                }
                worker.Dispose();
            };
            worker.RunWorkerAsync();
        }
    }
    public static class Program {
        public static byte[] Resource(string name) { using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(name)) { if (stream == null) throw new IOException("Missing bundled resource: " + name); using (var memory = new MemoryStream()) { stream.CopyTo(memory); return memory.ToArray(); } } }
        static Manager Bundled() {
            var release = new JavaScriptSerializer().Deserialize<Release>(Encoding.UTF8.GetString(Resource("release.json")).TrimStart('\ufeff'));
            var files = release.companion_files.Keys.Concat(new [] {"version.dll"}).ToDictionary(name => name,name => Resource(name));
            var manager=new Manager(release,files);
            if(release.new_engine!=null){var payload=release.new_engine.companion_files.Keys.Concat(new [] {"version.dll"}).ToDictionary(name=>name,name=>Resource(name));manager.NewEngine=new Manager(release.new_engine,payload,"UFC 6 novo.exe","PORTHEON PS5 Pro / DLSS");}
            var kytyRel=release.new_engine??release;
            var kytyFiles=kytyRel.companion_files.Keys.Concat(new [] {"version.dll"}).ToDictionary(name=>name,name=>Resource(name));
            manager.KytyEngine=new Manager(kytyRel,kytyFiles,"kyty_emulator.exe","Clean Kyty",true);
            if (release.ufc4 != null) {
                var u4Files = release.ufc4.companion_files.Keys.ToDictionary(name => name, name => Resource(name));
                u4Files["winmm.dll"] = Resource("ufc4_winmm.dll");
                manager.Ufc4Engine = new Manager(release.ufc4, u4Files, "sacredfire-game.exe", "UFC 4 (SacredFire)", true);
            }
            return manager;
        }
        [STAThread] public static int Main(string[] args) {
            try {
                if (args.Length == 2 && args[0] == "--self-test") { SelfTests.Run(args[1]); return 0; }
                if (args.Length == 2 && args[0] == "--install") { Bundled().ApplyAll(args[1],false,false); return 0; }
                if (args.Length == 2 && args[0] == "--repair") { Bundled().ApplyAll(args[1],false,true); return 0; }
                Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
                var manager = Bundled();
                using (var form = new SetupForm(manager)) {
                    if(args.Length==2&&args[0]=="--folder")form.Shown+=delegate{form.SelectFolder(args[1]);};
                    else if(args.Length!=0&&!(args.Length==2&&args[0]=="--preview"))throw new ArgumentException("Use --folder with the UFC 6 folder.");

                    if (args.Length == 2 && args[0] == "--preview") { form.Show(); Application.DoEvents(); using (var bitmap = new Bitmap(form.Width,form.Height)) { form.DrawToBitmap(bitmap,new Rectangle(Point.Empty,bitmap.Size)); bitmap.Save(args[1]); } return 0; }
                    Application.Run(form);
                }
                return 0;
            } catch (Exception error) {
                if (args.Length == 2 && args[0] == "--self-test") { File.WriteAllText(args[1],"FAIL: " + error); return 1; }
                MessageBox.Show(error.Message,"UFC 6 Parsectagon Overlay Setup",MessageBoxButtons.OK,MessageBoxIcon.Error); return 1;
            }
        }
    }
}
