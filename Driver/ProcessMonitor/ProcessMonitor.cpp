
//#define WINXP

#include "ProcessMonitor.h"


PDEVICE_OBJECT g_DeviceObject;

BOOLEAN g_MonitoringOn;
BOOLEAN g_DisableTPCreationOn;

PKEVENT g_pUserEvent;
HANDLE  g_hUserEvent;

#define THREADNOTIFYEVENTNAME L"\\BaseNamedObjects\\ThreadNotifyEvent"
HANDLE hThreadNotifyEvent;
PKEVENT pThreadNotifyEvent;


PSPTERMINATETHREADBYPOINTER PspTerminateThreadByPointer;


extern "C" NTSTATUS DriverEntry (
            IN PDRIVER_OBJECT pDriObj,
            IN PUNICODE_STRING pRegPath)
{
    NTSTATUS status;
    PDEVICE_EXTENSION pDevExt;


    pDriObj->DriverUnload = DriverUnloadRoutine;
    pDriObj->MajorFunction[IRP_MJ_CREATE] = OnCreate;
    pDriObj->MajorFunction[IRP_MJ_CLOSE] = OnCreate;
    pDriObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;


    UNICODE_STRING DeviceName;
    UNICODE_STRING SymLinkName;
    RtlInitUnicodeString(&DeviceName, DEVICENAME);
    RtlInitUnicodeString(&SymLinkName, SYMLINKNAME);

    status = IoCreateDevice(
        pDriObj,
        sizeof(DEVICE_EXTENSION),
        &DeviceName,
        FILE_DEVICE_UNKNOWN,
        0, FALSE,
        &g_DeviceObject);

    if (!NT_SUCCESS(status))
    {
        KdPrint(("[ProcMon] 设备无法创建\n"));
        return status;
    }
    g_DeviceObject->Flags |= DO_BUFFERED_IO;

    pDevExt = (PDEVICE_EXTENSION)g_DeviceObject->DeviceExtension;

    status = IoCreateSymbolicLink(&SymLinkName, &DeviceName);
    if (!NT_SUCCESS(status))
    {
        KdPrint(("[ProcMon] 符号链接无法创建, 0x%08x\n", status));
        IoDeleteDevice(g_DeviceObject);
        return status;
    }

    pDevExt->pDevObj = g_DeviceObject;
    pDevExt->ustrDevSymLinkName = SymLinkName;

    UNICODE_STRING KeEventName;
    RtlInitUnicodeString(&KeEventName, KEPROCEVENT);
    pDevExt->pKeEvent = IoCreateSynchronizationEvent(&KeEventName, &pDevExt->hKeEvent);
    if (!pDevExt->pKeEvent)
    {
        KdPrint(("[ProcMon] 同步事件1创建失败\n"));
        IoDeleteDevice(g_DeviceObject);
        IoDeleteSymbolicLink(&SymLinkName);
        return STATUS_UNSUCCESSFUL;
    }

    KeClearEvent(pDevExt->pKeEvent);

    g_pUserEvent = 0;

    UNICODE_STRING p;
    RtlInitUnicodeString(&p, THREADNOTIFYEVENTNAME);
    pThreadNotifyEvent = IoCreateSynchronizationEvent(&p, &hThreadNotifyEvent);
    if (!pThreadNotifyEvent)
    {
        KdPrint(("[ProcMon] 同步事件2创建失败\n"));
        IoDeleteDevice(g_DeviceObject);
        IoDeleteSymbolicLink(&SymLinkName);
        ZwClose(pDevExt->hKeEvent);
        return STATUS_UNSUCCESSFUL;
    }


//-----------------------------------------------------------------------------
// 通过特征码获取 PspTerminateThreadByPointer 地址

#ifndef WINXP
    ULONG32 callcode = 0;
    ULONG64 AddressOfPsTST = 0;

    UNICODE_STRING FuncName;
    RtlInitUnicodeString(&FuncName, L"PsTerminateSystemThread");

    AddressOfPsTST = (ULONG64)MmGetSystemRoutineAddress(&FuncName);
    if(!AddressOfPsTST)
        status = STATUS_UNSUCCESSFUL;
    else
    {
        for (int i=1; i<0xff; ++i)
        {
            if (MmIsAddressValid((PVOID)(AddressOfPsTST+i)))
            {
                if (*(PUCHAR)(AddressOfPsTST + i)  == 0x01 &&
                   *(PUCHAR)(AddressOfPsTST + i + 1) == 0xe8)
                {
                    RtlMoveMemory(&callcode, (PVOID)(AddressOfPsTST+i+2), 4);
                    PspTerminateThreadByPointer = (PSPTERMINATETHREADBYPOINTER)
                        ((ULONG64)callcode + 5 + AddressOfPsTST+i+1);
                    break;
                }
            }
        }
    }
#else
    KdPrint(("PsTerminateSystemThread, 0x%08x\n", PsTerminateSystemThread));
    for (int i=1; i<0x50; ++i)
    {
        if (MmIsAddressValid((PUCHAR)PsTerminateSystemThread + i))
        {
            if (*((PUCHAR)PsTerminateSystemThread + i) == 0x50 &&
                *((PUCHAR)PsTerminateSystemThread + i + 1) == 0xe8)
            {
                PUCHAR call = (PUCHAR)PsTerminateSystemThread + i + 1;
                PspTerminateThreadByPointer = (PSPTERMINATETHREADBYPOINTER)
                    (*(PLONG)(call + 1) + (ULONG)call + 5);
                break;
            }
        }
    }
#endif

    if (!PspTerminateThreadByPointer)
        KdPrint(("[ProcMon] 找不到 PspTerminateThreadByPointer\n"));
    else
        KdPrint(("[ProcMon] PspTerminateThreadByPointer: 0x%08x\n", PspTerminateThreadByPointer));

//-----------------------------------------------------------------------------
//  挂钩进/线程回调函数

//*****************************************************************************
//
// 因为PsSetCreateProcessNotifyRoutineEx的存在,编译生成的sys文件需要
// 在PE文件头中的DllCharacteristics成员中设置
// IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY(0x80)位(距'PE\0\0'标志偏移0x5e)
// 否则PsSetCreateProcessNotifyRoutineEx会返回拒绝访问的错误
//
//*****************************************************************************

#ifndef WINXP
    status = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyEx, FALSE);
#else
    status = PsSetCreateProcessNotifyRoutine(CreateProcessNotify, FALSE);

#endif
    if (!NT_SUCCESS(status))
    {
        KdPrint(("[ProcMon] PsSetCreateProcessNotifyRoutineEx注册失败\n"));
        IoDeleteDevice(g_DeviceObject);
        IoDeleteSymbolicLink(&SymLinkName);
        ZwClose(pDevExt->hKeEvent);
        ZwClose(hThreadNotifyEvent);
        return status;
    }


    status = PsSetCreateThreadNotifyRoutine(CreateThreadNotify);
    if (!NT_SUCCESS(status))
    {
        KdPrint(("[ProcMon] PsSetCreateProcessNotifyRoutineEx注册失败\n"));
        IoDeleteDevice(g_DeviceObject);
        IoDeleteSymbolicLink(&SymLinkName);
        ZwClose(pDevExt->hKeEvent);
        ZwClose(hThreadNotifyEvent);
        PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyEx, TRUE);
        return status;
    }

    g_DisableTPCreationOn = FALSE;
    g_MonitoringOn = FALSE;


    KdPrint(("[ProcMon] 进程监控驱动加载完毕[0x%08x]\n", status));
    return status;
}


///////////////////////////////////////////////////////////////////////////////


BOOLEAN IsWIN7()
{
    RTL_OSVERSIONINFOW osVer;

    RtlGetVersion(&osVer);

    KdPrint(("Major: %d, Minor: %d\n",
        osVer.dwMajorVersion, osVer.dwMinorVersion));

    return osVer.dwMajorVersion == 6 ? TRUE : FALSE;
}


///////////////////////////////////////////////////////////////////////////////



#ifdef WINXP

BOOLEAN
SD_PsGetNextProcessThread(
    PEPROCESS eProcess,
    PETHREAD eThread,
    HANDLE Tid
)
{
    PETHREAD FoundThread;
    PLIST_ENTRY Entry;
    PLIST_ENTRY ThreadListEntry;
    PLIST_ENTRY ListHead;

__try{
    if (eThread)
    {
        ThreadListEntry = (PLIST_ENTRY)((ULONG)eThread + 0x22c);    // ThreadListHead
        Entry = ThreadListEntry->Flink;
    }
    else
    {
        ThreadListEntry = (PLIST_ENTRY)((ULONG)eProcess + 0x190);    // ThreadListHead
        Entry = ThreadListEntry->Flink;
    }

    ListHead = (PLIST_ENTRY)((ULONG)eProcess + 0x190);    // ThreadListHead

    while (ListHead != Entry)
    {
        FoundThread = (PETHREAD)((ULONG)Entry - 0x22c);    // ThreadListHead

        //KdPrint(("[SD_PsGetNextProcessThread] Tid: %d\n", *(PULONG)((PUCHAR)FoundThread + 0x1f0)));
        if (*(PULONG)((PUCHAR)FoundThread + 0x1f0) == (ULONG)Tid)    // CLIENT_ID->TID
        {
            KdPrint(("终止线程: %08x\n", FoundThread));

            PspTerminateThreadByPointer(FoundThread, 0);
            return TRUE;
        }

        Entry = Entry->Flink;
    }

}__except(1) { KdPrint(("[SD_PsGetNextProcessThread] Error\n")); }

    return FALSE;
}

#else

BOOLEAN
SD_PsGetNextProcessThread(
    PEPROCESS eProcess,
    PETHREAD eThread,
    HANDLE Tid
)
{
    PETHREAD FoundThread;
    PLIST_ENTRY Entry;
    PLIST_ENTRY ThreadListEntry;
    PLIST_ENTRY ListHead;

__try{
    if (eThread)
    {
        ThreadListEntry = (PLIST_ENTRY)((ULONG)eThread + 0x420);    // ethread->ThreadListEntry
        Entry = ThreadListEntry->Flink;
    }
    else
    {
        ThreadListEntry = (PLIST_ENTRY)((ULONG)eProcess + 0x300);    // ThreadListHead
        Entry = ThreadListEntry->Flink;
    }

    ListHead = (PLIST_ENTRY)((ULONG)eProcess + 0x300);    // ThreadListHead

    while (ListHead != Entry)
    {
        FoundThread = (PETHREAD)((ULONG)Entry - 0x420);    // ethread->ThreadListEntry

        //KdPrint(("[SD_PsGetNextProcessThread] Tid: %d\n", *(PULONG)((PUCHAR)FoundThread + 0x1f0)));
        if (*(PULONG)((PUCHAR)FoundThread + 0x3b0) == (ULONG)Tid)    // CLIENT_ID->TID
        {
            KdPrint(("终止线程: %08x\n", FoundThread));

            PspTerminateThreadByPointer(FoundThread, 0, TRUE);
            return TRUE;
        }

        Entry = Entry->Flink;
    }

}__except(1) { KdPrint(("[SD_PsGetNextProcessThread] Error\n")); }

    return FALSE;
}

#endif



VOID CreateThreadNotify(
    IN HANDLE  ProcessId,
    IN HANDLE  ThreadId,
    IN BOOLEAN  Create
    )
{
    if (g_DisableTPCreationOn && Create)
    {
        // KeSetEvent(pThreadNotifyEvent, IO_NO_INCREMENT, FALSE);
        PEPROCESS ep = LookupProcess(ProcessId);
        if (ep)
        {
            KdPrint(("[CreateThreadNotify] 准备终止: %d\n", ThreadId));
            SD_PsGetNextProcessThread(ep, 0, ThreadId);
        }
        else
            KdPrint(("[CreateThreadNotify] ep==0\n"));
    }

/*    KeWaitForSingleObject(pThreadNotifyEvent, Executive, KernelMode, FALSE, NULL);

    Create ? KdPrint(("[CreateThread]\n")) : KdPrint(("[DeleteThread]\n"));
    PEPROCESS pep = LookupProcess(ProcessId);
    if (pep)
    {
        PUNICODE_STRING p = GetProcessFullImageFileName(pep);
        if (p)
            // 已存在的进程创建新线程
            KdPrint(("\t[Pid] %d\n\t[Proc] %ws\n\t[Tid] %d", ProcessId, p->Buffer, ThreadId));
        else
            // 新进程启动，并创建其主线程
            KdPrint(("\t[Pid] %d\n\t[Proc] ???\n\t[Tid] %d", ProcessId, ThreadId));
    }

    KeSetEvent(pThreadNotifyEvent, IO_NO_INCREMENT, FALSE);*/
}


///////////////////////////////////////////////////////////////////////////////


PEPROCESS LookupProcess(HANDLE Pid)
{
  PEPROCESS eProcess = NULL;
  if (NT_SUCCESS(PsLookupProcessByProcessId(Pid, &eProcess)))
    return eProcess;
  else
    return NULL;
}


///////////////////////////////////////////////////////////////////////////////

#ifdef WINXP
PUNICODE_STRING GetProcessFullImageFileName(PEPROCESS pep)
{
    return *(PUNICODE_STRING*)((PUCHAR)pep + 0x1f4);
}
#else
PUNICODE_STRING GetProcessFullImageFileName(PEPROCESS pep)
{
    // 被Windbg坑了一下,在我机子上显示的是0x388,
    // 翻了一下发现是偏移为0x2d0的Session显示为0x2c8了,是符号有错？
    return *(PUNICODE_STRING*)((PUCHAR)pep + 0x390);
}
#endif

// 用的是模块枚举的一部分
BOOLEAN GetProcessUserModeFullImageFileName(
    IN PEPROCESS pep,
    OUT wchar_t *Buffer,
    OUT int cch
    )
{
    BOOLEAN ret = FALSE;
    PPEB Peb = PsGetProcessPeb(pep);
    KAPC_STATE ks;
    KeStackAttachProcess((PRKPROCESS)pep, &ks);
    if (Peb && Peb->Ldr)
    {
        PLIST_ENTRY Module = Peb->Ldr->InLoadOrderModuleList.Flink;
        if (Module)
        {
            ret = TRUE;
            int copyByte = ((PLDR_DATA_TABLE_ENTRY)Module)->FullDllName.Length;
            if (copyByte >= cch*2)
                copyByte = cch * 2 - 2;

            memset(Buffer, 0, cch*2);
            memcpy(Buffer,
                ((PLDR_DATA_TABLE_ENTRY)Module)->FullDllName.Buffer,
                copyByte);
        }
    }
    KeUnstackDetachProcess(&ks);

    return ret;
}

HANDLE GetProcessNameByPID(IN HANDLE PID, OUT PUNICODE_STRING pName)
{
    HANDLE hChildHandle;

    if (!pName) return NULL;
    __try
    {
        memset(pName, 0, sizeof(UNICODE_STRING));

        CLIENT_ID ClientID;
        ClientID.UniqueProcess = PID;
        ClientID.UniqueThread  = 0;

        OBJECT_ATTRIBUTES oa;
        InitializeObjectAttributes(&oa, 0, OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE, 0, 0);

        ZwOpenProcess(&hChildHandle, PROCESS_ALL_ACCESS, &oa, &ClientID);

        ULONG SizeNeed;
        ZwQueryInformationProcess(hChildHandle, ProcessImageFileName,
            NULL, 0, &SizeNeed);
        PVOID Buffer = ExAllocatePoolWithTag(PagedPool, SizeNeed, 'ggaT');
        ZwQueryInformationProcess(hChildHandle, ProcessImageFileName,
            Buffer, SizeNeed, &SizeNeed);

        RtlInitUnicodeString(pName, ((PUNICODE_STRING)Buffer)->Buffer);
        ExFreePool(Buffer);
        ZwClose(hChildHandle);
    }
    __except(1){ }
    return NULL;
}
///////////////////////////////////////////////////////////////////////////////


//枚举进程
PVOID EnumProcess(OUT PULONG pcnt)
{
    static const int ProcessNum = 1024;
    PEPROCESS     eProcess = NULL;
    PENUMPROCINFO Buf;
    ULONG         cnt = 0;

    if (pcnt) *pcnt = 0;
    if (!(Buf = (PENUMPROCINFO)ExAllocatePoolWithTag(PagedPool,
        sizeof(ENUMPROCINFO)*ProcessNum, 'rPmE')))
        return 0;
    memset(Buf, 0, sizeof(ENUMPROCINFO)*ProcessNum);

    for(ULONG i=4; i<262144; i+=4)    // 2^18, ID号在这个范围差不多了
    {
        eProcess = LookupProcess((HANDLE)i);
        if(eProcess != NULL)
        {   // 退出时间不为0或句柄表为空，都表示进程已退出
            //*(PULONG)((char*)eProcess+0x78) == NULL ||
#ifdef WINXP
            if (*(PULONG)((PUCHAR)eProcess + 0xc4))
#else
            if (*(PULONG)((PUCHAR)eProcess + 0x200))
#endif
            {
                Buf[cnt].eProcessAddr = (ULONG64)eProcess;
                Buf[cnt].Pid = (ULONG)PsGetProcessId(eProcess);
                Buf[cnt].PPid = (ULONG)PsGetProcessInheritedFromUniqueProcessId(eProcess);
                memcpy(Buf[cnt].ShortName, PsGetProcessImageFileName(eProcess), 16);

                PUNICODE_STRING p = 0;
                if (p = GetProcessFullImageFileName(eProcess))
                    memcpy(Buf[cnt].ImageName, p->Buffer, p->Length);

                GetProcessUserModeFullImageFileName(eProcess,
                    Buf[cnt].ImageName, 1024);

                if (cnt < ProcessNum-1) ++cnt;
            }
            //KdPrint(("EPROCESS=%p, PID=%ld, PPID=%ld, Name=%s\n",
            //    eProcess, (ULONG)PsGetProcessId(eProcess),
            //    (ULONG)PsGetProcessInheritedFromUniqueProcessId(eProcess),
            //    PsGetProcessImageFileName(eProcess)));
            ObDereferenceObject(eProcess);
        }
    }
    if (pcnt) *pcnt = cnt;
    return Buf;
}


///////////////////////////////////////////////////////////////////////////////


void ZwKillByMemClear(PEPROCESS eProcess)
{
    KAPC_STATE ks;

    KdPrint(("ZwKillByMemClear Called!\n"));

    KeStackAttachProcess((PRKPROCESS)eProcess, &ks);

    for(ULONG i=0x10000; i<0x20000000; i+=PAGE_SIZE)
    {
        __try
        {
          memset((PVOID)i,0,PAGE_SIZE); //把进程内存全部置零
        }
        _except(1) { }
    }

    KeUnstackDetachProcess(&ks);
}

NTSTATUS ZwKillProcess(HANDLE Pid)
{
    NTSTATUS status;
    HANDLE hProcess = NULL;
    CLIENT_ID ClientId;
    OBJECT_ATTRIBUTES oa;

    ClientId.UniqueProcess = Pid;
    ClientId.UniqueThread = 0;

    // InitializeObjectAttributes(&oa, 0, OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE, 0, 0);
    oa.Length = sizeof(oa);
    oa.RootDirectory = 0;
    oa.ObjectName = 0;
    oa.Attributes = 0;
    oa.SecurityDescriptor = 0;
    oa.SecurityQualityOfService = 0;

    status = ZwOpenProcess(&hProcess, 1, &oa, &ClientId);    // PROCESS_TERMINATE
    if(hProcess)
    {
        status = ZwTerminateProcess(hProcess, 0);
        ZwClose(hProcess);
    }
    else
        KdPrint(("ZwKillProcess 进程打不开\n"));

    if (!NT_SUCCESS(status))
    {
        PEPROCESS pep = LookupProcess(Pid);
        if (pep)
            ZwKillByMemClear(pep);
    }

    return status;
}


///////////////////////////////////////////////////////////////////////////////


PETHREAD LookupThread(HANDLE Tid)
{
    PETHREAD eThread;
    if(NT_SUCCESS(PsLookupThreadByThreadId(Tid, &eThread)))
        return eThread;
    else
        return NULL;
}


///////////////////////////////////////////////////////////////////////////////


PVOID EnumThread(PEPROCESS Process, OUT PULONG pcnt)
{
    PETHREAD eThread = NULL;
    PEPROCESS eProcess = NULL;
    PENUMTHREADINFO Buf = 0;
    if (pcnt) *pcnt = 0;
    Buf = (PENUMTHREADINFO)ExAllocatePoolWithTag(PagedPool,
        sizeof(ENUMTHREADINFO) * 512, 'hTnE');
    if (!Buf)
        return 0;
    ULONG cnt = 0;
    KdPrint(("Begin Enum Threads in (%#llx)\n", Process));
    for(ULONG i=4; i<262144; i+=4)        // 2^18
    {
        eThread = LookupThread((HANDLE)i);
#ifdef WINXP
        if(eThread != NULL && !(*((PUCHAR)eThread + 0x248) & 1))    // 线程未退出
        {
            //获得线程所属进程
            eProcess = IoThreadToProcess(eThread);
            if(eProcess == Process)
            {
                Buf[cnt].eThreadAddr = (ULONG64)eThread;
                Buf[cnt].Tid = (ULONG)PsGetThreadId(eThread);
                Buf[cnt].Priority = (ULONG)(*((char*)eThread + 0x33) - *((char*)eThread + 0x6e));
                if (cnt < 511) ++cnt;
                DbgPrint("eThread=%p, TID=%ld, Priority=%d, BasePri=%d, PriDec=%d\n", eThread,
                    (ULONG)PsGetThreadId(eThread), *((char*)eThread + 0x33),
                    *((char*)eThread + 0x6c), *((char*)eThread + 0x6e));
            }
            ObDereferenceObject(eThread);
        }
#else
        if(eThread != NULL && !(*((PUCHAR)eThread + 0x448) & 1))    // 线程未退出
        {
            //获得线程所属进程
            eProcess = IoThreadToProcess(eThread);
            // DbgPrint("可能的eProcess: %#llx\n", eProcess);
            if(eProcess == Process)
            {
                Buf[cnt].eThreadAddr = (ULONG64)eThread;
                Buf[cnt].Tid = (ULONG)PsGetThreadId(eThread);
                Buf[cnt].Priority = (ULONG)(*((char*)eThread + 0x7b));
                if (cnt < 511) ++cnt;
                DbgPrint("eThread=%p, TID=%ld, Priority=%d, BasePri=%d, PriDec=%d, Adj=%d\n",
                    eThread,(ULONG)PsGetThreadId(eThread), *((char*)eThread + 0x7b),
                    *((char*)eThread + 0x1f1), *((char*)eThread + 0x1f2),
                    *((char*)eThread + 0x1f5));
            }
            ObDereferenceObject(eThread);
        }
#endif
    }
    KdPrint(("Thread num: %d\n", cnt));
    if (pcnt) *pcnt = cnt;
    return Buf;
}


///////////////////////////////////////////////////////////////////////////////


BOOLEAN ZwKillThread(HANDLE Tid)
{
    BOOLEAN ret = FALSE;
    PETHREAD eThread = LookupThread(Tid);
    KdPrint(("[ZwKillThread] Tid: %d, ETHREAD:%08X\n", Tid, eThread));
    if (eThread && PspTerminateThreadByPointer)
    {
#ifndef WINXP
        PspTerminateThreadByPointer(eThread, 0, TRUE);
#else
        PspTerminateThreadByPointer(eThread, 0);
#endif
        ret = TRUE;
    }
    return ret;
}


///////////////////////////////////////////////////////////////////////////////


// DLL模块记录在PEB的LDR链表里, LDR是一个双向链表，枚举它即可。
// 另外，DLL模块列表包含EXE的相关信息。换句话说，枚举DLL模块即可实现枚举进程路径。
//声明偏移
//ULONG64 LdrInPebOffset = 0x018;    //peb.ldr
//ULONG64 ModListInPebOffset = 0x010;  //peb.ldr.InLoadOrderModuleList

//声明API
NTKERNELAPI PPEB PsGetProcessPeb(PEPROCESS Process);

//声明结构体

/*
typedef struct _LDR_DATA_TABLE_ENTRY
{
    LIST_ENTRY64 InLoadOrderLinks;
    LIST_ENTRY64 InMemoryOrderLinks;
    LIST_ENTRY64 InInitializationOrderLinks;
    PVOID      DllBase;
    PVOID      EntryPoint;
    ULONG      SizeOfImage;
    UNICODE_STRING  FullDllName;
    UNICODE_STRING   BaseDllName;
    ULONG      Flags;
    USHORT      LoadCount;
    USHORT      TlsIndex;
    PVOID      SectionPointer;
    ULONG      CheckSum;
    PVOID      LoadedImports;
    PVOID      EntryPointActivationContext;
    PVOID      PatchInformation;
    LIST_ENTRY64 ForwarderLinks;
    LIST_ENTRY64 ServiceTagLinks;
    LIST_ENTRY64 StaticLinks;
    PVOID      ContextInformation;
    ULONG64      OriginalBase;
    LARGE_INTEGER  LoadTime;
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;
 */


///////////////////////////////////////////////////////////////////////////////


//根据进程枚举模块
PVOID EnumModule(PEPROCESS Process, OUT PULONG pcnt)
{
    static const int ModuleNum = 1024;
    PPEB Peb = 0;
    ULONG64 Ldr = 0;
    PLIST_ENTRY ModListHead = 0;
    PLIST_ENTRY Module = 0;
    ANSI_STRING AnsiString;
    KAPC_STATE ks;

    if (pcnt) *pcnt = 0;

    if(!MmIsAddressValid(Process))
    {
        KdPrint(("Enum Module Failed 1\n"));
        return 0;
    }
    if (!(Peb = PsGetProcessPeb(Process)))
    {
        KdPrint(("Enum Module Failed 2\n"));
        return 0;
    }


    PENUMMODULEINFO Buf = (PENUMMODULEINFO)ExAllocatePoolWithTag(PagedPool,
        sizeof(ENUMMODULEINFO)*ModuleNum, 'oMnE');
    if (!Buf)
        return 0;
    else
        memset(Buf, 0, sizeof(ENUMMODULEINFO)*ModuleNum);

    KdPrint(("Enum Module Begin\n"));

    int cnt = 0;
    KeStackAttachProcess((PRKPROCESS)Process, &ks);
    __try{
        if (!Peb->Ldr)
        {
            ExFreePool(Buf);
            Buf = 0;
            goto EXIT;
        }
        KdPrint(("PEB:%08x, LDR:%08x, InLoadOrderModuleList:%08x\n",
            Peb, Peb->Ldr, &Peb->Ldr->InLoadOrderModuleList));

        //获得链表头
        ModListHead = &Peb->Ldr->InLoadOrderModuleList;
        Module = ModListHead->Flink;
        while (ModListHead != Module && Module)
        {
            //打印信息：基址、大小、DLL路径
            Buf[cnt].Base = (ULONG64)((PLDR_DATA_TABLE_ENTRY)Module)->DllBase;
            Buf[cnt].Size = (ULONG)((PLDR_DATA_TABLE_ENTRY)Module)->SizeOfImage;
            memcpy(Buf[cnt].Path,
                ((PLDR_DATA_TABLE_ENTRY)Module)->FullDllName.Buffer,
                ((PLDR_DATA_TABLE_ENTRY)Module)->FullDllName.Length);
            if (cnt < ModuleNum-1) ++cnt;

            KdPrint(("Base=%08x, Size=%08x, Path=%ws\n",
                (ULONG)((PLDR_DATA_TABLE_ENTRY)Module)->DllBase,
                (ULONG)(((PLDR_DATA_TABLE_ENTRY)Module)->SizeOfImage),
                ((PLDR_DATA_TABLE_ENTRY)Module)->FullDllName.Buffer));

            Module = Module->Flink;
        }

        if (pcnt) *pcnt = cnt;
        KdPrint(("共计：%d\n", cnt));

    }
    __except(1)
    {
        KdPrint(("EnumMoudule Error\n"));
        ExFreePool(Buf);
        Buf = 0;
    }
EXIT:
    KeUnstackDetachProcess(&ks);
    return Buf;
}


///////////////////////////////////////////////////////////////////////////////


NTSTATUS UnloadModule(HANDLE pid, PVOID base)
{
    PEPROCESS eProcess;
    KdPrint(("UnloadModule [%d, 0x%08x]\n", pid, (ULONG)base));
    if (!NT_SUCCESS(PsLookupProcessByProcessId(pid, &eProcess)))
        return STATUS_INVALID_PARAMETER;

    return MmUnmapViewOfSection(eProcess, base);
}

///////////////////////////////////////////////////////////////////////////////

#ifdef WINXP

VOID CreateProcessNotify(
    IN HANDLE  ParentId,
    IN HANDLE  ProcessId,
    IN BOOLEAN Create
    )
{
    KdPrint((Create ? "[create] " : "[delete] "));

    PEPROCESS  peProc = PsGetCurrentProcess();
    KdPrint(("Image Name: %s, PID = %d\n", (char*)peProc + 0x174, (long)ProcessId));

    if (!Create) return;

    if (g_DisableTPCreationOn)
    {
        ZwKillProcess(ProcessId);
        KdPrint(("阻止新进程创建: PPID=%d, PID=%d", ParentId, ProcessId));
        return;
    }

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)g_DeviceObject->DeviceExtension;
    if (g_MonitoringOn)
    {
        PROCCREATIONINFO pci;

        LARGE_INTEGER st, lt;
        TIME_FIELDS tf;
        KeQuerySystemTime(&st);
        ExSystemTimeToLocalTime(&st, &lt);
        RtlTimeToTimeFields(&lt, &tf);

        pDevExt->ProcCreationInfo.Year        = tf.Year;
        pDevExt->ProcCreationInfo.Month        = tf.Month;
        pDevExt->ProcCreationInfo.Day        = tf.Day;
        pDevExt->ProcCreationInfo.Hour        = tf.Hour;
        pDevExt->ProcCreationInfo.Minute    = tf.Minute;
        pDevExt->ProcCreationInfo.Second    = tf.Second;

        UNICODE_STRING ImageNameChild;
        GetProcessNameByPID(ProcessId, &ImageNameChild);

        PEPROCESS eProcessParent = LookupProcess(ParentId);
        PCUNICODE_STRING pImageNameParent =
            GetProcessFullImageFileName(eProcessParent);

//        KdPrint(("Parent: %08x\nChild: %08x\n",ImageNameParent, ImageNameChild));
//        if (! ImageNameChild || !ImageNameParent)
//            return;

        KdPrint(("Parent: %S\nChild: %S\n",
            pImageNameParent->Buffer, ImageNameChild.Buffer));

        wcscpy(pDevExt->ProcCreationInfo.ProcessImage, ImageNameChild.Buffer);
        wcscpy(pDevExt->ProcCreationInfo.ParentProcImage, pImageNameParent->Buffer);

        pDevExt->ProcCreationInfo.ProcessID    = (ULONG)ProcessId;
        pDevExt->ProcCreationInfo.ParentProcID = (ULONG)ParentId;


        if (g_pUserEvent)
            KeSetEvent(g_pUserEvent, IO_NO_INCREMENT, FALSE);
    }
}

#else

VOID CreateProcessNotifyEx(
    PEPROCESS              Process,
    HANDLE                 ProcessId,
    PPS_CREATE_NOTIFY_INFO CreateInfo
    )
{
    if (!CreateInfo)
        return;

    if (g_DisableTPCreationOn)
    {
        CreateInfo->CreationStatus = STATUS_UNSUCCESSFUL;
        KdPrint(("阻止新进程创建: PID=%d", ProcessId));
        return;
    }

    if (!g_MonitoringOn)
        return;

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)g_DeviceObject->DeviceExtension;

    PsSuspendProcess(Process);


    PROCCREATIONINFO pci = {0};
    memset(&pDevExt->ProcCreationInfo, 0, sizeof(PROCCREATIONINFO));

    LARGE_INTEGER st, lt;
    TIME_FIELDS tf;
    KeQuerySystemTime(&st);
    ExSystemTimeToLocalTime(&st, &lt);
    RtlTimeToTimeFields(&lt, &tf);

    pDevExt->ProcCreationInfo.Year        = tf.Year;
    pDevExt->ProcCreationInfo.Month        = tf.Month;
    pDevExt->ProcCreationInfo.Day        = tf.Day;
    pDevExt->ProcCreationInfo.Hour        = tf.Hour;
    pDevExt->ProcCreationInfo.Minute    = tf.Minute;
    pDevExt->ProcCreationInfo.Second    = tf.Second;


    PCUNICODE_STRING ImageNameChild = 0;
    if (CreateInfo->Flags & 1)
        ImageNameChild = CreateInfo->ImageFileName;
    else
        ImageNameChild = GetProcessFullImageFileName(Process);

    PUNICODE_STRING ImageNameParent = GetProcessFullImageFileName(
        PsGetCurrentProcess());


    KdPrint(("[ProcMon] CreateProcessNotifyEx:\n\tParent(%d): %wZ\n\tChild(%d): %wZ\n",
        CreateInfo->ParentProcessId, ImageNameParent,
        ProcessId, ImageNameChild));
    __try {
        if (ImageNameChild)
            memcpy(pDevExt->ProcCreationInfo.ProcessImage,
                ImageNameChild->Buffer, ImageNameChild->Length);
        if (ImageNameParent)
            memcpy(pDevExt->ProcCreationInfo.ParentProcImage,
                ImageNameParent->Buffer, ImageNameParent->Length);
        pDevExt->ProcCreationInfo.ParentProcID = (ULONG)CreateInfo->ParentProcessId;
        pDevExt->ProcCreationInfo.ProcessID    = (ULONG)ProcessId;

    } __except(1)
    {
        KdPrint(("[ProcMon] CreateProcessNotifyEx (memcpy) Error\n"));
    }

    if (g_pUserEvent)
        KeSetEvent(g_pUserEvent, IO_NO_INCREMENT, FALSE);
}

#endif

///////////////////////////////////////////////////////////////////////////////


NTSTATUS OnCreate(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
    pIrp->IoStatus.Status = STATUS_SUCCESS;
    pIrp->IoStatus.Information = 0;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}


VOID DriverUnloadRoutine(IN PDRIVER_OBJECT pDriObj)
{
#ifdef WINXP
    PsSetCreateProcessNotifyRoutine(CreateProcessNotify, TRUE);
#else
    PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyEx, TRUE);
#endif

    PsRemoveCreateThreadNotifyRoutine(CreateThreadNotify);

    ZwClose(hThreadNotifyEvent);

    if (((PDEVICE_EXTENSION)g_DeviceObject->DeviceExtension)->hKeEvent)
        ZwClose(((PDEVICE_EXTENSION)g_DeviceObject->DeviceExtension)->hKeEvent);
    if (g_hUserEvent)
        ZwClose(g_hUserEvent);
    if (g_pUserEvent)
        ObDereferenceObject(g_pUserEvent);


    PDEVICE_OBJECT pNextObj = pDriObj->DeviceObject;
    while (pNextObj)
    {
        PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
            pNextObj->DeviceExtension;

        UNICODE_STRING pLinkName = pDevExt->ustrDevSymLinkName;
        IoDeleteSymbolicLink(&pLinkName);
        IoDeleteDevice(pDevExt->pDevObj);

        pNextObj = pNextObj->NextDevice;
    }

    KdPrint(("进程驱动成功卸载\n"));
}


///////////////////////////////////////////////////////////////////////////////


NTSTATUS DeviceIoControl(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
    NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_EXTENSION pExt = (PDEVICE_EXTENSION)g_DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION stk = IoGetCurrentIrpStackLocation(pIrp);
    ULONG cbIn  = stk->Parameters.DeviceIoControl.InputBufferLength;
    ULONG cbOut = stk->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG code  = stk->Parameters.DeviceIoControl.IoControlCode;
    ULONG info  = 0;

    switch (code)
    {
    case IOCTL_TURNON_MONITORING:
        g_MonitoringOn = TRUE;
        break;

    case IOCTL_TURNOFF_MONITORING:
        g_MonitoringOn = FALSE;
        KeSetEvent(pExt->pKeEvent, IO_NO_INCREMENT, FALSE);

        KdPrint(("进程监控关闭.\n"));
        break;

    case IOCTL_GETPROCINFO:
        if (cbOut < sizeof(PROCCREATIONINFO))
        {
            KdPrint(("Buffer Too Small\n"));
            status = STATUS_BUFFER_TOO_SMALL;
        }
        else if (g_MonitoringOn)
        {
/*            KdPrint(("\tParent: %S\n\tChild: %S\n\tPID:%d\n\tPPID:%d\n\tHANDLE: 0x%08x",
                pExt->ProcCreationInfo.ParentProcImage,    pExt->ProcCreationInfo.ProcessImage,
                pExt->ProcCreationInfo.ProcessID, pExt->ProcCreationInfo.ParentProcID,
                pExt->ProcCreationInfo.hProcess));
            KdPrint(("size=%d\n", sizeof(PROCCREATIONINFO)));*/
            __try{
                memcpy(pIrp->AssociatedIrp.SystemBuffer, &pExt->ProcCreationInfo,
                    sizeof(PROCCREATIONINFO));
                info = sizeof(PROCCREATIONINFO);
            }
            __except(1)
            {
                status = STATUS_UNSUCCESSFUL;
            }

        }
        break;

    case IOCTL_RECEIVE_USER_EVENT:
        g_hUserEvent = *(HANDLE*)pIrp->AssociatedIrp.SystemBuffer;
        ObReferenceObjectByHandle(g_hUserEvent, EVENT_MODIFY_STATE, *ExEventObjectType,
            UserMode, (PVOID*)&g_pUserEvent, NULL);
        KdPrint(("User Event Receive! [%d]\n", g_pUserEvent ? 1 : 0));
        break;

    case IOCTL_TURNON_TP_CREATION_DISABLE:
        g_DisableTPCreationOn = TRUE;
        KdPrint(("禁止创建打开\n"));
        break;

    case IOCTL_TURNOFF_TP_CREATION_DISABLE:
        g_DisableTPCreationOn = FALSE;
        KdPrint(("禁止创建关闭\n"));
        break;

    case IOCTL_ENUMPROCESS:
    {
        ULONG num = 0;
        PVOID Buf = EnumProcess(&num);
        if (Buf)
        {
            memcpy(pIrp->AssociatedIrp.SystemBuffer, Buf, num*sizeof(ENUMPROCINFO));
            info = num*sizeof(ENUMPROCINFO);
            ExFreePool(Buf);
        }
        else
            status = STATUS_UNSUCCESSFUL;
    }
        break;

    case IOCTL_ENUMTHREAD:
    case IOCTL_ENUMMODULE:
    {
        ULONG num = 0;
        PEPROCESS pe;
        KdPrint(("pid %I64u, Size %d, ", *(ULONG*)pIrp->AssociatedIrp.SystemBuffer, cbIn));
        if (!NT_SUCCESS(status = PsLookupProcessByProcessId(
            *(HANDLE*)pIrp->AssociatedIrp.SystemBuffer, &pe)))
        {
            KdPrint(("PsLookupProcessByProcessId错误(0x%08x)\n", status));
            break;
        }
        PVOID Buf = 0;
        if (code == IOCTL_ENUMTHREAD)
        {
            KdPrint(("eProcess: %#llx\n", pe));
            if (Buf = EnumThread(pe, &num))
            {
                memcpy(pIrp->AssociatedIrp.SystemBuffer, Buf, num*sizeof(ENUMTHREADINFO));
                info = num*sizeof(ENUMTHREADINFO);
            }
            else
                status = STATUS_UNSUCCESSFUL;
        }
        else
        {
            if (Buf = EnumModule(pe, &num))
            {
                memcpy(pIrp->AssociatedIrp.SystemBuffer, Buf, num*sizeof(ENUMMODULEINFO));
                info = num*sizeof(ENUMMODULEINFO);
            }
            else
                status = STATUS_UNSUCCESSFUL;
        }

        ObDereferenceObject(pe);
        if (Buf) ExFreePool(Buf);
    }
        break;

    case IOCTL_TERMINATEPROCESS:
        status = ZwKillProcess(*(HANDLE*)pIrp->AssociatedIrp.SystemBuffer);
        break;

    case IOCTL_TERMINATETHREAD:
        ZwKillThread(*(HANDLE*)pIrp->AssociatedIrp.SystemBuffer);
        break;

    case IOCTL_UNLOADMODULE:
        status = UnloadModule(*(HANDLE*)pIrp->AssociatedIrp.SystemBuffer,
            (PVOID)(*((HANDLE*)pIrp->AssociatedIrp.SystemBuffer + 1)));
        break;

    default:
        status = STATUS_INVALID_VARIANT;
        break;
    }

    pIrp->IoStatus.Status      = status;
    pIrp->IoStatus.Information = info;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);


    return status;
}



