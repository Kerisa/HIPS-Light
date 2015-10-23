
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <ntddk.h>

#ifdef __cplusplus
}
#endif

const long MAXPATH = 300;

#pragma pack(1)
typedef struct _REGMODIFYINFO{
	short			Year;
	short			Month;
	short			Day;
	short			Hour;
	short			Minute;
	short			Second;
	bool			Result;		// true 表示放行
	bool			NeedProcess;
	wchar_t			ProcessName[MAXPATH];
	wchar_t			RegKeyName[MAXPATH];
} REGMODIFYINFO, *PREGMODIFYINFO;
#pragma pack()


class MYQUEUE
{
	enum {LIMIT = 16000};
	ULONG CAPACITY;
	PKEVENT KeEvent;
	LONG volatile InUse;
	PREGMODIFYINFO Queue;

public:
	ULONG ptrRead;	// 指向将要读出的数据块的前一个
	ULONG ptrWrite;	// 指向可供写入的内存块

	MYQUEUE() { }

	void FreeQueue()
	{
		if (Queue)
		{
			ExFreePool(Queue);
			Queue = 0;
		}
	}

	bool Init(void)
	{
		CAPACITY = 1000;
		ptrRead  = CAPACITY - 1;
		ptrWrite = 0;

		InUse = 0;

		if (Queue)
			ExFreePool(Queue);

		Queue = (PREGMODIFYINFO)ExAllocatePoolWithTag(PagedPool,
			CAPACITY * sizeof(REGMODIFYINFO), '0gaT');
		if (!Queue)
		{
			KdPrint(("MYQUEUE::Init Mem Allocate Failed.\n"));
			return false;
		}
		return true;
	}

	bool IsEmpty()
	{
		KdPrint(("队列 R:%d, W:%d\n", ptrRead, ptrWrite));
		return ((ptrRead + 1) % CAPACITY) == ptrWrite;
	}
	bool Enqueue(PREGMODIFYINFO pfmi);
	bool Dequeue(OUT PVOID pfmi);
	~MYQUEUE()
	{
		ExFreePool(Queue);
	}
};

bool MYQUEUE::Enqueue(IN PREGMODIFYINFO pfmi)
{
	bool ret = false;

	while (InterlockedExchange(&InUse, 1) == 1);

	if (ptrWrite == ptrRead)		// 队列满
	{
		if (2 * CAPACITY <= LIMIT)
		{
			CAPACITY *= 2;
			PREGMODIFYINFO p = (PREGMODIFYINFO)ExAllocatePoolWithTag(PagedPool,
				CAPACITY * sizeof(REGMODIFYINFO), '1gaT');
			if (!p)
				KdPrint(("MYQUEUE::Enqueue Mem Allocate Failed.\n"));
			else
			{
				memcpy(p, Queue, CAPACITY / 2 * sizeof(REGMODIFYINFO));
				ExFreePool(Queue);
				Queue = p;
				goto RETRY;
			}
		}
		else
			KdPrint(("队列满\n"));
	}
	else
	{
RETRY:
		__try{
			memcpy(&Queue[ptrWrite], pfmi, sizeof(REGMODIFYINFO));
			if (++ptrWrite >= CAPACITY)
				ptrWrite = 0;
			ret = true;
		}
		__except(1)	{ }
	}

	InterlockedExchange(&InUse, 0);

	return ret;
}


bool MYQUEUE::Dequeue(OUT PVOID pfmi)
{
	bool ret = false;
	LONG tmp = (ptrRead + 1) % CAPACITY;

	while (InterlockedExchange(&InUse, 1) == 1);

	if (tmp == ptrWrite)	// 队列空
	{
		KdPrint(("队列空\n"));
		pfmi = NULL;
	}
	else
	{

		__try{
			memcpy(pfmi, &Queue[tmp], sizeof(REGMODIFYINFO));
			ptrRead = tmp;
			ret = true;
		}
		__except(1)	{ }
	}

	InterlockedExchange(&InUse, 0);
	return ret;
}




class VECTOR
{
	enum {MAXPATH = 300, LIMIT = 32768};
	void *List;
	unsigned long Capacity;
	unsigned long Size;


public:
	VECTOR() { }

	~VECTOR()
	{
		if (List) ExFreePool(List);
	}

	void FreeList()
	{
		if (List)
		{
			ExFreePool(List);
			List = 0;
		}
	}

	bool Init(const void *list, unsigned long num)
	{
		Capacity = 32767;

		if (num > LIMIT) return false;

		while (Capacity < num) Capacity <<= 1;
		Size = num;

		List = ExAllocatePoolWithTag(PagedPool, Capacity * sizeof(wchar_t) * MAXPATH, '2gaT');
		if (!List)
		{
			KdPrint(("VECTOR::Init Mem Allocate Failed.\n"));
			return false;
		}

		__try {
			for (int i=0; i<Size; ++i)
				wcscpy((wchar_t*)List + i * MAXPATH, (wchar_t*)list + i * MAXPATH);

			return true;
		}
		__except(1) { }

		return false;
	}

	void ToLower(const wchar_t *src, wchar_t *out)
	{
		while (*src)
		{
			*out = *src + (*src >= L'A' && *src <= L'Z' ? 0x20 : 0);
			++out;
			++src;
		}
	}

	int my_wcsncmp(const wchar_t *src, const wchar_t *pat, long count)
	{
		if (!count) return 0;

		while (--count && *src && (*src == *pat || *pat == L'?'))
			++src, ++pat;

		return (int)(*src - *pat);
	}

	int my_wcscmp(const wchar_t *src, const wchar_t *pat)
	{
		int ret = 0;

		while (!((ret = (int)(*src - *pat)) && (ret = (int)(*pat - L'?'))) && *pat)
			++src, ++pat;

		return ret;
	}

	bool SubMatch(const wchar_t *src, const wchar_t *sub, int srclen, int *pbegin)
	{
		int begin = *pbegin;
		int sublen = wcslen(sub);

		for (int i=begin; i<=srclen-sublen; ++i)
			if (!my_wcsncmp(&src[i], sub, sublen))
			{
				*pbegin = i + sublen;
				return true;
			}

		return false;
	}

	bool Match(const wchar_t *src, const wchar_t *_pat)
	{ // 思路就是按*拆分串，然后每块单独匹配，配上就对了
		if (!src || !_pat) return false;
		if (_pat[0] == L'*' && _pat[1] == 0) return true;

		int subCnt = 0, last = 0;
		int subPos[8] = {0};
		bool wildcard = false;
		int patlen = wcslen(_pat);
		wchar_t pat[MAXPATH];

		wcscpy(pat, _pat);

		int i;
		if (pat[0] == L'*')
		{
			last = i = 1;
			wildcard = true;
		}
		else
			last = i = 0;

		for (last=i; i<patlen; ++i)
			if (pat[i] == L'*' && subCnt<7)
			{
				wildcard = true;
				subPos[subCnt++] = last;
				pat[i] = 0;
				last = i + 1;
			}

		if (last < patlen) subPos[subCnt++] = last; // 最后一段

		if (!wildcard) return !my_wcscmp(src, _pat);

		int begin = 0, subNow = 0;
		for (i=0; i<subCnt; ++i)
			if (!SubMatch(src, &pat[subPos[i]], wcslen(src), &begin))
				return false;

		return true;
	}

	bool IsContained(const wchar_t *Str)
	{
		__try {
			wchar_t format[MAXPATH] = {0};
			ToLower(Str, format);

			for (int i=0; i<Size; ++i)
				if (Match(format, (wchar_t*)List + i * MAXPATH))
				{
					KdPrint(("匹配到 %ws\n", (wchar_t*)List + i * MAXPATH));
					return true;
				}
		}
		__except(1) { }
		return false;
	}

	bool Add(const wchar_t *Str)
	{
		__try {
			wchar_t format[MAXPATH] = {0};
			ToLower(Str, format);

			wcscpy((wchar_t*)List + Size*MAXPATH, format);
			++Size;
			return true;
		}
		__except(1) { }

		return false;
	}

	bool Remove(const wchar_t *Str)
	{
		__try {
			wchar_t format[MAXPATH] = {0};
			ToLower(Str, format);

			for (int i=0; i<Size; ++i)
				if (!wcscmp((wchar_t*)List + i*MAXPATH, Str))
				{
					memcpy((wchar_t*)List + i*MAXPATH,
						   (wchar_t*)List + (i+1)*MAXPATH,
						   (Size-i-1)*MAXPATH*sizeof(wchar_t));
					--Size;
					return true;
				}
		}
		__except(1) { }

		return false;
	}

};


typedef struct _DEVICE_EXTENSION {
	PDEVICE_OBJECT pDevObj;
	MYQUEUE		   Queue;
	VECTOR		   WhiteList;
	VECTOR		   BlockList;
	PKEVENT		   pEventKtoU;
	HANDLE		   hEventKtoU;
	PKEVENT		   pEventUtoK;
	HANDLE		   hEventUtoK;
	REGMODIFYINFO  RegModifyInfo;
	LARGE_INTEGER  RegCallbackCookies;
	UNICODE_STRING ustrDevSymLinkName;
	UNICODE_STRING ustrDevName;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;



#define DEVICENAME  L"\\Device\\RegisterMonitor"
#define SYMLINKNAME L"\\??\\RegMon"

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



extern "C" NTSYSAPI NTSTATUS NTAPI ObQueryNameString(
  IN           PVOID                    Object,
  OUT OPTIONAL POBJECT_NAME_INFORMATION ObjectNameInfo,
  IN           ULONG                    Length,
  OUT          PULONG                   ReturnLength
);


VOID DriverUnloadRoutine(IN PDRIVER_OBJECT pDriObj);
BOOLEAN GetProcessName(OUT PLONG pPID, OUT PUNICODE_STRING pName);
NTSTATUS DeviceIoControl(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp);
NTSTATUS OnCreate(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp);

NTSTATUS RegistryCallback(
	IN			PVOID CallbackContext,
	IN OPTIONAL	PVOID Arg1,
	IN OPTIONAL PVOID Arg2
);