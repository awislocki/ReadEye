using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using Microsoft.Win32;

namespace ReadEye
{
    public class TrayApplicationContext : ApplicationContext
    {
        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool DestroyIcon(IntPtr hIcon);

        private readonly NotifyIcon notifyIcon;
        private readonly ContextMenuStrip contextMenu;
        private readonly System.Windows.Forms.Timer stateTimer;
        private readonly System.Windows.Forms.Timer jigglerTimer;

        private const string SettingsKeyPath = @"Software\ReadEye";

        // State variables
        private bool isAwakeActive = false;
        private bool isJigglerMode = true;
        private bool disableOnLidClose = true;
        private bool hasExpiration = false;
        private DateTime expirationTime = DateTime.MinValue;
        private DateTime sessionStartTime = DateTime.MinValue;
        private IntPtr currentIconHandle = IntPtr.Zero;
        private readonly LidMonitor lidMonitor;

        // Menu items that need dynamic updates
        private ToolStripMenuItem itemStatus = null!;
        private ToolStripMenuItem itemToggle = null!;
        private ToolStripMenuItem itemJigglerMode = null!;
        private ToolStripMenuItem itemLidClose = null!;
        private ToolStripMenuItem itemStartup = null!;

        public TrayApplicationContext()
        {
            // Load persisted settings before building the menu (defaults: jiggler on, turn-off-on-lid-close on)
            isJigglerMode = ReadSetting("JigglerMode", true);
            disableOnLidClose = ReadSetting("DisableOnLidClose", true);

            // Initialize Context Menu
            contextMenu = new ContextMenuStrip();
            InitializeContextMenu();

            // Initialize NotifyIcon (start invisible to avoid Windows ghost icon bugs)
            notifyIcon = new NotifyIcon
            {
                ContextMenuStrip = contextMenu,
                Visible = false
            };
            notifyIcon.DoubleClick += NotifyIcon_DoubleClick;

            // State timer (checks expirations and updates icon/tooltip every second)
            stateTimer = new System.Windows.Forms.Timer { Interval = 1000 };
            stateTimer.Tick += StateTimer_Tick;
            stateTimer.Start();

            // Jiggler timer (simulates input every 50 seconds to bypass Group Policy timeouts)
            jigglerTimer = new System.Windows.Forms.Timer { Interval = 50000 };
            jigglerTimer.Tick += JigglerTimer_Tick;

            // Lid monitor (turns off keep-awake when the laptop lid closes, if enabled)
            lidMonitor = new LidMonitor();
            lidMonitor.LidStateChanged += LidMonitor_LidStateChanged;

            // Set initial state & generate icon (active by default so Teams stays awake immediately)
            SetAwakeState(true);
            UpdateTrayIconAndTooltip();

            // Now make it visible after the icon is assigned
            notifyIcon.Visible = true;

            // Show a startup notification
            notifyIcon.ShowBalloonTip(2000, "ReadEye", "ReadEye is running and keeping your system awake.", ToolTipIcon.Info);
        }

        private void InitializeContextMenu()
        {
            // Title/Status indicator (disabled menu item)
            itemStatus = new ToolStripMenuItem("ReadEye: Passive") { Enabled = false };
            itemStatus.Font = new Font(itemStatus.Font, FontStyle.Bold);

            // Toggle Keep Awake
            itemToggle = new ToolStripMenuItem("Keep Awake", null, ToggleAwake_Click);

            // Timers Submenu
            var itemTimers = new ToolStripMenuItem("Enable For...");
            itemTimers.DropDownItems.Add(new ToolStripMenuItem("15 Minutes", null, (s, e) => StartTimedAwake(TimeSpan.FromMinutes(15))));
            itemTimers.DropDownItems.Add(new ToolStripMenuItem("30 Minutes", null, (s, e) => StartTimedAwake(TimeSpan.FromMinutes(30))));
            itemTimers.DropDownItems.Add(new ToolStripMenuItem("1 Hour", null, (s, e) => StartTimedAwake(TimeSpan.FromHours(1))));
            itemTimers.DropDownItems.Add(new ToolStripMenuItem("2 Hours", null, (s, e) => StartTimedAwake(TimeSpan.FromHours(2))));
            itemTimers.DropDownItems.Add(new ToolStripSeparator());
            itemTimers.DropDownItems.Add(new ToolStripMenuItem("Custom Time...", null, CustomTime_Click));

            // Enable Until (opens the picker preset to "until specific time")
            var itemUntil = new ToolStripMenuItem("Enable Until...", null, EnableUntil_Click);

            // Options Submenu
            var itemOptions = new ToolStripMenuItem("Settings");
            itemJigglerMode = new ToolStripMenuItem("Keep Teams/Status Active (Jiggler)", null, ToggleJigglerMode_Click) { Checked = isJigglerMode };
            itemLidClose = new ToolStripMenuItem("Turn Off When Lid Closes", null, ToggleLidClose_Click) { Checked = disableOnLidClose };
            itemStartup = new ToolStripMenuItem("Start on Windows Startup", null, ToggleStartup_Click) { Checked = IsStartupEnabled() };

            itemOptions.DropDownItems.Add(itemJigglerMode);
            itemOptions.DropDownItems.Add(itemLidClose);
            itemOptions.DropDownItems.Add(new ToolStripSeparator());
            itemOptions.DropDownItems.Add(itemStartup);

            // Exit
            var itemExit = new ToolStripMenuItem("Exit", null, Exit_Click);

            // Build full context menu
            contextMenu.Items.Add(itemStatus);
            contextMenu.Items.Add(new ToolStripSeparator());
            contextMenu.Items.Add(itemToggle);
            contextMenu.Items.Add(itemTimers);
            contextMenu.Items.Add(itemUntil);
            contextMenu.Items.Add(itemOptions);
            contextMenu.Items.Add(new ToolStripSeparator());
            contextMenu.Items.Add(itemExit);
        }

        private void NotifyIcon_DoubleClick(object? sender, EventArgs e)
        {
            // Double click toggles between Indefinite Awake and Passive
            if (isAwakeActive)
            {
                SetAwakeState(false);
            }
            else
            {
                SetAwakeState(true);
            }
        }

        private void ToggleAwake_Click(object? sender, EventArgs e)
        {
            SetAwakeState(!isAwakeActive);
        }

        private void StartTimedAwake(TimeSpan duration)
        {
            sessionStartTime = DateTime.Now;
            expirationTime = sessionStartTime.Add(duration);
            hasExpiration = true;
            SetAwakeState(true);
        }

        private void CustomTime_Click(object? sender, EventArgs e)
        {
            using (var form = new CustomTimeForm())
            {
                if (form.ShowDialog() == DialogResult.OK)
                {
                    sessionStartTime = DateTime.Now;
                    expirationTime = form.SelectedEndTime;
                    hasExpiration = true;
                    SetAwakeState(true);
                }
            }
        }

        private void EnableUntil_Click(object? sender, EventArgs e)
        {
            using (var form = new CustomTimeForm(defaultToEndTime: true))
            {
                if (form.ShowDialog() == DialogResult.OK)
                {
                    sessionStartTime = DateTime.Now;
                    expirationTime = form.SelectedEndTime;
                    hasExpiration = true;
                    SetAwakeState(true);
                }
            }
        }

        private void ToggleJigglerMode_Click(object? sender, EventArgs e)
        {
            isJigglerMode = !isJigglerMode;
            itemJigglerMode.Checked = isJigglerMode;
            WriteSetting("JigglerMode", isJigglerMode);

            // Re-apply states if currently active
            if (isAwakeActive)
            {
                ApplySystemWakeState(true);
            }
        }

        private void ToggleLidClose_Click(object? sender, EventArgs e)
        {
            disableOnLidClose = !disableOnLidClose;
            itemLidClose.Checked = disableOnLidClose;
            WriteSetting("DisableOnLidClose", disableOnLidClose);
        }

        private void LidMonitor_LidStateChanged(bool lidOpen)
        {
            if (!lidOpen && disableOnLidClose && isAwakeActive)
            {
                SetAwakeState(false);
                notifyIcon.ShowBalloonTip(3000, "ReadEye", "Lid closed. Sleep prevention disabled.", ToolTipIcon.Info);
            }
        }

        private void SetAwakeState(bool active)
        {
            isAwakeActive = active;
            itemToggle.Checked = active;

            if (!active)
            {
                hasExpiration = false;
                expirationTime = DateTime.MinValue;
            }

            ApplySystemWakeState(active);
            UpdateTrayIconAndTooltip();
        }

        private void ApplySystemWakeState(bool active)
        {
            if (active)
            {
                // Always call the standard sleep prevention API first as it is cleanest
                NativeMethods.SetThreadExecutionState(
                    NativeMethods.EXECUTION_STATE.ES_CONTINUOUS |
                    NativeMethods.EXECUTION_STATE.ES_DISPLAY_REQUIRED |
                    NativeMethods.EXECUTION_STATE.ES_SYSTEM_REQUIRED
                );

                if (isJigglerMode)
                {
                    jigglerTimer.Start();
                    // Perform an immediate jiggle to start
                    NativeMethods.SimulateActivity();
                }
                else
                {
                    jigglerTimer.Stop();
                }
            }
            else
            {
                // Return execution state control back to OS defaults
                NativeMethods.SetThreadExecutionState(NativeMethods.EXECUTION_STATE.ES_CONTINUOUS);
                jigglerTimer.Stop();
            }
        }

        private void StateTimer_Tick(object? sender, EventArgs e)
        {
            try
            {
                if (isAwakeActive && hasExpiration)
                {
                    TimeSpan remaining = expirationTime - DateTime.Now;
                    if (remaining <= TimeSpan.Zero)
                    {
                        // Timer expired! Toggle inactive and inform user
                        SetAwakeState(false);
                        notifyIcon.ShowBalloonTip(3000, "ReadEye", "Awake timer completed. Sleep prevention disabled.", ToolTipIcon.Info);
                    }
                    else
                    {
                        UpdateTrayIconAndTooltip();
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"StateTimer_Tick error: {ex.Message}");
            }
        }

        private void JigglerTimer_Tick(object? sender, EventArgs e)
        {
            try
            {
                if (isAwakeActive && isJigglerMode)
                {
                    NativeMethods.SimulateActivity();
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"JigglerTimer_Tick error: {ex.Message}");
            }
        }

        private void UpdateTrayIconAndTooltip()
        {
            try
            {
                float progressPercentage = -1f;
                string statusText = "ReadEye: Passive";

                if (isAwakeActive)
                {
                    if (hasExpiration)
                    {
                        TimeSpan total = expirationTime - sessionStartTime;
                        TimeSpan remaining = expirationTime - DateTime.Now;

                        if (total.TotalSeconds > 0)
                        {
                            progressPercentage = (float)(remaining.TotalSeconds / total.TotalSeconds);
                            progressPercentage = Math.Clamp(progressPercentage, 0f, 1f);
                        }

                        // Format nicely: e.g. "1h 12m remaining"
                        string timeStr;
                        if (remaining.TotalHours >= 1)
                        {
                            timeStr = $"{(int)remaining.TotalHours}h {remaining.Minutes}m";
                        }
                        else if (remaining.TotalMinutes >= 1)
                        {
                            timeStr = $"{remaining.Minutes}m {remaining.Seconds}s";
                        }
                        else
                        {
                            timeStr = $"{remaining.Seconds}s";
                        }

                        statusText = $"ReadEye: Active ({timeStr} remaining)";
                    }
                    else
                    {
                        statusText = "ReadEye: Active (Indefinitely)";
                    }
                }

                // Update Status in menu item and tooltip
                itemStatus.Text = statusText;
                notifyIcon.Text = statusText;

                // Dynamic painting of tray icon
                using (Bitmap bitmap = new Bitmap(16, 16))
                {
                    using (Graphics g = Graphics.FromImage(bitmap))
                    {
                        g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
                        g.Clear(Color.Transparent);

                        if (isAwakeActive)
                        {
                            if (progressPercentage >= 0)
                            {
                                // Background ring (dim red background track)
                                using (Pen bgPen = new Pen(Color.FromArgb(60, 244, 67, 54), 1.5f))
                                {
                                    g.DrawEllipse(bgPen, 1, 1, 13, 13);
                                }
                                // Foreground active arc (vibrant cyan progress)
                                using (Pen activePen = new Pen(Color.FromArgb(0, 188, 212), 1.5f))
                                {
                                    float sweepAngle = 360f * progressPercentage;
                                    g.DrawArc(activePen, 1, 1, 13, 13, -90, sweepAngle);
                                }
                                // Central eye dot (vibrant red)
                                using (Brush dotBrush = new SolidBrush(Color.FromArgb(244, 67, 54)))
                                {
                                    g.FillEllipse(dotBrush, 5, 5, 6, 6);
                                }
                            }
                            else
                            {
                                // Indefinite Active state: Glowing red ring and center dot
                                using (Pen ringPen = new Pen(Color.FromArgb(244, 67, 54), 1.5f))
                                {
                                    g.DrawEllipse(ringPen, 1, 1, 13, 13);
                                }
                                using (Brush dotBrush = new SolidBrush(Color.FromArgb(244, 67, 54)))
                                {
                                    g.FillEllipse(dotBrush, 5, 5, 6, 6);
                                }
                            }
                        }
                        else
                        {
                            // Inactive state: Distinct solid gray eye outline and center dot (100% opacity)
                            // (Clearly visible on both dark and light taskbars, universally representing 'Passive')
                            using (Pen ringPen = new Pen(Color.FromArgb(160, 160, 160), 1.5f))
                            {
                                g.DrawEllipse(ringPen, 2, 2, 11, 11);
                            }
                            using (Brush dotBrush = new SolidBrush(Color.FromArgb(160, 160, 160)))
                            {
                                g.FillEllipse(dotBrush, 6, 6, 4, 4);
                            }
                        }
                    }

                    IntPtr newHIcon = bitmap.GetHicon();
                    Icon newIcon = Icon.FromHandle(newHIcon);

                    notifyIcon.Icon = newIcon;

                    // Safely clean up previous native icon handle to prevent leaks
                    if (currentIconHandle != IntPtr.Zero)
                    {
                        DestroyIcon(currentIconHandle);
                    }
                    currentIconHandle = newHIcon;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"UpdateTrayIconAndTooltip error: {ex.Message}");
            }
        }

        // --- Settings Persistence (HKCU\Software\ReadEye) ---

        private static bool ReadSetting(string name, bool defaultValue)
        {
            try
            {
                using (RegistryKey? key = Registry.CurrentUser.OpenSubKey(SettingsKeyPath, false))
                {
                    if (key?.GetValue(name) is int val)
                    {
                        return val != 0;
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Failed to read setting '{name}': {ex.Message}");
            }
            return defaultValue;
        }

        private static void WriteSetting(string name, bool value)
        {
            try
            {
                using (RegistryKey key = Registry.CurrentUser.CreateSubKey(SettingsKeyPath))
                {
                    key.SetValue(name, value ? 1 : 0, RegistryValueKind.DWord);
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Failed to write setting '{name}': {ex.Message}");
            }
        }

        // --- Windows Startup Registry Integration ---

        private bool IsStartupEnabled()
        {
            try
            {
                using (RegistryKey? key = Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Run", false))
                {
                    if (key != null)
                    {
                        object? val = key.GetValue("ReadEye");
                        return val != null;
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Failed to read registry: {ex.Message}");
            }
            return false;
        }

        private void ToggleStartup_Click(object? sender, EventArgs e)
        {
            bool current = IsStartupEnabled();
            bool target = !current;

            try
            {
                using (RegistryKey? key = Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Run", true))
                {
                    if (key != null)
                    {
                        if (target)
                        {
                            key.SetValue("ReadEye", $"\"{Application.ExecutablePath}\"");
                        }
                        else
                        {
                            key.DeleteValue("ReadEye", false);
                        }
                        itemStartup.Checked = target;
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Could not toggle startup behavior: {ex.Message}", "Registry Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void Exit_Click(object? sender, EventArgs e)
        {
            // Turn off sleep prevention
            SetAwakeState(false);

            // Hide NotifyIcon
            notifyIcon.Visible = false;
            notifyIcon.Dispose();

            // Destroy the last dynamic icon handle
            if (currentIconHandle != IntPtr.Zero)
            {
                DestroyIcon(currentIconHandle);
            }

            // Exit the message loop
            ExitThread();
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                stateTimer.Dispose();
                jigglerTimer.Dispose();
                contextMenu.Dispose();
                lidMonitor.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}
