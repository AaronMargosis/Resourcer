#pragma once
#include <Windows.h>


typedef const wchar_t* RESTYPE;

typedef struct {
	RESTYPE resType;
	const wchar_t* szResType;
} ResType_t;
extern ResType_t aResType[];

inline const wchar_t* ResTypeToStr(RESTYPE resType)
{
	for (ResType_t* pRT = aResType; pRT->szResType != NULL; ++pRT)
	{
		if (pRT->resType == resType)
			return pRT->szResType;
	}
	return NULL;
}

inline RESTYPE StrToResType(const wchar_t* szResType)
{
	for (ResType_t* pRT = aResType; pRT->szResType != NULL; ++pRT)
	{
		if (0 == _wcsicmp(szResType, pRT->szResType))
			return pRT->resType;
	}
	return 0;
}
