
#include "Common.h"


extern HINSTANCE g_Inst;
extern HWND      g_hMainDlg;

extern RULES     Rules;
extern DRIVERCONTROL DriCtl;


unsigned int WINAPI WaitRegEvent(PVOID pv)
{
	DWORD R;


	while (DriCtl.RegMonitoringOn)
	{
		
		PREGINFO pri = new REGINFO;
		memset(pri, 0, sizeof(REGINFO));

		
		WaitForSingleObject(DriCtl.hRegEventKtoU, INFINITE);


		if (!DriCtl.RegMonitoringOn)
		{
			if (pri) delete pri;
			break;
		}


		int ret = DeviceIoControl(DriCtl.hDevReg, IOCTL_GET_REG_INFO, NULL, 0, pri, sizeof(REGINFO), &R, 0);

		if (ret && R == sizeof(REGINFO))
			AppendRecordToLog(pri, EnumRegType);

		else
			SetDlgItemText(g_hMainDlg, IDC_LOGEDIT, L"返回数据有误\r\n");

	}
	
	
	return 0;
}