
#include "Common.h"
#include <cstdlib>


extern DRIVERCONTROL DriCtl;


BOOL CALLBACK ThreadListDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ModuleListDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);




static void UpdateListView(HWND hDlg, HWND hList, PENUMPROCINFO ProcList, int num)
{
    wchar_t buf[MAXPATH];

    ListView_DeleteAllItems(hList);
    for (int i=0; i<num; ++i)
    {
        LVITEM        lvI;
        RtlZeroMemory(&lvI, sizeof(LVITEM));
        lvI.iItem      = i;
        lvI.mask       = LVIF_TEXT;
        lvI.cchTextMax = MAXPATH;

        lvI.iSubItem = 0;
        MultiByteToWideChar(CP_ACP, 0, ProcList[i].ShortName, 20, buf, MAXPATH);
        lvI.pszText  = buf;
        ListView_InsertItem(hList, &lvI);

        lvI.iSubItem = 1;
        StringCchPrintf(buf, MAXPATH, L"%d", ProcList[i].Pid);
        lvI.pszText  = buf;
        ListView_SetItem(hList, &lvI);

        lvI.iSubItem = 2;
        lvI.pszText  = ProcList[i].ImageName;
        ListView_SetItem(hList, &lvI);

        lvI.iSubItem = 3;
        StringCchPrintf(buf, MAXPATH, L"%#llx", ProcList[i].eProcessAddr);
        lvI.pszText  = buf;
        ListView_SetItem(hList, &lvI);

        lvI.iSubItem = 4;
        StringCchPrintf(buf, MAXPATH, L"%d", ProcList[i].PPid);
        lvI.pszText  = buf;
        ListView_SetItem(hList, &lvI);
    }

    StringCchPrintf(buf, MAXPATH, L"进程 - 数量：%d", num);
    SetWindowText(hDlg, buf);
}


///////////////////////////////////////////////////////////////////////////////


BOOL CALLBACK ProcessListDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static const wchar_t ColumeTitle[][16] = {
        L"父进程ID",
        L"EPROCESS",
        L"程序路径",
        L"进程ID",
        L"进程映像名",
    };
    static HWND hList;
    static HMENU hMenu;
    static PENUMPROCINFO ProcList = 0;
    static int SelectedItem;

    int cx;


    switch (msg)
    {
        case WM_INITDIALOG:
        {    
            RECT rect;
            GetClientRect(hDlg, &rect);
            rect.left += 3;        rect.right  -= 3;
            rect.top  += 3;        rect.bottom -= 50;

            hList = CreateWindowEx(0, WC_LISTVIEW, NULL,
                LVS_REPORT | LVS_EDITLABELS | WS_CHILD | WS_VISIBLE |
                WS_BORDER | LVS_SHOWSELALWAYS,
                rect.left, rect.top,
                rect.right-rect.left, rect.bottom-rect.top,
                hDlg, (HMENU)ID_LISTVIEW5,
                (HINSTANCE)GetWindowLong(hDlg, GWL_HINSTANCE), NULL);


            ListView_SetExtendedListViewStyleEx(hList, 0,
                LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);


            LVCOLUMN lvc;
            lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
            lvc.fmt     = LVCFMT_LEFT;
            lvc.cchTextMax = MAXPATH;
            lvc.cx = 1;

            for (int i=0; i<5; ++i)
            {
                lvc.iSubItem = i;
                lvc.pszText = (wchar_t*)ColumeTitle[i];
                if (ListView_InsertColumn(hList, 0, &lvc) == -1)
                    LOG(L"规则列表初始化失败F", L"Init");
            }

        
            cx = rect.right-rect.left;
            ListView_SetColumnWidth(hList, 0, cx>>3);
            ListView_SetColumnWidth(hList, 1, cx>>3);
            ListView_SetColumnWidth(hList, 2, cx>>1);
            ListView_SetColumnWidth(hList, 3, cx>>3);
            ListView_SetColumnWidth(hList, 4, cx>>3);

        
            PostMessage(hDlg, WM_COMMAND, IDC_ENUM_REFRESH, 0);
            return TRUE;
        }
//-----------------------------------------------------------------------------

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDC_ENUM_REFRESH:
                {
                    int num = 0;
                    if (ProcList) VirtualFree(ProcList, 0, MEM_RELEASE);
                    if (DriCtl.GetProcList(&ProcList, &num))
                        UpdateListView(hDlg, hList, ProcList, num);
                    else
                        MessageBox(hDlg, L"进程列表获取失败", 0, MB_ICONERROR);
                    return TRUE;
                }

                case IDC_ENUM_THREAD_INFO:
                case IDC_ENUM_MODULE_INFO:
                {
                    wchar_t buf[16];
                    ListView_GetItemText(hList, SelectedItem, 1, buf, 16);

                    if (IDC_ENUM_THREAD_INFO == LOWORD(wParam))
                        DialogBoxParam((HINSTANCE)GetWindowLong(hDlg, GWL_HINSTANCE),
                            MAKEINTRESOURCE(IDD_THREADLIST), hDlg,
                            ThreadListDlgProc, (LPARAM)_wtoi(buf));
                    else
                        DialogBoxParam((HINSTANCE)GetWindowLong(hDlg, GWL_HINSTANCE),
                            MAKEINTRESOURCE(IDD_MODULELIST), hDlg,
                            ModuleListDlgProc, (LPARAM)_wtoi(buf));

                    return TRUE;
                }

                case IDC_ENUM_TERMINATE:
                {
                    wchar_t buf[16];
                    ListView_GetItemText(hList, SelectedItem, 1, buf, 16);

                    DriCtl.TerminateProcess(_wtoi(buf));
                    return TRUE;
                }
            }
            break;


//-----------------------------------------------------------------------------


        case WM_NOTIFY:
            if (NM_RCLICK == ((LPNMHDR)lParam)->code)
            {
                POINT pt;
                RECT rect;

                GetCursorPos(&pt);
                ScreenToClient(hDlg, &pt);

                GetClientRect(hDlg, &rect);
                if (pt.x > rect.left+3 && pt.x < rect.right-3
                    && pt.y > rect.top+7 && pt.y < rect.bottom-3)
                {
                    ClientToScreen(hDlg, &pt);
                    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hDlg, NULL);
                }
                return TRUE;
            }
            else if (NM_CLICK == ((LPNMHDR)lParam)->code)
            {
                LPNMITEMACTIVATE nmitem = (LPNMITEMACTIVATE)lParam;
                SelectedItem = nmitem->iItem;

                return TRUE;
            }
            break;

//-----------------------------------------------------------------------------

        case WM_CLOSE:
            EndDialog(hDlg, 0);
            return TRUE;
    }

    return FALSE;
}


///////////////////////////////////////////////////////////////////////////////


static void UpdateThreadListView(HWND hDlg, HWND hList, PENUMTHREADINFO ThreadList, int num)
{
    wchar_t buf[MAXPATH];

    ListView_DeleteAllItems(hList);
    for (int i=0; i<num; ++i)
    {
        LVITEM        lvI;
        RtlZeroMemory(&lvI, sizeof(LVITEM));
        lvI.iItem      = i;
        lvI.mask       = LVIF_TEXT;
        lvI.cchTextMax = MAXPATH;

        lvI.iSubItem = 0;
        StringCchPrintf(buf, MAXPATH, L"%d", ThreadList[i].Tid);
        lvI.pszText  = buf;
        ListView_InsertItem(hList, &lvI);

        lvI.iSubItem = 1;
        StringCchPrintf(buf, MAXPATH, L"%#llx", ThreadList[i].eThreadAddr);
        lvI.pszText  = buf;
        ListView_SetItem(hList, &lvI);

        lvI.iSubItem = 2;
        StringCchPrintf(buf, MAXPATH, L"%d", ThreadList[i].Priority);
        lvI.pszText  = buf;
        ListView_SetItem(hList, &lvI);
    }

    StringCchPrintf(buf, MAXPATH, L"线程 - 数量：%d", num);
    SetWindowText(hDlg, buf);
}


///////////////////////////////////////////////////////////////////////////////


BOOL CALLBACK ThreadListDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static const wchar_t ColumeTitle[][16] = {
        L"优先级",
        L"ETHREAD",
        L"TID"
    };

    static HWND hList;
    static int Pid, SelectedItem;

    switch (msg)
    {
        case WM_NOTIFY:
            if (NM_CLICK == ((LPNMHDR)lParam)->code)
            {
                LPNMITEMACTIVATE nmitem = (LPNMITEMACTIVATE)lParam;
                SelectedItem = nmitem->iItem;

                return TRUE;
            }
            break;

        case WM_INITDIALOG:
        {
            Pid = (int)lParam;

            RECT rect;
            GetClientRect(hDlg, &rect);
            rect.left += 3;        rect.right  -= 110;
            rect.top  += 3;        rect.bottom -= 3;

            hList = CreateWindowEx(0, WC_LISTVIEW, NULL,
                LVS_REPORT | LVS_EDITLABELS | WS_CHILD | WS_VISIBLE |
                WS_BORDER | LVS_SHOWSELALWAYS,
                rect.left, rect.top,
                rect.right-rect.left, rect.bottom-rect.top,
                hDlg, (HMENU)ID_LISTVIEW6,
                (HINSTANCE)GetWindowLong(hDlg, GWL_HINSTANCE), NULL);


            ListView_SetExtendedListViewStyleEx(hList, 0,
                LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);


            LVCOLUMN lvc;
            lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
            lvc.fmt     = LVCFMT_LEFT;
            lvc.cchTextMax = MAXPATH;
            lvc.cx = 1;

            for (int i=0; i<3; ++i)
            {
                lvc.iSubItem = i;
                lvc.pszText = (wchar_t*)ColumeTitle[i];
                if (ListView_InsertColumn(hList, 0, &lvc) == -1)
                    LOG(L"规则列表初始化失败F", L"Init");
            }

        
            int cx = rect.right - rect.left;
            ListView_SetColumnWidth(hList, 0, cx/4);
            ListView_SetColumnWidth(hList, 1, cx/3);
            ListView_SetColumnWidth(hList, 2, cx/3);

        
            PostMessage(hDlg, WM_COMMAND, IDC_ENUM_THREAD_REFRESH, 0);
            return TRUE;
        }


        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_ENUM_THREAD_REFRESH:
                {
                    PENUMTHREADINFO ThreadList = 0;
                    int num = 0;
                    DriCtl.GetThreadListofProcess(Pid, &ThreadList, &num);
                    UpdateThreadListView(hDlg, hList, ThreadList, num);
                    if (ThreadList)
                        VirtualFree(ThreadList, 0, MEM_RELEASE);
                    return TRUE;
                }

                case IDC_ENUM_THREAD_TERMINATE:
                {
                    wchar_t base[16];
                    ListView_GetItemText(hList, SelectedItem, 0, base, 16);
                    DriCtl.TerminateThread((HANDLE)wcstol(base, NULL, 10));
                    return TRUE;
                }
            }
            break;

        case WM_CLOSE:
            EndDialog(hDlg, 0);
            return TRUE;
    }

    return FALSE;
}


///////////////////////////////////////////////////////////////////////////////


static void UpdateModuleListView(HWND hDlg, HWND hList, PENUMMODULEINFO ModuleList, int num)
{
    wchar_t buf[MAXPATH];

    ListView_DeleteAllItems(hList);
    for (int i=0; i<num; ++i)
    {
        LVITEM        lvI;
        RtlZeroMemory(&lvI, sizeof(LVITEM));
        lvI.iItem      = i;
        lvI.mask       = LVIF_TEXT;
        lvI.cchTextMax = MAXPATH;

        lvI.iSubItem = 0;
        StringCchPrintf(buf, MAXPATH, L"%#llx", ModuleList[i].Base);
        lvI.pszText  = buf;
        ListView_InsertItem(hList, &lvI);

        lvI.iSubItem = 1;
        StringCchPrintf(buf, MAXPATH, L"0x%08x", ModuleList[i].Size);
        lvI.pszText  = buf;
        ListView_SetItem(hList, &lvI);

        lvI.iSubItem = 2;
        lvI.pszText  = ModuleList[i].Path;
        ListView_SetItem(hList, &lvI);
    }

    StringCchPrintf(buf, MAXPATH, L"模块 - 数量：%d", num);
    SetWindowText(hDlg, buf);
}


///////////////////////////////////////////////////////////////////////////////


unsigned long long simple_wcstoull(wchar_t *p)
{
    unsigned long long num = 0;

    if (!p) return num;

    if (*p == L'0' && (*(p+1) == L'X' || *(p+1) == L'x'))
        p += 2;

    for (wchar_t *tmp = p; *tmp; ++tmp)
        *tmp = towlower(*tmp);

    while (*p)
    {
        if (iswdigit(*p))
            num = num * 16 + *p - L'0';
        else
            num = num * 16 + *p - L'a' + 10;

        ++p;
    }

    return num;
}


///////////////////////////////////////////////////////////////////////////////


BOOL CALLBACK ModuleListDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static const wchar_t ColumeTitle[][16] = {
        L"模块名称",
        L"映像大小",
        L"模块基址"
    };

    static HWND hList;
    static int Pid;
    static int SelectedItem;

    switch (msg)
    {
        case WM_NOTIFY:
            if (NM_CLICK == ((LPNMHDR)lParam)->code)
            {
                LPNMITEMACTIVATE nmitem = (LPNMITEMACTIVATE)lParam;
                SelectedItem = nmitem->iItem;

                return TRUE;
            }
            break;

        case WM_INITDIALOG:
        {
            Pid = (int)lParam;

            RECT rect;
            GetClientRect(hDlg, &rect);
            rect.left += 3;        rect.right  -= 100;
            rect.top  += 3;        rect.bottom -= 3;

            hList = CreateWindowEx(0, WC_LISTVIEW, NULL,
                LVS_REPORT | LVS_EDITLABELS | WS_CHILD | WS_VISIBLE |
                WS_BORDER | LVS_SHOWSELALWAYS,
                rect.left, rect.top,
                rect.right-rect.left, rect.bottom-rect.top,
                hDlg, (HMENU)ID_LISTVIEW7,
                (HINSTANCE)GetWindowLong(hDlg, GWL_HINSTANCE), NULL);


            ListView_SetExtendedListViewStyleEx(hList, 0,
                LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);


            LVCOLUMN lvc;
            lvc.mask       = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
            lvc.fmt        = LVCFMT_LEFT;
            lvc.cchTextMax = MAXPATH;
            lvc.cx         = 1;

            for (int i=0; i<3; ++i)
            {
                lvc.iSubItem = i;
                lvc.pszText = (wchar_t*)ColumeTitle[i];
                if (ListView_InsertColumn(hList, 0, &lvc) == -1)
                    LOG(L"规则列表初始化失败F", L"Init");
            }

        
            int cx = rect.right - rect.left;
            ListView_SetColumnWidth(hList, 0, cx*3>>4);
            ListView_SetColumnWidth(hList, 1, cx*3>>4);
            ListView_SetColumnWidth(hList, 2, cx*10>>4);

        
            PostMessage(hDlg, WM_COMMAND, IDC_ENUM_MODULE_REFRESH, 0);
            return TRUE;
        }


        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_ENUM_MODULE_REFRESH:
                {
                    PENUMMODULEINFO ModuleList = 0;
                    int num = 0;
                    DriCtl.GetModuleListofProcess(Pid, &ModuleList, &num);
                    UpdateModuleListView(hDlg, hList, ModuleList, num);
                    if (ModuleList) VirtualFree(ModuleList, 0, MEM_RELEASE);
                    return TRUE;
                }

                case IDC_ENUM_MODULE_UNLOAD:
                {
                    wchar_t base[32];
                    ListView_GetItemText(hList, SelectedItem, 0, base, 32);
                    DriCtl.UnloadModule(Pid, simple_wcstoull(base));
                    return TRUE;
                }
                    break;
            }
            break;

        case WM_CLOSE:
            EndDialog(hDlg, 0);
            return TRUE;
    }

    return FALSE;
}