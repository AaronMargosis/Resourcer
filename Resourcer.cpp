// Resourcer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <iostream>
#include "StringUtils.h"
#include "Wow64FsRedirection.h"
#include "SysErrorMessage.h"
#include "PredefResTypes.h"

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

enum class eListType_t { eListAll, eListNamedTypes, eListOneType, eUndefined };
static void DoList(HMODULE hMod, eListType_t listType, RESTYPE resType);
static void DoExport(HMODULE hMod, RESTYPE resType, const wchar_t* resName, const wchar_t* szOutfile);

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
	eListType_t listType = eListType_t::eListNamedTypes;
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

	FreeLibrary(hMod);

	return 0;
}

static void DoList(HMODULE hMod, eListType_t listType, RESTYPE resType)
{

}

static void DoExport(HMODULE hMod, RESTYPE resType, const wchar_t* resName, const wchar_t* szOutfile)
{

}
