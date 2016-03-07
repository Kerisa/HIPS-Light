
#include "Common.h"



extern HINSTANCE     g_Inst;
extern HWND          g_hMainDlg;
extern RULES         Rules;
extern DRIVERCONTROL DriCtl;


unsigned int WINAPI WaitFileEvent(PVOID pv)
{
    DWORD R;
    bool first = true;

    SendMessage(GetDlgItem(g_hMainDlg, IDC_LOGEDIT), EM_LIMITTEXT, -1, 0);


    while (DriCtl.FileMonitoringOn)
    {
        PFILEINFO pfi = new FILEINFO;
        memset(pfi, 0, sizeof(FILEINFO));

        WaitForSingleObject(DriCtl.hFileEvent, INFINITE);


        if (!DriCtl.FileMonitoringOn) break;

        FILTER_MESSAGE fm;
        fm.Cmd = ENUM_GET_FILE_INFO;
        int ret = FilterSendMessage(DriCtl.hFileFilterPort, &fm, sizeof(fm), pfi, sizeof(FILEINFO), &R);
        

        if (ret == S_OK)
        {
            AppendRecordToLog(pfi, EnumFileType);
        }
        else
            ResetEvent(DriCtl.hFileEvent);

        delete pfi;
        
    }
    
    return 0;
}