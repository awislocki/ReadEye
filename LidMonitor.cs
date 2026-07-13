using System;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace ReadEye
{
    /// <summary>
    /// Hidden window that subscribes to Windows lid-switch power notifications
    /// and raises an event when the laptop lid is opened or closed.
    /// </summary>
    internal sealed class LidMonitor : NativeWindow, IDisposable
    {
        private IntPtr notificationHandle = IntPtr.Zero;

        /// <summary>Raised on lid state change. Argument is true when the lid is open, false when closed.</summary>
        public event Action<bool>? LidStateChanged;

        public LidMonitor()
        {
            CreateHandle(new CreateParams());

            Guid lidGuid = NativeMethods.GUID_LIDSWITCH_STATE_CHANGE;
            notificationHandle = NativeMethods.RegisterPowerSettingNotification(
                this.Handle, ref lidGuid, NativeMethods.DEVICE_NOTIFY_WINDOW_HANDLE);

            if (notificationHandle == IntPtr.Zero)
            {
                // Desktops without a lid switch may fail registration; the monitor stays inert.
                System.Diagnostics.Debug.WriteLine("LidMonitor: RegisterPowerSettingNotification failed.");
            }
        }

        protected override void WndProc(ref Message m)
        {
            try
            {
                if (m.Msg == NativeMethods.WM_POWERBROADCAST &&
                    m.WParam == (IntPtr)NativeMethods.PBT_POWERSETTINGCHANGE &&
                    m.LParam != IntPtr.Zero)
                {
                    var setting = Marshal.PtrToStructure<NativeMethods.POWERBROADCAST_SETTING>(m.LParam);
                    if (setting.PowerSetting == NativeMethods.GUID_LIDSWITCH_STATE_CHANGE)
                    {
                        LidStateChanged?.Invoke(setting.Data != 0);
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"LidMonitor.WndProc error: {ex.Message}");
            }

            base.WndProc(ref m);
        }

        public void Dispose()
        {
            if (notificationHandle != IntPtr.Zero)
            {
                NativeMethods.UnregisterPowerSettingNotification(notificationHandle);
                notificationHandle = IntPtr.Zero;
            }

            if (this.Handle != IntPtr.Zero)
            {
                DestroyHandle();
            }
        }
    }
}
