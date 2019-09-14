#include <iostream>
#include <string>

#include <Windows.h>


#include <winternl.h>


extern "C"
NTSYSCALLAPI NTSTATUS NTAPI NtWaitForDebugEvent	(	_In_ HANDLE 	DebugObjectHandle,
_In_ BOOLEAN 	Alertable,
_In_opt_ PLARGE_INTEGER 	Timeout,
_Out_ PVOID 	WaitStateChange
);

#define DEBUG_READ_EVENT 0x0001
#define DEBUG_PROCESS_ASSIGN 0x0002
#define DEBUG_SET_INFORMATION 0x0004
#define DEBUG_QUERY_INFORMATION 0x0008
#define DEBUG_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | \
    DEBUG_READ_EVENT | DEBUG_PROCESS_ASSIGN | DEBUG_SET_INFORMATION | \
    DEBUG_QUERY_INFORMATION)

#pragma comment (linker, "/defaultlib:ntdll.lib")

extern "C"
NTSYSCALLAPI
NTSTATUS
NTAPI
NtDebugActiveProcess(
    _In_ HANDLE ProcessHandle,
    _In_ HANDLE DebugObjectHandle
    );

extern "C"
NTSYSAPI
NTSTATUS
NTAPI
NtCreateDebugObject(
  OUT PHANDLE             DebugObjectHandle,
  IN ACCESS_MASK          DesiredAccess,
  IN POBJECT_ATTRIBUTES   ObjectAttributes OPTIONAL,
  IN BOOLEAN              KillProcessOnExit );

 //
 // Debug States
 //
 typedef enum _DBG_STATE
 {
     DbgIdle,
     DbgReplyPending,
     DbgCreateThreadStateChange,
     DbgCreateProcessStateChange,
     DbgExitThreadStateChange,
     DbgExitProcessStateChange,
     DbgExceptionStateChange,
     DbgBreakpointStateChange,
     DbgSingleStepStateChange,
     DbgLoadDllStateChange,
     DbgUnloadDllStateChange
 } DBG_STATE, *PDBG_STATE;

 //
 // Debug Message Structures
 //
 typedef struct _DBGKM_EXCEPTION
 {
     EXCEPTION_RECORD ExceptionRecord;
     ULONG FirstChance;
 } DBGKM_EXCEPTION, *PDBGKM_EXCEPTION;

 typedef struct _DBGKM_CREATE_THREAD
 {
     ULONG SubSystemKey;
     PVOID StartAddress;
 } DBGKM_CREATE_THREAD, *PDBGKM_CREATE_THREAD;

 typedef struct _DBGKM_CREATE_PROCESS
 {
     ULONG SubSystemKey;
     HANDLE FileHandle;
     PVOID BaseOfImage;
     ULONG DebugInfoFileOffset;
     ULONG DebugInfoSize;
     DBGKM_CREATE_THREAD InitialThread;
 } DBGKM_CREATE_PROCESS, *PDBGKM_CREATE_PROCESS;

 typedef struct _DBGKM_EXIT_THREAD
 {
     NTSTATUS ExitStatus;
 } DBGKM_EXIT_THREAD, *PDBGKM_EXIT_THREAD;

 typedef struct _DBGKM_EXIT_PROCESS
 {
     NTSTATUS ExitStatus;
 } DBGKM_EXIT_PROCESS, *PDBGKM_EXIT_PROCESS;

 typedef struct _DBGKM_LOAD_DLL
 {
     HANDLE FileHandle;
     PVOID BaseOfDll;
     ULONG DebugInfoFileOffset;
     ULONG DebugInfoSize;
     PVOID NamePointer;
 } DBGKM_LOAD_DLL, *PDBGKM_LOAD_DLL;

 typedef struct _DBGKM_UNLOAD_DLL
 {
     PVOID BaseAddress;
 } DBGKM_UNLOAD_DLL, *PDBGKM_UNLOAD_DLL;

typedef struct _DBGUI_WAIT_STATE_CHANGE
 {
     DBG_STATE NewState;
     CLIENT_ID AppClientId;
     union
     {
         struct
         {
             HANDLE HandleToThread;
             DBGKM_CREATE_THREAD NewThread;
         } CreateThread;
         struct
         {
             HANDLE HandleToProcess;
             HANDLE HandleToThread;
             DBGKM_CREATE_PROCESS NewProcess;
         } CreateProcessInfo;
         DBGKM_EXIT_THREAD ExitThread;
         DBGKM_EXIT_PROCESS ExitProcess;
         DBGKM_EXCEPTION Exception;
         DBGKM_LOAD_DLL LoadDll;
         DBGKM_UNLOAD_DLL UnloadDll;
     } StateInfo;
 } DBGUI_WAIT_STATE_CHANGE, *PDBGUI_WAIT_STATE_CHANGE;

constexpr auto processPath = L"C:\\Windows\\SYSWOW64\\notepad.exe";
PROCESS_INFORMATION startProcessSuspended()
{
	STARTUPINFOW startupInfo = {};
	PROCESS_INFORMATION processInformation = {};

	const auto result = CreateProcessW(
		processPath,
		nullptr,
		nullptr,
		nullptr,
		false,
		CREATE_SUSPENDED,
		nullptr,
		nullptr,
		&startupInfo,
		&processInformation);
	std::cout << "Create process " << processInformation.dwProcessId
		<< " " << (result ? "success" : "error") << std::endl;

	return processInformation;
}

void terminateProcess(PROCESS_INFORMATION processInformation)
{
	const auto result = TerminateProcess(processInformation.hProcess, EXIT_SUCCESS);
	CloseHandle(processInformation.hProcess);
	CloseHandle(processInformation.hThread);
	std::cout << "Terminate process " << processInformation.dwProcessId << (result ? " success" : " error") << std::endl;
}

extern "C"
NTSYSCALLAPI NTSTATUS NTAPI 	NtDebugContinue (_In_ HANDLE DebugObjectHandle, _In_ CLIENT_ID* ClientId, _In_ NTSTATUS ContinueStatus);

bool debugProcessLoop(HANDLE debugObject)
{
	// DEBUG_EVENT debugEvent = {};
	auto firstThreadCreated = false;
	DBGUI_WAIT_STATE_CHANGE debugEvent = {};
	LARGE_INTEGER timeout = {};
	timeout.LowPart = 60000;
	NTSTATUS waitStatus;
	while ((waitStatus = NtWaitForDebugEvent(debugObject, true, nullptr, &debugEvent)) == 0)
	{
		std::cout << "DebugEvent: " << debugEvent.NewState << std::endl;
		switch (debugEvent.NewState)
		{
		case DbgUnloadDllStateChange:
			std::cout << "  DbgUnloadDllStateChange" << std::endl;
	        break;
		case DbgCreateProcessStateChange:
			std::cout << "  DbgCreateProcessStateChange: "
				<< "ProcessId = " << GetProcessId(debugEvent.StateInfo.CreateProcessInfo.HandleToProcess)
				<< " ThreadId = " << GetThreadId(debugEvent.StateInfo.CreateProcessInfo.HandleToThread) << std::endl;
			break;
		case DbgExceptionStateChange: {
			auto exceptionCode = debugEvent.StateInfo.Exception.ExceptionRecord.ExceptionCode;
			std::cout << "  DbgExceptionStateChange: " << exceptionCode << std::endl;
			if (exceptionCode == EXCEPTION_ACCESS_VIOLATION)
			{
				std::cout << "    This is EXCEPTION_ACCESS_VIOLATION" << std::endl;
				return false;
			}
			break;
		}
		case DbgCreateThreadStateChange:
			std::cout << "  DbgCreateThreadStateChange: " << GetThreadId(debugEvent.StateInfo.CreateThread.HandleToThread) << std::endl;
			firstThreadCreated = true;
			break;
		case DbgLoadDllStateChange:
			std::cout << "  DbgLoadDllStateChange" << std::endl;
			CloseHandle(debugEvent.StateInfo.LoadDll.FileHandle);
			if (firstThreadCreated)
				return true; // success
			break;
		case DbgExitProcessStateChange:
			std::cout << "  DbgExitProcessStateChange" << std::endl;
			// to close all the handles

			// ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, DBG_CONTINUE);
			return true;
		default:
			std::cout << "  (UNKNOWN)" << std::endl;
			break;
		}

		const auto result = NtDebugContinue(debugObject, &debugEvent.AppClientId, DBG_EXCEPTION_NOT_HANDLED);
		if (result != 0)
		{
			std::cout << "NtDebugContinue error: " << result << std::endl;
			return false;
		}
	}

	std::cout << "NtWaitForDebugEvent returned error code: " << waitStatus << std::endl;
	return false;
}

bool debugProcess(PROCESS_INFORMATION processInformation)
{
	OBJECT_ATTRIBUTES attributes;
	InitializeObjectAttributes(&attributes, 0, 0, 0, 0);

	HANDLE debugObjectHandle = nullptr;
	auto createResult = NtCreateDebugObject(&debugObjectHandle, DEBUG_ALL_ACCESS, &attributes, false);
	std::cout << "NtCreateDebugObject result: " << createResult << std::endl;

	if (NtDebugActiveProcess(processInformation.hProcess, debugObjectHandle) != 0)
	{
		std::cout << "DebugActiveProcess error: " << GetLastError() << std::endl;
		return false;
	}

	if (ResumeThread(processInformation.hThread) == (DWORD)-1)
	{
		std::cout << "ResumeThread error: " << GetLastError() << std::endl;
		return false;
	}

	const auto result = debugProcessLoop(debugObjectHandle);
	if (!DebugActiveProcessStop(processInformation.dwProcessId))
	{
		std::cout << "DebugActiveProcessStop error: " << GetLastError() << std::endl;
	}

	return result;
}

int main()
{
	auto successCount = 0, failureCount = 0;
	for (auto i = 0; i < 1000; ++i)
	{
		const auto processInformation = startProcessSuspended();
		if (debugProcess(processInformation))
			++successCount;
		else
			++failureCount;
		terminateProcess(processInformation);
	}

	std::cerr << "All tests finished. Success = " << successCount << ", failure = " << failureCount << std::endl;
}
