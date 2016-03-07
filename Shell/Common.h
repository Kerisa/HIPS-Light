

#pragma once

#include <Windows.h>
#include <WindowsX.h>
#include <process.h>
#include <commctrl.h>
#include <assert.h>
#include <fltUser.h>
#include <vector>
#include <string>
#include <strsafe.h>
#include "resource.h"


#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "fltLib.lib")
#pragma comment(lib, "fltMgr.lib")

using std::vector;
using std::wstring;

#define WIN7

const long MAXPATH = 300;



#define PROCDEVNAME L"\\\\.\\ProcMon"
#define FILEDEVNAME L"Myminifilter"
#define REGDEVNAME  L"\\\\.\\RegMon"
//#define PROCDRIVERPATH L"\\??\\C:\\Users\\Lapw\\Desktop\\Shell\\Debug\\ProcMon.sys"
#define FILEDRIVERPATH L"system32\\DRIVERS\\Myminifilter.sys"
//#define REGDRIVERPATH  L"\\??\\C:\\Users\\Lapw\\Desktop\\Shell\\Debug\\RegMon.sys"
#define ALTITUDE    L"370030"

#define USPROCEVENT L"\\BaseNamedObjects\\UserProcMonitorEvent"

#define FILEUSEREVENT L"MyminiFilterUserEvent000"
#define COMMUNICATE_PORT_NAME L"\\MyminiFilterPort000"

#define ID_LISTVIEW1 1
#define ID_LISTVIEW2 2
#define ID_LISTVIEW3 3
#define ID_LISTVIEW4 4
#define ID_LISTVIEW5 5
#define ID_LISTVIEW6 6
#define ID_LISTVIEW7 7

//////////////////////////////
// Proc控制码
#define IOCTL_TRUNON_PROC_MONITORING  \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFA0, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_TURNOFF_PROC_MONITORING  \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFA1, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_GETPROCINFO  \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFA2, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_RECEIVE_USER_EVENT  \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFA3, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_TURNON_TP_CREATION_DISABLE  \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFA4, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_TURNOFF_TP_CREATION_DISABLE  \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFA5, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_ENUMPROCESS \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFA6, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_ENUMTHREAD  \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFA7, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_ENUMMODULE  \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFA8, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_TERMINATEPROCESS  \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFA9, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_TERMINATETHREAD  \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFAA, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_UNLOADMODULE  \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFAB, METHOD_BUFFERED, FILE_ANY_ACCESS))



//////////////////////////////
// Reg控制码
#define IOCTL_TRUNON_REG_MONITORING  \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFC0, METHOD_BUFFERED, FILE_ANY_ACCESS))
        
#define IOCTL_TURNOFF_REG_MONITORING \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFC1, METHOD_BUFFERED, FILE_ANY_ACCESS))
        
#define IOCTL_ALLOW_REG_MODIFY \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFC2, METHOD_BUFFERED, FILE_ANY_ACCESS))
        
#define IOCTL_DISALLOW_REG_MODIFY \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFC3, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_RECEIVE_USER_REG_EVENT  \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFC4, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_GET_REG_INFO \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFC5, METHOD_BUFFERED, FILE_ANY_ACCESS))
        
#define IOCTL_ADD_REG_BLOCK_LIST \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFC6, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_ADD_REG_WHITE_LIST \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFC7, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_REMOVE_REG_BLOCK_LIST \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFC8, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_REMOVE_REG_WHITE_LIST \
        (CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFC9, METHOD_BUFFERED, FILE_ANY_ACCESS))




#define LOG(Msg, Type) MessageBox(0, Msg, Type, 0)



#define HANDLE_WM_COMMAND(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (int)(LOWORD(wParam)), (HWND)(lParam), (UINT)HIWORD(wParam)))



typedef LONG (NTAPI *NTSUSPENDPROCESS) (HANDLE ProcessHandle);
typedef LONG (NTAPI *NTRESUMEPROCESS)  (HANDLE ProcessHandle);
typedef NTSTATUS (NTAPI *NTTERMINATEPROCESS)(HANDLE ProcessHandle, NTSTATUS ExitStatus);


// 对文件驱动使用的操作命令
typedef enum _COMMAND {
    ENUM_TURNON_PROTECT = 0,
    ENUM_TURNOFF_PROTECT,
    ENUM_GET_FILE_INFO,
    ENUM_RECEIVE_USER_FILE_EVENT,
    ENUM_ADD_FILE_BLOCK_LIST,
    ENUM_ADD_FILE_WHITE_LIST,
    ENUM_REMOVE_FILE_BLOCK_LIST,
    ENUM_REMOVE_FILE_WHITE_LIST,
} COMMAND;


#pragma pack(1)

// 用来对文件过滤驱动使用的命令结构
typedef struct _FILTER_MESSAGE {
    COMMAND     Cmd;
    union {
        ULONG64  hEvent;
        wchar_t String[MAXPATH];
    };
} FILTER_MESSAGE, *PFILTER_MESSAGE;


// 用来记录进程信息日志的结构
typedef struct _PROCCREATIONINFO{
    short    Year;
    short    Month;
    short    Day;
    short    Hour;
    short    Minute;
    short    Second;
    ULONG    PID;
    ULONG    PPID;
    bool    Result;
    wchar_t ProcessImage[MAXPATH];
    wchar_t ParentProcImage[MAXPATH];
} PROCCREATIONINFO, *PPROCCREATIONINFO;


// 用来记录文件信息日志的结构
typedef struct _FILEINFO{
    short    Year;
    short    Month;
    short    Day;
    short    Hour;
    short    Minute;
    short    Second;
    bool    Result;
    wchar_t    ProcessName[MAXPATH];
    wchar_t    FileName[MAXPATH];
} FILEINFO, *PFILEINFO;


// 用来记录注册表信息日志的结构
typedef struct _REGINFO{
    short    Year;
    short    Month;
    short    Day;
    short    Hour;
    short    Minute;
    short    Second;
    bool     Result;        // true 表成功
    bool     NeedProcess;
    wchar_t  ProcessName[MAXPATH];
    wchar_t  RegKeyName[MAXPATH];
} REGINFO, *PREGINFO;
#pragma pack()


// 枚举进程时传递的结构
typedef struct _ENUMPROCINFO
{
    ULONG64 eProcessAddr;
    ULONG   Pid;
    ULONG   PPid;
    char    ShortName[20];
    wchar_t ImageName[1024];
} ENUMPROCINFO, *PENUMPROCINFO;


// 枚举线程时传递的结构
typedef struct _ENUMTHREADINFO
{
    ULONG64 eThreadAddr;
    ULONG   Tid;
    ULONG   Priority;
} ENUMTHREADINFO, *PENUMTHREADINFO;


// 枚举模块时传递的结构
typedef struct _ENUMMODULEINFO
{
    ULONG64 Base;
    ULONG   Size;
    wchar_t Path[1024];
} ENUMMODULEINFO, *PENUMMODULEINFO;


// 图片界面相关的结构
#define BMPNUM    14
#define PICCTLNUM 13

static PTSTR BmpName[] = {
    MAKEINTRESOURCE(IDB_BACKGROUND),
    MAKEINTRESOURCE(IDB_LOG),
    MAKEINTRESOURCE(IDB_LOG_HIT),
    MAKEINTRESOURCE(IDB_OFF),
    MAKEINTRESOURCE(IDB_OFF_HIT),
    MAKEINTRESOURCE(IDB_ON),
    MAKEINTRESOURCE(IDB_ON_HIT),
    MAKEINTRESOURCE(IDB_PROCINFO),
    MAKEINTRESOURCE(IDB_PROCINFO_HIT),
    MAKEINTRESOURCE(IDB_RULE),
    MAKEINTRESOURCE(IDB_RULE_HIT),
    MAKEINTRESOURCE(IDB_UNLOAD),
    MAKEINTRESOURCE(IDB_UNLOAD_HIT),
    MAKEINTRESOURCE(IDB_BACK_1)
};

typedef struct _PICTURE_INFO{
    BITMAP  Bmp;
    HBITMAP hBitmap;
} PICTUREINFO, *PPICTUREINFO;




// 规则中使用的三个类型
enum {EnumProcType = 1, EnumFileType = 2, EnumRegType = 3};


class RULES;

// 与驱动通信及控制的相关操作
class DRIVERCONTROL
{
public:
    bool   ProcMonitoringOn;
    bool   FileMonitoringOn;
    bool   RegMonitoringOn;

    HANDLE hDevProc, hDevReg;
    HANDLE hFileFilterPort;

    HANDLE hRegEventKtoU, hRegEventUtoK;
    HANDLE hFileEvent;
    HANDLE hProcEvent;

    // 加载/卸载驱动的基本操作
    bool LoadDriver(const wchar_t *DriverName, const wchar_t *DriverPath);
    bool UnloadDriver(const wchar_t *DriverName, const wchar_t *DriverPath, bool Delete);

    // 驱动加载的包装
    bool InstallProcessDriver(void);
    bool InstallFileDriver(const wchar_t* DriverName, const wchar_t* DriverPath, const wchar_t* Altitude);
    bool InstallRegistryDriver(void);

    // 关闭设备句柄等
    bool CloseDevice(void);

    // 启动/关闭监视
    HANDLE StartProcMonitor(void);
    void StopProcMonitor(void);

    HANDLE StartFileMonitor(void);
    void StopFileMonitor(void);

    HANDLE StartRegMonitor(void);
    void StopRegMonitor(void);

    // 向驱动更新规则列表(不含进程驱动)
    bool InitProcDriverRule(RULES &r, HANDLE hDev);
    bool SendRuleToProcDriver(HANDLE hDev, const wchar_t *launcher, const wchar_t *target, int block, bool del);

    bool InitFileDriverRule(RULES & r, HANDLE hDev);
    bool SendRuleToFileDriver(HANDLE hDev, const wchar_t *target, int block, bool del);

    bool InitRegDriverRule(RULES &r, HANDLE hDev);
    bool SendRuleToRegDriver(HANDLE hDev, const wchar_t *target, int block, bool del);

    // 禁止/允许线程创建
    bool SwitchThreadAndProcCreate(void);    

    // 进程列表相关
    bool GetProcList(_Out_ PENUMPROCINFO *pepi, _Out_ int *pnum);
    bool GetThreadListofProcess(_In_ int Pid, _Out_ PENUMTHREADINFO *peti, _Out_ int *pnum);
    bool GetModuleListofProcess(_In_ int Pid, _Out_ PENUMMODULEINFO *pemi, _Out_ int *pnum);
    bool UnloadModule(_In_ int Pid, _In_ ULONG64 base);
    bool TerminateProcess(_In_ int Pid);
    bool TerminateThread(_In_ HANDLE Tid);
};



// 与规则相关的操作
class RULES
{
public:
    static enum _Type{
        ProcType = 1,
        FileType,
        RegType,
        LauncherType,
        TargetType
    };

    HWND   m_hLVProc, m_hLVFile, m_hLVReg;    // ListView 控件句柄
    HANDLE hProcDev, hRegDev;                // 3个设备句柄
    HANDLE hFileFilterPort;

private:
    enum {MAXPATH = 300, LIMIT = 32767, DefaultSize = 64};

    typedef struct _LIST{
        wchar_t       Target[MAXPATH];
        bool          Block;
        struct _LIST *next;
    } LIST, *PLIST;

    // 进程规则列表结构
    typedef struct _PROC_RULE{
        wchar_t Launcher[MAXPATH];
        LIST    Header;
    } PROCRULE, *PPROCRULE;

    // 文件/注册表规则列表结构
    typedef struct _FILE_REG_RULE{
        bool    Block;
        wchar_t Target[MAXPATH];
    } FILERULE, *PFILERULE, REGRULE, *PREGRULE;

    
    unsigned short    m_CapacityPi, m_CapacityFi, m_CapacityRi;
    unsigned short    m_SizePi, m_SizeFi, m_SizeRi;
    PPROCRULE         m_arrPi;
    PFILERULE         m_arrFi;
    PREGRULE          m_arrRi;

    // 用于确认是否需要更新规则文件
    bool m_IsProcRuleModified, m_IsFileRuleModified, m_IsRegRuleModified;

    bool m_IsFileRuleInitialized, m_IsRegRuleInitialized;
    



    bool AddProcRule(const wchar_t *launcher, const wchar_t *target, bool block);
    bool AddFileRule(const wchar_t *target, bool block);
    bool AddRegRule(const wchar_t *target, bool block);
    
    bool DeleteProcRule(const wchar_t *launcher, const wchar_t *target);
    bool DeleteFileRule(const wchar_t *target);
    bool DeleteRegRule(const wchar_t *target);

    bool IsContainProc(const wchar_t *launcher, const wchar_t *target, int *pblock);
    bool IsContainFile(const wchar_t *target, int *pblock);
    bool IsContainReg(const wchar_t *target, int *pblock);

    bool CheckDataHash(void);

public:
    RULES()    : m_SizePi(0), m_SizeFi(0), m_SizeRi(0), m_arrPi(0), m_arrFi(0), m_arrRi(0),
              m_CapacityPi(0), m_CapacityFi(0), m_CapacityRi(0),m_IsProcRuleModified(0),
              m_IsFileRuleModified(0), m_IsRegRuleModified(0) { }
    ~RULES();

    unsigned int EncryptData(HANDLE hFile);
    bool DecryptData(unsigned char *data, unsigned long Size);

    bool Init(unsigned short cp, unsigned short cf, unsigned short cr);
    bool LoadRulesFromFile(void);
    bool AddRule(const wchar_t *Launcher, const wchar_t *Target, bool block, enum _Type t);
    bool DeleteRule(enum _Type t);
    bool IsContain(const wchar_t *Launcher, const wchar_t *Target, enum _Type t1, int *pblock);

    bool UpdateRulesToFile(void);

    friend bool DRIVERCONTROL::InitFileDriverRule(RULES & r, HANDLE hDev);
    friend bool DRIVERCONTROL::SendRuleToFileDriver(HANDLE hDev, const wchar_t *target, int block, bool del);
    friend bool DRIVERCONTROL::InitRegDriverRule(RULES & r, HANDLE hDev);
    friend bool DRIVERCONTROL::SendRuleToRegDriver(HANDLE hRegDev, const wchar_t *target, int block, bool del);
};



///////////////////////////////////////////////////////////////////////////////
// 函数声明


unsigned int WINAPI WaitProcEvent(PVOID pv);
unsigned int WINAPI WaitFileEvent(void* pv);
unsigned int WINAPI WaitRegEvent(void* pv);
unsigned int WINAPI HandleFileInfo(PVOID pv);
unsigned int WINAPI HandleRegInfo(PVOID pv);
unsigned int WINAPI HandleProcInfo(PVOID pv);


////////////////////////////////////////////////
// Log.cpp
bool AppendRecordToLog(const void *log, int Type);



BOOL CALLBACK DisProcMsg(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK RulesDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ToolDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ProcessListDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);