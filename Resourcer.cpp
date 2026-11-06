// Resourcer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <iostream>
#include <sstream>
#include <list>
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
		<< L"    " << sExe << L" srcFile -list [-t resType] [-c|-ct]" << std::endl
		<< L"    " << sExe << L" srcFile -export -t resType {-n resName | -i resID} -o outfile" << std::endl
		<< std::endl
		<< L"srcFile must be the first parameter: the path to a portable executable (PE) file with resources." << std::endl
		<< L"-list or -export must be the second parameter." << std::endl
		<< std::endl
		<< L"With -list:" << std::endl
		<< L"  resType : if *, lists all resources;" << std::endl
		<< L"            if a particular resource type, lists resources of that type" << std::endl
		<< L"                (see list below for predefined types);" << std::endl
		<< L"            if omitted, lists resources with named (non-MAKEINTRESOURCE) types." << std::endl
		<< L"  -c      : Output as comma-delimited text." << std::endl
		<< L"  -ct     : Output as tab-delimited text." << std::endl
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

enum class eListType_t { eListAll, eListOnlyNamedTypes, eListOneType, eUndefined };
enum class eListFormat_t { eTable, eCommaDelimited, eTabDelimited };
static void DoList(HMODULE hModule, eListFormat_t listFmt, eListType_t listType, RESTYPE resType);
static void DoExport(HMODULE hModule, RESTYPE resType, const wchar_t* resName, const wchar_t* szOutfile);

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
	eListFormat_t listFmt = eListFormat_t::eTable;
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
		else if (0 == _wcsicmp(argv[ixArg], L"-c"))
		{
			listFmt = eListFormat_t::eCommaDelimited;
		}
		else if (0 == _wcsicmp(argv[ixArg], L"-ct"))
		{
			listFmt = eListFormat_t::eTabDelimited;
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
	HMODULE hModule = LoadLibraryExW(szSrcFile, NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
	DWORD dwLastErr = GetLastError();
	fsRedir.Revert();
	if (NULL == hModule)
	{
		std::wcerr << SysErrorMessageWithCode(dwLastErr) << std::endl << std::endl;
		Usage(argv[0]);
	}

	if (bList)
		DoList(hModule, listFmt, listType, resType);
	else // bExport
		DoExport(hModule, resType, resName, szOutFile);

	if (hModule) FreeLibrary(hModule);

	return 0;
}

// ------------------------------------------------------------------------------------------

/// <summary>
/// Utility function used by both list and export features.
/// Returns resource size through reference variable, and address of resource data as function return value.
/// On failure, writes error information to stderr and returns nullptr.
/// </summary>
static LPVOID GetResourceSizeAndData(HMODULE hModule, RESTYPE resType, const wchar_t* resName, DWORD& dwResourceSize)
{
	dwResourceSize = 0;
	const wchar_t* szErrLabel = L"";

	szErrLabel = L"Cannot find resource";
	HRSRC hRsrc = FindResourceW(hModule, resName, resType);
	if (hRsrc)
	{
		szErrLabel = L"Cannot get resource size";
		dwResourceSize = SizeofResource(hModule, hRsrc);
		if (dwResourceSize > 0)
		{
			szErrLabel = L"Cannot load resource";
			HGLOBAL hGbl = LoadResource(hModule, hRsrc);
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

typedef struct {
	std::wstring sType, sName, sFirstBytes;
	DWORD dwResourceSize;
} resourceData_t;
typedef std::list<resourceData_t> resourceList_t;

class resourceListData_t
{
public:
	resourceListData_t() = default;
	~resourceListData_t() = default;

	void Add(const std::wstring& sResType, const std::wstring& sResName, DWORD dwResourceSize, const std::wstring& sFirstBytes)
	{
		resourceData_t data;
		data.sType = sResType;
		data.sName = sResName;
		data.dwResourceSize = dwResourceSize;
		data.sFirstBytes = sFirstBytes;
		m_maxTypeLen = max(m_maxTypeLen, sResType.length());
		m_maxNameLen = max(m_maxNameLen, sResName.length());
		m_maxSize = max(m_maxSize, dwResourceSize);
		m_resourceList.push_back(data);
	}

	resourceList_t::const_iterator Begin() const
	{
		return m_resourceList.begin();
	}
	resourceList_t::const_iterator End() const
	{
		return m_resourceList.end();
	}

	size_t MaxTypeLen() const { return m_maxTypeLen; }
	size_t MaxNameLen() const { return m_maxNameLen; }
	size_t MaxSizeDecimalDigits() const {
		if (m_maxSize < 10'000)
			return 4;
		if (m_maxSize < 100'000)
			return 5;
		if (m_maxSize < 1'000'000)
			return 6;
		if (m_maxSize < 10'000'000)
			return 7;
		if (m_maxSize < 100'000'000)
			return 8;
		if (m_maxSize < 1'000'000'000)
			return 9;
		return 10;
	}

private:
	size_t m_maxTypeLen = 5, m_maxNameLen = 5, m_maxSize = 1;
	resourceList_t m_resourceList;

private:
	resourceListData_t(const resourceListData_t&) = delete;
	resourceListData_t& operator = (const resourceListData_t&) = delete;
};

// ------------------------------------------------------------------------------------------

static resourceListData_t st_resourceListData;

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
		std::wstringstream
			strResType,
			strResName,
			strFirstBytes;

		// Resource type
		if (!bTypeIsIntResource)
		{
			strResType << resType;
		}
		else
		{
			const wchar_t* szType = ResTypeToStr(resType);
			if (szType)
				strResType << szType;
			else
				strResType << L"INT(" << ULONGLONG(resType) << L")";
		}

		// Resource name
		if (!bNameIsIntResource)
			strResName << resName;
		else
			strResName << L"INT(" << ULONGLONG(resName) << L")";

		// Format first bytes
		const byte* pBytes = (const byte*)pData;
		DWORD numBytes = min(dwResourceSize, 16);
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
			//case 0x20: szSpecial = L"SP"; break;
			//case 0x22: szSpecial = L"DQ"; break;
			}
			if (szSpecial)
			{
				if (0 != ixByte) strFirstBytes << L" ";
				strFirstBytes << szSpecial;
				bAddSpace = true;
			}
			else if (0x20 < b && b < 0x7f)
			{
				if (bAddSpace) { strFirstBytes << L" "; }
				strFirstBytes << (wchar_t)b;
				bAddSpace = false;
			}
			else
			{
				if (0 != ixByte) strFirstBytes << L" ";
				strFirstBytes << HEXW(pBytes[ixByte]);
				bAddSpace = true;
			}
		}

		st_resourceListData.Add(strResType.str(), strResName.str(), dwResourceSize, strFirstBytes.str());
	}
	return TRUE;
}

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

static void DoList(HMODULE hModule, eListFormat_t listFmt, eListType_t listType, RESTYPE resType)
{
	const wchar_t* szHdrType = L"Type";
	const wchar_t* szHdrName = L"Name";
	const wchar_t* szHdrSize = L"Size";
	const wchar_t* szHdrFirstBytes = L"First bytes";

	if (eListType_t::eListOneType == listType)
	{
		EnumResourceNamesExW(hModule, resType, EnumResNameProcW, NULL, RESOURCE_ENUM_LN, 0);
	}
	else
	{
		EnumResourceTypesExW(hModule, EnumResTypeProcW, (LONG_PTR)&listType, RESOURCE_ENUM_LN, 0);
	}

	if (eListFormat_t::eTable == listFmt)
	{
		const size_t
			nColType = st_resourceListData.MaxTypeLen() + 2,
			nColName = st_resourceListData.MaxNameLen() + 2,
			nColSize = st_resourceListData.MaxSizeDecimalDigits();
		std::wcout
			<< std::left << std::setw(nColType) << szHdrType
			<< std::left << std::setw(nColName) << szHdrName
			<< std::right << std::setw(nColSize) << szHdrSize << L"  "
			<< std::left << szHdrFirstBytes << std::endl;
		for (resourceList_t::const_iterator iter = st_resourceListData.Begin();
			iter != st_resourceListData.End();
			iter++)
		{
			std::wcout
				<< std::left << std::setw(nColType) << iter->sType
				<< std::left << std::setw(nColName) << iter->sName
				<< std::right << std::setw(nColSize) << iter->dwResourceSize << L"  "
				<< std::left << iter->sFirstBytes << std::endl;
		}
	}
	else
	{
		// Comma- or tab-delimited headers
		const wchar_t* szDelim = (eListFormat_t::eCommaDelimited == listFmt) ? L"," : L"\t";
		std::wcout
			<< szHdrType << szDelim
			<< szHdrName << szDelim
			<< szHdrSize << szDelim
			<< szHdrFirstBytes << std::endl;
		for (resourceList_t::const_iterator iter = st_resourceListData.Begin();
			iter != st_resourceListData.End();
			iter++)
		{
			std::wcout
				<< iter->sType << szDelim
				<< iter->sName << szDelim
				<< iter->dwResourceSize << szDelim
				<< iter->sFirstBytes << std::endl;
		}
	}
}

// ------------------------------------------------------------------------------------------

static void DoExport(HMODULE hModule, RESTYPE resType, const wchar_t* resName, const wchar_t* szOutfile)
{
	DWORD dwResourceSize = 0;
	LPVOID pData = GetResourceSizeAndData(hModule, resType, resName, dwResourceSize);
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
