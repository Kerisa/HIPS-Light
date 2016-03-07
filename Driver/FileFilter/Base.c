
#include "Myminifilter.h"

int InitQueue(PMYQUEUE q)
{
    if (!q) return 0;

    if (q->Queue)
        ExFreePool(q->Queue);

    memset(q, 0, sizeof(MYQUEUE));

    q->Capacity = 1024;
    q->ptrRead  = q->Capacity - 1;


    q->Queue = (PFILEMODIFYINFO)ExAllocatePoolWithTag(PagedPool,
        q->Capacity * sizeof(FILEMODIFYINFO), 'BgaT');
    if (!q->Queue)
    {
        KdPrint(("Mem Allocate Failed.\n"));
        return 0;
    }
    return 1;
}


///////////////////////////////////////////////////////////////////////////////


int IsEmpty(PMYQUEUE q)
{
    if (!q) return 1;
    KdPrint(("队列 R:%d, W:%d\n", q->ptrRead, q->ptrWrite));
    return ((q->ptrRead + 1) % q->Capacity) == q->ptrWrite;
}


///////////////////////////////////////////////////////////////////////////////


int FreeQueue(PMYQUEUE q)
{
    int ret = 0;
    if (q && q->Queue)
    {
        KdPrint(("释放Queue内存\n"));
        ExFreePool(q->Queue);
        q->Queue = 0;
        ret = 1;
    }
    return ret;
}


///////////////////////////////////////////////////////////////////////////////


int Enqueue(PMYQUEUE q, PFILEMODIFYINFO pfmi)
{
    int ret = 0;
    PFILEMODIFYINFO p = 0;
    if (!q || !pfmi) return 0;
    while (InterlockedExchange(&q->InUse, 1) == 1);

    if (q->ptrWrite == q->ptrRead)        // 队列满
    {
        if (2 * q->Capacity <= LIMIT)
        {
            q->Capacity *= 2;
            p = (PFILEMODIFYINFO)ExAllocatePoolWithTag(
                PagedPool, q->Capacity * sizeof(FILEMODIFYINFO), 'CgaT');
            if (!p)
            {
                KdPrint(("Enqueue内存分配失败\n"));
                q->Capacity /= 2;
            }
            else
            {
                memcpy(p, q->Queue, q->Capacity / 2 * sizeof(FILEMODIFYINFO));
                ExFreePool(q->Queue);
                q->Queue = p;
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
            memcpy(&q->Queue[q->ptrWrite], pfmi, sizeof(FILEMODIFYINFO));
            if (++q->ptrWrite >= q->Capacity)
                q->ptrWrite = 0;
            ret = 1;
        }
        __except(1)    { }
    }

    InterlockedExchange(&q->InUse, 0);

    return ret;
}


///////////////////////////////////////////////////////////////////////////////


int Dequeue(PMYQUEUE q, PFILEMODIFYINFO pfmi)
{
    int ret = 0, tmp;

    if (!q || !pfmi) return 0;

    tmp = (q->ptrRead + 1) % q->Capacity;

    while (InterlockedExchange(&q->InUse, 1) == 1);

    if (tmp == q->ptrWrite)
        KdPrint(("队列空\n"));

    else
    {
        __try{
            memcpy(pfmi, &q->Queue[tmp], sizeof(FILEMODIFYINFO));
            q->ptrRead = tmp;
            ret = 1;
        }
        __except(1)    { }
    }

    InterlockedExchange(&q->InUse, 0);

    return ret;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


int FreeList(PVECTOR v)
{
    int ret = 0;
    if (v && v->List)
    {
        KdPrint(("释放Vector内存\n"));
        ExFreePool(v->List);
        v->List = 0;
        ret = 1;
    }
    return ret;
}


///////////////////////////////////////////////////////////////////////////////


int InitList(PVECTOR v, wchar_t *list, unsigned long num)
{
    int ret = 0;
    unsigned long i = 0;

    if (!v) return 0;

    v->Capacity = 1024;

    do
    {
        if (num > LIMIT)
            break;

        while (v->Capacity < num)
        {
            v->Capacity <<= 1;
            KdPrint(("Capacity: %d\n", v->Capacity));
        }
        v->Size = num;

        v->List = (wchar_t*)ExAllocatePoolWithTag(PagedPool,
            v->Capacity * sizeof(wchar_t) * MAXPATH, 'AgaT');
        if (!v->List)
        {
            KdPrint(("VECTOR::Init 内存分配失败\n"));
            break;
        }

        if (num && !list)
            break;

        __try {
            for (i=0; i<v->Size; ++i)
            {
#ifdef WINXP
                wcscpy(v->List + i*MAXPATH, list + i*MAXPATH);
#else
                wcscpy_s(v->List + i*MAXPATH, MAXPATH, list + i*MAXPATH);
#endif
            }
            ret = 1;
        }
        __except(1) { }

    } while (0);

    return ret;
}


///////////////////////////////////////////////////////////////////////////////


void ToLower(const wchar_t *src, wchar_t *out)
{
    while (*src)
    {
        *out = *src + (*src >= L'A' && *src <= L'Z' ? 0x20 : 0);
        ++out, ++src;
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

int SubMatch(const wchar_t *src, const wchar_t *sub, int srclen, int *pbegin)
{
    int i, ret = 0;
    int begin = *pbegin;
    int sublen = wcslen(sub);

    for (i=begin; i<=srclen-sublen; ++i)
        if (!my_wcsncmp(&src[i], sub, sublen))
        {
            *pbegin = i + sublen;
            ret = 1;
            break;
        }

    return ret;
}

int Match(const wchar_t *src, const wchar_t *_pat)
{ // 思路就是按*拆分串，然后每块单独匹配，配上就对了
    int subPos[8] = {0};        // 限定分成8段
    int wildcard = 0, begin = 0, subNow = 0;
    unsigned long patlen = wcslen(_pat);
    unsigned long i, subCnt = 0, last = 0;
    wchar_t pat[MAXPATH];


    if (!src || !_pat) return 0;
    if (_pat[0] == L'*' && _pat[1] == 0) return 1;

#ifdef WINXP
    wcscpy(pat, _pat);
#else
    wcscpy_s(pat, MAXPATH, _pat);
#endif

    if (pat[0] == L'*')
    {
        last = i = 1;
        wildcard = 1;
    }
    else
        last = i = 0;

    for (last=i; i<patlen; ++i)
        if (pat[i] == L'*' && subCnt<7)
        {
            wildcard = 1;
            subPos[subCnt++] = last;
            pat[i] = 0;
            last = i + 1;
        }

    if (last < patlen) subPos[subCnt++] = last; // 最后一段

    if (!wildcard)
        return !my_wcscmp(src, _pat);

    for (i=0; i<subCnt; ++i)
        if (!SubMatch(src, &pat[subPos[i]], wcslen(src), &begin))
            return 0;

    return 1;
}


///////////////////////////////////////////////////////////////////////////////


int IsContained(PVECTOR v, const wchar_t *Str)
{
    unsigned long i;
    wchar_t format[MAXPATH] = {0};

    __try {
        ToLower(Str, format);

        for (i=0; i<v->Size; ++i) {
            // KdPrint(("pat: %ws\n", (wchar_t*)v->List + i*MAXPATH));
            if (Match(format, (wchar_t*)v->List + i*MAXPATH))
            {
                KdPrint(("[MyminiFilter] [Vector] 匹配到 %ws\n", (wchar_t*)v->List + i*MAXPATH));
                return 1;
            }
        }
    }
    __except(1) { }

    return 0;
}


///////////////////////////////////////////////////////////////////////////////


int Add(PVECTOR v, const wchar_t *Str)
{
    wchar_t format[MAXPATH] = {0};

    __try {
        ToLower(Str, format);

#ifdef WINXP
        wcscpy((wchar_t*)v->List + v->Size*MAXPATH, format);
#else
        wcscpy_s((wchar_t*)v->List + v->Size*MAXPATH, MAXPATH, format);
#endif

        ++v->Size;
        return 1;
    }
    __except(1) { }

    return 0;
}


///////////////////////////////////////////////////////////////////////////////


int Remove(PVECTOR v, const wchar_t *Str)
{
    unsigned long i;
    wchar_t format[MAXPATH] = {0};

    __try {
        ToLower(Str, format);

        for (i=0; i<v->Size; ++i)
            if (!wcscmp(v->List + i*MAXPATH, format))
            {
                memcpy(v->List + i*MAXPATH,
                       v->List + (i+1)*MAXPATH,
                       (v->Size-i-1)*MAXPATH*sizeof(wchar_t));
                --v->Size;
                return 1;
            }
    }
    __except(1) { }

    return 0;
}
