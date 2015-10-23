
#pragma once


#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#include <ntddscsi.h>


#define COMMUNICATE_PORT_NAME L"\\MyminiFilterPort000"
// #define USEREVENT L"\\BaseNamedObjects\\MyminiFilterUserEvent000"

#define MAXPATH 300
#define LIMIT   16384

#pragma pack(1)
typedef struct _FILEMODIFYINFO{
	short	Year;
	short	Month;
	short	Day;
	short	Hour;
	short	Minute;
	short	Second;
	char	Result;
	wchar_t ProcessName[MAXPATH];
	wchar_t FileName[MAXPATH];
} FILEMODIFYINFO, *PFILEMODIFYINFO;
#pragma pack()

typedef struct _MYQUEUE
{
	unsigned long   Capacity;
	long volatile   InUse;
	PFILEMODIFYINFO Queue;
	unsigned long   ptrRead;	// 指向将要读出的数据块的前一个
	unsigned long   ptrWrite;	// 指向可供写入的内存块
} MYQUEUE, *PMYQUEUE;


typedef struct _VECTOR
{
	wchar_t *List;
	unsigned long Capacity;
	unsigned long Size;
} VECTOR, *PVECTOR;


int InitQueue(PMYQUEUE q);
int IsEmpty(PMYQUEUE q);
int FreeQueue(PMYQUEUE q);
int Enqueue(PMYQUEUE q, PFILEMODIFYINFO pfmi);
int Dequeue(PMYQUEUE q, PFILEMODIFYINFO pfmi);

//-----------------------------------------------------------------------------

int FreeList(PVECTOR v);
int InitList(PVECTOR v, wchar_t *list, unsigned long num);
int IsContained(PVECTOR v, const wchar_t *Str);
int Add(PVECTOR v, const wchar_t *Str);
int Remove(PVECTOR v, const wchar_t *Str);




FLT_PREOP_CALLBACK_STATUS NPPreCreate(
	__inout			PFLT_CALLBACK_DATA	  Data,
	__in			PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID				  *CompletionContext
);

FLT_PREOP_CALLBACK_STATUS NPPreSetInformation(
	__inout			PFLT_CALLBACK_DATA	  Data,
	__in			PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID				  *CompletionContext
);

FLT_PREOP_CALLBACK_STATUS NPPreRead(
	__inout			PFLT_CALLBACK_DATA	  Data,
	__in			PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID				  *CompletionContext
);

FLT_PREOP_CALLBACK_STATUS NPPreWrite(
	__inout			PFLT_CALLBACK_DATA	  Data,
	__in			PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID				  *CompletionContext
);

VOID MyMiniDisconnect(
    __in_opt PVOID ConnectionCookie
);

NTSTATUS MyMiniConnect(
    __in PFLT_PORT ClientPort,
    __in PVOID ServerPortCookie,
    __in_bcount(SizeOfContext) PVOID ConnectionContext,
    __in ULONG SizeOfContext,
    __deref_out_opt PVOID *ConnectionCookie
);

NTSTATUS MyMiniMessage (
    __in PVOID ConnectionCookie,
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer,
    __in ULONG InputBufferSize,
    __out_bcount_part_opt(OutputBufferSize,*ReturnOutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferSize,
    __out PULONG ReturnOutputBufferLength
);

NTSTATUS NPUnload (
    __in FLT_FILTER_UNLOAD_FLAGS Flags
);




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
typedef struct _FILTER_MESSAGE {
	COMMAND 	Cmd;
	union {
		HANDLE  hEvent;
		wchar_t String[MAXPATH];
	};
} FILTER_MESSAGE, *PFILTER_MESSAGE;
#pragma pack()