
// #define WINXP

#include "RegisterMonitor.h"


PDEVICE_OBJECT			g_DeviceObject;
BOOLEAN					g_MonitoringOn;

LONG					InUse;
BOOLEAN					UserAllow;



extern "C" NTSTATUS DriverEntry (
			IN PDRIVER_OBJECT pDriObj,
			IN PUNICODE_STRING pRegPath)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	PDEVICE_EXTENSION pDevExt;


	pDriObj->DriverUnload = DriverUnloadRoutine;
	pDriObj->MajorFunction[IRP_MJ_CREATE] = OnCreate;
	pDriObj->MajorFunction[IRP_MJ_CLOSE] = OnCreate;
	pDriObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;

	g_MonitoringOn = FALSE;
	InUse = 0;

	UNICODE_STRING DeviceName;
	UNICODE_STRING SymLinkName;
	RtlInitUnicodeString(&DeviceName, DEVICENAME);
	RtlInitUnicodeString(&SymLinkName, SYMLINKNAME);

	do{
		status = IoCreateDevice(pDriObj, sizeof(DEVICE_EXTENSION),
			&DeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE,
			&g_DeviceObject);

		if (!NT_SUCCESS(status)) {
			KdPrint(("[RegMon] 设备无法创建(0x%08x)\n", status));
			break;
		}
		g_DeviceObject->Flags |= DO_BUFFERED_IO;
		pDevExt = (PDEVICE_EXTENSION)g_DeviceObject->DeviceExtension;
		pDevExt->pDevObj = g_DeviceObject;
		
		
		status = IoCreateSymbolicLink(&SymLinkName, &DeviceName);
		if (!NT_SUCCESS(status)) {
			KdPrint(("[RegMon] 符号链接无法创建(0x%08x)\n", status));
			IoDeleteDevice(g_DeviceObject);
			break;
		}
		pDevExt->ustrDevSymLinkName = SymLinkName;


		status = CmRegisterCallback(RegistryCallback, pDevExt, &pDevExt->RegCallbackCookies);
		if (!NT_SUCCESS(status)) {
			KdPrint(("[RegMon] 注册表回调函数无法注册(0x%08x)\n", status));
			IoDeleteSymbolicLink(&SymLinkName);
			IoDeleteDevice(g_DeviceObject);
			break;
		}


		if (!pDevExt->Queue.Init() ||
			!pDevExt->WhiteList.Init(NULL, 0) ||
			!pDevExt->BlockList.Init(NULL, 0))
		{
			KdPrint(("[RegMon] 列表初始化失败\n"));
			CmUnRegisterCallback(pDevExt->RegCallbackCookies);
			pDevExt->BlockList.FreeList();
			pDevExt->WhiteList.FreeList();
			pDevExt->Queue.FreeQueue();
			IoDeleteSymbolicLink(&SymLinkName);
			IoDeleteDevice(g_DeviceObject);
			break;
		}
		
		status = STATUS_SUCCESS;
	} while(0);
	
	KdPrint(("[RegMon] 注册表监控驱动加载完毕(0x%08x)\n", status));
	return status;
}


///////////////////////////////////////////////////////////////////////////////

#ifdef WINXP

PUNICODE_STRING GetProcessFullImageFileName(PEPROCESS pep)
{
	return *(PUNICODE_STRING*)((char*)pep + 0x1f4);
}

#else

PUNICODE_STRING GetProcessFullImageFileName(PEPROCESS pep)
{
	return *(PUNICODE_STRING*)((PUCHAR)pep + 0x390);
}

#endif

///////////////////////////////////////////////////////////////////////////////


//typedef struct _REG_SET_VALUE_KEY_INFORMATION {
//	PVOID           Object;
//	PUNICODE_STRING ValueName;
//	ULONG           TitleIndex;
//	ULONG           Type;
//	PVOID           Data;
//	ULONG           DataSize;
//	PVOID           CallContext;
//	PVOID           ObjectContext;
//	PVOID           Reserved;
//} REG_SET_VALUE_KEY_INFORMATION, *PREG_SET_VALUE_KEY_INFORMATION;

void Test_ShowInfo(PREGMODIFYINFO prmi)
{
	__try{
		if (prmi)
			KdPrint(("[RegMon] Proc: %ws\n\tTarget: %ws\n",
				prmi->ProcessName, prmi->RegKeyName));
	}__except(1)
	{
		KdPrint(("[RegMon] Test_ShowInfo Error\n"));
	}
}


NTSTATUS RegistryCallback(
	IN			PVOID CallbackContext,
	IN OPTIONAL	PVOID Arg1,
	IN OPTIONAL PVOID Arg2
	)
{
	NTSTATUS		  status = STATUS_SUCCESS;
	ULONG			  R;
	PDEVICE_EXTENSION pExt = (PDEVICE_EXTENSION) CallbackContext;

	if (!g_MonitoringOn) return status;

	switch ((int)Arg1)
	{
		case RegNtSetValueKey:
		{
			REGMODIFYINFO rmi = {0};

			LARGE_INTEGER st, lt;
			TIME_FIELDS tf;
			KeQuerySystemTime(&st);
			ExSystemTimeToLocalTime(&st, &lt);
			RtlTimeToTimeFields(&lt, &tf);

			rmi.Year	= tf.Year;
			rmi.Month	= tf.Month;
			rmi.Day		= tf.Day;
			rmi.Hour	= tf.Hour;
			rmi.Minute	= tf.Minute;
			rmi.Second	= tf.Second;


			// 获取进程名
			PUNICODE_STRING pProcName = GetProcessFullImageFileName(
				PsGetCurrentProcess());


			PUNICODE_STRING pKeyName =
				(PUNICODE_STRING)ExAllocatePoolWithTag(
				PagedPool, 1024 + 4, 'emaN');
			if (pKeyName)
			{
				memset(pKeyName, 0, 1024 + 4);
				pKeyName->MaximumLength = 1024;

				if (STATUS_SUCCESS != ObQueryNameString(
					((PREG_SET_VALUE_KEY_INFORMATION)Arg2)->Object,
					(POBJECT_NAME_INFORMATION)pKeyName, 1024 + 4, &R))
					KdPrint(("[RegMon] Regcallback中的ObQueryNameString失败.\n"));
				else
				{
					__try {
						if (pProcName)
							memcpy(rmi.ProcessName, pProcName->Buffer,
								pProcName->Length);
						if (pKeyName)
							memcpy(rmi.RegKeyName, pKeyName->Buffer,
								pKeyName->Length);
					}
					__except(1)
					{
						KdPrint(("[RegMon] Regcallback大概是缓冲区溢出了\n"));
					}
				}

				ExFreePool(pKeyName);
			}
			else KdPrint(("[RegMon] Regcallback中的内存分配失败.\n"));


			Test_ShowInfo(&rmi);

			// 例外:不包含才记录,反之不记录日志,省得添堵
			if (!pExt->WhiteList.IsContained(rmi.RegKeyName))
			{
				rmi.Result = pExt->BlockList.IsContained(rmi.RegKeyName);

				// 入队以记录日志
				if (!pExt->Queue.Enqueue(&rmi))
					KdPrint(("regcallback中的入队失败\n"));
					
				else {	// 通知应用层可以从队列中取数据了
					KdPrint(("[RegMon] 队列 R:%d, W:%d\n",
						pExt->Queue.ptrRead, pExt->Queue.ptrWrite));
					if (pExt->pEventKtoU)
						KeSetEvent(pExt->pEventKtoU, IO_NO_INCREMENT, FALSE);
				}

				if (rmi.Result)		// 直接ban掉
				{
					KdPrint(("[RegMon] Regcallback, 阻止修改\n"));
					status = STATUS_ACCESS_DENIED;
				}
			}
			else	// 白名单中，全部放过
				KdPrint(("[RegMon] [白名单] %ws.\n", rmi.RegKeyName));
		}
			break;
	}

	return status;
}


///////////////////////////////////////////////////////////////////////////////


NTSTATUS OnCreate(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}


///////////////////////////////////////////////////////////////////////////////


VOID DriverUnloadRoutine(IN PDRIVER_OBJECT pDriObj)
{
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_EXTENSION pExt = (PDEVICE_EXTENSION)g_DeviceObject->DeviceExtension;

	CmUnRegisterCallback(pExt->RegCallbackCookies);

	pExt->BlockList.FreeList();
	pExt->WhiteList.FreeList();
	pExt->Queue.FreeQueue();

	__try
	{
		if (pExt->pEventKtoU) ObDereferenceObject(pExt->pEventKtoU);
		if (pExt->pEventUtoK)
		{
			KeSetEvent(pExt->pEventUtoK, IO_NO_INCREMENT, FALSE);
			ObDereferenceObject(pExt->pEventUtoK);
		}
		ZwClose(pExt->hEventKtoU);
		ZwClose(pExt->hEventUtoK);
	}
	__except(1) { }


	PDEVICE_OBJECT pNextObj = pDriObj->DeviceObject;
	while (pNextObj)
	{
		PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pNextObj->DeviceExtension;

		IoDeleteSymbolicLink(&pDevExt->ustrDevSymLinkName);
		IoDeleteDevice(pDevExt->pDevObj);

		pNextObj = pNextObj->NextDevice;
	}

	KdPrint(("[RegMon] 注册表驱动卸载完毕\n"));
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
	case IOCTL_TRUNON_REG_MONITORING:
		if (pExt->hEventUtoK && pExt->hEventKtoU)
			g_MonitoringOn = TRUE;
		else
			KdPrint(("[RegMon] 同步事件未初始化\n"));
		break;

	case IOCTL_TURNOFF_REG_MONITORING:
		g_MonitoringOn = FALSE;
		if (pExt->pEventKtoU) KeSetEvent(pExt->pEventKtoU, IO_NO_INCREMENT, FALSE);
		if (pExt->pEventUtoK) KeSetEvent(pExt->pEventUtoK, IO_NO_INCREMENT, FALSE);

		KdPrint(("[RegMon] 注册表监控关闭\n"));
		break;

	case IOCTL_ALLOW_REG_MODIFY:
	case IOCTL_DISALLOW_REG_MODIFY:
		UserAllow = IOCTL_ALLOW_REG_MODIFY == code ? TRUE : FALSE;
		KeSetEvent(pExt->pEventUtoK, IO_NO_INCREMENT, FALSE);
		break;

	case IOCTL_GET_REG_INFO:
		if (cbOut < sizeof(REGMODIFYINFO))
		{
			KdPrint(("[RegMon] IOCTL_GET_REG_INFO缓冲区太小\n"));
			status = STATUS_BUFFER_TOO_SMALL;
		}
		else if (g_MonitoringOn)
		{
			if (!pExt->Queue.Dequeue(pIrp->AssociatedIrp.SystemBuffer))
			{
				status = STATUS_DATA_ERROR;
				if (pExt->Queue.IsEmpty())
				{
					KdPrint(("[RegMon] Reset Event, status=0x%08x, info=%d\n", status, info));
					KeResetEvent(pExt->pEventKtoU);
				}
			}
			else
				info = sizeof(REGMODIFYINFO);
		}
		break;

	case IOCTL_RECEIVE_USER_REG_EVENT:
		pExt->hEventUtoK = *(HANDLE*)pIrp->AssociatedIrp.SystemBuffer;
		ObReferenceObjectByHandle(pExt->hEventUtoK, EVENT_MODIFY_STATE, *ExEventObjectType,
			UserMode, (PVOID*)&pExt->pEventUtoK, NULL);

		pExt->hEventKtoU = *((HANDLE*)pIrp->AssociatedIrp.SystemBuffer + 1);
		ObReferenceObjectByHandle(pExt->hEventKtoU, EVENT_MODIFY_STATE, *ExEventObjectType,
			UserMode, (PVOID*)&pExt->pEventKtoU, NULL);

		KdPrint(("[RegMon] Event Receive! [K->U:0x%08x, U->K: 0x%08x]\n",
			pExt->pEventKtoU, pExt->pEventUtoK));
		break;

	case IOCTL_ADD_REG_BLOCK_LIST:
		if (cbIn < (sizeof(wchar_t) * MAXPATH))
		{
			KdPrint(("[RegMon] Block List Buffer Too Small\n"));
			status = STATUS_BUFFER_TOO_SMALL;
		}
		else
		{
			KdPrint(("[RegMon] Add Block List, cbIn:%d\n%ws\n", cbIn,
				(wchar_t*)pIrp->AssociatedIrp.SystemBuffer));
			if (!pExt->BlockList.Add((wchar_t*)pIrp->AssociatedIrp.SystemBuffer))
			{
				KdPrint(("[RegMon] Block List 添加失败\n"));
				status = STATUS_INVALID_PARAMETER;
			}
		}

		break;

	case IOCTL_ADD_REG_WHITE_LIST:
		if (cbIn < (sizeof(wchar_t) * MAXPATH))
		{
			KdPrint(("[RegMon] White List Buffer Too Small\n"));
			status = STATUS_BUFFER_TOO_SMALL;
		}
		else
		{
			KdPrint(("[RegMon] Add White List, cbIn:%d\n%ws\n", cbIn,
				(wchar_t*)pIrp->AssociatedIrp.SystemBuffer));
			if (!pExt->WhiteList.Add((wchar_t*)pIrp->AssociatedIrp.SystemBuffer))
			{
				KdPrint(("[RegMon] White List 添加失败\n"));
				status = STATUS_INVALID_PARAMETER;
			}
		}
		break;

	case IOCTL_REMOVE_REG_BLOCK_LIST:
		if (cbIn < (sizeof(wchar_t) * MAXPATH))
		{
			KdPrint(("[RegMon] Del Block List Buffer Too Small\n"));
			status = STATUS_BUFFER_TOO_SMALL;
		}
		else
		{
			KdPrint(("[RegMon] Del Block List, cbIn:%d\n%S\n", cbIn,
				(wchar_t*)pIrp->AssociatedIrp.SystemBuffer));
			if (!pExt->BlockList.Remove((wchar_t*)pIrp->AssociatedIrp.SystemBuffer))
			{
				KdPrint(("[RegMon] Block List 删除失败\n"));
				status = STATUS_INVALID_PARAMETER;
			}
		}
		break;

	case IOCTL_REMOVE_REG_WHITE_LIST:
		if (cbIn < (sizeof(wchar_t) * MAXPATH))
		{
			KdPrint(("[RegMon] Del White List Buffer Too Small\n"));
			status = STATUS_BUFFER_TOO_SMALL;
		}
		else
		{
			KdPrint(("[RegMon] Del White List, cbIn:%d\n%S\n", cbIn,
				(wchar_t*)pIrp->AssociatedIrp.SystemBuffer));
			if (!pExt->WhiteList.Remove((wchar_t*)pIrp->AssociatedIrp.SystemBuffer))
			{
				KdPrint(("[RegMon] White List 删除失败\n"));
				status = STATUS_INVALID_PARAMETER;
			}
		}
		break;

	default:
		status = STATUS_INVALID_VARIANT;
	}

	pIrp->IoStatus.Status      = status;
	pIrp->IoStatus.Information = info;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);


	return status;
}



