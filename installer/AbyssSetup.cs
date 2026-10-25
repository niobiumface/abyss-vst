using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace AbyssSetup
{
    static class Program
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new SetupForm());
        }
    }

    sealed class SetupForm : Form
    {
        readonly CheckedListBox folders = new CheckedListBox();
        readonly TextBox log = new TextBox();
        readonly Button addButton = new Button();
        readonly Button removeButton = new Button();
        readonly Button installButton = new Button();

        public SetupForm()
        {
            Text = "Abyss 1.0.0 Setup";
            Font = new Font("Segoe UI", 9f);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new Size(560, 460);

            var title = new Label
            {
                Text = "Abyss",
                Font = new Font("Segoe UI", 18f, FontStyle.Bold),
                AutoSize = true,
                Location = new Point(16, 12)
            };
            var subtitle = new Label
            {
                Text = "Dark sub-octave VST3 by n-f.pro\r\nChoose one or more VST3 folders. The same plugin is copied into each selected folder.",
                AutoSize = false,
                Size = new Size(528, 40),
                Location = new Point(16, 46)
            };

            folders.Location = new Point(16, 96);
            folders.Size = new Size(528, 180);
            folders.CheckOnClick = true;

            addButton.Text = "Add folder...";
            addButton.Location = new Point(16, 286);
            addButton.Size = new Size(120, 28);
            addButton.Click += OnAdd;

            removeButton.Text = "Remove";
            removeButton.Location = new Point(144, 286);
            removeButton.Size = new Size(100, 28);
            removeButton.Click += OnRemove;

            installButton.Text = "Install";
            installButton.Location = new Point(424, 286);
            installButton.Size = new Size(120, 28);
            installButton.Click += OnInstall;

            log.Location = new Point(16, 326);
            log.Size = new Size(528, 118);
            log.Multiline = true;
            log.ReadOnly = true;
            log.ScrollBars = ScrollBars.Vertical;

            Controls.Add(title);
            Controls.Add(subtitle);
            Controls.Add(folders);
            Controls.Add(addButton);
            Controls.Add(removeButton);
            Controls.Add(installButton);
            Controls.Add(log);

            AddFolder(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Common Files", "VST3"), true);
            AddFolder(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Programs", "Common", "VST3"), true);
        }

        void AddFolder(string path, bool check)
        {
            var full = Path.GetFullPath(path);
            for (int i = 0; i < folders.Items.Count; ++i)
            {
                if (string.Equals(folders.Items[i].ToString(), full, StringComparison.OrdinalIgnoreCase))
                {
                    folders.SetItemChecked(i, true);
                    return;
                }
            }
            folders.Items.Add(full, check);
        }

        void OnAdd(object sender, EventArgs e)
        {
            using (var dialog = new FolderBrowserDialog())
            {
                dialog.Description = "Select a VST3 folder";
                dialog.ShowNewFolderButton = true;
                if (dialog.ShowDialog(this) == DialogResult.OK && !string.IsNullOrWhiteSpace(dialog.SelectedPath))
                    AddFolder(dialog.SelectedPath, true);
            }
        }

        void OnRemove(object sender, EventArgs e)
        {
            var index = folders.SelectedIndex;
            if (index >= 0)
                folders.Items.RemoveAt(index);
        }

        void OnInstall(object sender, EventArgs e)
        {
            var selected = new List<string>();
            foreach (var item in folders.CheckedItems)
                selected.Add(item.ToString());

            if (selected.Count == 0)
            {
                MessageBox.Show(this, "Select at least one folder.", "Abyss Setup", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            var source = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Abyss.vst3");
            var binary = Path.Combine(source, "Contents", "x86_64-win", "Abyss.vst3");
            if (!File.Exists(binary))
            {
                MessageBox.Show(this, "Abyss.vst3 was not found next to the installer.\r\n" + source, "Abyss Setup", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            installButton.Enabled = false;
            log.Clear();
            int ok = 0;
            foreach (var folder in selected)
            {
                try
                {
                    Directory.CreateDirectory(folder);
                    var dest = Path.Combine(folder, "Abyss.vst3");
                    if (Directory.Exists(dest))
                        Directory.Delete(dest, true);
                    CopyTree(source, dest);
                    log.AppendText("Installed  " + dest + Environment.NewLine);
                    ok++;
                }
                catch (Exception ex)
                {
                    log.AppendText("Failed  " + folder + Environment.NewLine + "  " + ex.Message + Environment.NewLine);
                }
            }

            log.AppendText(Environment.NewLine + ok + " of " + selected.Count + " folders updated." + Environment.NewLine);
            log.AppendText("Restart the DAW and rescan VST3 plugins.");
            installButton.Enabled = true;
            MessageBox.Show(this, ok + " of " + selected.Count + " folders updated.\r\nRestart the DAW and rescan plugins.", "Abyss Setup", MessageBoxButtons.OK, ok == selected.Count ? MessageBoxIcon.Information : MessageBoxIcon.Warning);
        }

        static void CopyTree(string source, string dest)
        {
            Directory.CreateDirectory(dest);
            foreach (var file in Directory.GetFiles(source))
                File.Copy(file, Path.Combine(dest, Path.GetFileName(file)), true);
            foreach (var dir in Directory.GetDirectories(source))
                CopyTree(dir, Path.Combine(dest, Path.GetFileName(dir)));
        }
    }
}
