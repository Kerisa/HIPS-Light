

#include "Myminifilter.h"


const FLT_OPERATION_REGISTRATION Callbacks[] = {
	{IRP_MJ_CREATE,	0, NPPreCreate, NULL, 0},
	{IRP_MJ_SET_INFORMATION, 0,	NPPreSetInformation, NULL, 0},
	//{IRP_MJ_READ, 0, NPPreRead,	NULL, 0},
	{IRP_MJ_WRITE, 0, NPPreWrite, NULL, 0},
	{IRP_MJ_OPERATION_END}
};

//This defines what we want to filter with FltMgr
const FLT_REGISTRATION FilterRegistration = {
	sizeof(FLT_REGISTRATION),	// Size
	FLT_REGISTRATION_VERSION,	// Version
	0,							// Flags
	NULL,						// Context
	Callbacks,					// Operation callbacks
	NPUnload,					// MiniFilterUnload
	NULL,						// InstanceSetup
	NULL,						// InstanceQueryTeardown
	NULL,						// InstanceTeardownStart
	NULL,						// InstanceTeardownComplete
	NULL,						// GenerateFileName
	NULL,						// GenerateDestinationFileName
	NULL						// NormalizeNameComponent
};


PFLT_FILTER g_pFilterHandle;
PFLT_PORT 	g_ServerPort;
PFLT_PORT 	g_ClientPort;

BOOLEAN	g_MonitoringOn;
HANDLE  hEventKtoU;
PKEVENT pEventKtoU;


MYQUEUE		   LogQueue;
VECTOR		   WhiteList;
VECTOR		   BlockList;



NTSTATUS DriverEntry (
	IN PDRIVER_OBJECT pDriObj,
	IN PUNICODE_STRING pRegPath
	)
{
	NTSTATUS status;
	PSECURITY_DESCRIPTOR sd;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING uniString;		// 通信端口名
    UNICODE_STRING UserEventName;


	UNREFERENCED_PARAMETER(pRegPath);

	//注册过滤器
	status = FltRegisterFilter(pDriObj, &FilterRegistration, &g_pFilterHandle);
	if (NT_SUCCESS(status))
	{
		//开启过滤器
		status = FltStartFiltering(g_pFilterHandle);
		if (!NT_SUCCESS(status))
		{
			FltUnregisterFilter(g_pFilterHandle); //失败则卸载过滤器
			g_pFilterHandle = NULL;
		}
	}


	InitQueue(&LogQueue);
	InitList(&BlockList, NULL, 0);
	InitList(&WhiteList, NULL, 0);

	g_MonitoringOn = FALSE;
	pEventKtoU = 0;
	hEventKtoU = 0;

	// 建立通信端口
	status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);
    if (!NT_SUCCESS(status))
       	goto final;

    RtlInitUnicodeString(&uniString, COMMUNICATE_PORT_NAME);
	InitializeObjectAttributes(&oa, &uniString,
		OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, sd);

	status = FltCreateCommunicationPort(g_pFilterHandle,
		&g_ServerPort, &oa, NULL, MyMiniConnect,
		MyMiniDisconnect, MyMiniMessage, 1);

    FltFreeSecurityDescriptor(sd);
    if (!NT_SUCCESS(status))
        goto final;


	
final :

    if (!NT_SUCCESS(status))
    {
         if (g_ServerPort) FltCloseCommunicationPort(g_ServerPort);
         if (g_pFilterHandle) FltUnregisterFilter(g_pFilterHandle);
    }


	KdPrint(("[MiniFilter] [DriverEntry] (0x%08x)(event:%08x)\n", status, pEventKtoU));
	return status;
}


///////////////////////////////////////////////////////////////////////////////


//卸载 MINIFILTER
NTSTATUS NPUnload (
    __in FLT_FILTER_UNLOAD_FLAGS Flags
    )
{
	UNREFERENCED_PARAMETER(Flags);
	PAGED_CODE();

	KdPrint(("[MiniFilter] [DriverUnload]\n"));

	g_MonitoringOn = FALSE;

	if (pEventKtoU) ObDereferenceObject(pEventKtoU);
	if (hEventKtoU) ZwClose(hEventKtoU);
	
	FreeList(&BlockList);
	FreeList(&WhiteList);
	FreeQueue(&LogQueue);

	FltCloseCommunicationPort(g_ServerPort);
	FltUnregisterFilter(g_pFilterHandle); //卸载

	return STATUS_SUCCESS;
}


///////////////////////////////////////////////////////////////////////////////


VOID MakeUserModeName(
	IN  PVOID FileObject,
	IN  PFLT_FILE_NAME_INFORMATION pnameInfo,
	IN  int Cch,
	OUT wchar_t *pOut
	)
{
	NTSTATUS status;
	UNICODE_STRING DosName = {0};
	int use = 0;

	if (!FileObject || !pnameInfo || !pOut || Cch<=0 ||
		pnameInfo->Name.MaximumLength >= Cch)
		return;

	if (pnameInfo->Format != FLT_FILE_NAME_NORMALIZED)
		return;

	_try {
		status = IoVolumeDeviceToDosName(
			((PFILE_OBJECT)FileObject)->DeviceObject, &DosName);
		if (!NT_SUCCESS(status))
			goto RELEASE;
	}
	__except(1) {
		KdPrint(("[MiniFilter] MakeUserModeName Error(%d)\n", status));
		goto RELEASE;
	}

	memcpy(pOut, DosName.Buffer, DosName.Length);
	use += DosName.Length;

	memcpy((char*)pOut+use, pnameInfo->ParentDir.Buffer,
		pnameInfo->ParentDir.Length);
	use += pnameInfo->ParentDir.Length;

	memcpy((char*)pOut+use, pnameInfo->FinalComponent.Buffer,
		pnameInfo->FinalComponent.Length);
	use += pnameInfo->FinalComponent.Length;

RELEASE:
	if (DosName.Buffer) ExFreePool(DosName.Buffer);
	pOut[use/2] = 0;
	return;
}


///////////////////////////////////////////////////////////////////////////////
// SeAuditProcessCreationInfo

#ifdef WINXP
PUNICODE_STRING GetProcessFullImageFileName(PEPROCESS pep)
{
	return *(PUNICODE_STRING*)((PUCHAR)pep + 0x1f4);
}
#else
PUNICODE_STRING GetProcessFullImageFileName(PEPROCESS pep)
{
	return *(PUNICODE_STRING*)((PUCHAR)pep + 0x390);
}
#endif

///////////////////////////////////////////////////////////////////////////////


VOID MakeRecord(const wchar_t *pTemp)
{
	FILEMODIFYINFO fmi = {0};
	PUNICODE_STRING p = 0;

	LARGE_INTEGER st, lt;
	TIME_FIELDS tf;
	KeQuerySystemTime(&st);
	ExSystemTimeToLocalTime(&st, &lt);
	RtlTimeToTimeFields(&lt, &tf);

	fmi.Year	= tf.Year;
	fmi.Month	= tf.Month;
	fmi.Day		= tf.Day;
	fmi.Hour	= tf.Hour;
	fmi.Minute	= tf.Minute;
	fmi.Second	= tf.Second;

__try {
	p = GetProcessFullImageFileName(PsGetCurrentProcess());
	if (p && MmIsAddressValid(p))
		memcpy(fmi.ProcessName, p->Buffer, p->Length);

#ifdef WINXP
	wcscpy(fmi.FileName, pTemp);
#else
	wcscpy_s(fmi.FileName, MAXPATH, pTemp);
#endif

}
__except(1) { KdPrint(("[MiniFilter] [MakeRecord] 错误(pTemp:%08x)\n", pTemp)); }

	Enqueue(&LogQueue, &fmi);
	
	if (pEventKtoU)
	{
		KdPrint(("[MiniFilter] [MakeRecord] 激活事件\n"));
		KeSetEvent(pEventKtoU, IO_NO_INCREMENT, FALSE);
	}
}


///////////////////////////////////////////////////////////////////////////////


FLT_PREOP_CALLBACK_STATUS NPPreCreate(
	__inout			PFLT_CALLBACK_DATA	  Data,
	__in			PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID				  *CompletionContext
	)
{
	UNREFERENCED_PARAMETER(CompletionContext);
	PAGED_CODE();

	if (g_MonitoringOn)
	{
		UCHAR MajorFunction = 0;
		ULONG Options = 0;
		PFLT_FILE_NAME_INFORMATION nameInfo;
		MajorFunction = Data->Iopb->MajorFunction;
		Options = Data->Iopb->Parameters.Create.Options;

		//如果是IRP_MJ_CREATE，且选项是FILE_DELETE_ON_CLOSE，并且能成功获得文件名信息
		if (IRP_MJ_CREATE == MajorFunction && FILE_DELETE_ON_CLOSE == Options &&
			NT_SUCCESS(FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED |
			FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo)))
		{
			//如果解析文件信息成功
			if (NT_SUCCESS(FltParseFileNameInformation(nameInfo)))
			{
				wchar_t pTempBuf[1024] = {0};
				wchar_t *pNonPageBuf = NULL, *pTemp = pTempBuf;
				if (nameInfo->Name.MaximumLength > 1023)
				{
					pNonPageBuf = (wchar_t*)ExAllocatePool(NonPagedPool,
						nameInfo->Name.MaximumLength);
					pTemp = pNonPageBuf;
				}

				RtlCopyMemory(pTemp, nameInfo->Name.Buffer, nameInfo->Name.MaximumLength);

				MakeUserModeName(FltObjects->FileObject, nameInfo, 1024, pTemp);

				KdPrint(("[MiniFilter] [IRP_MJ_CREATE] %ws\n", pTemp));

				if (IsContained(&BlockList, pTemp))    //  检查是不是要保护的文件
				{
					if (pNonPageBuf)
						ExFreePool(pNonPageBuf);
					FltReleaseFileNameInformation(nameInfo);
					return FLT_PREOP_DISALLOW_FASTIO;
				}
				if (pNonPageBuf)
					ExFreePool(pNonPageBuf);
			}
			FltReleaseFileNameInformation(nameInfo);
		}
	}
	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}


///////////////////////////////////////////////////////////////////////////////


// IRP_MJ_SET_INFORMATION
FLT_PREOP_CALLBACK_STATUS NPPreSetInformation(
	__inout			PFLT_CALLBACK_DATA	  Data,
	__in			PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID				  *CompletionContext
	)
{
	UNREFERENCED_PARAMETER( FltObjects );
	UNREFERENCED_PARAMETER( CompletionContext );
	PAGED_CODE();
	if (g_MonitoringOn)
	{
		UCHAR MajorFunction = 0;
		PFLT_FILE_NAME_INFORMATION nameInfo;
		MajorFunction = Data->Iopb->MajorFunction;

		//如果操作是IRP_MJ_SET_INFORMATION且成功获得文件名信息
		if (IRP_MJ_SET_INFORMATION == MajorFunction &&
			NT_SUCCESS(FltGetFileNameInformation(Data,
				FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo)))
		{
			if (NT_SUCCESS(FltParseFileNameInformation(nameInfo)))
			{
				wchar_t pTempBuf[1024] = {0};
				wchar_t *pNonPageBuf = NULL, *pTemp = pTempBuf;
				if(nameInfo->Name.MaximumLength > 1023)
				{
					pNonPageBuf = (wchar_t*)ExAllocatePool(NonPagedPool,
						nameInfo->Name.MaximumLength);
					pTemp = pNonPageBuf;
				}
				memcpy(pTemp, nameInfo->Name.Buffer, nameInfo->Name.MaximumLength);

				MakeUserModeName(FltObjects->FileObject, nameInfo, 1024, pTemp);

				KdPrint(("[MiniFilter] [IRP_MJ_SET_INFORMATION] %ws\n", pTemp));
				if (IsContained(&BlockList, pTemp))    //  检查是不是要保护的文件
				{
					KdPrint(("[MyminiFilter] [阻止修改文件信息]\n"));
					MakeRecord(pTemp);

					if(pNonPageBuf)
						ExFreePool(pNonPageBuf);

					FltReleaseFileNameInformation(nameInfo);
					Data->IoStatus.Status = STATUS_ACCESS_DENIED;
					Data->IoStatus.Information = 0;
					return FLT_PREOP_COMPLETE;
				}
				if (pNonPageBuf)
					ExFreePool(pNonPageBuf);
			}
			FltReleaseFileNameInformation(nameInfo);
		}
	}
	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}


///////////////////////////////////////////////////////////////////////////////

/*
// IRP_MJ_READ
FLT_PREOP_CALLBACK_STATUS NPPreRead(
	__inout			PFLT_CALLBACK_DATA	  Data,
	__in			PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID				  *CompletionContext
	)
{
	UNREFERENCED_PARAMETER( FltObjects );
	UNREFERENCED_PARAMETER( CompletionContext );
	PAGED_CODE();
	{
		PFLT_FILE_NAME_INFORMATION nameInfo;
		//直接获得文件名并检查
		if(NT_SUCCESS(FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED |
			FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo)))
		{
			if( NT_SUCCESS( FltParseFileNameInformation( nameInfo ) ) )
			{
				WCHAR pTempBuf[ 1024 ] = { 0 };
				WCHAR *pNonPageBuf = NULL, *pTemp = pTempBuf;
				if( nameInfo->Name.MaximumLength > 1023 )
				{
					pNonPageBuf = (wchar_t*)ExAllocatePool(NonPagedPool,
						nameInfo->Name.MaximumLength);
					pTemp = pNonPageBuf;
				}

				RtlCopyMemory(pTemp, nameInfo->Name.Buffer, nameInfo->Name.MaximumLength);
//				KdPrint(("[MiniFilter][IRP_MJ_READ]%wZ\n", &nameInfo->Name));

				if(pNonPageBuf)
					ExFreePool(pNonPageBuf);
			}
			FltReleaseFileNameInformation( nameInfo );
		}
	}
	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}
*/

///////////////////////////////////////////////////////////////////////////////


// IRP_MJ_WRITE
FLT_PREOP_CALLBACK_STATUS NPPreWrite(
	__inout			PFLT_CALLBACK_DATA	  Data,
	__in			PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID				  *CompletionContext
	)
{
	UNREFERENCED_PARAMETER( FltObjects );
	UNREFERENCED_PARAMETER( CompletionContext );
	PAGED_CODE();
	if (g_MonitoringOn)
	{
		PFLT_FILE_NAME_INFORMATION nameInfo;
		//直接获得文件名并检查
		if (NT_SUCCESS(FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED |
			FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo)))
		{
			if (NT_SUCCESS(FltParseFileNameInformation(nameInfo)))
			{
				WCHAR pTempBuf[1024] = {0};
				WCHAR *pNonPageBuf = NULL, *pTemp = pTempBuf;
				if (nameInfo->Name.MaximumLength > 1023)
				{
					pNonPageBuf = (wchar_t*)ExAllocatePool(NonPagedPool,
						nameInfo->Name.MaximumLength);
					pTemp = pNonPageBuf;
				}

				RtlCopyMemory(pTemp, nameInfo->Name.Buffer, nameInfo->Name.MaximumLength);

				MakeUserModeName(FltObjects->FileObject, nameInfo, 1024, pTemp);

				KdPrint(("[MiniFilter] [IRP_MJ_WRITE] %ws\n", pTemp));
				if (IsContained(&BlockList, pTemp))    //  检查是不是要保护的文件
				{
					KdPrint(("[MyminiFilter] [阻止文件写入]\n"));
					MakeRecord(pTemp);

					if(pNonPageBuf)
						ExFreePool(pNonPageBuf);
					FltReleaseFileNameInformation(nameInfo);
					Data->IoStatus.Status = STATUS_MEDIA_WRITE_PROTECTED;
					Data->IoStatus.Information = 0;
					return FLT_PREOP_COMPLETE;
				}
				if (pNonPageBuf)
					ExFreePool( pNonPageBuf );
			}
			FltReleaseFileNameInformation(nameInfo);
		}
	}
	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}


///////////////////////////////////////////////////////////////////////////////


NTSTATUS MyMiniConnect(
    __in PFLT_PORT ClientPort,
    __in PVOID ServerPortCookie,
    __in_bcount(SizeOfContext) PVOID ConnectionContext,
    __in ULONG SizeOfContext,
    __deref_out_opt PVOID *ConnectionCookie
    )
{
	KdPrint(("[MyminiFilter] MyMiniConnect\n"));
    PAGED_CODE();

    UNREFERENCED_PARAMETER(ServerPortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(SizeOfContext);
    UNREFERENCED_PARAMETER(ConnectionCookie);

    ASSERT(g_ClientPort == NULL);
    g_ClientPort = ClientPort;
    return STATUS_SUCCESS;
}


///////////////////////////////////////////////////////////////////////////////


VOID MyMiniDisconnect(
    __in_opt PVOID ConnectionCookie
   )
{
    PAGED_CODE();
    UNREFERENCED_PARAMETER(ConnectionCookie);

	KdPrint(("[MyminiFilter] MyMiniDisconnect\n"));

    //  Close our handle
    FltCloseClientPort(g_pFilterHandle, &g_ClientPort);
}


///////////////////////////////////////////////////////////////////////////////


NTSTATUS MyMiniMessage (
    __in PVOID ConnectionCookie,
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer,
    __in ULONG InputBufferSize,
    __out_bcount_part_opt(OutputBufferSize,*ReturnOutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferSize,
    __out PULONG ReturnOutputBufferLength
    )
{
    COMMAND command;
    NTSTATUS status = STATUS_SUCCESS;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(ConnectionCookie);

	KdPrint(("[MyminiFilter] MyMiniMessage\n"));

    //                      **** PLEASE READ ****
    //  The INPUT and OUTPUT buffers are raw user mode addresses.  The filter
    //  manager has already done a ProbedForRead (on InputBuffer) and
    //  ProbedForWrite (on OutputBuffer) which guarentees they are valid
    //  addresses based on the access (user mode vs. kernel mode).  The
    //  minifilter does not need to do their own probe.
    //  The filter manager is NOT doing any alignment checking on the pointers.
    //  The minifilter must do this themselves if they care (see below).
    //  The minifilter MUST continue to use a try/except around any access to
    //  these buffers.

    if ((InputBuffer != NULL) && (InputBufferSize >= sizeof(COMMAND)))
	{
        try  {
			switch (((PFILTER_MESSAGE) InputBuffer)->Cmd)
			{
			case ENUM_TURNON_PROTECT:
				KdPrint(("[MyminiFilter] 打开文件监控\n"));
				g_MonitoringOn = TRUE;
				break;

			case ENUM_TURNOFF_PROTECT:
				KdPrint(("[MyminiFilter] 关闭文件监控\n"));
				g_MonitoringOn = FALSE;
				break;

			case ENUM_GET_FILE_INFO:
				if (OutputBufferSize >= sizeof(FILEMODIFYINFO))
				{
					if (!Dequeue(&LogQueue, (PFILEMODIFYINFO)OutputBuffer))
					{
						status = STATUS_DATA_ERROR;
						if (IsEmpty(&LogQueue))
						{
							KdPrint(("[MyminiFilter] 日志队列空, status=0x%08x\n", status));
							if (pEventKtoU)
								KeClearEvent(pEventKtoU);
						}
					}
				}
				else {
					KdPrint(("[MiniFilter] [ENUM_GET_FILE_INFO] Buffer Size: %d, Size Need: %d\n",
						OutputBufferSize, sizeof(FILEMODIFYINFO)));
					status = STATUS_BUFFER_TOO_SMALL;
				}

				break;


			case ENUM_RECEIVE_USER_FILE_EVENT:
				if (1)//InputBufferSize >= sizeof(HANDLE))	// InputBufferSize老是为0
				{
					hEventKtoU = ((PFILTER_MESSAGE) InputBuffer)->hEvent;
					status = ObReferenceObjectByHandle(hEventKtoU, EVENT_MODIFY_STATE,
						*ExEventObjectType, UserMode, (PVOID*)&pEventKtoU, NULL);
					KdPrint(("[MyminiFilter] 接收到用户事件(h:%#xll, p:%#xll)\n",
						hEventKtoU, pEventKtoU));
				}
				else {
					KdPrint(("Buffer Size: %d, Size Need: %d\n",
						InputBufferSize, sizeof(HANDLE)));
					status = STATUS_BUFFER_TOO_SMALL;
				}
				break;

			case ENUM_ADD_FILE_BLOCK_LIST:
				if (InputBufferSize < 4 + (sizeof(wchar_t) * MAXPATH))
					status = STATUS_BUFFER_TOO_SMALL;

				else
				{
					KdPrint(("[MyminiFilter] 添加黑名单, cbIn:%d\n%S\n", InputBufferSize,
						((PFILTER_MESSAGE)InputBuffer)->String));
					if (!Add(&BlockList, ((PFILTER_MESSAGE)InputBuffer)->String))
					{
						KdPrint(("[MyminiFilter] 黑名单添加失败\n"));
						status = STATUS_UNSUCCESSFUL;
					}
				}
				break;


			case ENUM_ADD_FILE_WHITE_LIST:
				if (InputBufferSize < 4 + (sizeof(wchar_t) * MAXPATH))
					status = STATUS_BUFFER_TOO_SMALL;

				else
				{
					KdPrint(("[MyminiFilter] 添加白名单, cbIn:%d\n%S\n", InputBufferSize,
						((PFILTER_MESSAGE)InputBuffer)->String));
					if (!Add(&WhiteList, ((PFILTER_MESSAGE)InputBuffer)->String))
					{
						KdPrint(("[MyminiFilter] 白名单添加失败\n"));
						status = STATUS_UNSUCCESSFUL;
					}
				}
				break;


			case ENUM_REMOVE_FILE_BLOCK_LIST:
				if (InputBufferSize < 4 + (sizeof(wchar_t) * MAXPATH))
						status = STATUS_BUFFER_TOO_SMALL;

				else
				{
					KdPrint(("[MyminiFilter] 删除黑名单, cbIn:%d\n%S\n", InputBufferSize,
						((PFILTER_MESSAGE)InputBuffer)->String));
					if (!Remove(&BlockList, ((PFILTER_MESSAGE)InputBuffer)->String))
					{
						KdPrint(("[MyminiFilter] 黑名单删除失败\n"));
						status = STATUS_UNSUCCESSFUL;
					}
				}
				break;


			case ENUM_REMOVE_FILE_WHITE_LIST:
				if (InputBufferSize < 4 + (sizeof(wchar_t) * MAXPATH))
					status = STATUS_BUFFER_TOO_SMALL;
				else
				{
					KdPrint(("[MyminiFilter] 删除白名单, cbIn:%d\n%S\n", InputBufferSize,
						((PFILTER_MESSAGE)InputBuffer)->String));
					if (!Remove(&WhiteList, ((PFILTER_MESSAGE)InputBuffer)->String))
					{
						KdPrint(("[MyminiFilter] 白名单删除失败\n"));
						status = STATUS_UNSUCCESSFUL;
					}
				}
				break;


			default:
				KdPrint(("[MyminiFilter] default\n"));
				status = STATUS_INVALID_PARAMETER;
				break;
			}

        } except(EXCEPTION_EXECUTE_HANDLER) {

            return GetExceptionCode();
        }
    }
    else
        status = STATUS_INVALID_PARAMETER;

    return status;
}

