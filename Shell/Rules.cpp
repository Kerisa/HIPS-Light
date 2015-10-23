
#include "Common.h"


extern DRIVERCONTROL DriCtl;


void create_crc_table();
unsigned int CRC32_4(const unsigned char* data, unsigned int reg, int len);
BOOL CALLBACK AddRuleProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);


bool RULES::Init(unsigned short cp, unsigned short cf, unsigned short cr)
{

	m_CapacityPi = cp;
	m_CapacityFi = cf;
	m_CapacityRi = cr;

	m_arrPi = (PPROCRULE)VirtualAlloc(NULL, sizeof(PROCRULE) * LIMIT,
		MEM_RESERVE | MEM_TOP_DOWN, PAGE_NOACCESS);
	m_arrFi = (PFILERULE)VirtualAlloc(NULL, sizeof(FILERULE) * LIMIT,
		MEM_RESERVE | MEM_TOP_DOWN, PAGE_NOACCESS);
	m_arrRi = (PREGRULE)VirtualAlloc(NULL, sizeof(REGRULE) * LIMIT,
		MEM_RESERVE | MEM_TOP_DOWN, PAGE_NOACCESS);

	if (!m_arrPi || !m_arrFi || !m_arrRi)
		return false;

	if (!VirtualAlloc(m_arrPi, cp * sizeof(PROCRULE), MEM_COMMIT, PAGE_READWRITE) ||
		!VirtualAlloc(m_arrFi, cf * sizeof(FILEINFO), MEM_COMMIT, PAGE_READWRITE) ||
		!VirtualAlloc(m_arrRi, cr * sizeof(REGINFO),  MEM_COMMIT, PAGE_READWRITE))
	{
		if (m_arrPi) VirtualFree(m_arrPi, 0, MEM_RELEASE);
		if (m_arrFi) VirtualFree(m_arrFi, 0, MEM_RELEASE);
		if (m_arrRi) VirtualFree(m_arrRi, 0, MEM_RELEASE);

		m_CapacityPi = m_CapacityFi = m_CapacityRi = 0;
		m_SizePi     = m_SizeFi     = m_SizeRi     = 0;
		m_arrPi = 0;
		m_arrFi = 0;
		m_arrRi = 0;
		return false;
	}

	return LoadRulesFromFile();
}


RULES::~RULES()
{
	if (m_arrPi)
	{
		for (int i=0; i<m_SizePi; ++i)
			for (PLIST p=m_arrPi[i].Header.next; p;)
			{
				PLIST del = p;
				p=p->next;
				delete del;
			}
		VirtualFree(m_arrPi, 0, MEM_RELEASE);
	}
	if (m_arrFi) VirtualFree(m_arrFi, 0, MEM_RELEASE);
	if (m_arrRi) VirtualFree(m_arrRi, 0, MEM_RELEASE);
}

bool RULES::AddRule(const wchar_t *launcher, const wchar_t *target, bool block, enum _Type t)
{
	bool ret = true;
	LVITEM lvI;

	RtlZeroMemory(&lvI, sizeof(LVITEM));
	lvI.mask		= LVIF_TEXT;
	lvI.cchTextMax	= MAXPATH;

	switch (t)
	{
	case ProcType:
		if (launcher && target && AddProcRule(launcher, target, block))
		{
			lvI.iItem = ListView_GetItemCount(m_hLVProc);

			lvI.iSubItem = 0;
			lvI.pszText  = (wchar_t*)launcher;
			ListView_InsertItem(m_hLVProc, &lvI);

			lvI.iSubItem = 1;
			lvI.pszText	 = (wchar_t*)target;
			ListView_SetItem(m_hLVProc, &lvI);

			lvI.iSubItem = 2;
			lvI.pszText	 = block ? L"阻止" : L"放行";
			ListView_SetItem(m_hLVProc, &lvI);

			m_IsProcRuleModified = true;
		}
		else
			ret = false;
		break;

	case FileType:
		if (target && AddFileRule(target, block))
		{
			lvI.iItem = ListView_GetItemCount(m_hLVFile);

			lvI.iSubItem = 0;
			lvI.pszText  = (wchar_t*)target;
			ListView_InsertItem(m_hLVFile, &lvI);

			lvI.iSubItem = 1;
			lvI.pszText  = block ? L"阻止" : L"放行";
			ListView_SetItem(m_hLVFile, &lvI);

			if (hFileFilterPort)
				ret = DriCtl.SendRuleToFileDriver(hFileFilterPort, target, block, false);

			m_IsFileRuleModified = true;
		}
		else
			ret = false;
		break;

	case RegType:
		if (target && AddRegRule(target, block))
		{
			lvI.iItem = ListView_GetItemCount(m_hLVReg);

			lvI.iSubItem = 0;
			lvI.pszText  = (wchar_t*)target;
			ListView_InsertItem(m_hLVReg, &lvI);

			lvI.iSubItem = 1;
			lvI.pszText  = block ? L"阻止" : L"放行";
			ListView_SetItem(m_hLVReg, &lvI);

			if (hRegDev)
				ret = DriCtl.SendRuleToRegDriver(hRegDev, target, block, false);

			m_IsRegRuleModified = true;
		}
		else
			ret = false;
		break;
	}

	
	return ret;
}


bool RULES::DeleteRule(enum _Type t)
{
	bool ret = true;
	wchar_t Launcher[MAXPATH];
	wchar_t Target[MAXPATH];
	wchar_t Block[8];

	switch (t)
	{
	case ProcType:		
		for (int i=ListView_GetItemCount(m_hLVProc)-1; i>=0; --i)
			if (ListView_GetCheckState(m_hLVProc, i))
			{
				ListView_GetItemText(m_hLVProc, i, 0, Launcher, MAXPATH);		// 根据布局不同可能要修改
				ListView_GetItemText(m_hLVProc, i, 1, Target, MAXPATH);
				if (DeleteProcRule(Launcher, Target))
				{
					ListView_DeleteItem(m_hLVProc, i);

					m_IsProcRuleModified = true;
				}
				else
					ret = false;
			}
		break;

	case FileType:		
		for (int i=ListView_GetItemCount(m_hLVFile)-1; i>=0; --i)
			if (ListView_GetCheckState(m_hLVFile, i))
			{
				ListView_GetItemText(m_hLVFile, i, 0, Target, MAXPATH);
				ListView_GetItemText(m_hLVFile, i, 1, Block, 8);
				
				if (DeleteFileRule(Target))
				{
					ListView_DeleteItem(m_hLVFile, i);
					if (!wcscmp(Block, L"阻止"))
						DriCtl.SendRuleToFileDriver(DriCtl.hFileFilterPort, Target, 1, true);
					else
						DriCtl.SendRuleToFileDriver(DriCtl.hFileFilterPort, Target, 0, true);

					m_IsFileRuleModified = true;
				}
				else
					ret = false;
			}
		break;

	case RegType:
		for (int i=ListView_GetItemCount(m_hLVReg)-1; i>=0; --i)
			if (ListView_GetCheckState(m_hLVReg, i))
			{
				ListView_GetItemText(m_hLVReg, i, 0, Target, MAXPATH);
				ListView_GetItemText(m_hLVReg, i, 1, Block, 8);

				if (DeleteRegRule(Target))
				{
					ListView_DeleteItem(m_hLVReg, i);
					if (!wcscmp(Block, L"阻止"))
						DriCtl.SendRuleToRegDriver(DriCtl.hDevReg, Target, 1, true);
					else
						DriCtl.SendRuleToRegDriver(DriCtl.hDevReg, Target, 0, true);

					m_IsRegRuleModified = true;
				}
				else
					ret = false;
			}
		break;
	}

	return ret;
}


bool RULES::AddProcRule(const wchar_t *launcher, const wchar_t *target, bool block)
{
	for (int i=0; i<m_SizePi; ++i)
	{
		if (!wcscmp(m_arrPi[i].Launcher, launcher))
		{
			for (PLIST p=m_arrPi[i].Header.next; p; p=p->next)
			{
				if (!wcscmp(p->Target, target))
					return false;		// 重复添加
			}

			
			PLIST node = new LIST;
			assert(node);
			node->Block = block;
			wcscpy_s(node->Target, MAXPATH, target);
				
			node->next = m_arrPi[i].Header.next;
			m_arrPi[i].Header.next = node;

			return true;
		}
	}

	if (m_SizePi >= m_CapacityPi)
	{
		if (m_CapacityPi * 2 <= LIMIT)
		{
			assert(VirtualAlloc(m_arrPi + m_CapacityPi,
				m_CapacityPi * sizeof(PROCRULE),
				MEM_COMMIT, PAGE_READWRITE));
			m_CapacityPi <<= 1;
		}
		else
		{
			LOG(L"超出容量", 0);
			return false;
		}
	}

	// 建新的项
	wcscpy_s(m_arrPi[m_SizePi].Launcher, MAXPATH, launcher);
	m_arrPi[m_SizePi].Header.next = new LIST;

	m_arrPi[m_SizePi].Header.next->Block = block;
	m_arrPi[m_SizePi].Header.next->next  = 0;
	wcscpy_s(m_arrPi[m_SizePi].Header.next->Target, MAXPATH, target);

	++m_SizePi;


	return true;
}




bool RULES::AddFileRule(const wchar_t *target, bool block)
{
	if (m_SizeFi >= m_CapacityFi)
	{
		if (m_CapacityFi * 2 <= LIMIT)
		{
			assert(VirtualAlloc(m_arrFi + m_CapacityFi,
						m_CapacityFi * sizeof(FILERULE),
						MEM_COMMIT, PAGE_READWRITE));
			m_CapacityFi <<= 1;
		}
		else
		{
			LOG(L"超出容量", 0);
			return false;
		}
	}

	
	wcscpy_s(m_arrFi[m_SizeFi].Target, MAXPATH, target);
	m_arrFi[m_SizeFi].Block = block;

	++m_SizeFi;
	

	return true;
}


bool RULES::AddRegRule(const wchar_t *target, bool block)
{
	if (m_SizeRi >= m_CapacityRi)
	{
		if (m_CapacityRi * 2 <= LIMIT)
		{
			VirtualAlloc(m_arrRi + m_CapacityRi, m_CapacityRi * sizeof(REGRULE), MEM_COMMIT, PAGE_READWRITE);
			m_CapacityRi <<= 1;
		}
		else
		{
			printf("超出容量");
			return false;
		}
	}
	__try{
		wcscpy_s(m_arrRi[m_SizeRi].Target, MAXPATH, target);
		m_arrRi[m_SizeRi].Block = block;

		++m_SizeRi;
	}
	__except(1) {
		wchar_t buf[1024];
		StringCchPrintf(buf, 1024, L"m_SizeRi:%d\ntarget:%s\nblock:%d", m_SizeRi,target,block);
		LOG(buf, L"debug");
		return false;
	}

	return true;
}
	

bool RULES::DeleteProcRule(const wchar_t *launcher, const wchar_t *target)
{
	for (int i=0; i<m_SizePi; ++i)
		if (!wcscmp(m_arrPi[i].Launcher, launcher))
		{
			for (PLIST p=&m_arrPi[i].Header; p->next; p=p->next)
				if (!wcscmp(p->next->Target, target))
				{
					PLIST tmp = p->next->next;
					delete p->next;
					p->next = tmp;
					
					if (!m_arrPi[i].Header.next)
					{
						for (int k=i; k<m_SizePi-1; ++k)
							m_arrPi[k] = m_arrPi[k+1];

						--m_SizePi;
					}
					return true;
				}
		}

	return false;
}


bool RULES::DeleteFileRule(const wchar_t *target)
{
	for (int i=0; i<m_SizeFi; ++i)
		if (!wcscmp(m_arrFi[i].Target, target))
		{
			for (int k=i; k<m_SizeFi-1; ++k)
				m_arrFi[k] = m_arrFi[k+1];

			--m_SizeFi;
			return true;
		}

	return false;
}


bool RULES::DeleteRegRule(const wchar_t *target)
{
	for (int i=0; i<m_SizeRi; ++i)
		if (!wcscmp(m_arrRi[i].Target, target))
		{
			for (int k=i; k<m_SizeRi-1; ++k)
				m_arrRi[k] = m_arrRi[k+1];

			--m_SizeRi;
			return true;
		}

	return false;
}


bool RULES::IsContainProc(const wchar_t *launcher, const wchar_t *target, int *pblock)
{
	for (int i=0; i<m_SizePi; ++i)
		if (!wcscmp(launcher, m_arrPi[i].Launcher))
		{
			for (PLIST p=m_arrPi[i].Header.next; p; p=p->next)
				if (!wcscmp(target, p->Target))
				{
					if (pblock)
						*pblock = p->Block;
					return true;
				}
			break;
		}

	return false;
}


bool RULES::IsContainFile(const wchar_t *target, int *pblock)
{
	for (int i=0; i<m_SizeFi; ++i)
		if (!wcscmp(target, m_arrFi[i].Target))
		{
			if (pblock)
				*pblock = m_arrFi[i].Block;
			return true;
		}

	return false;
}


bool RULES::IsContainReg(const wchar_t *target, int *pblock)
{
	for (int i=0; i<m_SizeRi; ++i)
		if (!wcscmp(target, m_arrRi[i].Target))
		{
			if (pblock)
				*pblock = m_arrRi[i].Block;
			return true;
		}

	return false;
}



bool RULES::IsContain(const wchar_t *launcher, const wchar_t *target, enum _Type t1, int *pblock)
{
	__try
	{
		switch (t1)
		{
		case ProcType:
			return IsContainProc(launcher, target, pblock);

		case FileType:
			return IsContainFile(target, pblock);

		case RegType:
			return IsContainReg(target, pblock);
		}
	}
	__except(1) { }

	return false;
}



bool RULES::CheckDataHash(void)
{
	return true;
}


unsigned int RULES::EncryptData(HANDLE hFile)
{
	DWORD R;

	if (hFile == INVALID_HANDLE_VALUE)
		return 0;

	SetFilePointer(hFile, 0, 0, FILE_BEGIN);

	unsigned long CRC32Result = 0;
	create_crc_table();
	
	unsigned long Size = GetFileSize(hFile, 0);
	unsigned char* data = (unsigned char*)VirtualAlloc(
		NULL, Size+7, MEM_COMMIT, PAGE_READWRITE);

	ReadFile(hFile, data, Size, &R, 0);

	CRC32Result = CRC32_4((unsigned char*)data, CRC32Result, Size);

	unsigned long key = CRC32Result ^ 0xCDCDCDCD;

	unsigned long *pp = (unsigned long*)data;
	for (int i=0; i<(Size+3)/4; ++i)
		pp[i] ^= key;


	SetFilePointer(hFile, 0, 0, FILE_BEGIN);
	WriteFile(hFile, data, Size, &R, 0);
	WriteFile(hFile, &CRC32Result, sizeof(unsigned long), &R, 0);
	SetEndOfFile(hFile);

	VirtualFree(data, 0, MEM_RELEASE);
	return 0;

}


bool RULES::DecryptData(unsigned char *data, unsigned long Size)
{
	unsigned long key = *(unsigned long *)(data + Size - 4) ^ 0xCDCDCDCD;
	unsigned long *pp = (unsigned long *)data;

	for (int i=0; i<(Size+3)/4; ++i)
		pp[i] ^= key;

	return true;
}

bool RULES::LoadRulesFromFile(void)		// 可以将其保存在注册表中
{
	static const wchar_t FilePath[] = L".\\Rules.dat";
	unsigned long R;

	HANDLE hFile = CreateFile(FilePath, GENERIC_READ, FILE_SHARE_READ, 0,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		LOG(L"规则文件加载失败", L"load");
		return false;
	}

	wchar_t *Buf = (wchar_t*)VirtualAlloc(NULL, GetFileSize(hFile, 0), MEM_COMMIT, PAGE_READWRITE);
	if (!Buf)
	{
		CloseHandle(hFile);
		return false;
	}

	unsigned long Size = GetFileSize(hFile, 0);
	wchar_t *q, *p = Buf, *BufEnd = (wchar_t*)((char*)Buf + Size - 4);  // 最后4字节不要

	ReadFile(hFile, Buf, Size, &R, 0);
	CloseHandle(hFile);

	DecryptData((unsigned char*)Buf, Size);

	if (!CheckDataHash())
	{
		LOG(L"规则文件数据错误", L"load");
		return false;
	}
	
	int len;
	for (; p<BufEnd;)	// Type: 1-Proc, 2-File, 3-Reg
	{
		switch (*(int*)p)
		{
		case 1:
			p += 2;
			q = p + 1 + wcslen(p);	// 指向下一个字符串
			len = wcslen(q);
			AddRule(p, q, *(int*)(q + len + 1), RULES::ProcType);

			p = q + len + 1 + 2;
			break;

		case 2:
			p += 2;
			len = wcslen(p);
			AddRule(NULL, p, *(int*)(p+len+1), RULES::FileType);

			p += len + 1 + 2;
			break;

		case 3:
			p += 2;
			len = wcslen(p);
			AddRule(NULL, p, *(int*)(p+len+1), RULES::RegType);

			p += len + 1 + 2;
			break;
		}
	}
	
	m_IsProcRuleModified = false;
	m_IsFileRuleModified = false;
	m_IsRegRuleModified  = false;
	VirtualFree(Buf, 0, MEM_RELEASE);

	return true;
}


bool RULES::UpdateRulesToFile(void)
{
	static const wchar_t OldFilePath[] = L".\\Rules.dat\0";
	static const wchar_t FilePath[] = L".\\Rules_000.dat\0";

	if (!m_IsProcRuleModified &&
		!m_IsFileRuleModified &&
		!m_IsRegRuleModified)
		return true;

	HANDLE hFile = CreateFile(FilePath, GENERIC_READ | GENERIC_WRITE,
		0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
	if (INVALID_HANDLE_VALUE == hFile)
		return false;

	int type = 1, block;
	DWORD R;
	for (int i=0; i<m_SizePi; ++i)
	{
		for (PLIST p=m_arrPi[i].Header.next; p; p=p->next)
		{
			WriteFile(hFile, &type, 4, &R, 0);
			WriteFile(hFile, m_arrPi[i].Launcher, 2*(1+wcslen(m_arrPi[i].Launcher)), &R, 0);
			
			WriteFile(hFile, p->Target, 2*(1+wcslen(p->Target)), &R, 0);
			block = p->Block;
			WriteFile(hFile, &block, 4, &R, 0);
		}
	}

	type = 2;
	for (int i=0; i<m_SizeFi; ++i)
	{
		WriteFile(hFile, &type, 4, &R, 0);
		WriteFile(hFile, m_arrFi[i].Target, 2*(1+wcslen(m_arrFi[i].Target)), &R, 0);
		block = m_arrFi[i].Block;
		WriteFile(hFile, &block, 4, &R, 0);
	}

	type = 3;
	for (int i=0; i<m_SizeRi; ++i)
	{
		WriteFile(hFile, &type, 4, &R, 0);
		WriteFile(hFile, m_arrRi[i].Target, 2*(1+wcslen(m_arrRi[i].Target)), &R, 0);
		block = m_arrRi[i].Block;
		WriteFile(hFile, &block, 4, &R, 0);
	}

	FlushFileBuffers(hFile);

	EncryptData(hFile);

	CloseHandle(hFile);

	
	// 重命名
	SHFILEOPSTRUCT shfo = {0};

	shfo.hwnd = 0;
	shfo.wFunc = FO_RENAME;
	shfo.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
	shfo.hNameMappings = 0;
	shfo.lpszProgressTitle = 0;
	shfo.pFrom = FilePath;
	shfo.pTo   = OldFilePath;

	if (SHFileOperation(&shfo))
	{
		LOG(L"规则库更新失败", L"update");
		DeleteFile(FilePath);
	}

	return true;
}




//*****************************************************************************

RULES Rules;

HMENU hMenu;

extern HINSTANCE g_Inst;

//*****************************************************************************
//
//   下面是规则库界面的代码
//
//*****************************************************************************

static bool Cls_OnInit(HWND hDlg, HWND hwndFocus, LPARAM lParam)
{
	static const wchar_t title[][6] = {L"应用程序", L"目标", L"操作"};

	HWND hTab = CreateWindow(WC_TABCONTROL, L"",
		WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, 0, 0, 1, 1,
		hDlg, NULL, g_Inst, NULL);

	SetWindowLong(hDlg, GWL_USERDATA, (LONG)hTab);


	TCITEM tie;
	tie.mask = TCIF_TEXT | TCIF_IMAGE;
	tie.iImage = -1;

	tie.pszText = L"进程规则"; 
	TabCtrl_InsertItem(hTab, 0, &tie);
	tie.pszText = L"文件规则";
	TabCtrl_InsertItem(hTab, 1, &tie);
	tie.pszText = L"注册表规则";
	TabCtrl_InsertItem(hTab, 2, &tie);

	Rules.m_hLVProc = CreateWindowEx(0, WC_LISTVIEW, NULL,
		LVS_REPORT | LVS_EDITLABELS | WS_CHILD | WS_VISIBLE | WS_BORDER |
		LVS_SHOWSELALWAYS ,
		0, 0, 50, 50, hDlg, (HMENU)ID_LISTVIEW2,
		g_Inst, NULL);

	SendMessage(Rules.m_hLVProc, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
		LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);


	Rules.m_hLVFile = CreateWindowEx(0, WC_LISTVIEW, NULL,
		LVS_REPORT | LVS_EDITLABELS | WS_CHILD | WS_BORDER | LVS_SHOWSELALWAYS,
		0, 0, 50, 50, hDlg, (HMENU)ID_LISTVIEW3,
		g_Inst, NULL);

	SendMessage(Rules.m_hLVFile, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
		LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);


	Rules.m_hLVReg = CreateWindowEx(0, WC_LISTVIEW, NULL,
		LVS_REPORT | LVS_EDITLABELS | WS_CHILD | WS_BORDER | LVS_SHOWSELALWAYS,
		0, 0, 50, 50, hDlg, (HMENU)ID_LISTVIEW4,
		g_Inst, NULL);

	SendMessage(Rules.m_hLVReg, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
		LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);


	LVCOLUMN lvc;
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvc.fmt	 = LVCFMT_LEFT;
	lvc.cchTextMax = MAXPATH;
	lvc.cx = 1;

	lvc.iSubItem = 0;
	lvc.pszText = L"目标文件列表";
	if (ListView_InsertColumn(Rules.m_hLVFile, 0, &lvc) == -1)
			LOG(L"规则列表初始化失败F", L"Init");

	lvc.iSubItem = 1;
	lvc.pszText = L"操作";
	if (ListView_InsertColumn(Rules.m_hLVFile, 1, &lvc) == -1)
			LOG(L"规则列表初始化失败F", L"Init");


	lvc.iSubItem = 0;
	lvc.pszText = L"注册表键值列表";
	if (ListView_InsertColumn(Rules.m_hLVReg, 0, &lvc) == -1)
			LOG(L"规则列表初始化失败R", L"Init");

	lvc.iSubItem = 1;
	lvc.pszText = L"操作";
	if (ListView_InsertColumn(Rules.m_hLVReg, 1, &lvc) == -1)
			LOG(L"规则列表初始化失败R", L"Init");


	for (int i=0; i<3; ++i)
	{
		lvc.iSubItem	= i;
		lvc.pszText		= (wchar_t*)title[i];
		if (ListView_InsertColumn(Rules.m_hLVProc, i, &lvc) == -1)
			LOG(L"规则列表初始化失败P", L"Init");
	}
		
	RECT rect;
	GetClientRect(hDlg, &rect);
	PostMessage(hDlg, WM_SIZE, SIZE_RESTORED,
		(long)(0xffff & (rect.right-rect.left)) |
		((long)(0xffff & (rect.bottom-rect.top))) << 16);

	hMenu = LoadMenu(g_Inst, MAKEINTRESOURCE(IDR_POPUP));
	hMenu = GetSubMenu(hMenu, 0);
	return true;
}


static void Cls_OnSize(HWND hDlg, UINT state, int cx, int cy)
{
	RECT rect;

	GetClientRect(hDlg, &rect);

	rect.left	+= 7;
	rect.right	-= 7;
	rect.bottom -= 7;
	rect.top	+= 4;

	MoveWindow((HWND)GetWindowLong(hDlg, GWL_USERDATA), rect.left, rect.top,
		rect.right-rect.left, 22+rect.top, FALSE);

	ListView_SetColumnWidth(Rules.m_hLVProc, 0, (rect.right-rect.left)*7/15);
	ListView_SetColumnWidth(Rules.m_hLVProc, 1, (rect.right-rect.left)*7/15);
	ListView_SetColumnWidth(Rules.m_hLVProc, 2, (rect.right-rect.left)/15);

	ListView_SetColumnWidth(Rules.m_hLVFile, 0, (rect.right-rect.left)*15>>4);
	ListView_SetColumnWidth(Rules.m_hLVFile, 1, (rect.right-rect.left)>>4);

	ListView_SetColumnWidth(Rules.m_hLVReg, 0, (rect.right-rect.left)*15>>4);
	ListView_SetColumnWidth(Rules.m_hLVReg, 1, (rect.right-rect.left)>>4);


	rect.top    += 25;

	MoveWindow(Rules.m_hLVProc, rect.left, rect.top, rect.right-rect.left,
		rect.bottom-rect.top, TRUE);

	MoveWindow(Rules.m_hLVFile, rect.left, rect.top, rect.right-rect.left,
		rect.bottom-rect.top, TRUE);

	MoveWindow(Rules.m_hLVReg, rect.left, rect.top, rect.right-rect.left,
		rect.bottom-rect.top, TRUE);

}


static void Cls_OnClose(HWND hDlg)
{
	ShowWindow(hDlg, SW_HIDE);
}


static bool Cls_OnCommand(HWND hDlg, int id, HWND hwndCtrl, UINT codeNotify)
{
	switch (id)
	{
	case IDM_ADD:		
		DialogBoxParam(g_Inst, MAKEINTRESOURCE(IDD_ADDRULE), hDlg, AddRuleProc,
			(LPARAM)TabCtrl_GetCurSel((HWND)GetWindowLong(hDlg, GWL_USERDATA)));

		return true;

	case IDM_DELSEL:
		enum RULES::_Type t;
		
		int cur = TabCtrl_GetCurSel((HWND)GetWindowLong(hDlg, GWL_USERDATA));
		switch (cur)
		{
		case 0:
			t = RULES::ProcType;
			
			break;

		case 1:
			t = RULES::FileType; break;
		case 2:
			t = RULES::RegType;  break;
		
		default: return false;
		}

		if (!Rules.DeleteRule(t))
			MessageBox(hDlg, L"删除失败", L"提示", MB_ICONWARNING);
		return true;
	}

	return false;
}


BOOL CALLBACK RulesDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND hTab;
	switch (msg)
	{
	case WM_NOTIFY:
		if (TCN_SELCHANGE == ((LPNMHDR)lParam)->code &&
			(hTab = (HWND)GetWindowLong(hDlg, GWL_USERDATA)) == ((LPNMHDR)lParam)->hwndFrom)
		{
			switch (TabCtrl_GetCurSel(hTab))
			{
			case 0:
				ShowWindow(Rules.m_hLVProc, SW_SHOWNORMAL);
				ShowWindow(Rules.m_hLVFile, SW_HIDE);
				ShowWindow(Rules.m_hLVReg, SW_HIDE);
				break;

			case 1:
				ShowWindow(Rules.m_hLVProc, SW_HIDE);
				ShowWindow(Rules.m_hLVFile, SW_SHOWNORMAL);
				ShowWindow(Rules.m_hLVReg, SW_HIDE);
				break;

			case 2:
				ShowWindow(Rules.m_hLVProc, SW_HIDE);
				ShowWindow(Rules.m_hLVFile, SW_HIDE);
				ShowWindow(Rules.m_hLVReg, SW_SHOWNORMAL);
				break;
			}

			return TRUE;
		}
		else if (NM_RCLICK == ((LPNMHDR)lParam)->code)
		{
			POINT pt;
			RECT rect;

			GetCursorPos(&pt);
			ScreenToClient(hDlg, &pt);

			GetClientRect(hDlg, &rect);
			if (pt.x > rect.left + 7 && pt.x < rect.right-7
				&& pt.y > rect.top + 25 && pt.y < rect.bottom-7)
			{
				ClientToScreen(hDlg, &pt);
				TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hDlg, NULL);
			}
			break;
		}
		break;

		HANDLE_MSG(hDlg, WM_COMMAND, Cls_OnCommand);
		HANDLE_MSG(hDlg, WM_INITDIALOG, Cls_OnInit);
		HANDLE_MSG(hDlg, WM_SIZE, Cls_OnSize);
		HANDLE_MSG(hDlg, WM_CLOSE, Cls_OnClose);
	default:
		break;
	}
	return FALSE;
}



BOOL CALLBACK AddRuleProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static enum RULES::_Type type;

	switch (msg)
	{
	case WM_INITDIALOG:
		CheckDlgButton(hDlg, IDC_RADIO2, TRUE);
		CheckDlgButton(hDlg, IDC_RADIO1, FALSE);

		switch ((int)lParam)
		{
		case 0:
			type = RULES::ProcType;
			SetFocus(GetDlgItem(hDlg, IDC_EDIT1));
			break;

		case 1:
		case 2:
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT1), FALSE);
//			EnableWindow(GetDlgItem(hDlg, IDC_RADIO1), FALSE);
//			EnableWindow(GetDlgItem(hDlg, IDC_RADIO2), FALSE);
			
			if (lParam == 1)
			{
				SetDlgItemText(hDlg, IDC_TITLE2, L"目标文件：");
				type = RULES::FileType;
			}
			else
			{
				SetDlgItemText(hDlg, IDC_TITLE2, L"目标键值：");
				type = RULES::RegType;
			}
			
			SetFocus(GetDlgItem(hDlg, IDC_EDIT2));
			break;

		default:
			LOG(L"类型错误", L"Type");
			EndDialog(hDlg, -1);
			break;
		}

		return FALSE;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
			case IDC_RADIO1:
				CheckDlgButton(hDlg, IDC_RADIO1, TRUE);
				CheckDlgButton(hDlg, IDC_RADIO2, FALSE);
				return TRUE;

			case IDC_RADIO2:
				CheckDlgButton(hDlg, IDC_RADIO1, FALSE);
				CheckDlgButton(hDlg, IDC_RADIO2, TRUE);
				return TRUE;

			case ID_OK:
			{
				wchar_t launcher[MAXPATH], target[MAXPATH];
				GetDlgItemText(hDlg, IDC_EDIT1, launcher, MAXPATH);
				GetDlgItemText(hDlg, IDC_EDIT2, target, MAXPATH);
				if (!Rules.AddRule(launcher, target, IsDlgButtonChecked(hDlg, IDC_RADIO2), type))
					MessageBox(hDlg, L"添加失败", L"提示", MB_ICONWARNING);

				EndDialog(hDlg, 0);
				return TRUE;
			}

			case ID_CANCEL:
				EndDialog(hDlg, 0);
				return TRUE;
		}
		break;

	case WM_CLOSE:
		EndDialog(hDlg, 0);
		return TRUE;
	}

	return FALSE;
}
