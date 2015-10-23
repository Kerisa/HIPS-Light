

#include "Common.h"




#pragma data_seg("Share")
bool bAdmin = FALSE;
#pragma data_seg()

#pragma comment(linker, "/SECTION:Share,RWS")


static const wchar_t PriorityName[34][64] = {
	SE_ASSIGNPRIMARYTOKEN_NAME,
	SE_AUDIT_NAME,
	SE_BACKUP_NAME,
	SE_CHANGE_NOTIFY_NAME,
	SE_CREATE_GLOBAL_NAME,
	SE_CREATE_PAGEFILE_NAME,
	SE_CREATE_PERMANENT_NAME,
	SE_CREATE_SYMBOLIC_LINK_NAME,
	SE_CREATE_TOKEN_NAME,
	SE_DEBUG_NAME,
	SE_ENABLE_DELEGATION_NAME,
	SE_IMPERSONATE_NAME,
	SE_INC_BASE_PRIORITY_NAME,
	SE_INCREASE_QUOTA_NAME,
	SE_INC_WORKING_SET_NAME,
	SE_LOAD_DRIVER_NAME,
	SE_LOCK_MEMORY_NAME,
	SE_MACHINE_ACCOUNT_NAME,
	SE_MANAGE_VOLUME_NAME,
	SE_PROF_SINGLE_PROCESS_NAME,
	SE_RELABEL_NAME,
	SE_REMOTE_SHUTDOWN_NAME,
	SE_RESTORE_NAME,
	SE_SECURITY_NAME,
	SE_SHUTDOWN_NAME,
	SE_SYNC_AGENT_NAME,
	SE_SYSTEM_ENVIRONMENT_NAME,
	SE_SYSTEM_PROFILE_NAME,
	SE_SYSTEMTIME_NAME,
	SE_TAKE_OWNERSHIP_NAME,
	SE_TCB_NAME,
	SE_TIME_ZONE_NAME,
	SE_TRUSTED_CREDMAN_ACCESS_NAME,
	SE_UNDOCK_NAME
};


///////////////////////////////////////////////////////////////////////////////
// 图片资源相关


PICTUREINFO PictureInfo[BMPNUM];


struct _PICTURE_POS_INFO{
	int DisplayIdx;		// 当前显示的图片索引(0:未选中的状态/1:选中的状态)
	RECT rc;			// 控件坐标
	int PicInfoIdx[2];	// 显示哪些图片,PictureInfo的索引
	
	int CtlId;
	HWND hCtl;
	
	int selected;			// 此控件(按钮)处于选中状态
	int numingroup;			// 与其同一组的其他控件数量
	int group[PICCTLNUM];	// 其他控件的索引(PicturePos中)
}PicturePos [PICCTLNUM] = {

	// 坐标和控件句柄没有初始化
	{0},	// Dummy

	// OFF & OFF_HIT
	// 初始假定OFF被选中
	{1, 0, 0, 0, 0, 3, 4, IDC_TURNOFF_PROC_MONITOR, 0, 1, 1, 5},
	{1, 0, 0, 0, 0, 3, 4, IDC_TURNOFF_FILE_MONITOR, 0, 1, 1, 6},
	{1, 0, 0, 0, 0, 3, 4, IDC_TURNOFF_REG_MONITOR, 0, 1, 1, 7},
	{1, 0, 0, 0, 0, 3, 4, IDC_TURNOFF_DISABLE_CREATE, 0, 1, 1, 8},

	// ON & ON_HIT
	{0, 0, 0, 0, 0, 5, 6, IDC_TURNON_PROC_MONITOR, 0, 0, 1, 1},
	{0, 0, 0, 0, 0, 5, 6, IDC_TURNON_FILE_MONITOR, 0, 0, 1, 2},
	{0, 0, 0, 0, 0, 5, 6, IDC_TURNON_REG_MONITOR, 0, 0, 1, 3},
	{0, 0, 0, 0, 0, 5, 6, IDC_TURNON_DISABLE_CREATE, 0, 0, 1, 4},

	// LOG & LOG_HIT
	{0, 0, 0, 0, 0, 1, 2, IDC_SHOWLOG},

	// PROCINFO & PROCINFO_HIT
	{0, 0, 0, 0, 0, 7, 8, IDC_QUERY_PROCESS_INFO},

	// RULE & RULE_HIT
	{0, 0, 0, 0, 0, 9, 10, IDC_RULES},

	// UNLOAD & UNLOAD_HIT
	{0, 0, 0, 0, 0, 11, 12, IDC_UNLOAD}
};


#define IsPointInRectxy(x, y, rect) \
	(x >= rect.left && x <= rect.right && \
	 y >= rect.top && y <= rect.bottom)

#define IsPointInRectpt(pt, rect) IsPointInRectxy((pt).x, (pt).y, rect)



void LoadImageFromExe(PPICTUREINFO pi, HINSTANCE Inst)
{
	for (int i=0; i<BMPNUM; ++i)
	{
		pi[i].hBitmap = LoadBitmap(Inst, BmpName[i]);
		GetObject(pi[i].hBitmap, sizeof(BITMAP), &pi[i].Bmp);
	}
	return;
}


void DeterminePicPos(HWND hwnd)
{
	PicturePos[1].hCtl = GetDlgItem(hwnd, IDC_TURNOFF_PROC_MONITOR);
	GetWindowRect(PicturePos[1].hCtl, &PicturePos[1].rc);

	PicturePos[2].hCtl = GetDlgItem(hwnd, IDC_TURNOFF_FILE_MONITOR);
	GetWindowRect(PicturePos[2].hCtl, &PicturePos[2].rc);

	PicturePos[3].hCtl = GetDlgItem(hwnd, IDC_TURNOFF_REG_MONITOR);
	GetWindowRect(PicturePos[3].hCtl, &PicturePos[3].rc);

	PicturePos[4].hCtl = GetDlgItem(hwnd, IDC_TURNOFF_DISABLE_CREATE);
	GetWindowRect(PicturePos[4].hCtl, &PicturePos[4].rc);


	PicturePos[5].hCtl = GetDlgItem(hwnd, IDC_TURNON_PROC_MONITOR);
	GetWindowRect(PicturePos[5].hCtl, &PicturePos[5].rc);

	PicturePos[6].hCtl = GetDlgItem(hwnd, IDC_TURNON_FILE_MONITOR);
	GetWindowRect(PicturePos[6].hCtl, &PicturePos[6].rc);

	PicturePos[7].hCtl = GetDlgItem(hwnd, IDC_TURNON_REG_MONITOR);
	GetWindowRect(PicturePos[7].hCtl, &PicturePos[7].rc);

	PicturePos[8].hCtl = GetDlgItem(hwnd, IDC_TURNON_DISABLE_CREATE);
	GetWindowRect(PicturePos[8].hCtl, &PicturePos[8].rc);


	PicturePos[9].hCtl = GetDlgItem(hwnd, IDC_SHOWLOG);
	GetWindowRect(PicturePos[9].hCtl, &PicturePos[9].rc);

	PicturePos[10].hCtl = GetDlgItem(hwnd, IDC_QUERY_PROCESS_INFO);
	GetWindowRect(PicturePos[10].hCtl, &PicturePos[10].rc);

	PicturePos[11].hCtl = GetDlgItem(hwnd, IDC_RULES);
	GetWindowRect(PicturePos[11].hCtl, &PicturePos[11].rc);

	PicturePos[12].hCtl = GetDlgItem(hwnd, IDC_UNLOAD);
	GetWindowRect(PicturePos[12].hCtl, &PicturePos[12].rc);


	for (int i=0; i<PICCTLNUM; ++i)
	{
		POINT pt;
		pt.x = PicturePos[i].rc.left;
		pt.y = PicturePos[i].rc.top;
		int cx   = PicturePos[i].rc.right  - PicturePos[i].rc.left;
		int cy   = PicturePos[i].rc.bottom - PicturePos[i].rc.top;

		ScreenToClient(hwnd, &pt);

		PicturePos[i].rc.left   = pt.x;
		PicturePos[i].rc.top    = pt.y;
		PicturePos[i].rc.right  = pt.x + cx;
		PicturePos[i].rc.bottom = pt.y + cy;
	}
}



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////



HINSTANCE g_Inst;
HWND      g_hMainDlg, g_hLogDlg, g_hRuleDlg;


DRIVERCONTROL DriCtl;
extern RULES Rules;

NTSUSPENDPROCESS NtSuspendProcess;
NTRESUMEPROCESS  NtResumeProcess;
NTTERMINATEPROCESS NtTerminateProcess;


BOOL CALLBACK DlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK LogDlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);


static BOOL EnablePrivilege(PCWSTR szPrivilege, BOOL fEnable)
{
   BOOL fOk = FALSE;
   HANDLE hToken;


   if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
   {
      TOKEN_PRIVILEGES tp;
      tp.PrivilegeCount = 1;
      LookupPrivilegeValue(NULL, szPrivilege, &tp.Privileges[0].Luid);
      tp.Privileges[0].Attributes = fEnable ? SE_PRIVILEGE_ENABLED : 0;
      AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
      fOk = (GetLastError() == ERROR_SUCCESS);


      CloseHandle(hToken);
   }
   return(fOk);
}


static bool EnablePrivilege()
{
	int ret = false;
	HANDLE hToken;
	PTOKEN_PRIVILEGES tp;

	int Size = sizeof(LUID_AND_ATTRIBUTES) * 3 + 4;
	tp = (PTOKEN_PRIVILEGES) malloc(Size);
	memset(tp, 0, Size);


	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		tp->PrivilegeCount = 3;
		for (DWORD i=0; i<tp->PrivilegeCount; ++i)
			tp->Privileges[i].Attributes = SE_PRIVILEGE_ENABLED;

		LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp->Privileges[0].Luid);
		LookupPrivilegeValue(NULL, SE_SECURITY_NAME, &tp->Privileges[1].Luid);
		LookupPrivilegeValue(NULL, SE_LOAD_DRIVER_NAME, &tp->Privileges[2].Luid);


		AdjustTokenPrivileges(hToken, FALSE, tp, Size, 0, 0);
		ret = (ERROR_SUCCESS == GetLastError());
	}
	
	free(tp);
	return ret;
}


static bool Cls_OnCommand(HWND hDlg, int id, HWND hwndCtl, UINT codeNotify)
{
	if (codeNotify != STN_CLICKED) return false;
	switch (codeNotify)
	{
	case STN_CLICKED:
		switch (id)
		{
		case IDC_TURNON_PROC_MONITOR:
			DriCtl.StartProcMonitor();
			break;

		case IDC_TURNON_FILE_MONITOR:
			DriCtl.StartFileMonitor();
			break;

		case IDC_TURNON_REG_MONITOR:
			DriCtl.StartRegMonitor();
			break;

		case IDC_TURNOFF_PROC_MONITOR:
			DriCtl.StopProcMonitor();
			break;

		case IDC_TURNOFF_FILE_MONITOR:
			DriCtl.StopFileMonitor();
			break;

		case IDC_TURNOFF_REG_MONITOR:
			DriCtl.StopRegMonitor();
			break;

		case IDC_SHOWLOG:
			ShowWindow(g_hLogDlg, SW_SHOWNORMAL);
			break;

		case IDC_RULES:
			ShowWindow(g_hRuleDlg, SW_SHOWNORMAL);
			break;

		case IDC_TURNON_DISABLE_CREATE:
			if (DriCtl.SwitchThreadAndProcCreate())
			{
				EnableWindow(GetDlgItem(hDlg, IDC_TURNON_DISABLE_CREATE), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_TURNOFF_DISABLE_CREATE), TRUE);
			}
			break;

		case IDC_TURNOFF_DISABLE_CREATE:
			if (DriCtl.SwitchThreadAndProcCreate())
			{
				EnableWindow(GetDlgItem(hDlg, IDC_TURNON_DISABLE_CREATE), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_TURNOFF_DISABLE_CREATE), FALSE);
			}
			break;

		case IDC_QUERY_PROCESS_INFO:
			DialogBoxParam(g_Inst, MAKEINTRESOURCE(IDD_PROCESSLIST), hDlg, ProcessListDlgProc, 0);
			break;

		case IDC_ENUMPROCESS:
			DialogBoxParam(g_Inst, MAKEINTRESOURCE(IDD_PROCESSLIST), hDlg, ProcessListDlgProc, 0);
			break;

		case IDC_UNLOAD:
			DriCtl.StopProcMonitor();
			DriCtl.StopFileMonitor();
			DriCtl.StopRegMonitor();

			DriCtl.CloseDevice();
		
			if (!DriCtl.UnloadDriver(L"ProcMon", 0, 0))
				LOG(L"进程驱动卸载失败！", L"Unload");

			if (!DriCtl.UnloadDriver(L"Myminifilter", 0, 0))
				LOG(L"文件驱动卸载失败！", L"Unload");
			
			if (!DriCtl.UnloadDriver(L"RegMon", 0, 0))
				LOG(L"注册表驱动卸载失败！", L"Unload");

			break;

		default: return false;
		}

		break;

	default: return false;
	}

	return true;
}


static void Cls_OnClose(HWND hDlg)
{
	DriCtl.StopProcMonitor();
	DriCtl.StopFileMonitor();
	DriCtl.StopRegMonitor();

	DriCtl.CloseDevice();


	if (!Rules.UpdateRulesToFile())
		LOG(L"更新规则库失败", 0);

	
	EndDialog(g_hLogDlg, 0);
	EndDialog(hDlg, 0);
}


static bool Cls_OnInitDialog(HWND hDlg, HWND hwndFocus, LPARAM lParam)
{
	g_hMainDlg = hDlg;
	
	HANDLE hIcon = LoadIcon((HINSTANCE)GetWindowLong(hDlg, GWL_HINSTANCE),MAKEINTRESOURCE(IDI_ICON1));

	SetClassLong(hDlg, GCL_HICON, (LONG)hIcon);

	SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)
		hIcon);//LoadIcon((HINSTANCE)GetWindowLong(hDlg, GWL_HINSTANCE),MAKEINTRESOURCE(IDI_ICON1)));
//		LoadImage((HINSTANCE)GetWindowLong(hDlg, GWL_HINSTANCE),
//		MAKEINTRESOURCE(IDI_ICON1),	IMAGE_ICON, 256, 256, LR_SHARED));


	g_hLogDlg = CreateDialog(g_Inst, MAKEINTRESOURCE(IDD_LOG), NULL, LogDlgProc);

	g_hRuleDlg  = CreateDialog(g_Inst, MAKEINTRESOURCE(IDD_RULES), g_hMainDlg, RulesDlgProc); 

	LoadImageFromExe(PictureInfo, g_Inst);
	DeterminePicPos(hDlg);

	if (!DriCtl.InstallProcessDriver())
	{
		EnableWindow(GetDlgItem(hDlg, IDC_TURNON_PROC_MONITOR), FALSE);
		LOG(L"Proc驱动初始化失败", L"Init");
	}

	if (!DriCtl.InstallRegistryDriver())
	{
		EnableWindow(GetDlgItem(hDlg, IDC_TURNON_REG_MONITOR), FALSE);
		LOG(L"Reg驱动初始化失败", L"Init");
	}

	if (!DriCtl.InstallFileDriver(
		FILEDEVNAME, FILEDRIVERPATH, ALTITUDE))
	{
		EnableWindow(GetDlgItem(hDlg, IDC_TURNON_FILE_MONITOR), FALSE);
		LOG(L"File驱动初始化失败", L"Init");
	}
	

	if (!Rules.Init(128, 128, 128))
		LOG(L"规则库初始化失败", L"Init");

	return true;
}


static void Cls_OnPaint(HWND hwnd)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	HDC hdcMem = CreateCompatibleDC(hdc);
	
	SelectObject(hdcMem, PictureInfo[0].hBitmap);
	BitBlt(hdc, 0, 0,
			PictureInfo[0].Bmp.bmWidth,
			PictureInfo[0].Bmp.bmHeight,
			hdcMem, 0, 0, SRCCOPY);

	POINT pt;
	GetCursorPos(&pt);
	ScreenToClient(hwnd, &pt);
	int x = pt.x, y = pt.y, i;

	for (i=1; i<PICCTLNUM; ++i)
	{
		SelectObject(hdcMem,
			PictureInfo[PicturePos[i].PicInfoIdx[PicturePos[i].DisplayIdx]].hBitmap);

		BitBlt(hdc, PicturePos[i].rc.left, PicturePos[i].rc.top,
			PicturePos[i].rc.right - PicturePos[i].rc.left,
			PicturePos[i].rc.bottom - PicturePos[i].rc.top,
			hdcMem, 0, 0, SRCCOPY);
	}
	EndPaint(hwnd, &ps);
	DeleteDC(hdcMem);
}


// x, y 为屏幕坐标
static void Cls_OnMouseMove(HWND hwnd, int x, int y, UINT keyFlags)
{
	HDC hdc = GetDC(hwnd);
	HDC hdcMem = CreateCompatibleDC(hdc);
	int i;

	for (i=1; i<=PICCTLNUM; ++i)
	{
		// 跳过处于打开状态的开关
		// 鼠标移到按钮上
		if (PicturePos[i].selected ||
		    IsPointInRectxy(x, y, PicturePos[i].rc))
		{
			if (PicturePos[i].selected ||
				0 == PicturePos[i].DisplayIdx)
			{
				SelectObject(hdcMem,
					PictureInfo[PicturePos[i].PicInfoIdx[1]].hBitmap);

				BitBlt(hdc, PicturePos[i].rc.left, PicturePos[i].rc.top,
					PicturePos[i].rc.right - PicturePos[i].rc.left,
					PicturePos[i].rc.bottom - PicturePos[i].rc.top,
					hdcMem, 0, 0, SRCCOPY);

				PicturePos[i].DisplayIdx = 1;
			}
		}

		// 鼠标从按钮上移开
		else if (1 == PicturePos[i].DisplayIdx)
		{
			SelectObject(hdcMem,
				PictureInfo[PicturePos[i].PicInfoIdx[0]].hBitmap);

			BitBlt(hdc, PicturePos[i].rc.left, PicturePos[i].rc.top,
				PicturePos[i].rc.right - PicturePos[i].rc.left,
				PicturePos[i].rc.bottom - PicturePos[i].rc.top,
				hdcMem, 0, 0, SRCCOPY);

			PicturePos[i].DisplayIdx = 0;
		}
	}

	DeleteDC(hdcMem);
	ReleaseDC(hwnd, hdc);
}


// x, y 为客户区坐标
static void Cls_OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
	for (int i=1; i<PICCTLNUM; ++i)
	{
		// 跳过处于打开状态(禁用)的开关
		if (PicturePos[i].selected)
			continue;

		// 鼠标在按钮上
		if (PicturePos[i].DisplayIdx &&
			IsPointInRectxy(x, y, PicturePos[i].rc))
		{
			PicturePos[i].DisplayIdx = 0;

			if (i < 9)	// 后面几个按钮不带状态所以不用改
			{
				PicturePos[i].DisplayIdx = 1;
				PicturePos[i].selected = 1;
				SendMessage(PicturePos[i].hCtl,	STM_SETIMAGE, IMAGE_BITMAP,
					(LPARAM)PictureInfo[PicturePos[i].PicInfoIdx[1]].hBitmap);

				for (int j=0; j<PicturePos[i].numingroup; ++j)
				{
					PicturePos[PicturePos[i].group[j]].selected = 0;	// 同组按钮改成未选中
					PicturePos[PicturePos[i].group[j]].DisplayIdx = 0;	// 显示未选中状态图片
					SendMessage(PicturePos[PicturePos[i].group[j]].hCtl,
						STM_SETIMAGE, IMAGE_BITMAP,
						(LPARAM)PictureInfo[PicturePos[PicturePos[i].group[j]].PicInfoIdx[0]].hBitmap);
				}
			}
			else
				InvalidateRect(PicturePos[i].hCtl, NULL, TRUE);
			Cls_OnCommand(hwnd, PicturePos[i].CtlId, PicturePos[i].hCtl, STN_CLICKED);
			break;
		}
	}
}


BOOL CALLBACK DlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static int CurCtlIdx;

	switch (Msg)
	{
		HANDLE_MSG(hDlg, WM_MOUSEMOVE,   Cls_OnMouseMove);
		HANDLE_MSG(hDlg, WM_PAINT,       Cls_OnPaint);
		HANDLE_MSG(hDlg, WM_LBUTTONDOWN, Cls_OnLButtonDown);
//		HANDLE_MSG(hDlg, WM_COMMAND,	 Cls_OnCommand);
		HANDLE_MSG(hDlg, WM_CLOSE,		 Cls_OnClose);
		HANDLE_MSG(hDlg, WM_INITDIALOG,  Cls_OnInitDialog);
		default:
			return false;
	}
}



int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pwCmdLine, int iShowCmd)
{
	HMODULE hNtdll;
	INITCOMMONCONTROLSEX icce;

#ifdef WIN7

	if (!bAdmin)
	{
		wchar_t szBuf[MAXPATH];
		SHELLEXECUTEINFO sei = {sizeof(SHELLEXECUTEINFO)};
		sei.lpVerb = L"runas";

		GetModuleFileName(hInstance, szBuf, MAXPATH);
		sei.lpFile = szBuf;

		sei.nShow = iShowCmd;
		sei.lpParameters = pwCmdLine;

		bAdmin = true;
		ShellExecuteEx(&sei);
		Sleep(500);
		ExitProcess(0);
	}

//	MessageBox(0, L"Wait Debugger...\n", L"Wait", 0);

	if (!EnablePrivilege())
		LOG(L"提权失败", L"debug");

#endif

	icce.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icce.dwICC  = ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icce);

	g_Inst = hInstance;
	
	hNtdll = LoadLibrary(L"ntdll");
	NtSuspendProcess   = (NTSUSPENDPROCESS)GetProcAddress(hNtdll, "ZwSuspendProcess");
	NtResumeProcess    = (NTRESUMEPROCESS)GetProcAddress(hNtdll, "NtResumeProcess");
	NtTerminateProcess = (NTTERMINATEPROCESS)GetProcAddress(hNtdll, "NtTerminateProcess");
	assert(NtSuspendProcess && NtResumeProcess && NtTerminateProcess);


	
	DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_MAIN), 0, DlgProc, 0);

	FreeLibrary(hNtdll);

	return 0;
}