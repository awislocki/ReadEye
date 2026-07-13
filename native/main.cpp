// ReadEye Native - Win32 C++ port of the ReadEye system tray utility.
// Keeps the system awake and keeps Teams/Slack presence active via a
// simulated F15 keypress (resets the OS idle timer that both apps watch).
//
// Feature parity with the C# version:
//   - Active by default on launch with jiggler enabled
//   - Enable For (15m/30m/1h/2h) and Custom Time dialog
//   - Enable Until (dialog preset to "until specific time")
//   - Turn Off When Lid Closes (lid-switch power notification)
//   - Start on Windows Startup (HKCU Run key)
//   - Settings persisted in HKCU\Software\ReadEye (shared with C# version)
//   - Dynamic tray icon: gray eye (passive), red ring (active),
//     cyan countdown arc (timed)

#include <windows.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <ctime>
#include <cwchar>
#include <vector>

// ---------------------------------------------------------------- constants

static const wchar_t APP_NAME[] = L"ReadEye";
static const wchar_t WINDOW_CLASS[] = L"ReadEyeNativeWnd";
static const wchar_t MUTEX_NAME[] = L"ReadEye_Native_SingleInstance";
static const wchar_t SETTINGS_KEY[] = L"Software\\ReadEye";
static const wchar_t RUN_KEY[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

static const UINT WMAPP_TRAY = WM_APP + 1;

static const UINT_PTR TIMER_STATE = 1;   // 1s: expiration check + icon refresh
static const UINT_PTR TIMER_JIGGLE = 2;  // 50s: simulated input

enum MenuId : UINT
{
    IDM_STATUS = 100,
    IDM_TOGGLE,
    IDM_FOR15,
    IDM_FOR30,
    IDM_FOR60,
    IDM_FOR120,
    IDM_CUSTOM,
    IDM_UNTIL,
    IDM_JIGGLER,
    IDM_LIDCLOSE,
    IDM_STARTUP,
    IDM_EXIT
};

// Custom time dialog control ids
enum DlgId : WORD
{
    IDC_RB_DURATION = 1001,
    IDC_ED_HOURS,
    IDC_ED_MINS,
    IDC_RB_UNTIL,
    IDC_ED_TIME
};

static const GUID GUID_LIDSWITCH_STATE_CHANGE_LOCAL =
    { 0xBA3E0F4D, 0xB817, 0x4094, { 0xA2, 0xD1, 0xD5, 0x63, 0x79, 0xE6, 0xA0, 0xF3 } };

// ------------------------------------------------------------------- state

static HWND g_hwnd = nullptr;
static NOTIFYICONDATAW g_nid = {};
static HICON g_icon = nullptr;
static HPOWERNOTIFY g_lidNotify = nullptr;
static ULONG_PTR g_gdiplusToken = 0;
static UINT g_taskbarCreatedMsg = 0;

static bool g_awake = false;
static bool g_jiggler = true;
static bool g_lidOff = true;
static bool g_hasExpiration = false;
static time_t g_expiration = 0;
static time_t g_sessionStart = 0;

// ------------------------------------------------------------ registry i/o

static bool ReadSetting(const wchar_t* name, bool defaultValue)
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, SETTINGS_KEY, name, RRF_RT_REG_DWORD,
                     nullptr, &value, &size) == ERROR_SUCCESS)
    {
        return value != 0;
    }
    return defaultValue;
}

static void WriteSetting(const wchar_t* name, bool value)
{
    DWORD dw = value ? 1 : 0;
    RegSetKeyValueW(HKEY_CURRENT_USER, SETTINGS_KEY, name, REG_DWORD, &dw, sizeof(dw));
}

static bool IsStartupEnabled()
{
    DWORD size = 0;
    return RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, APP_NAME, RRF_RT_REG_SZ,
                        nullptr, nullptr, &size) == ERROR_SUCCESS;
}

static void ToggleStartup()
{
    if (IsStartupEnabled())
    {
        RegDeleteKeyValueW(HKEY_CURRENT_USER, RUN_KEY, APP_NAME);
    }
    else
    {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        wchar_t quoted[MAX_PATH + 2];
        swprintf(quoted, MAX_PATH + 2, L"\"%s\"", exePath);
        RegSetKeyValueW(HKEY_CURRENT_USER, RUN_KEY, APP_NAME, REG_SZ,
                        quoted, (DWORD)((wcslen(quoted) + 1) * sizeof(wchar_t)));
    }
}

// -------------------------------------------------------------- jiggle input

static void SimulateActivity()
{
    // F15: harmless key no application reacts to, but it resets the OS idle
    // timer that Teams and Slack use for away/idle detection.
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = 0x7E; // VK_F15
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 0x7E;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

// ------------------------------------------------------------- tray drawing

static void FormatStatus(wchar_t* buffer, size_t count, float* progressOut)
{
    *progressOut = -1.0f;

    if (!g_awake)
    {
        swprintf(buffer, count, L"ReadEye: Passive");
        return;
    }

    if (!g_hasExpiration)
    {
        swprintf(buffer, count, L"ReadEye: Active (Indefinitely)");
        return;
    }

    time_t now = time(nullptr);
    double remaining = difftime(g_expiration, now);
    double total = difftime(g_expiration, g_sessionStart);
    if (remaining < 0) remaining = 0;
    if (total > 0)
    {
        float p = (float)(remaining / total);
        *progressOut = p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
    }

    int totalSecs = (int)remaining;
    int hours = totalSecs / 3600;
    int mins = (totalSecs % 3600) / 60;
    int secs = totalSecs % 60;

    if (hours >= 1)
        swprintf(buffer, count, L"ReadEye: Active (%dh %dm remaining)", hours, mins);
    else if (mins >= 1)
        swprintf(buffer, count, L"ReadEye: Active (%dm %ds remaining)", mins, secs);
    else
        swprintf(buffer, count, L"ReadEye: Active (%ds remaining)", secs);
}

static HICON DrawTrayIcon(float progress)
{
    using namespace Gdiplus;

    Bitmap bmp(16, 16, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(0, 0, 0, 0));

    if (g_awake)
    {
        if (progress >= 0.0f)
        {
            // Timed: dim red track, cyan countdown arc, red center dot
            Pen bgPen(Color(60, 244, 67, 54), 1.5f);
            g.DrawEllipse(&bgPen, 1, 1, 13, 13);

            Pen arcPen(Color(255, 0, 188, 212), 1.5f);
            g.DrawArc(&arcPen, 1.0f, 1.0f, 13.0f, 13.0f, -90.0f, 360.0f * progress);

            SolidBrush dot(Color(255, 244, 67, 54));
            g.FillEllipse(&dot, 5, 5, 6, 6);
        }
        else
        {
            // Indefinite: red ring and dot
            Pen ring(Color(255, 244, 67, 54), 1.5f);
            g.DrawEllipse(&ring, 1, 1, 13, 13);

            SolidBrush dot(Color(255, 244, 67, 54));
            g.FillEllipse(&dot, 5, 5, 6, 6);
        }
    }
    else
    {
        // Passive: solid gray outline and dot, visible on light and dark taskbars
        Pen ring(Color(255, 160, 160, 160), 1.5f);
        g.DrawEllipse(&ring, 2, 2, 11, 11);

        SolidBrush dot(Color(255, 160, 160, 160));
        g.FillEllipse(&dot, 6, 6, 4, 4);
    }

    HICON icon = nullptr;
    bmp.GetHICON(&icon);
    return icon;
}

static void UpdateTray()
{
    float progress;
    wchar_t status[128];
    FormatStatus(status, 128, &progress);

    HICON newIcon = DrawTrayIcon(progress);
    if (newIcon)
    {
        g_nid.hIcon = newIcon;
    }
    wcsncpy_s(g_nid.szTip, status, _TRUNCATE);
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);

    if (newIcon)
    {
        if (g_icon) DestroyIcon(g_icon);
        g_icon = newIcon;
    }
}

static void ShowBalloon(const wchar_t* text)
{
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
    wcsncpy_s(g_nid.szInfo, text, _TRUNCATE);
    wcsncpy_s(g_nid.szInfoTitle, APP_NAME, _TRUNCATE);
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.szInfo[0] = L'\0';
}

// ------------------------------------------------------------ awake control

static void ApplyWakeState(bool active)
{
    if (active)
    {
        SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
        if (g_jiggler)
        {
            SetTimer(g_hwnd, TIMER_JIGGLE, 50000, nullptr);
            SimulateActivity();
        }
        else
        {
            KillTimer(g_hwnd, TIMER_JIGGLE);
        }
    }
    else
    {
        SetThreadExecutionState(ES_CONTINUOUS);
        KillTimer(g_hwnd, TIMER_JIGGLE);
    }
}

static void SetAwakeState(bool active)
{
    g_awake = active;
    if (!active)
    {
        g_hasExpiration = false;
        g_expiration = 0;
    }
    ApplyWakeState(active);
    UpdateTray();
}

static void StartTimedAwake(int minutes)
{
    g_sessionStart = time(nullptr);
    g_expiration = g_sessionStart + (time_t)minutes * 60;
    g_hasExpiration = true;
    SetAwakeState(true);
}

// -------------------------------------------------- custom time dialog (in-memory template)

struct TimerDialogParams
{
    bool defaultUntil;
    time_t expiration; // out
};

static void DlgAlign(std::vector<BYTE>& buf)
{
    while (buf.size() % 4 != 0) buf.push_back(0);
}

static void DlgAppend(std::vector<BYTE>& buf, const void* data, size_t len)
{
    const BYTE* p = (const BYTE*)data;
    buf.insert(buf.end(), p, p + len);
}

static void DlgWord(std::vector<BYTE>& buf, WORD w) { DlgAppend(buf, &w, 2); }
static void DlgDword(std::vector<BYTE>& buf, DWORD d) { DlgAppend(buf, &d, 4); }
static void DlgStr(std::vector<BYTE>& buf, const wchar_t* s)
{
    DlgAppend(buf, s, (wcslen(s) + 1) * sizeof(wchar_t));
}

static void DlgItem(std::vector<BYTE>& buf, DWORD style, short x, short y,
                    short cx, short cy, WORD id, WORD classAtom, const wchar_t* text)
{
    DlgAlign(buf);
    DlgDword(buf, style | WS_CHILD | WS_VISIBLE);
    DlgDword(buf, 0);           // exstyle
    DlgWord(buf, (WORD)x); DlgWord(buf, (WORD)y);
    DlgWord(buf, (WORD)cx); DlgWord(buf, (WORD)cy);
    DlgWord(buf, id);
    DlgWord(buf, 0xFFFF); DlgWord(buf, classAtom); // 0x0080 button, 0x0081 edit, 0x0082 static
    DlgStr(buf, text);
    DlgWord(buf, 0);            // no creation data
}

static void DlgEnableGroups(HWND hDlg)
{
    bool duration = IsDlgButtonChecked(hDlg, IDC_RB_DURATION) == BST_CHECKED;
    EnableWindow(GetDlgItem(hDlg, IDC_ED_HOURS), duration);
    EnableWindow(GetDlgItem(hDlg, IDC_ED_MINS), duration);
    EnableWindow(GetDlgItem(hDlg, IDC_ED_TIME), !duration);
}

static INT_PTR CALLBACK TimerDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        SetWindowLongPtrW(hDlg, DWLP_USER, lParam);
        TimerDialogParams* params = (TimerDialogParams*)lParam;

        CheckRadioButton(hDlg, IDC_RB_DURATION, IDC_RB_UNTIL,
                         params->defaultUntil ? IDC_RB_UNTIL : IDC_RB_DURATION);
        SetDlgItemInt(hDlg, IDC_ED_HOURS, 0, FALSE);
        SetDlgItemInt(hDlg, IDC_ED_MINS, 30, FALSE);

        // Default "until" time: one hour from now
        time_t later = time(nullptr) + 3600;
        struct tm lt;
        localtime_s(&lt, &later);
        wchar_t buf[8];
        swprintf(buf, 8, L"%02d:%02d", lt.tm_hour, lt.tm_min);
        SetDlgItemTextW(hDlg, IDC_ED_TIME, buf);

        DlgEnableGroups(hDlg);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_RB_DURATION:
        case IDC_RB_UNTIL:
            DlgEnableGroups(hDlg);
            return TRUE;
        case IDOK:
        {
            TimerDialogParams* params =
                (TimerDialogParams*)GetWindowLongPtrW(hDlg, DWLP_USER);
            time_t now = time(nullptr);

            if (IsDlgButtonChecked(hDlg, IDC_RB_DURATION) == BST_CHECKED)
            {
                BOOL ok = FALSE;
                int hours = (int)GetDlgItemInt(hDlg, IDC_ED_HOURS, &ok, FALSE);
                int mins = (int)GetDlgItemInt(hDlg, IDC_ED_MINS, nullptr, FALSE);
                if (hours == 0 && mins == 0)
                {
                    MessageBoxW(hDlg, L"Please select a duration greater than 0 minutes.",
                                L"Invalid Duration", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                params->expiration = now + (time_t)hours * 3600 + (time_t)mins * 60;
            }
            else
            {
                wchar_t buf[16] = {};
                GetDlgItemTextW(hDlg, IDC_ED_TIME, buf, 16);
                int hour = -1, min = -1;
                if (swscanf_s(buf, L"%d:%d", &hour, &min) != 2 ||
                    hour < 0 || hour > 23 || min < 0 || min > 59)
                {
                    MessageBoxW(hDlg, L"Please enter the time as HH:MM (24-hour).",
                                L"Invalid Time", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                struct tm lt;
                localtime_s(&lt, &now);
                lt.tm_hour = hour;
                lt.tm_min = min;
                lt.tm_sec = 0;
                time_t target = mktime(&lt);
                if (target <= now)
                {
                    lt.tm_mday += 1; // earlier than now: assume tomorrow
                    target = mktime(&lt);
                }
                params->expiration = target;
            }
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// Returns true and fills expirationOut when the user confirms a time.
static bool ShowTimerDialog(bool defaultUntil, time_t* expirationOut)
{
    std::vector<BYTE> t;

    // DLGTEMPLATE header
    DlgDword(t, DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU);
    DlgDword(t, 0);
    DlgWord(t, 9);              // item count
    DlgWord(t, 0); DlgWord(t, 0);       // x, y
    DlgWord(t, 190); DlgWord(t, 118);   // cx, cy (dialog units)
    DlgWord(t, 0);              // no menu
    DlgWord(t, 0);              // default dialog class
    DlgStr(t, L"Custom Awake Timer - ReadEye");
    DlgWord(t, 9);              // font size
    DlgStr(t, L"Segoe UI");

    DlgItem(t, BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP,
            10, 10, 120, 10, IDC_RB_DURATION, 0x0080, L"For a duration");
    DlgItem(t, SS_LEFT, 22, 28, 24, 8, (WORD)-1, 0x0082, L"Hours:");
    DlgItem(t, ES_NUMBER | WS_BORDER | WS_TABSTOP,
            48, 26, 28, 12, IDC_ED_HOURS, 0x0081, L"");
    DlgItem(t, SS_LEFT, 88, 28, 20, 8, (WORD)-1, 0x0082, L"Mins:");
    DlgItem(t, ES_NUMBER | WS_BORDER | WS_TABSTOP,
            110, 26, 28, 12, IDC_ED_MINS, 0x0081, L"");
    DlgItem(t, BS_AUTORADIOBUTTON | WS_TABSTOP,
            10, 48, 160, 10, IDC_RB_UNTIL, 0x0080, L"Until specific time (HH:MM, 24h)");
    DlgItem(t, WS_BORDER | WS_TABSTOP | WS_GROUP,
            22, 64, 40, 12, IDC_ED_TIME, 0x0081, L"");
    DlgItem(t, BS_DEFPUSHBUTTON | WS_TABSTOP | WS_GROUP,
            68, 92, 58, 14, IDOK, 0x0080, L"Start Awake");
    DlgItem(t, BS_PUSHBUTTON | WS_TABSTOP,
            132, 92, 48, 14, IDCANCEL, 0x0080, L"Cancel");

    TimerDialogParams params = { defaultUntil, 0 };
    SetForegroundWindow(g_hwnd);
    INT_PTR result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr),
                                             (LPCDLGTEMPLATEW)t.data(), g_hwnd,
                                             TimerDlgProc, (LPARAM)&params);
    if (result == IDOK)
    {
        *expirationOut = params.expiration;
        return true;
    }
    return false;
}

// -------------------------------------------------------------- context menu

static void ShowContextMenu()
{
    float progress;
    wchar_t status[128];
    FormatStatus(status, 128, &progress);

    HMENU timers = CreatePopupMenu();
    AppendMenuW(timers, MF_STRING, IDM_FOR15, L"15 Minutes");
    AppendMenuW(timers, MF_STRING, IDM_FOR30, L"30 Minutes");
    AppendMenuW(timers, MF_STRING, IDM_FOR60, L"1 Hour");
    AppendMenuW(timers, MF_STRING, IDM_FOR120, L"2 Hours");
    AppendMenuW(timers, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(timers, MF_STRING, IDM_CUSTOM, L"Custom Time...");

    HMENU settings = CreatePopupMenu();
    AppendMenuW(settings, MF_STRING | (g_jiggler ? MF_CHECKED : 0),
                IDM_JIGGLER, L"Keep Teams/Slack Active (Jiggler)");
    AppendMenuW(settings, MF_STRING | (g_lidOff ? MF_CHECKED : 0),
                IDM_LIDCLOSE, L"Turn Off When Lid Closes");
    AppendMenuW(settings, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(settings, MF_STRING | (IsStartupEnabled() ? MF_CHECKED : 0),
                IDM_STARTUP, L"Start on Windows Startup");

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | MF_GRAYED, IDM_STATUS, status);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (g_awake ? MF_CHECKED : 0), IDM_TOGGLE, L"Keep Awake");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)timers, L"Enable For...");
    AppendMenuW(menu, MF_STRING, IDM_UNTIL, L"Enable Until...");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)settings, L"Settings");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(g_hwnd); // required so the menu closes on outside click
    UINT cmd = (UINT)TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
                                    pt.x, pt.y, 0, g_hwnd, nullptr);
    DestroyMenu(menu);

    time_t expiration = 0;
    switch (cmd)
    {
    case IDM_TOGGLE:
        SetAwakeState(!g_awake);
        break;
    case IDM_FOR15: StartTimedAwake(15); break;
    case IDM_FOR30: StartTimedAwake(30); break;
    case IDM_FOR60: StartTimedAwake(60); break;
    case IDM_FOR120: StartTimedAwake(120); break;
    case IDM_CUSTOM:
    case IDM_UNTIL:
        if (ShowTimerDialog(cmd == IDM_UNTIL, &expiration))
        {
            g_sessionStart = time(nullptr);
            g_expiration = expiration;
            g_hasExpiration = true;
            SetAwakeState(true);
        }
        break;
    case IDM_JIGGLER:
        g_jiggler = !g_jiggler;
        WriteSetting(L"JigglerMode", g_jiggler);
        if (g_awake) ApplyWakeState(true);
        break;
    case IDM_LIDCLOSE:
        g_lidOff = !g_lidOff;
        WriteSetting(L"DisableOnLidClose", g_lidOff);
        break;
    case IDM_STARTUP:
        ToggleStartup();
        break;
    case IDM_EXIT:
        DestroyWindow(g_hwnd);
        break;
    }
}

// ---------------------------------------------------------------- wndproc

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WMAPP_TRAY:
        switch (LOWORD(lParam))
        {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowContextMenu();
            return 0;
        case WM_LBUTTONDBLCLK:
            SetAwakeState(!g_awake);
            return 0;
        }
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_STATE)
        {
            if (g_awake && g_hasExpiration)
            {
                if (difftime(g_expiration, time(nullptr)) <= 0)
                {
                    SetAwakeState(false);
                    ShowBalloon(L"Awake timer completed. Sleep prevention disabled.");
                }
                else
                {
                    UpdateTray();
                }
            }
        }
        else if (wParam == TIMER_JIGGLE)
        {
            if (g_awake && g_jiggler) SimulateActivity();
        }
        return 0;

    case WM_POWERBROADCAST:
        if (wParam == PBT_POWERSETTINGCHANGE && lParam != 0)
        {
            const POWERBROADCAST_SETTING* setting = (const POWERBROADCAST_SETTING*)lParam;
            if (IsEqualGUID(setting->PowerSetting, GUID_LIDSWITCH_STATE_CHANGE_LOCAL) &&
                setting->DataLength >= 1)
            {
                bool lidOpen = setting->Data[0] != 0;
                if (!lidOpen && g_lidOff && g_awake)
                {
                    SetAwakeState(false);
                    ShowBalloon(L"Lid closed. Sleep prevention disabled.");
                }
            }
        }
        return TRUE;

    case WM_DESTROY:
        SetThreadExecutionState(ES_CONTINUOUS);
        KillTimer(hwnd, TIMER_STATE);
        KillTimer(hwnd, TIMER_JIGGLE);
        if (g_lidNotify) UnregisterPowerSettingNotification(g_lidNotify);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        if (g_icon) DestroyIcon(g_icon);
        PostQuitMessage(0);
        return 0;

    default:
        // Re-add the tray icon if Explorer restarts
        if (msg == g_taskbarCreatedMsg && g_taskbarCreatedMsg != 0)
        {
            Shell_NotifyIconW(NIM_ADD, &g_nid);
            UpdateTray();
            return 0;
        }
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ----------------------------------------------------------------- winmain

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    // Single instance
    CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        return 0;
    }

    Gdiplus::GdiplusStartupInput gdiplusInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusInput, nullptr);

    g_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    // Load persisted settings (defaults: jiggler on, turn-off-on-lid-close on)
    g_jiggler = ReadSetting(L"JigglerMode", true);
    g_lidOff = ReadSetting(L"DisableOnLidClose", true);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS;
    RegisterClassW(&wc);

    // Hidden top-level window: message-only windows do not receive the
    // WM_POWERBROADCAST broadcasts needed for lid detection.
    g_hwnd = CreateWindowExW(0, WINDOW_CLASS, APP_NAME, WS_OVERLAPPED,
                             0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
    if (!g_hwnd) return 1;

    GUID lidGuid = GUID_LIDSWITCH_STATE_CHANGE_LOCAL;
    g_lidNotify = RegisterPowerSettingNotification(g_hwnd, &lidGuid,
                                                   DEVICE_NOTIFY_WINDOW_HANDLE);

    // Tray icon
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WMAPP_TRAY;
    g_icon = DrawTrayIcon(-1.0f);
    g_nid.hIcon = g_icon;
    wcsncpy_s(g_nid.szTip, L"ReadEye", _TRUNCATE);
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    SetTimer(g_hwnd, TIMER_STATE, 1000, nullptr);

    // Active by default so Teams/Slack stay awake immediately
    SetAwakeState(true);
    ShowBalloon(L"ReadEye is running and keeping your system awake.");

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Gdiplus::GdiplusShutdown(g_gdiplusToken);
    return (int)msg.wParam;
}
