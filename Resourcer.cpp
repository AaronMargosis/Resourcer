// Resourcer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <iostream>
#include "StringUtils.h"
#include "Wow64FsRedirection.h"
#include "SysErrorMessage.h"
#include "HEX.h"
#include "PredefResTypes.h"

// ------------------------------------------------------------------------------------------
/// <summary>
/// Write command-line syntax to stderr and then exit.
/// </summary>
/// <param name="argv0">Input: The program's argv[0] value</param>
/// <param name="szError">Optional input: caller-supplied error text</param>
static void Usage(const wchar_t* argv0, const wchar_t* szError = nullptr)
{
	std::wstring sExe = GetFileNameFromFilePath(argv0);
	if (szError)
		std::wcerr << szError << std::endl;
	std::wcerr
		<< std::endl
		<< sExe << L":" << std::endl
		<< L"    Resource listing and exporting utility" << std::endl
		<< std::endl
		<< L"Usage:" << std::endl
		<< std::endl
		<< L"    " << sExe << L" srcFile -list [-t resType]" << std::endl
		<< L"    " << sExe << L" srcFile -export -t resType {-n resName | -i resID} -o outfile" << std::endl
		<< std::endl
		<< L"srcFile must be the first parameter: the path to a portable executable (PE) file with resources." << std::endl
		<< L"-list or -export must be the second parameter." << std::endl
		<< std::endl
		<< L"With -list:" << std::endl
		<< L"  resType : if *, lists all resources;" << std::endl
		<< L"            if a particular resource type, lists resources of that type" << std::endl
		<< L"                (see list below for predefined types);" << std::endl
		<< L"            if omitted, lists resources with named(non-MAKEINTRESOURCE) types." << std::endl
		<< std::endl
		<< L"With -export:" << std::endl
		<< L"  resType : resource type (see list below for predefined types)." << std::endl
		<< L"  resName : resource name." << std::endl
		<< L"  resID   : resource name as a MAKEINTRESOURCE integer value." << std::endl
		<< L"  outFile : file path to export the resource." << std::endl
		<< std::endl
		<< L"Known pre-defined MAKEINTRESOURCE types are:" << std::endl;
	for (ResType_t* pRT = aResType; pRT->szResType != NULL; ++pRT)
		std::wcerr << L"    " << pRT->szResType << std::endl;
	exit(-1);
}

// ------------------------------------------------------------------------------------------

const wchar_t* szDelim = L"\t";
enum class eListType_t { eListAll, eListOnlyNamedTypes, eListOneType, eUndefined };
static void DoList(HMODULE hMod, eListType_t listType, RESTYPE resType);
static void DoExport(HMODULE hMod, RESTYPE resType, const wchar_t* resName, const wchar_t* szOutfile);

// ------------------------------------------------------------------------------------------

int wmain(int argc, wchar_t** argv)
{
	if (argc < 3)
		Usage(argv[0]);

	// Make sure the first two parameters are present; validate the command now.
	// (Determine whether the source file is valid when we go to use it.)
	const wchar_t* szSrcFile = argv[1];
	const wchar_t* szCommand = argv[2];
	bool bList = (0 == _wcsicmp(szCommand, L"-list"));
	bool bExport = (0 == _wcsicmp(szCommand, L"-export"));
	if (!bList && !bExport)
	{
		Usage(argv[0]);
	}

	// Parse the rest of the command line.
	eListType_t listType = eListType_t::eListOnlyNamedTypes;
	RESTYPE resType = 0;
	wchar_t* resName = 0;
	wchar_t* szOutFile = 0;
	for (int ixArg = 3; ixArg < argc; ++ixArg)
	{
		if (0 == _wcsicmp(argv[ixArg], L"-t"))
		{
			if (++ixArg >= argc)
				Usage(argv[0]);
			wchar_t* szResType = argv[ixArg];
			if (0 == wcscmp(szResType, L"*"))
				listType = eListType_t::eListAll;
			else
			{
				listType = eListType_t::eListOneType;
				resType = StrToResType(szResType);
				if (0 == resType)
					resType = szResType;
			}
		}
		else if (0 == _wcsicmp(argv[ixArg], L"-n"))
		{
			// Parameter valid only with export, and only if resName not already set
			if (!bExport || (0 != resName))
				Usage(argv[0]);
			if (++ixArg >= argc)
				Usage(argv[0]);
			resName = argv[ixArg];
		}
		else if (0 == _wcsicmp(argv[ixArg], L"-i"))
		{
			// Parameter valid only with export, and only if resName not already set
			if (!bExport || (0 != resName))
				Usage(argv[0]);
			if (++ixArg >= argc)
				Usage(argv[0]);
			DWORD dwRsrc;
			if (1 != swscanf_s(argv[ixArg], L"%u", &dwRsrc))
				Usage(argv[0]);
			resName = MAKEINTRESOURCEW(dwRsrc);
		}
		else if (0 == _wcsicmp(argv[ixArg], L"-o"))
		{
			if (!bExport)
				Usage(argv[0]);
			if (++ixArg >= argc)
				Usage(argv[0]);
			szOutFile = argv[ixArg];
		}
		else
		{
			Usage(argv[0]);
		}
	}

	// For export, ensure that all necessary Validate selections
	if (bExport && !(resType && resName && szOutFile))
	{
		Usage(argv[0]);
	}

	Wow64FsRedirection fsRedir(true);
	HMODULE hMod = LoadLibraryExW(szSrcFile, NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
	DWORD dwLastErr = GetLastError();
	fsRedir.Revert();
	if (NULL == hMod)
	{
		std::wcerr << SysErrorMessageWithCode(dwLastErr) << std::endl << std::endl;
		Usage(argv[0]);
	}

	if (bList)
		DoList(hMod, listType, resType);
	else // bExport
		DoExport(hMod, resType, resName, szOutFile);

	if (hMod) FreeLibrary(hMod);

	return 0;
}

// ------------------------------------------------------------------------------------------

static LPVOID GetResourceSizeAndData(HMODULE hMod, RESTYPE resType, const wchar_t* resName, DWORD& dwResourceSize)
{
	dwResourceSize = 0;
	const wchar_t* szErrLabel = L"";

	szErrLabel = L"Cannot find resource";
	HRSRC hRsrc = FindResourceW(hMod, resName, resType);
	if (hRsrc)
	{
		szErrLabel = L"Cannot get resource size";
		dwResourceSize = SizeofResource(hMod, hRsrc);
		if (dwResourceSize > 0)
		{
			szErrLabel = L"Cannot load resource";
			HGLOBAL hGbl = LoadResource(hMod, hRsrc);
			if (NULL != hGbl)
			{
				szErrLabel = L"Cannot lock resource";
				LPVOID pData = LockResource(hGbl);
				if (pData)
				{
					return pData;
				}
			}
		}
	}

	DWORD dwLastErr = GetLastError();
	std::wcerr << szErrLabel << L": " << SysErrorMessageWithCode(dwLastErr) << std::endl;
	return nullptr;
}

// ------------------------------------------------------------------------------------------

BOOL CALLBACK EnumResNameProcW(
	_In_opt_ HMODULE hModule,
	_In_ LPCWSTR resType,
	_In_ LPWSTR resName,
	_In_ LONG_PTR) //lParam)
{
	bool bTypeIsIntResource = IS_INTRESOURCE(resType);
	bool bNameIsIntResource = IS_INTRESOURCE(resName);
	DWORD dwResourceSize = 0;
	LPVOID pData = GetResourceSizeAndData(hModule, resType, resName, dwResourceSize);
	if (pData)
	{
		// Output type
		if (!bTypeIsIntResource)
		{
			std::wcout << resType << szDelim;
		}
		else
		{
			const wchar_t* szType = ResTypeToStr(resType);
			if (szType)
				std::wcout << szType << szDelim;
			else
				std::wcout << L"INT(" << ULONGLONG(resType) << L")" << szDelim;
		}

		// Output name
		if (!bNameIsIntResource)
			std::wcout << resName << szDelim;
		else
			std::wcout << L"INT(" << ULONGLONG(resName) << L")" << szDelim;

		// Output resource size
		std::wcout << dwResourceSize << szDelim;

		// Output first bytes
		const byte* pBytes = (const byte*)pData;
		DWORD numBytes = min(dwResourceSize, 8);
		bool bAddSpace = false;
		for (DWORD ixByte = 0; ixByte < numBytes; ++ixByte)
		{
			byte b = pBytes[ixByte];
			const wchar_t* szSpecial = nullptr;
			switch (b)
			{
			case 0x09: szSpecial = L"TAB"; break;
			case 0x0A: szSpecial = L"LF"; break;
			case 0x0D: szSpecial = L"CR"; break;
			case 0x22: szSpecial = L"DQ"; break;
			}
			if (szSpecial)
			{
				if (0 != ixByte) std::wcout << L" ";
				std::wcout << szSpecial;
				bAddSpace = true;
			}
			else if (0x20 <= b && b < 0x7f)
			{
				if (bAddSpace) { std::wcout << L" "; }
				std::wcout << (wchar_t)b;
				bAddSpace = false;
			}
			else
			{
				if (0 != ixByte) std::wcout << L" ";
				std::wcout << HEXW(pBytes[ixByte]);
				bAddSpace = true;
			}
		}
		std::wcout << std::endl;
	}
	return TRUE;
}

//typedef struct {
//	eListType_t listType;
//	RESTYPE resType;
//} DoListLParam_t;

BOOL CALLBACK EnumResTypeProcW(
	_In_opt_ HMODULE hModule,
	_In_ LPWSTR resType,
	_In_ LONG_PTR lParam
)
{
	if (!lParam)
		return TRUE;
	eListType_t* pListType = (eListType_t*)lParam;
	if (eListType_t::eListOnlyNamedTypes == *pListType && IS_INTRESOURCE(resType))
		return TRUE;
	EnumResourceNamesExW(hModule, resType, EnumResNameProcW, NULL, RESOURCE_ENUM_LN, 0);
	return TRUE;
}

static void DoList(HMODULE hMod, eListType_t listType, RESTYPE resType)
{
	// Tab-delimited headers
	std::wcout
		<< L"Type" << szDelim
		<< L"Name" << szDelim
		<< L"Size" << szDelim
		<< L"First bytes" << std::endl;

	if (eListType_t::eListOneType == listType)
	{
		EnumResourceNamesExW(hMod, resType, EnumResNameProcW, NULL, RESOURCE_ENUM_LN, 0);
	}
	else
	{
		EnumResourceTypesExW(hMod, EnumResTypeProcW, (LONG_PTR)&listType, RESOURCE_ENUM_LN, 0);
	}
}

// ------------------------------------------------------------------------------------------

static void DoExport(HMODULE hMod, RESTYPE resType, const wchar_t* resName, const wchar_t* szOutfile)
{
	DWORD dwResourceSize = 0;
	LPVOID pData = GetResourceSizeAndData(hMod, resType, resName, dwResourceSize);
	if (pData)
	{
		Wow64FsRedirection fsRedir(true);
		HANDLE hFile = CreateFileW(szOutfile, FILE_GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		DWORD dwLastErr = GetLastError();
		fsRedir.Revert();
		if (INVALID_HANDLE_VALUE == hFile)
		{
			std::wcerr << L"Cannot create export file: " << SysErrorMessageWithCode(dwLastErr) << std::endl;
			return;
		}
		DWORD dwNumWritten = 0;
		BOOL wfRet = WriteFile(hFile, pData, dwResourceSize, &dwNumWritten, NULL);
		dwLastErr = GetLastError();
		CloseHandle(hFile);
		if (wfRet)
			std::wcerr << L"Success." << std::endl;
		else
			std::wcerr << L"Could not write to export file: " << SysErrorMessageWithCode(dwLastErr) << std::endl;
	}
}
