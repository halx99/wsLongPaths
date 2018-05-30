// wsLongPaths.cpp : Defines the exported functions for the DLL application.
// V2.0

#include "stdafx.h"
#include <stdio.h>
#include <string>
#include <string_view>
#include <Windows.h>
#include <Shlwapi.h>
#include <io.h>
#include "../libwsls/libwsls.h"
#include "../minhook/include/MinHook.h"

#define ENABLE_MSGBOX_TRACE 0

#if defined(_DEBUG)
#pragma comment(lib, "../lib/Debug/libMinHook.x64.lib")
#else
#pragma comment(lib, "../lib/Release/libMinHook.x64.lib")
#endif

/*
copy命令, 无论怎样均不支持超过249的长路径
echo命令, 可以支持260长路径, 但必须有UNC前缀, 且路径必须是反斜杠
ndk提供的echo.exe,(可以支持260长路径, 但必须有UNC前缀, 且路径必须是反斜杠)
del命令, 无论如何都不支持260长路径
*/
static auto CreateFileW_imp = &CreateFileW;
static auto CreateFileA_imp = &CreateFileA;
static auto CreateProcessA_imp = &CreateProcessA;
static auto GetFullPathNameA_imp = &GetFullPathNameA;
static auto GetFileAttributesA_imp = &GetFileAttributesA;
static auto GetFileAttributesW_imp = &GetFileAttributesW;
static auto FindFirstFileExW_imp = &FindFirstFileExW;
static auto GetFileAttributesExW_imp = &GetFileAttributesExW;
static auto CreateProcessW_imp = &CreateProcessW;

#if 0
static auto _findfirst64_imp = &_findfirst64;
static auto _stat64_imp = &_stat64;
static auto _errno_imp = &_errno;
static auto _open_imp = &_open;
static auto fopen_imp = &fopen;
static auto strerror_imp = &strerror;
#endif

static auto _access_imp = &::_access;
static auto _stat64_imp = &_stat64;
static auto _open_imp = &_open;
static auto _fstat64_imp = &_fstat64;

HANDLE
WINAPI
CreateFileW_hook(
	_In_ LPCWSTR lpFileName,
	_In_ DWORD dwDesiredAccess,
	_In_ DWORD dwShareMode,
	_In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	_In_ DWORD dwCreationDisposition,
	_In_ DWORD dwFlagsAndAttributes,
	_In_opt_ HANDLE hTemplateFile
)
{
#if ENABLE_MSGBOX_TRACE
	MessageBox(nullptr, L"Call API: CreateFileW...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
	auto styledPath = wsls::makeStyledPath(lpFileName);
	if (styledPath.empty())
		return CreateFileW_imp(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

	return CreateFileW_imp(styledPath.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

HANDLE
WINAPI
CreateFileA_hook(
	_In_ LPCSTR lpFileName,
	_In_ DWORD dwDesiredAccess,
	_In_ DWORD dwShareMode,
	_In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	_In_ DWORD dwCreationDisposition,
	_In_ DWORD dwFlagsAndAttributes,
	_In_opt_ HANDLE hTemplateFile
)
{
#if ENABLE_MSGBOX_TRACE
	MessageBox(nullptr, L"Call API: CreateFileA...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
	auto styledPath = wsls::makeStyledPath(lpFileName);
	if (styledPath.empty())
		return CreateFileA_imp(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

	return CreateFileW_imp(styledPath.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

BOOL
WINAPI
CreateProcessA_hook(
	_In_opt_ LPCSTR lpApplicationName,
	_Inout_opt_ LPSTR lpCommandLine,
	_In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
	_In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
	_In_ BOOL bInheritHandles,
	_In_ DWORD dwCreationFlags,
	_In_opt_ LPVOID lpEnvironment,
	_In_opt_ LPCSTR lpCurrentDirectory,
	_In_ LPSTARTUPINFOA lpStartupInfo,
	_Out_ LPPROCESS_INFORMATION lpProcessInformation
)
{
#if ENABLE_MSGBOX_TRACE
	MessageBox(nullptr, L"Call API: CreateProcessA...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
	if (lpApplicationName != nullptr && stricmp(PathFindExtensionA(lpApplicationName), ".bat") == 0) {
		std::string fileContent = wsls::readFileData(lpApplicationName);
		if (fileContent.find("lcopy") == std::string::npos && wsls::replace_once(fileContent, "copy /b/y", "lcopy /b/y"))
		{// Replace the system copy which does not support long path >= 249
            wsls::writeFileData(lpApplicationName, fileContent);
		}
		else if (fileContent.find("echo.exe") != std::string::npos && fileContent.find("\\\\?\\") == std::string::npos)
		{

			int offset = 0;
			auto relpos = fileContent.find(">>");
			if (relpos != std::string::npos) {
				offset = 2;
			}
			else {
				relpos = fileContent.find_first_of('>');
				offset = 1;
			}

			if (relpos != std::string::npos)
			{
				const char* outPath = fileContent.c_str() + relpos + offset;
				while (*outPath == ' ' && *outPath != '\0') ++outPath; // skip whitespace

				auto offset = outPath - fileContent.c_str();
				fileContent.insert(offset, "\\\\?\\", sizeof("\\\\?\\") - 1);

                wsls::convertPathToWinStyle(fileContent, offset);

                wsls::writeFileData(lpApplicationName, fileContent);
			}
		}
		else if (fileContent.find("ldel") == std::string::npos && wsls::replace_once(fileContent, "del", "ldel"))
		{// Replace the system del which does not support long path >= 249
            wsls::writeFileData(lpApplicationName, fileContent);
		}

#if defined(_DEBUG)
		OutputDebugStringA(wsls::sfmt("Create Process:%s, content:%s\n",
			lpApplicationName,
			fileContent.c_str()
		).c_str());
#endif
	}
	else { // execute command line
		OutputDebugStringA(wsls::sfmt("execute command: %s\n", lpCommandLine).c_str());
	}

	return CreateProcessA_imp(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
		bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
}

BOOL
WINAPI
CreateProcessW_hook(
	_In_opt_ LPCWSTR lpApplicationName,
	_Inout_opt_ LPWSTR lpCommandLine,
	_In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
	_In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
	_In_ BOOL bInheritHandles,
	_In_ DWORD dwCreationFlags,
	_In_opt_ LPVOID lpEnvironment,
	_In_opt_ LPCWSTR lpCurrentDirectory,
	_In_ LPSTARTUPINFOW lpStartupInfo,
	_Out_ LPPROCESS_INFORMATION lpProcessInformation
)
{
#if ENABLE_MSGBOX_TRACE
	MessageBox(nullptr, L"Call API: CreateProcessW...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
	return CreateProcessW_imp(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
		bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
}

DWORD
WINAPI
GetFullPathNameA_hook(
	_In_ LPCSTR lpFileName,
	_In_ DWORD nBufferLength,
	_Out_writes_to_opt_(nBufferLength, return +1) LPSTR lpBuffer,
	_Outptr_opt_ LPSTR* lpFilePart
)
{
#if ENABLE_MSGBOX_TRACE
	MessageBox(nullptr, L"Call API: GetFullPathNameA...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif

	auto styledPath = wsls::makeStyledPath(lpFileName);
	if (styledPath.empty())
		return GetFullPathNameA_imp(lpFileName, nBufferLength, lpBuffer, lpFilePart);

	int n = ::WideCharToMultiByte(CP_ACP, 0, styledPath.c_str(), styledPath.length() + 1, NULL, 0, NULL, NULL);
	if (nBufferLength > n)
		return ::WideCharToMultiByte(CP_ACP, 0, styledPath.c_str(), styledPath.length() + 1, lpBuffer, nBufferLength, NULL, NULL);
	else
		return n;
}

DWORD
WINAPI
GetFileAttributesA_hook(
	_In_ LPCSTR lpFileName
)
{
#if ENABLE_MSGBOX_TRACE
	MessageBox(nullptr, L"Call API: GetFileAttributesA...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
	auto styledPath = wsls::makeStyledPath(lpFileName);
	if (styledPath.empty())
		return GetFileAttributesA_imp(lpFileName);

	return GetFileAttributesW_imp(styledPath.c_str());
}

DWORD
WINAPI
GetFileAttributesW_hook(
	_In_ LPCWSTR lpFileName
)
{
#if ENABLE_MSGBOX_TRACE
	MessageBox(nullptr, L"Call API: GetFileAttributesW...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
	auto styledPath = wsls::makeStyledPath(lpFileName);
	if (styledPath.empty()) {
		return GetFileAttributesW_imp(lpFileName);
	}
	return GetFileAttributesW_imp(styledPath.c_str());
}

HANDLE
WINAPI
FindFirstFileExW_hook(
	_In_ LPCWSTR lpFileName,
	_In_ FINDEX_INFO_LEVELS fInfoLevelId,
	_Out_writes_bytes_(sizeof(WIN32_FIND_DATAW)) LPVOID lpFindFileData,
	_In_ FINDEX_SEARCH_OPS fSearchOp,
	_Reserved_ LPVOID lpSearchFilter,
	_In_ DWORD dwAdditionalFlags
)
{
#if ENABLE_MSGBOX_TRACE
	MessageBox(nullptr, L"Call API: FindFirstFileExW...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
	auto styledPath = wsls::makeStyledPath(lpFileName);
	if (styledPath.empty())
		return FindFirstFileExW_imp(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);

	return FindFirstFileExW_imp(styledPath.c_str(), fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
}

BOOL
WINAPI
GetFileAttributesExW_hook(
	_In_ LPCWSTR lpFileName,
	_In_ GET_FILEEX_INFO_LEVELS fInfoLevelId,
	_Out_writes_bytes_(sizeof(WIN32_FILE_ATTRIBUTE_DATA)) LPVOID lpFileInformation
)
{
#if ENABLE_MSGBOX_TRACE
	MessageBox(nullptr, L"Call API: GetFileAttributesExW...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
	auto styledPath = wsls::makeStyledPath(lpFileName);
	if (styledPath.empty())
		return GetFileAttributesExW_imp(lpFileName, fInfoLevelId, lpFileInformation);

	return GetFileAttributesExW_imp(styledPath.c_str(), fInfoLevelId, lpFileInformation);
}

int __cdecl _access_hook(
	_In_z_ char const* _FileName,
	_In_   int         _AccessMode
)
{
#if ENABLE_MSGBOX_TRACE
	MessageBox(nullptr, L"Call API: _access_hook...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
	auto styledPath = wsls::makeStyledPath(_FileName);
	if (styledPath.empty())
		return _access_imp(_FileName, _AccessMode);

	return _waccess(styledPath.c_str(), _AccessMode);
}

int __cdecl _stat64_hook(
	_In_z_ char const*     _FileName,
	_Out_  struct _stat64* _Stat
)
{
#if ENABLE_MSGBOX_TRACE
	MessageBox(nullptr, L"Call API: _stat64_hook...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
	auto styledPath = wsls::makeStyledPath(_FileName);
	if (styledPath.empty())
		return _stat64_imp(_FileName, _Stat);

	return _wstat64(styledPath.c_str(), _Stat);
}

int __CRTDECL _open_hook(
	_In_z_ char const* const _FileName,
	_In_   int         const _OFlag,
	_In_   int         const _PMode = 0
)
{
	auto styledPath = wsls::makeStyledPath(_FileName);
	if (styledPath.empty())
		return _open_imp(_FileName, _OFlag, _PMode);

	return _wopen(styledPath.c_str(), _OFlag, _PMode);
}

#if 0

char* __cdecl strerror_hook(
	_In_ int _ErrorMessage
)
{
	return strerror_imp(_ErrorMessage);
}

int __cdecl _fstat64_hook(
	_In_  int             _FileHandle,
	_Out_ struct _stat64* _Stat
)
{
	return _fstat64_imp(_FileHandle, _Stat);
}

int __cdecl _access_hook(
	_In_z_ char const* _FileName,
	_In_   int         _AccessMode
)
{
	MessageBox(nullptr, L"Call API: _access_hook...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
	return _access_imp(_FileName, _AccessMode);
}

intptr_t __cdecl _findfirst64_hook(
	_In_z_ char const*            _FileName,
	_Out_  struct __finddata64_t* _FindData
)
{
	MessageBox(nullptr, L"Call API: _findfirst64...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
	return _findfirst64_imp(_FileName, _FindData);
}

int* __cdecl _errno_hook(void)
{
	MessageBox(nullptr, L"Call API: _errno_hook...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
	auto perr = _errno_imp();
	return perr;
}

void* __cdecl memcpy_hook(
	_Out_writes_bytes_all_(_Size) void* _Dst,
	_In_reads_bytes_(_Size)       void const* _Src,
	_In_                          size_t      _Size
)
{
	MessageBox(nullptr, L"Call API: memcpy...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
	return memcpy_imp(_Dst, _Src, _Size);
}

FILE* __cdecl fopen_hook(
	_In_z_ char const* _FileName,
	_In_z_ char const* _Mode
)
{
	return fopen_imp(_FileName, _Mode);
}
#endif

void InstallHook()
{
	char appName[MAX_PATH + 1];
	GetModuleFileNameA(GetModuleHandle(nullptr), appName, MAX_PATH);

	HMODULE hModule = GetModuleHandle(L"kernel32.dll");

	// wchar_t modulePath[MAX_PATH];
	// GetModuleFileNameW(GetModuleHandle(nullptr), modulePath, MAX_PATH);
	CreateFileW_imp = (decltype(CreateFileW_imp))GetProcAddress(hModule, "CreateFileW");
	CreateFileA_imp = (decltype(CreateFileA_imp))GetProcAddress(hModule, "CreateFileA");

	CreateProcessA_imp = (decltype(CreateProcessA_imp))GetProcAddress(hModule, "CreateProcessA");
	CreateProcessW_imp = (decltype(CreateProcessW_imp))GetProcAddress(hModule, "CreateProcessW");
	GetFullPathNameA_imp = (decltype(GetFullPathNameA_imp))GetProcAddress(hModule, "GetFullPathNameA");

	GetFileAttributesA_imp = (decltype(GetFileAttributesA_imp))GetProcAddress(hModule, "GetFileAttributesA");
	GetFileAttributesW_imp = (decltype(GetFileAttributesW_imp))GetProcAddress(hModule, "GetFileAttributesW");
	GetFileAttributesExW_imp = (decltype(GetFileAttributesExW_imp))GetProcAddress(hModule, "GetFileAttributesExW");
	FindFirstFileExW_imp = (decltype(FindFirstFileExW_imp))GetProcAddress(hModule, "FindFirstFileExW");

	hModule = GetModuleHandle(L"msvcrt.dll");
	_access_imp = (decltype(_access_imp))GetProcAddress(hModule, "_access");
	_stat64_imp = (decltype(_stat64_imp))GetProcAddress(hModule, "_stat64");
	_open_imp = (decltype(_open_imp))GetProcAddress(hModule, "_open");
	_fstat64_imp = (decltype(_fstat64_imp))GetProcAddress(hModule, "_fstat64");

	MH_Initialize();
	MH_CreateHook(CreateFileW_imp, CreateFileW_hook, (LPVOID*)&CreateFileW_imp);
	MH_CreateHook(CreateFileA_imp, CreateFileA_hook, (LPVOID*)&CreateFileA_imp);

	MH_CreateHook(CreateProcessA_imp, CreateProcessA_hook, (LPVOID*)&CreateProcessA_imp);
	MH_CreateHook(CreateProcessW_imp, CreateProcessW_hook, (LPVOID*)&CreateProcessW_imp);

	MH_CreateHook(GetFullPathNameA_imp, GetFullPathNameA_hook, (LPVOID*)&GetFullPathNameA_imp);

	MH_CreateHook(GetFileAttributesA_imp, GetFileAttributesA_hook, (LPVOID*)&GetFileAttributesA_imp);
	MH_CreateHook(GetFileAttributesW_imp, GetFileAttributesW_hook, (LPVOID*)&GetFileAttributesW_imp);
	MH_CreateHook(GetFileAttributesExW_imp, GetFileAttributesExW_hook, (LPVOID*)&GetFileAttributesExW_imp);
	MH_CreateHook(FindFirstFileExW_imp, FindFirstFileExW_hook, (LPVOID*)&FindFirstFileExW_imp);

	if (_access_imp)
		MH_CreateHook(_access_imp, _access_hook, (LPVOID*)&_access_imp);

	if (_stat64_imp)
		MH_CreateHook(_stat64_imp, _stat64_hook, (LPVOID*)&_stat64_imp);

	if (_open_imp)
		MH_CreateHook(_open_imp, _open_hook, (LPVOID*)&_open_imp);

#if 0
	if (strerror_imp)
		MH_CreateHook(strerror_imp, strerror_hook, (LPVOID*)&strerror_imp);
	if (_fstat64_imp)
		MH_CreateHook(_fstat64_imp, _fstat64_hook, (LPVOID*)&_fstat64_imp);
#endif

	MH_EnableHook(MH_ALL_HOOKS);

	system(wsls::sfmt("echo \"Install patch: wsLongPaths.dll for %s succeed.\"", appName).c_str());

#if defined(_DEBUG)
	MessageBoxA(nullptr, wsls::sfmt("Install patch: wsLongPaths.dll for %s succeed.", appName).c_str(), "Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif

}
