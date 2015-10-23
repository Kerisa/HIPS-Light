
#include "Common.h"


extern HINSTANCE g_Inst;
extern RULES Rules;


static HWND  g_hLogList;
static HMENU hMenu;


bool AppendRecordToLog(const void *log, int Type)
{
	LVITEM		lvI;
	wchar_t		Buf[MAXPATH];

	RtlZeroMemory(&lvI, sizeof(LVITEM));
	

	lvI.iItem		= ListView_GetItemCount(g_hLogList);;			// 行索引
	lvI.mask		= LVIF_TEXT;
	lvI.cchTextMax	= MAXPATH;

	PPROCCREATIONINFO ppi = (PPROCCREATIONINFO) log;

	if (Type != EnumProcType && Type != EnumFileType && Type != EnumRegType)
		return false;

	__try
	{
		StringCchPrintf(Buf, 128, L"%04d/%02d/%02d %02d:%02d:%02d", ppi->Year, ppi->Month,
			ppi->Day, ppi->Hour, ppi->Minute, ppi->Second);

		lvI.iSubItem = 0;						// 列索引
		lvI.pszText  = Buf;						// 内容
		ListView_InsertItem(g_hLogList, &lvI);	// 先对subitem=0使用InsertItem

		if (Type == EnumProcType)
		{
			lvI.iSubItem = 1;
			lvI.pszText	 = ppi->ParentProcImage;		// 内容
			ListView_SetItem(g_hLogList, &lvI);			// 对subitem>0使用SetItem

			lvI.iSubItem = 2;
			lvI.pszText	 = L"运行可执行程序";
			ListView_SetItem(g_hLogList, &lvI);

			lvI.iSubItem = 3;
			lvI.pszText	 = ppi->ProcessImage;
			ListView_SetItem(g_hLogList, &lvI);

			lvI.iSubItem = 4;
			lvI.pszText	 = ppi->Result ? L"×" : L"√";	
			ListView_SetItem(g_hLogList, &lvI);
		}
		else if (Type == EnumFileType)
		{
			PFILEINFO pfi = (PFILEINFO) log;

			lvI.iSubItem = 1;
			lvI.pszText	 = pfi->ProcessName;
			ListView_SetItem(g_hLogList, &lvI);

			lvI.iSubItem = 2;
			lvI.pszText	 = L"修改文件";
			ListView_SetItem(g_hLogList, &lvI);

			lvI.iSubItem = 3;
			lvI.pszText	 = pfi->FileName;
			ListView_SetItem(g_hLogList, &lvI);

			lvI.iSubItem = 4;
			lvI.pszText	 = L"×"; //pfi->Result ? L"×" : L"√";	// 记录禁止修改的
			ListView_SetItem(g_hLogList, &lvI);
		}
		else if (Type == EnumRegType)
		{
			PREGINFO pri = (PREGINFO) log;

			lvI.iSubItem = 1;
			lvI.pszText	 = pri->ProcessName;
			ListView_SetItem(g_hLogList, &lvI);

			lvI.iSubItem = 2;
			lvI.pszText	 = L"修改注册表";
			ListView_SetItem(g_hLogList, &lvI);

			lvI.iSubItem = 3;
			lvI.pszText	 = pri->RegKeyName;
			ListView_SetItem(g_hLogList, &lvI);

			lvI.iSubItem = 4;
			lvI.pszText	 = pri->Result ? L"×" : L"√";
			ListView_SetItem(g_hLogList, &lvI);
		}

		return true;
	}
	__except(1)
	{
		return false;
	}

}


static bool Cls_OnInitDlg(HWND hDlg, HWND hwndFocus, LPARAM lParam)
{
	static const wchar_t szTitle [5][8] = {L"日期", L"应用程序", L"操作", L"目标", L"结果"};
	static const int iWidth[5] = {5, 10, 4, 11, 2};

	g_hLogList = CreateWindowEx(0, WC_LISTVIEW, NULL,
		LVS_REPORT | LVS_EDITLABELS | WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_SHOWSELALWAYS,
		0, 0, 50, 50, hDlg, (HMENU)ID_LISTVIEW1,
		g_Inst, NULL);


	SendMessage(g_hLogList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
		LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);
//-------------------------------------------------------------------------------------------------

	RECT rect;
	GetClientRect(hDlg, &rect);

	LVCOLUMN lvc;
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvc.fmt	 = LVCFMT_LEFT;
	for (int i=0; i<5; ++i)
	{
		lvc.iSubItem	= i;										// 索引
		lvc.cx			= (rect.right-rect.left) * iWidth[i] >> 5;	// 像素宽度
		lvc.pszText		= (PWSTR)szTitle[i];						// 显示文字
		lvc.cchTextMax	= sizeof(lvc.pszText) / sizeof(lvc.pszText[0]);		// 长度
		if (ListView_InsertColumn(g_hLogList, i, &lvc) == -1)		// 执行,失败返回-1
			LOG(L"日志列表初始化失败", L"Init");
	}

//-------------------------------------------------------------------------------------------------
	
	GetClientRect(hDlg, &rect);
	PostMessage(hDlg, WM_SIZE, SIZE_RESTORED,
		(long)(0xffff & (rect.right-rect.left)) | ((long)(0xffff & (rect.bottom-rect.top))) << 16);
	
	hMenu = LoadMenu(g_Inst, MAKEINTRESOURCE(IDR_LOGPOPUP));
	hMenu = GetSubMenu(hMenu, 0);

	return true;
}


static void Cls_OnSize(HWND hwnd, UINT state, int cx, int cy)
{
	static const int iWidth[5] = {5, 10, 4, 11, 2};
	RECT rect;
	
	GetClientRect(hwnd, &rect);

	rect.left	+= 3;
	rect.right	-= 3;
	rect.bottom -= 3;
	rect.top	+= 3;

	MoveWindow(g_hLogList, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, TRUE);

}

static void Cls_OnClose(HWND hDlg)
{
	ShowWindow(hDlg, SW_HIDE);
}


static bool CopyTextToClipboard(HWND hwnd)
{
	return false;
}


static bool Cls_OnCommand(HWND hDlg, int id, HWND hwndCtl, UINT codeNotify)
{
	switch (id)
	{
	case IDM_COPYTOCLIPBOARD:
		CopyTextToClipboard(hDlg);
		return true;
	}

	return false;
}


BOOL CALLBACK LogDlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
	case WM_NOTIFY:
		break;

		HANDLE_MSG(hDlg, WM_COMMAND,	Cls_OnCommand);
		HANDLE_MSG(hDlg, WM_SIZE,		Cls_OnSize);
		HANDLE_MSG(hDlg, WM_INITDIALOG, Cls_OnInitDlg);
		HANDLE_MSG(hDlg, WM_CLOSE,		Cls_OnClose);
	}
	return FALSE;
}