
#include "Common.h"


extern HWND          g_hMainDlg;
extern HINSTANCE     g_Inst;
extern RULES         Rules;
extern DRIVERCONTROL DriCtl;


extern NTSUSPENDPROCESS   NtSuspendProcess;
extern NTRESUMEPROCESS    NtResumeProcess;
extern NTTERMINATEPROCESS NtTerminateProcess;



BOOL CALLBACK DisProcMsg(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);


PTP_WAIT g_tpWait;



static unsigned int WINAPI HandleProcInfo(PVOID pv)
{
    HANDLE hProc;
    PPROCCREATIONINFO pi = (PPROCCREATIONINFO)pv;
    int ret;

    if (!Rules.IsContain(pi->ParentProcImage, pi->ProcessImage, RULES::ProcType, &ret))
        ret = DialogBoxParam(g_Inst, MAKEINTRESOURCE(IDD_DISPROCMSG), 0, DisProcMsg, (LPARAM)pv);
    if (hProc = OpenProcess(PROCESS_SUSPEND_RESUME | PROCESS_TERMINATE,
        FALSE, pi->PID))
    {
        if (ret & 1)
            NtTerminateProcess(hProc, 0);
    
        else
            NtResumeProcess(hProc);        
    
        CloseHandle(hProc);
    }
    else
        LOG(L"进程打开失败", 0);

    pi->Result = ret & 1;

    AppendRecordToLog(pi, EnumProcType);

    if (ret & 2)    // 记录
    {
        Rules.AddRule(pi->ParentProcImage, pi->ProcessImage, ret&1, RULES::ProcType); 
    }

    delete pi;

    return 0;
}



unsigned int WINAPI WaitProcEvent(PVOID pv)
{
    DWORD R;

    while (DriCtl.ProcMonitoringOn)
    {
        PPROCCREATIONINFO ppci = new PROCCREATIONINFO;
        memset(ppci, 0, sizeof(PROCCREATIONINFO));

        WaitForSingleObject(DriCtl.hProcEvent, INFINITE);

        if (!DriCtl.ProcMonitoringOn) break;

        int ret = DeviceIoControl(DriCtl.hDevProc, IOCTL_GETPROCINFO, NULL, 0, ppci, sizeof(PROCCREATIONINFO), &R, 0);
        
        if(!ret || R != sizeof(PROCCREATIONINFO))
        {
            wchar_t buf[256];
            StringCchPrintf(buf, 256, L"ret=0x%08x, R=0x%08x", ret, R);
            LOG(buf, 0);
        }

#ifdef WINXP
        // WIN7下可以在驱动中方便地暂停进程了

        ppci->hProcess = OpenProcess(PROCESS_SUSPEND_RESUME | PROCESS_TERMINATE, FALSE, ppci->ProcessID);
        if (GetLastError())
        {
            wchar_t Buf[128];
            StringCchPrintf(Buf, 128, L"Error %d", GetLastError());
            LOG(Buf, L"Pause");
        }
        if (!NtSuspendProcess(ppci->hProcess));
            
        else
            LOG(L"暂停进程失败!", L"Pause");
#endif

        _beginthreadex(0, 0, HandleProcInfo, ppci, 0, 0);
    }
    
    return 0;
}







extern PICTUREINFO PictureInfo[BMPNUM];

BOOL CALLBACK DisProcMsg(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static const int CharsOneLine = 64;
    static PPROCCREATIONINFO pInfo;
    static HANDLE hProcInCtl;
    static wchar_t Buf[MAXPATH];
    PAINTSTRUCT ps;

    switch (msg)
    {
    case WM_PAINT:
        {
            HDC hdc = BeginPaint(hDlg, &ps);
            HDC hdcMem = CreateCompatibleDC(hdc);
            SelectObject(hdcMem, PictureInfo[13].hBitmap);
            
            RECT rect;
            GetClientRect(hDlg, &rect);

            StretchBlt(hdc, 0, 0, rect.right-rect.left, rect.bottom-rect.top,
                hdcMem, 0, 0,
                PictureInfo[13].Bmp.bmWidth, PictureInfo[13].Bmp.bmHeight,
                SRCCOPY);

            DeleteDC(hdcMem);
            EndPaint(hDlg, &ps);
        }
        break;

    case WM_CTLCOLORSTATIC:
            SetBkMode((HDC)wParam, TRANSPARENT);
            return (INT_PTR)GetStockObject(NULL_BRUSH);

    case WM_INITDIALOG:
        {
            pInfo = (PPROCCREATIONINFO)lParam;
            CheckDlgButton(hDlg, IDC_REMEMBER, FALSE);

            if (pInfo)
            {
                int cnt = wcslen(pInfo->ParentProcImage) / CharsOneLine + 1;
                wchar_t *src = pInfo->ParentProcImage, *dst = Buf;
                while (cnt--)
                {
                    int c = CharsOneLine;
                    while (c--) *dst++ = *src++;
                    *dst++ = L'\r'; *dst++ = L'\n';
                }
                SetDlgItemText(hDlg, IDC_PARINFO, Buf);

                cnt = wcslen(pInfo->ProcessImage) / CharsOneLine + 1;
                src = pInfo->ProcessImage;
                dst = Buf;
                while (cnt--)
                {
                    int c = CharsOneLine;
                    while (c--) *dst++ = *src++;
                    *dst++ = L'\r'; *dst++ = L'\n';
                }
                SetDlgItemText(hDlg, IDC_CHINFO, Buf);

                StringCchPrintf(Buf, MAXPATH, L" 父进程PID: %d, 子进程PID: %d",
                    pInfo->PPID, pInfo->PID);
                SetDlgItemText(hDlg, IDC_EXTRAINFO, Buf);
            }

            SetFocus(GetDlgItem(hDlg, IDC_DISALLOW));
        }
        break;


    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_ALLOW)
            EndDialog(hDlg, 0 | (IsDlgButtonChecked(hDlg, IDC_REMEMBER) << 1));
        
        else if (LOWORD(wParam) == IDC_DISALLOW)
            EndDialog(hDlg, 1 | (IsDlgButtonChecked(hDlg, IDC_REMEMBER) << 1));
        
        else break;

        return TRUE;


    case WM_CLOSE:
        EndDialog(hDlg, 1);
        return TRUE;
    }
    return FALSE;
}