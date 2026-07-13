// Automated test for the custom time dialog: verifies the in-memory dialog
// template is valid, the "until" radio preselects, and HH:MM parsing computes
// the right expiration. Build with test_build.cmd; exits 0 on pass.

#define wWinMain readeye_disabled_wWinMain
#include "main.cpp"
#undef wWinMain

#include <cstdio>
#include <process.h>

static unsigned __stdcall DriverThread(void*)
{
    HWND dlg = nullptr;
    for (int i = 0; i < 50 && !dlg; ++i)
    {
        Sleep(100);
        dlg = FindWindowW(nullptr, L"Custom Awake Timer - ReadEye");
    }
    if (!dlg)
    {
        printf("FAIL: dialog window not found (template invalid?)\n");
        ExitProcess(2);
    }

    LRESULT untilChecked = SendMessageW(GetDlgItem(dlg, IDC_RB_UNTIL), BM_GETCHECK, 0, 0);
    if (untilChecked != BST_CHECKED)
    {
        printf("FAIL: 'until' radio not preselected in Enable Until mode\n");
        ExitProcess(3);
    }

    SetDlgItemTextW(dlg, IDC_ED_TIME, L"23:45");
    SendMessageW(dlg, WM_COMMAND, IDOK, 0);
    return 0;
}

int main()
{
    HANDLE thread = (HANDLE)_beginthreadex(nullptr, 0, DriverThread, nullptr, 0, nullptr);
    if (!thread)
    {
        printf("FAIL: could not start driver thread\n");
        return 4;
    }

    time_t expiration = 0;
    bool ok = ShowTimerDialog(true, &expiration);
    WaitForSingleObject(thread, 2000);
    CloseHandle(thread);

    if (!ok)
    {
        printf("FAIL: dialog returned cancel\n");
        return 1;
    }

    struct tm lt;
    localtime_s(&lt, &expiration);
    bool future = difftime(expiration, time(nullptr)) > 0;
    printf("expiration: %04d-%02d-%02d %02d:%02d (future=%d)\n",
           lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, future);

    if (lt.tm_hour == 23 && lt.tm_min == 45 && future)
    {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL: expected 23:45 in the future\n");
    return 1;
}
