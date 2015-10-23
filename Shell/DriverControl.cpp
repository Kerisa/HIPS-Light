

#include "Common.h"


extern HWND  g_hMainDlg;
extern RULES Rules;


bool DRIVERCONTROL::LoadDriver(const wchar_t *DriverName, const wchar_t *DriverPath)
{
	bool ret = true;
	wchar_t msgBuf[128];

	SC_HANDLE hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!hServiceMgr)
	{
		LOG(L"SCManager open failed", L"Init");
		return false;
	}

	SC_HANDLE hServiceDDK = CreateService(
		hServiceMgr,
		DriverName,
		DriverName,
		SERVICE_ALL_ACCESS | SERVICE_START,
		SERVICE_KERNEL_DRIVER,//SERVICE_FILE_SYSTEM_DRIVER,
		SERVICE_DEMAND_START,
		SERVICE_ERROR_NORMAL,
		DriverPath,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL);
	
	if (hServiceDDK == NULL)
	{
		DWORD e = GetLastError();
		
		if (e != ERROR_IO_PENDING && e != ERROR_SERVICE_EXISTS)
		{
			StringCchPrintf(msgBuf, _countof(msgBuf),
				L"CreateFailed for other reason %d.\n",	e);
			LOG(msgBuf, L"Init");
			ret = false;
			goto END;
		}
		else
		{
			printf("Already exitst...Open\n");
		}
		// 转为打开
		hServiceDDK = OpenService(
			hServiceMgr,
			DriverName,
			SERVICE_ALL_ACCESS | SERVICE_START);
		
		if (hServiceDDK == NULL)
		{
			StringCchPrintf(msgBuf, _countof(msgBuf),
				L"Service open Fail %d\n", GetLastError());
			LOG(msgBuf, L"Init");
			ret = false;
			goto END;
		} else {
			printf("Open Succeed.\n");
		}
	} else {
		printf("Create Succeed.\n");
	}

	if (!StartService(hServiceDDK, NULL, NULL))
	{
		DWORD e = GetLastError();
		
		if (e != ERROR_IO_PENDING && e != ERROR_SERVICE_ALREADY_RUNNING)
		{
			StringCchPrintf(msgBuf, _countof(msgBuf), L"Start Fail %d\n", e);
			LOG(msgBuf, L"Init");
			ret = false;
			goto END;
		}
		else
		{
			if (e == ERROR_IO_PENDING)
			{
				LOG(L"Start ERROR_IO_PENDING", L"Init");
				ret = false;
				goto END;
			}
			else
			{
				printf("Start ERROR_ALREADY_RUNNING\n");
			}
		}
	}
	else
		printf("All Green.\n");

END:
	if (hServiceDDK) CloseServiceHandle(hServiceDDK);
	if (hServiceMgr) CloseServiceHandle(hServiceMgr);
	return ret;
}


bool DRIVERCONTROL::UnloadDriver(const wchar_t *DriverName, const wchar_t *, bool Delete)
{
	bool ret = true;
	SC_HANDLE hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!hServiceMgr)
	{
		LOG(L"SCManager open failed", L"Init");
		return false;
	}

	SC_HANDLE hServiceDDK;
	if (!(hServiceDDK = OpenService(hServiceMgr, DriverName, SERVICE_ALL_ACCESS)))
	{
		LOG(L"Service open Error", L"Init");
		ret = false;
		goto END;
	}

	SERVICE_STATUS SvrSta;
	if (!ControlService(hServiceDDK, SERVICE_CONTROL_STOP, &SvrSta) &&
		GetLastError() != 1062)	// 服务未启动
	{
		printf("Stop Failed.\n");
		ret = false;
		goto END;
	}
	else printf("stop service...ok.\n");

	if (Delete)
	{
		if (!DeleteService(hServiceDDK))
		{
			printf("Delete Failed.\n");
			ret = false;
			goto END;
		}
		else printf("delete service...ok.\n");
	}

	printf("All Process Done.\n");


END:
	if (hServiceDDK) CloseServiceHandle(hServiceDDK);
	if (hServiceMgr) CloseServiceHandle(hServiceMgr);
	return ret;
}


bool DRIVERCONTROL::InstallProcessDriver(void)
{
	DWORD R;
	bool success = true;
	wchar_t path[MAX_PATH] = L"\\??\\";
	
	GetModuleFileName(NULL, path + 4, MAX_PATH-5);

	int k = wcslen(path);
	while (k>=0 && path[k-1] != L'\\') --k;
	wcscpy(&path[k], L"ProcMon.sys");

	do
	{
		if (!(success &= LoadDriver(L"ProcMon", path)))
			break;

		hDevProc = CreateFile(PROCDEVNAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (!(success &= (hDevProc != INVALID_HANDLE_VALUE)))
			break;

		hProcEvent = CreateEvent(0, FALSE, FALSE, 0);
		if (!(success &= !!hProcEvent))
			break;

		success &= (bool)DeviceIoControl(hDevProc, IOCTL_RECEIVE_USER_EVENT,
					&hProcEvent, sizeof(hProcEvent), NULL, 0, &R, 0);
		if (!success)
			break;
	
		Rules.hProcDev = hDevProc;	// 同步到Rules中
		return true;

	} while (0);

	if (hProcEvent)
		CloseHandle(hProcEvent);

	if (hDevProc != INVALID_HANDLE_VALUE)
		CloseHandle(hDevProc);

	return false;
}



bool DRIVERCONTROL::InstallFileDriver(
	const wchar_t *DriverName,
	const wchar_t *DriverPath,
	const wchar_t *Altitude
	)
{
	wchar_t szTempStr[MAX_PATH];
    HKEY	hKey;
    DWORD	dwData;
    wchar_t szInfPath[MAX_PATH] = L" \"";    

    if(!DriverName || !DriverPath)
        return FALSE;
    
    //得到完整的驱动路径	
	GetModuleFileName(NULL, szInfPath + 2, MAX_PATH-1);

	int k = wcslen(szInfPath);
	while (k>=0 && szInfPath[k-1] != L'\\') --k;
	wcscpy(&szInfPath[k], L"Myminifilter.inf");
	wcscat(szInfPath, L"\"");
	
	PROCESS_INFORMATION pi;
	STARTUPINFO si = {0};
	si.cb = sizeof(si);

	GetEnvironmentVariable(L"SystemRoot", szTempStr, MAX_PATH);
	wcscat_s(szTempStr, MAX_PATH, L"\\Sysnative\\InfDefaultInstall.exe");
	wcscat_s(szTempStr, MAX_PATH, szInfPath);

	CreateProcess(0, szTempStr, 0, 0, 0, 0, 0, 0, &si, &pi);

	if (WAIT_TIMEOUT == WaitForSingleObject(pi.hProcess, 10000))
	{
		MessageBox(0, L"文件驱动安装失败", L"Init", MB_ICONWARNING);
		return false;
	}

    SC_HANDLE hServiceMgr, hService;
/*
    //打开服务控制管理器
    hServiceMgr = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
    if(!hServiceMgr) 
    {
        CloseServiceHandle(hServiceMgr);
        return FALSE;        
    }



    //创建驱动所对应的服务
    hService = CreateService(
		hServiceMgr,
        DriverName,                  // 驱动程序的在注册表中的名字
        DriverName,                  // 注册表驱动程序的DisplayName 值
        SERVICE_ALL_ACCESS,          // 加载驱动程序的访问权限
        SERVICE_FILE_SYSTEM_DRIVER,  // 表示加载的服务是文件系统驱动程序
        SERVICE_DEMAND_START,        // 注册表驱动程序的Start 值
        SERVICE_ERROR_IGNORE,        // 注册表驱动程序的ErrorControl 值
        szDriverImagePath,           // 注册表驱动程序的ImagePath 值
        L"FSFilter Activity Monitor",// 注册表驱动程序的Group 值
        NULL, 
        L"FltMgr",                   // 注册表驱动程序的DependOnService 值
        NULL, 
        NULL);

    if(!hService) 
    {
		int err = GetLastError();
        CloseServiceHandle(hServiceMgr);
        if (err == ERROR_SERVICE_EXISTS) 
        {
            //服务创建失败，是由于服务已经创立过
			CloseServiceHandle(hService);       // 服务句柄
			CloseServiceHandle(hServiceMgr);    // SCM句柄
            goto STARTFILEFILTER;
        }
        else
            return FALSE;
    }

	CloseServiceHandle(hService);       // 服务句柄
    CloseServiceHandle(hServiceMgr);    // SCM句柄
    
    //-------------------------------------------------------------------------------------------------------
    // SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances子健下的键值项 
    //-------------------------------------------------------------------------------------------------------
    wcscpy_s(szTempStr, MAX_PATH, L"SYSTEM\\CurrentControlSet\\Services\\");
    wcscat_s(szTempStr, MAX_PATH, DriverName);
    wcscat_s(szTempStr, MAX_PATH, L"\\Instances");
    if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, L"", TRUE,
		KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData))
    {
        return FALSE;
    }

    // 注册表驱动程序的DefaultInstance 值 
    wcscpy_s(szTempStr, MAX_PATH, DriverName);
    wcscat_s(szTempStr, MAX_PATH, L" Instance");
    if (ERROR_SUCCESS != RegSetValueEx(hKey, L"DefaultInstance", 0, REG_SZ,
		(CONST BYTE*)szTempStr, (DWORD)wcslen(szTempStr)))
    {
        return FALSE;
    }

    RegFlushKey(hKey);//刷新注册表
    RegCloseKey(hKey);
    //-------------------------------------------------------------------------------------------------------

    //-------------------------------------------------------------------------------------------------------
    // SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances\\DriverName Instance子健下的键值项 
    //-------------------------------------------------------------------------------------------------------
    wcscpy_s(szTempStr, MAX_PATH, L"SYSTEM\\CurrentControlSet\\Services\\");
    wcscat_s(szTempStr, MAX_PATH, DriverName);
    wcscat_s(szTempStr, MAX_PATH, L"\\Instances\\");
    wcscat_s(szTempStr, MAX_PATH, DriverName);
    wcscat_s(szTempStr, MAX_PATH, L" Instance");
    if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, L"", TRUE,
		KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData))
    {
        return FALSE;
    }

    // 注册表驱动程序的Altitude 值
    wcscpy_s(szTempStr, MAX_PATH, Altitude);
    if (ERROR_SUCCESS != RegSetValueEx(hKey, L"Altitude", 0, REG_SZ,
		(CONST BYTE*)szTempStr, (DWORD)wcslen(szTempStr)))
    {
        return FALSE;
    }

    // 注册表驱动程序的Flags 值
    dwData=0x0;
    if (ERROR_SUCCESS != RegSetValueEx(hKey, L"Flags", 0, REG_DWORD,
		(CONST BYTE*)&dwData, sizeof(DWORD)))
    {
        return FALSE;
    }

    RegFlushKey(hKey);//刷新注册表
    RegCloseKey(hKey);
*/
//-------------------------------------------------------------------------------------------------------
//  开启服务
STARTFILEFILTER:

    hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if(!hServiceMgr)
    {
        CloseServiceHandle(hServiceMgr);
        return FALSE;
    }
    hService = OpenService(hServiceMgr, DriverName, SERVICE_ALL_ACCESS);
    if(!hService)
    {
        CloseServiceHandle(hService);
        CloseServiceHandle(hServiceMgr);
        return FALSE;
    }

    if(!StartService(hService, 0, NULL))
    {
        CloseServiceHandle(hService);
        CloseServiceHandle(hServiceMgr);
        if(GetLastError() == ERROR_SERVICE_ALREADY_RUNNING) 
        {             
            // 服务已经开启
        } 
        else return FALSE;
    }
    
    CloseServiceHandle(hService);
    CloseServiceHandle(hServiceMgr);


	hFileEvent = CreateEvent(0, FALSE, FALSE, FILEUSEREVENT);
	if (!hFileEvent)
		LOG(L"无法创建文件同步事件\n", L"Init");

	if (S_OK != FilterConnectCommunicationPort(COMMUNICATE_PORT_NAME, 0, NULL,
		0, 0, &hFileFilterPort))
		LOG(L"文件驱动通信端口连接失败", 0);


	FILTER_MESSAGE fm;
	fm.Cmd = ENUM_RECEIVE_USER_FILE_EVENT;
	fm.hEvent = (ULONG64) hFileEvent;

	DWORD R;
	if (S_OK != FilterSendMessage(hFileFilterPort, &fm, sizeof(fm), 0, 0, &R))
		LOG(L"同步驱动事件失败", 0);

	Rules.hFileFilterPort = hFileFilterPort;
	InitFileDriverRule(Rules, Rules.hFileFilterPort);

    return TRUE;
 
}



bool DRIVERCONTROL::InstallRegistryDriver(void)
{
	bool success = true;
	DWORD R;
	wchar_t path[MAX_PATH] = L"\\??\\";
	
	GetModuleFileName(NULL, path + 4, MAX_PATH-5);

	int k = wcslen(path);
	while (k>=0 && path[k-1] != L'\\') --k;
	wcscpy(&path[k], L"RegMon.sys");

	do
	{
		if (!(success &= LoadDriver(L"RegMon", path)))
			break;

		hDevReg = CreateFile(REGDEVNAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (!(success &= (hDevReg != INVALID_HANDLE_VALUE)))
			break;

		hRegEventUtoK = CreateEvent(0, FALSE, FALSE, 0);		// 自动
		hRegEventKtoU = CreateEvent(0, TRUE, FALSE, 0);			// 手动

		success &= (hRegEventKtoU && hRegEventUtoK);
		if (!success)
			break;
		

		ULONG64 tmp[2] = {(ULONG64)hRegEventUtoK, (ULONG64)hRegEventKtoU};
		success &= (bool)DeviceIoControl(hDevReg, IOCTL_RECEIVE_USER_REG_EVENT,
			tmp, 2 * sizeof(tmp[0]), NULL, 0, &R, 0);
		if (!success)
			break;

		Rules.hRegDev  = hDevReg;
		InitRegDriverRule(Rules, hDevReg);

		return true;
	
	} while (0);

	if (hRegEventUtoK) CloseHandle(hRegEventUtoK);
	if (hRegEventKtoU) CloseHandle(hRegEventKtoU);
	if (hDevReg != INVALID_HANDLE_VALUE) CloseHandle(hDevReg);

	return false;
}



bool DRIVERCONTROL::CloseDevice(void)
{
	CloseHandle(hDevProc);
	CloseHandle(hDevReg);

	CloseHandle(hFileFilterPort);

	CloseHandle(hProcEvent);
	CloseHandle(hRegEventUtoK);
	CloseHandle(hRegEventKtoU);
	return true;
}



HANDLE DRIVERCONTROL::StartRegMonitor(void)
{
	DWORD R;

	RegMonitoringOn = true;
	CloseHandle((HANDLE)_beginthreadex(0, 0, WaitRegEvent, 0, 0, 0));

	InitRegDriverRule(Rules, hDevReg);

	DeviceIoControl(hDevReg, IOCTL_TRUNON_REG_MONITORING, 0, 0, 0, 0, &R, 0);

	return 0;
}


void DRIVERCONTROL::StopRegMonitor(void)
{
	DWORD R;

	RegMonitoringOn = false;

	SetEvent(hRegEventUtoK);
	DeviceIoControl(hDevReg, IOCTL_TURNOFF_REG_MONITORING, 0, 0, 0, 0, &R, 0);

}


HANDLE DRIVERCONTROL::StartFileMonitor(void)
{
	DWORD R;
	FILTER_MESSAGE fm;

	FileMonitoringOn = true;
	CloseHandle((HANDLE)_beginthreadex(0, 0, WaitFileEvent, 0, 0, 0));

	fm.Cmd = ENUM_TURNON_PROTECT;
	FilterSendMessage(hFileFilterPort, &fm, sizeof(fm), 0, 0, &R);
	
	InitFileDriverRule(Rules, hFileFilterPort);

	return 0;
}


void DRIVERCONTROL::StopFileMonitor(void)
{
	DWORD R;

	FileMonitoringOn = false;
	SetEvent(hFileEvent);
	
	FILTER_MESSAGE fm;
	fm.Cmd = ENUM_TURNOFF_PROTECT;
	FilterSendMessage(hFileFilterPort, &fm, sizeof(fm), 0, 0, &R);

}


HANDLE DRIVERCONTROL::StartProcMonitor(void)
{
	DWORD R;

	ProcMonitoringOn = true;
	CloseHandle((HANDLE)_beginthreadex(0, 0, WaitProcEvent, 0, 0, 0));
	

	DeviceIoControl(hDevProc, IOCTL_TRUNON_PROC_MONITORING, 0, 0, 0, 0, &R, 0); 

	return 0;
}


void DRIVERCONTROL::StopProcMonitor(void)
{
	DWORD R;
	
	ProcMonitoringOn = false;

	SetEvent(hProcEvent);
	DeviceIoControl(hDevProc, IOCTL_TURNOFF_PROC_MONITORING, 0, 0, 0, 0, &R, 0);
}



bool DRIVERCONTROL::InitFileDriverRule(RULES & r, HANDLE hPort)
{
	int ret;
	if (!r.m_IsFileRuleInitialized) return true;

	for (int i=0; i<r.m_SizeFi; ++i)
	{
		ret = SendRuleToFileDriver(hPort, r.m_arrFi[i].Target, r.m_arrFi[i].Block, false);
		if (!ret)
		{
			LOG(L"文件驱动过滤列表添加失败", L"init");
			return false;
		}
	}

	r.m_IsFileRuleInitialized = true;
	return true;
}


bool DRIVERCONTROL::SendRuleToFileDriver(HANDLE hPort, const wchar_t *target, int block, bool del)
{
	DWORD R;
	FILTER_MESSAGE fm;
	wcscpy_s(fm.String, MAXPATH, target);

	if (block)
	{
		if (del)
			fm.Cmd = ENUM_REMOVE_FILE_BLOCK_LIST;
		
		else
			fm.Cmd = ENUM_ADD_FILE_BLOCK_LIST;
	}
	else
	{
		if (del)
			fm.Cmd = ENUM_REMOVE_FILE_WHITE_LIST;
		
		else
			fm.Cmd = ENUM_ADD_FILE_WHITE_LIST;
	}

	return S_OK == FilterSendMessage(hPort, &fm, sizeof(fm), 0, 0, &R);
}


bool DRIVERCONTROL::InitRegDriverRule(RULES &r, HANDLE hDev)
{
	int ret;
	if (!r.m_IsRegRuleInitialized) return true;

	for (int i=0; i<r.m_SizeRi; ++i)
	{
		ret = SendRuleToRegDriver(hDev, r.m_arrRi[i].Target,
			r.m_arrRi[i].Block, false);
		if (!ret)
		{
			LOG(L"注册表驱动过滤列表添加失败", L"init");
			return false;
		}
	}

	r.m_IsRegRuleInitialized = true;
	return true;
}


bool DRIVERCONTROL::SendRuleToRegDriver(HANDLE hDev, const wchar_t *target, int block, bool del)
{
	DWORD R;
	if (block)
	{
		if (del)
			return DeviceIoControl(hDev, IOCTL_REMOVE_REG_BLOCK_LIST, (PVOID)target,
				RULES::MAXPATH*2, 0, 0, &R, 0);
		else
			return DeviceIoControl(hDev, IOCTL_ADD_REG_BLOCK_LIST, (PVOID)target,
					RULES::MAXPATH*2, 0, 0, &R, 0);
	}
	else
	{
		if (del)
			return DeviceIoControl(hDev, IOCTL_REMOVE_REG_WHITE_LIST, (PVOID)target,
					RULES::MAXPATH*2, 0, 0, &R, 0);
		else
			return DeviceIoControl(hDev, IOCTL_ADD_REG_WHITE_LIST, (PVOID)target,
					RULES::MAXPATH*2, 0, 0, &R, 0);
	}
	
}


bool DRIVERCONTROL::SwitchThreadAndProcCreate(void)
{
	static bool bOn = false;
	bool ret = false;
	DWORD R;

	if (bOn)
		ret = DeviceIoControl(hDevProc, IOCTL_TURNOFF_TP_CREATION_DISABLE, 0, 0, 0, 0, &R, 0);
	else
		ret = DeviceIoControl(hDevProc, IOCTL_TURNON_TP_CREATION_DISABLE, 0, 0, 0, 0, &R, 0);

	if (ret) bOn ^= true;
	return ret;
}


bool DRIVERCONTROL::GetProcList(_Out_ PENUMPROCINFO *pepi, _Out_ int *pnum)
{
	DWORD R;

	if (pepi) *pepi = 0;
	if (pnum) *pnum = 0;

	PENUMPROCINFO Buf = (PENUMPROCINFO)VirtualAlloc(NULL,
		sizeof(ENUMPROCINFO)*1024, MEM_COMMIT, PAGE_READWRITE);

	if (!Buf) return false;

	int ret = DeviceIoControl(hDevProc, IOCTL_ENUMPROCESS,
		0, 0, Buf, sizeof(ENUMPROCINFO)*1024, &R, 0);
	if (!ret)
	{
		VirtualFree(Buf, 0, MEM_RELEASE);
		return false;
	}

	*pepi = Buf;
	*pnum = R / sizeof(ENUMPROCINFO);
	
	return true;
}


bool DRIVERCONTROL::GetThreadListofProcess(
	_In_  int Pid,
	_Out_ PENUMTHREADINFO *peti,
	_Out_ int *pnum)
{
	if (!peti || !pnum) return false;

	DWORD R;

	*peti = 0;
	*pnum = 0;

	PENUMTHREADINFO Buf = (PENUMTHREADINFO)VirtualAlloc(NULL,
		sizeof(ENUMTHREADINFO)*1024, MEM_COMMIT, PAGE_READWRITE);

	if (!Buf) return false;

	ULONG64 pid64 = (ULONG64)Pid;

	int ret = DeviceIoControl(hDevProc, IOCTL_ENUMTHREAD, &pid64, 8,
		Buf, sizeof(ENUMTHREADINFO)*1024, &R, 0);
	if (!ret)
	{
		VirtualFree(Buf, 0, MEM_RELEASE);
		return false;
	}

	*pnum = R / sizeof(ENUMTHREADINFO);
	*peti = Buf;

	return true;
}


bool DRIVERCONTROL::GetModuleListofProcess(
	_In_  int Pid,
	_Out_ PENUMMODULEINFO *pemi,
	_Out_ int *pnum)
{
	if (!pemi || !pnum) return false;

	DWORD R;

	*pemi = 0;
	*pnum = 0;

	PENUMMODULEINFO Buf = (PENUMMODULEINFO)VirtualAlloc(NULL,
		sizeof(ENUMMODULEINFO)*1024, MEM_COMMIT, PAGE_READWRITE);

	if (!Buf) return false;

	ULONG64 pid64 = (ULONG64)Pid;

	int ret = DeviceIoControl(hDevProc, IOCTL_ENUMMODULE, &pid64, 8,
		Buf, sizeof(ENUMMODULEINFO)*1024, &R, 0);
	if (!ret)
	{
		VirtualFree(Buf, 0, MEM_RELEASE);
		return false;
	}

	*pnum = R / sizeof(ENUMMODULEINFO);
	*pemi = Buf;

	return true;
}


bool DRIVERCONTROL::UnloadModule(
	_In_ int Pid,
	_In_ ULONG64 base)
{
	if (!Pid || !base) return false;

	ULONG64 buf[2] = {(ULONG64)Pid, base};
	DWORD R;

	return DeviceIoControl(hDevProc, IOCTL_UNLOADMODULE, buf, 16, 0, 0, &R, 0);
}


bool DRIVERCONTROL::TerminateProcess(_In_ int Pid)
{
	if (!Pid) return false;

	DWORD R;
	return DeviceIoControl(hDevProc,
		IOCTL_TERMINATEPROCESS, &Pid, 4, 0, 0, &R, 0);
}


bool DRIVERCONTROL::TerminateThread(_In_ HANDLE Tid)
{
	if (!Tid) return false;

	DWORD R;
	return DeviceIoControl(hDevProc, IOCTL_TERMINATETHREAD,
		&Tid, sizeof(HANDLE), 0, 0, &R, 0);
}