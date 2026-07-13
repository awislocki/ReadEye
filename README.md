# ReadEye

ReadEye is a lightweight Windows system tray utility that runs entirely in the background, keeping your laptop from locking and preventing Microsoft Teams and Slack from marking you as "Away".

It ships in two implementations with identical features:

- **Native C++ (Win32)** - `ReadEye_Native.exe`, ~178 KB, ~5-12 MB memory, zero dependencies. The recommended version.
- **C# / .NET 10 (WinForms)** - `ReadEye.exe` / `ReadEye_SelfContained.exe`, ~25-50 MB memory.

Both feature a dynamic system tray icon that shows status and countdown progress at a glance, and share the same settings (registry: `HKCU\Software\ReadEye`).

---

## Features

- **Active by Default**: Launches straight into Active state with the jiggler enabled, so Teams/Slack stay green from the moment it starts.
- **Sleep Prevention**: Cleanly signals Windows via native APIs (SetThreadExecutionState) to prevent the display from turning off and the computer from locking.
- **Teams/Slack Status Keeper (Jiggler)**: Simulates a harmless, virtual keystroke (F15) every 50 seconds. This resets the OS idle timer that both Microsoft Teams and Slack use for away detection, keeping you active (green status) even under strict corporate policies. Enabled by default; toggle persists in the registry.
- **Turn Off When Lid Closes**: Subscribes to the Windows lid-switch power notification and automatically disables sleep prevention when you close the laptop, so it can actually sleep in your bag. Enabled by default; toggle in Settings.
- **Timed Keep-Awake**: 
  - Quick toggles for 15m, 30m, 1h, and 2h under "Enable For...".
  - "Enable Until..." opens the timer dialog preset to a target end time (e.g. Until 17:30).
  - A Custom Awake Timer to specify a custom duration (hours/minutes) or target end time.
- **Auto-Start**: Easily register the application to run automatically on Windows boot via a right-click settings option.
- **Dynamic Progress Icon**: Drawn programmatically in the system tray to visually communicate state:
  - **Dim Red Dot**: Passive state (standard Windows sleep rules apply).
  - **Bright Red Dot & Ring**: Active state (Indefinitely).
  - **Bright Red Dot with Cyan Arc**: Active state with a timer (the outer cyan progress ring empty/dwindles as the countdown expires).

---

## How to Run the App

All compiled single-file executables are located directly in the root of this project:

1. **ReadEye_Native.exe (~178 KB)** - *recommended*
   - **Native C++ Binary**: Statically linked Win32 application. Zero dependencies, smallest footprint (~5-12 MB memory), starts instantly on any 64-bit Windows machine. Single-instance guarded.
2. **ReadEye_SelfContained.exe (~52 MB)**
   - **Zero-Dependency .NET Bundle**: Contains the complete .NET runtime compressed inside the single binary. Runs out-of-the-box even if .NET is not installed.
3. **ReadEye.exe (~262 KB)**
   - **Lightweight .NET Binary**: Ultra-compact version that runs by sharing the host system's .NET runtime.

> [!NOTE]
> When you launch the application, it starts silently in the Windows System Tray (near the clock, you may need to click the chevron arrow to expand hidden icons). A notification toast will appear to confirm that the app is successfully running in the background.
>
> - Double-click the Red Eye icon: Toggles between Active (Indefinite) and Passive (Off) states.
> - Right-click the Red Eye icon: Opens the full settings menu.

---

## Build and Compilation Instructions

### Native C++ Version

Run `native\build.cmd`. It uses the ScopeCppSDK MSVC toolchain bundled with Visual Studio (no C++ workload install required) and outputs `ReadEye_Native.exe` to the repo root. An automated dialog test lives at `native\test_dialog.cpp`.

### .NET Version

If you want to compile the C# project from source, ensure you have the .NET 10 SDK installed:

### Compile Lightweight Binary
```powershell
dotnet publish -c Release -r win-x64 -p:PublishSingleFile=true --self-contained false
```
*Outputs to bin/Release/net10.0-windows/win-x64/publish/ReadEye.exe*

### Compile Self-Contained Binary (Compressed)
```powershell
dotnet publish -c Release -r win-x64 -p:PublishSingleFile=true -p:EnableCompressionInSingleFile=true --self-contained true
```
*Outputs to a single compressed executable at bin/Release/net10.0-windows/win-x64/publish/ReadEye.exe*

---

## Project Structure

### Native C++ Version
- **native/main.cpp**: The complete Win32 application - tray icon, GDI+ dynamic icon painting, context menu, timers, in-memory dialog template, lid-switch monitoring, registry settings.
- **native/build.cmd**: Build script using the VS-bundled ScopeCppSDK toolchain.
- **native/test_dialog.cpp**: Automated test that drives the custom time dialog and verifies time parsing.

### .NET Version
- **Program.cs**: Entry point setting up the high-DPI WinForms environment and launching the context message loop.
- **TrayApplicationContext.cs**: The core controller managing state, timers, context menus, registry operations, and GDI+ dynamic icon painting.
- **CustomTimeForm.cs**: A customized dark-theme modal for setting timers.
- **LidMonitor.cs**: Hidden window subscribing to lid-switch power notifications.
- **NativeMethods.cs**: P/Invoke imports for the Windows Sleep API (SetThreadExecutionState), Keyboard Input Simulator (SendInput), and power setting notifications.
