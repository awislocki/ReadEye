# 👁️ ReadEye

**ReadEye** is a lightweight, modern Windows system tray utility built with C# and .NET 10.0 (WinForms). It runs entirely in the background, keeping your laptop from locking and preventing Microsoft Teams from marking you as "Away". 

It is designed to consume minimal resources (~25 MB memory) and features a dynamic system tray icon that shows status and countdown progress at a glance.

---

## ✨ Features

- **🔒 Sleep Prevention**: Cleanly signals Windows via native APIs (`SetThreadExecutionState`) to prevent the display from turning off and the computer from locking.
- **💬 Teams Status Keeper**: An optional **Jiggler Mode** simulates a harmless, virtual keystroke (`F15`) every 50 seconds. This resets the OS idle timer and keeps Microsoft Teams or Slack active (green status) even under strict corporate policies.
- **⏱️ Timed Keep-Awake**: 
  - Quick toggles for 15m, 30m, 1h, and 2h.
  - A modern dark-themed **Custom Awake Timer** to specify a custom duration (hours/minutes) or target end time (e.g. *Until 5:30 PM*).
- **🔄 Auto-Start**: Easily register the application to run automatically on Windows boot via a right-click settings option.
- **🎨 Dynamic Progress Icon**: Drawn programmatically in the system tray to visually communicate state:
  - ⚪ **Dim Red Dot**: Passive state (standard Windows sleep rules apply).
  - 🔴 **Bright Red Dot & Ring**: Active state (Indefinitely).
  - 🔴 **Bright Red Dot with Cyan Arc**: Active state with a timer (the outer cyan progress ring empty/dwindles as the countdown expires).

---

## 🚀 How to Run the App

Both compiled single-file executables are located directly in the root of this project:

1. **[`ReadEye_SelfContained.exe`](ReadEye_SelfContained.exe) (~52 MB)**
   - **Zero-Dependency Bundle**: Contains the complete .NET runtime compressed inside the single binary. It runs out-of-the-box on any 64-bit Windows machine, even if .NET is not installed.
2. **[`ReadEye.exe`](ReadEye.exe) (~258 KB)**
   - **Lightweight Binary**: Ultra-compact version that runs instantly by sharing the host system's .NET runtime.

> [!NOTE]
> When you launch the application, it starts silently in the **Windows System Tray** (near the clock, you may need to click the `^` chevron arrow to expand hidden icons). A notification toast will appear to confirm that the app is successfully running in the background.
>
> - **Double-Click** the Red Eye icon: Toggles between Active (Indefinite) and Passive (Off) states.
> - **Right-Click** the Red Eye icon: Opens the full settings menu.

---

## 🛠️ Build and Compilation Instructions

If you want to compile the project from source, ensure you have the .NET 10 SDK installed:

### Compile Lightweight Binary
```powershell
dotnet publish -c Release -r win-x64 -p:PublishSingleFile=true --self-contained false
```
*Outputs to `bin/Release/net10.0-windows/win-x64/publish/ReadEye.exe`*

### Compile Self-Contained Binary (Compressed)
```powershell
dotnet publish -c Release -r win-x64 -p:PublishSingleFile=true -p:EnableCompressionInSingleFile=true --self-contained true
```
*Outputs to a single compressed executable at `bin/Release/net10.0-windows/win-x64/publish/ReadEye.exe`*

---

## 🔍 Project Structure

- **[`Program.cs`](Program.cs)**: Entry point setting up the high-DPI WinForms environment and launching the context message loop.
- **[`TrayApplicationContext.cs`](TrayApplicationContext.cs)**: The core controller managing state, timers, context menus, registry operations, and GDI+ dynamic icon painting.
- **[`CustomTimeForm.cs`](CustomTimeForm.cs)**: A customized dark-theme modal for setting timers.
- **[`NativeMethods.cs`](NativeMethods.cs)**: P/Invoke imports for the Windows Sleep API (`SetThreadExecutionState`) and Keyboard Input Simulator (`SendInput`).
