
// #pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <ntddk.h>
#include <windef.h>

#ifdef __cplusplus
}
#endif

#define MAXPATH 300

#define DEVICENAME  L"\\Device\\ProcMonitor"
#define SYMLINKNAME L"\\??\\ProcMon"
#define KEPROCEVENT L"\\BaseNamedObjects\\KernelProcessMonitorEvent"
// #define USPROCEVENT L"\\BaseNamedObjects\\UserProcMonitorEvent"


#define IOCTL_TURNON_MONITORING  \
		(CTL_CODE(FILE_DEVICE_UNKNOWN, 0xFA0, METHOD_BUFFERED, FILE_ANY_ACCESS))
#define IOCTL_TURNOFF_MONITORING  \
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

#pragma pack(1)
typedef struct _PROCCREATIONINFO{
	short Year;
	short Month;
	short Day;
	short Hour;
	short Minute;
	short Second;
	ULONG ProcessID;
	ULONG ParentProcID;
	bool Result;
	wchar_t ProcessImage[MAXPATH];
	wchar_t ParentProcImage[MAXPATH];
} PROCCREATIONINFO, *PPROCCREATIONINFO;
#pragma pack()

typedef struct _KAPC_STATE
{
     LIST_ENTRY ApcListHead[2];
     PKPROCESS Process;
     UCHAR KernelApcInProgress;
     UCHAR KernelApcPending;
     UCHAR UserApcPending;
} KAPC_STATE, *PKAPC_STATE, *PRKAPC_STATE;

extern "C" NTSYSAPI NTSTATUS NTAPI ZwQueryInformationProcess(
	IN HANDLE ProcHandle,
	IN PROCESSINFOCLASS ProcInfoClass,
	OUT PVOID ProcInfo,
	IN ULONG ProcInfoLen,
	OUT OPTIONAL PULONG RetLen
);

extern "C" NTSYSAPI NTSTATUS NTAPI ZwCreateThread(
	OUT PHANDLE 	ThreadHandle,
	IN ACCESS_MASK 	DesiredAccess,
	IN OPTIONAL POBJECT_ATTRIBUTES 	ObjectAttributes,
	IN HANDLE 	ProcessHandle,
	OUT PCLIENT_ID 	ClientId,
	IN PCONTEXT 	ThreadContext,
	IN PVOID 	UserStack,		// PINITIAL_TEB
	IN BOOLEAN 	CreateSuspended
);

extern "C" NTSYSAPI
NTSTATUS NTAPI PsLookupProcessByProcessId(
	HANDLE Pid,
	PEPROCESS *eProcess
);

extern "C" NTSYSAPI
HANDLE NTAPI PsGetProcessInheritedFromUniqueProcessId(
	PEPROCESS eProcess
);

extern "C" NTSYSAPI
LPSTR NTAPI PsGetProcessImageFileName(
	PEPROCESS eProcess
);

extern "C" NTSYSAPI
NTSTATUS NTAPI PsLookupThreadByThreadId(
	HANDLE Tid,
	PETHREAD *eThread
);

extern "C" NTSYSAPI
PEPROCESS NTAPI IoThreadToProcess(
	PETHREAD eThread
);

extern "C" NTSYSAPI
VOID NTAPI KeStackAttachProcess(
	PRKPROCESS   Process,
	PRKAPC_STATE ApcState
);

extern "C" NTSYSAPI
VOID NTAPI KeUnstackDetachProcess(
  PRKAPC_STATE ApcState
);

extern "C" NTSYSAPI
PPEB NTAPI PsGetProcessPeb(PEPROCESS Process);

extern "C" NTSYSAPI
NTSTATUS NTAPI MmUnmapViewOfSection(
	IN PEPROCESS Process,
	IN PVOID	 BaseAddress
);

extern "C" NTSYSAPI
NTSTATUS NTAPI PsSuspendProcess(IN PEPROCESS Process);



typedef NTSTATUS (NTAPI *ZWCREATETHREAD)(
	OUT PHANDLE 	ThreadHandle,
	IN ACCESS_MASK 	DesiredAccess,
	IN OPTIONAL POBJECT_ATTRIBUTES 	ObjectAttributes,
	IN HANDLE 	ProcessHandle,
	OUT PCLIENT_ID 	ClientId,
	IN PCONTEXT 	ThreadContext,
	IN PVOID 	UserStack,		// PINITIAL_TEB
	IN BOOLEAN 	CreateSuspended
);


 typedef struct _RTL_USER_PROCESS_PARAMETERS {
  UCHAR          Reserved1[16];
  PVOID          Reserved2[10];
  UNICODE_STRING ImagePathName;
  UNICODE_STRING CommandLine;
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;


 typedef struct _PEB_LDR_DATA {
  ULONG                   Length;
  BOOLEAN                 Initialized;
  PVOID                   SsHandle;
  LIST_ENTRY              InLoadOrderModuleList;
  LIST_ENTRY              InMemoryOrderModuleList;
  LIST_ENTRY              InInitializationOrderModuleList;
} PEB_LDR_DATA, *PPEB_LDR_DATA;


 typedef struct _PEB {
  UCHAR                          Reserved1[2];
  UCHAR                          BeingDebugged;
  UCHAR                          Reserved2[1];
  PVOID                          Reserved3[2];
  PPEB_LDR_DATA                  Ldr;
  PRTL_USER_PROCESS_PARAMETERS   ProcessParameters;
  UCHAR                          Reserved4[104];
  PVOID                          Reserved5[52];
  PVOID                          PostProcessInitRoutine;/*PPS_POST_PROCESS_INIT_ROUTINE*/
  UCHAR                          Reserved6[128];
  PVOID                          Reserved7[1];
  ULONG                          SessionId;
} PEB, *PPEB;


 typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY     InLoadOrderLinks;
	LIST_ENTRY     InMemoryOrderLinks;
	LIST_ENTRY     InInitializationOrderLinks;
    PVOID          DllBase;
    PVOID          EntryPoint;
    ULONG          SizeOfImage;// PVOID Reserved3;
    UNICODE_STRING FullDllName;
    UCHAR          Reserved4[8];
    PVOID          Reserved5[3];
    union {
        ULONG      CheckSum;
        PVOID      Reserved6;
    };
    ULONG          TimeDateStamp;
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;



typedef struct _DEVICE_EXTENSION {
	PDEVICE_OBJECT pDevObj;
	PKEVENT pKeEvent;
	HANDLE  hKeEvent;
	PROCCREATIONINFO ProcCreationInfo;
	UNICODE_STRING ustrDevSymLinkName;
	UNICODE_STRING ustrDevName;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;


#ifndef WINXP
typedef NTSTATUS (__fastcall *PSPTERMINATETHREADBYPOINTER)(
	IN PETHREAD Thread,
	IN NTSTATUS ExitStatus,
	IN BOOLEAN  Self
);
#else
typedef NTSTATUS (NTAPI *PSPTERMINATETHREADBYPOINTER)(
	IN PETHREAD Thread,
	IN NTSTATUS ExitStatus
);
#endif

///////////////////////////////////////////////////////////////////////////////
// 函数声明

VOID DriverUnloadRoutine(IN PDRIVER_OBJECT pDriObj);
// VOID ProcCreateRoutine  (IN HANDLE ParentId, IN HANDLE ProcessId, IN BOOLEAN Create);
NTSTATUS GetProcessInfo(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp);
NTSTATUS DeviceIoControl(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp);
NTSTATUS OnCreate(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp);

VOID CreateProcessNotify(
	IN HANDLE  ParentId,
	IN HANDLE  ProcessId,
	IN BOOLEAN Create
);

VOID CreateProcessNotifyEx(
	PEPROCESS              Process,
	HANDLE                 ProcessId,
	PPS_CREATE_NOTIFY_INFO CreateInfo
);

VOID CreateThreadNotify(
	IN HANDLE  ProcessId,
	IN HANDLE  ThreadId,
	IN BOOLEAN  Create
);

VOID EnumProcess();
VOID EnumThread(PEPROCESS Process);
VOID EnumModule(PEPROCESS Process);

NTSTATUS ZwKillProcess(HANDLE Pid);
BOOLEAN ZwKillThread(HANDLE Tid);
VOID Kill(PVOID pv);

PEPROCESS LookupProcess(HANDLE Pid);
PUNICODE_STRING GetProcessFullImageFileName(PEPROCESS pep);


///////////////////////////////////////////////////////////////////////////////
// 与用户模式程序通讯用数据结构

typedef struct _ENUMPROCINFO
{
	ULONG64 eProcessAddr;
	ULONG	Pid;
	ULONG	PPid;
	char	ShortName[20];
	wchar_t	ImageName[1024];
} ENUMPROCINFO, *PENUMPROCINFO;


typedef struct _ENUMTHREADINFO
{
	ULONG64 eThreadAddr;
	ULONG	Tid;
	ULONG	Priority;
} ENUMTHREADINFO, *PENUMTHREADINFO;


typedef struct _ENUMMODULEINFO
{
	ULONG64 Base;
	ULONG	Size;
	wchar_t Path[1024];
} ENUMMODULEINFO, *PENUMMODULEINFO;


///////////////////////////////////////////////////////////////////////////////