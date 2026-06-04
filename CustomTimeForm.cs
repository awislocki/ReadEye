using System;
using System.Drawing;
using System.Windows.Forms;

namespace ReadEye
{
    public class CustomTimeForm : Form
    {
        private RadioButton rbDuration = null!;
        private RadioButton rbEndTime = null!;
        private NumericUpDown numHours = null!;
        private NumericUpDown numMinutes = null!;
        private DateTimePicker dtpEndTime = null!;
        private Button btnOk = null!;
        private Button btnCancel = null!;

        // Output results
        public bool IsDurationMode { get; private set; } = true;
        public TimeSpan SelectedDuration { get; private set; }
        public DateTime SelectedEndTime { get; private set; }

        public CustomTimeForm()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            this.Text = "Custom Awake Timer - ReadEye";
            this.Size = new Size(380, 310);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.ShowInTaskbar = false;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Color.FromArgb(28, 28, 28); // Dark Theme Background
            this.ForeColor = Color.FromArgb(240, 240, 240);

            // Custom Fonts
            Font headerFont = new Font("Segoe UI", 12F, FontStyle.Bold);
            Font labelFont = new Font("Segoe UI", 10F);
            Font inputFont = new Font("Segoe UI", 9.5F);

            // Header Title
            Label lblTitle = new Label
            {
                Text = "Custom Awake Timer",
                Font = headerFont,
                ForeColor = Color.FromArgb(0, 188, 212), // Cyan Accent
                Location = new Point(20, 20),
                Size = new Size(340, 25),
                AutoSize = false
            };
            this.Controls.Add(lblTitle);

            // Horizontal Line
            Panel pnlSeparator = new Panel
            {
                BackColor = Color.FromArgb(50, 50, 50),
                Location = new Point(20, 50),
                Size = new Size(324, 1)
            };
            this.Controls.Add(pnlSeparator);

            // Option 1: Duration
            rbDuration = new RadioButton
            {
                Text = "For a duration",
                Font = labelFont,
                Location = new Point(20, 65),
                Size = new Size(340, 24),
                Checked = true
            };
            rbDuration.CheckedChanged += Option_CheckedChanged;
            this.Controls.Add(rbDuration);

            // Duration Inputs Panel
            Panel pnlDuration = new Panel
            {
                Location = new Point(45, 95),
                Size = new Size(300, 35)
            };
            this.Controls.Add(pnlDuration);

            Label lblHours = new Label
            {
                Text = "Hours:",
                Font = inputFont,
                ForeColor = Color.FromArgb(180, 180, 180),
                Location = new Point(0, 5),
                Size = new Size(50, 20),
                TextAlign = ContentAlignment.MiddleLeft
            };
            numHours = new NumericUpDown
            {
                Font = inputFont,
                BackColor = Color.FromArgb(45, 45, 48),
                ForeColor = Color.White,
                BorderStyle = BorderStyle.FixedSingle,
                Location = new Point(50, 4),
                Size = new Size(55, 24),
                Minimum = 0,
                Maximum = 23,
                Value = 0
            };
            pnlDuration.Controls.Add(lblHours);
            pnlDuration.Controls.Add(numHours);

            Label lblMinutes = new Label
            {
                Text = "Mins:",
                Font = inputFont,
                ForeColor = Color.FromArgb(180, 180, 180),
                Location = new Point(125, 5),
                Size = new Size(45, 20),
                TextAlign = ContentAlignment.MiddleLeft
            };
            numMinutes = new NumericUpDown
            {
                Font = inputFont,
                BackColor = Color.FromArgb(45, 45, 48),
                ForeColor = Color.White,
                BorderStyle = BorderStyle.FixedSingle,
                Location = new Point(175, 4),
                Size = new Size(55, 24),
                Minimum = 0,
                Maximum = 59,
                Value = 30
            };
            pnlDuration.Controls.Add(lblMinutes);
            pnlDuration.Controls.Add(numMinutes);

            // Option 2: Target Time
            rbEndTime = new RadioButton
            {
                Text = "Until specific time",
                Font = labelFont,
                Location = new Point(20, 140),
                Size = new Size(340, 24)
            };
            rbEndTime.CheckedChanged += Option_CheckedChanged;
            this.Controls.Add(rbEndTime);

            // Target Time Panel
            Panel pnlEndTime = new Panel
            {
                Location = new Point(45, 170),
                Size = new Size(300, 35)
            };
            this.Controls.Add(pnlEndTime);

            Label lblTime = new Label
            {
                Text = "Time:",
                Font = inputFont,
                ForeColor = Color.FromArgb(180, 180, 180),
                Location = new Point(0, 5),
                Size = new Size(50, 20),
                TextAlign = ContentAlignment.MiddleLeft
            };
            dtpEndTime = new DateTimePicker
            {
                Font = inputFont,
                BackColor = Color.FromArgb(45, 45, 48),
                ForeColor = Color.White,
                Format = DateTimePickerFormat.Time,
                ShowUpDown = true,
                Location = new Point(50, 4),
                Size = new Size(110, 24)
            };
            pnlEndTime.Controls.Add(lblTime);
            pnlEndTime.Controls.Add(dtpEndTime);

            // Cancel Button
            btnCancel = new Button
            {
                Text = "Cancel",
                Font = labelFont,
                BackColor = Color.FromArgb(60, 60, 60),
                ForeColor = Color.FromArgb(220, 220, 220),
                FlatStyle = FlatStyle.Flat,
                Location = new Point(254, 225),
                Size = new Size(90, 32),
                DialogResult = DialogResult.Cancel
            };
            btnCancel.FlatAppearance.BorderSize = 0;
            btnCancel.MouseEnter += (s, e) => btnCancel.BackColor = Color.FromArgb(80, 80, 80);
            btnCancel.MouseLeave += (s, e) => btnCancel.BackColor = Color.FromArgb(60, 60, 60);
            this.Controls.Add(btnCancel);

            // OK Button
            btnOk = new Button
            {
                Text = "Start Awake",
                Font = labelFont,
                BackColor = Color.FromArgb(0, 188, 212),
                ForeColor = Color.Black,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(130, 225),
                Size = new Size(115, 32)
            };
            btnOk.FlatAppearance.BorderSize = 0;
            btnOk.MouseEnter += (s, e) => btnOk.BackColor = Color.FromArgb(0, 150, 170);
            btnOk.MouseLeave += (s, e) => btnOk.BackColor = Color.FromArgb(0, 188, 212);
            btnOk.Click += BtnOk_Click;
            this.Controls.Add(btnOk);

            this.AcceptButton = btnOk;
            this.CancelButton = btnCancel;

            // Trigger initial state sync
            Option_CheckedChanged(null, EventArgs.Empty);
        }

        private void Option_CheckedChanged(object? sender, EventArgs e)
        {
            numHours.Enabled = rbDuration.Checked;
            numMinutes.Enabled = rbDuration.Checked;
            dtpEndTime.Enabled = rbEndTime.Checked;

            if (rbDuration.Checked)
            {
                numHours.BackColor = Color.FromArgb(45, 45, 48);
                numMinutes.BackColor = Color.FromArgb(45, 45, 48);
                dtpEndTime.BackColor = Color.FromArgb(35, 35, 35);
            }
            else
            {
                numHours.BackColor = Color.FromArgb(35, 35, 35);
                numMinutes.BackColor = Color.FromArgb(35, 35, 35);
                dtpEndTime.BackColor = Color.FromArgb(45, 45, 48);
            }
        }

        private void BtnOk_Click(object? sender, EventArgs e)
        {
            IsDurationMode = rbDuration.Checked;

            if (IsDurationMode)
            {
                int hrs = (int)numHours.Value;
                int mins = (int)numMinutes.Value;

                if (hrs == 0 && mins == 0)
                {
                    MessageBox.Show("Please select a duration greater than 0 minutes.", "Invalid Duration", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    return;
                }

                SelectedDuration = new TimeSpan(hrs, mins, 0);
                SelectedEndTime = DateTime.Now.Add(SelectedDuration);
            }
            else
            {
                DateTime now = DateTime.Now;
                DateTime selectedTime = dtpEndTime.Value;

                // Create a full DateTime based on today or tomorrow
                DateTime targetTime = new DateTime(now.Year, now.Month, now.Day, selectedTime.Hour, selectedTime.Minute, 0);
                if (targetTime <= now)
                {
                    // If target time is earlier than current time today, assume tomorrow
                    targetTime = targetTime.AddDays(1);
                }

                SelectedEndTime = targetTime;
                SelectedDuration = targetTime - now;
            }

            this.DialogResult = DialogResult.OK;
            this.Close();
        }
    }
}
