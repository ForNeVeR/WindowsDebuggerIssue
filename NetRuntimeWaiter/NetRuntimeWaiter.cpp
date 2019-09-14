#include <iostream>
#include <string>
#include <vector>
#include <cstddef>

#include <Windows.h>

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

void resumeThread(DWORD threadId)
{
	auto handle = OpenThread(THREAD_SUSPEND_RESUME, false, threadId);
	if (handle == nullptr)
		throw std::runtime_error(std::string("OpenThread error ") + std::to_string(GetLastError()));
	if (ResumeThread(handle) == (DWORD)-1)
		throw std::runtime_error(std::string("ResumeThread error ") + std::to_string(GetLastError()));
	if (CloseHandle(handle) == 0)
		throw std::runtime_error(std::string("CloseHandle error ") + std::to_string(GetLastError()));
}

bool debugProcessLoop()
{
	DEBUG_EVENT debugEvent = {};
	auto firstThreadCreated = false;
	while (WaitForDebugEvent(&debugEvent, INFINITE))
	{
		std::cout << "DebugEvent: " << debugEvent.dwDebugEventCode << " (thread: " << debugEvent.dwThreadId << ")" << std::endl;
		auto continueStatus = DBG_CONTINUE;
		switch (debugEvent.dwDebugEventCode)
		{
		case OUTPUT_DEBUG_STRING_EVENT:
			std::cout << "  OUTPUT_DEBUG_STRING_EVENT" << std::endl;
			break;
		case UNLOAD_DLL_DEBUG_EVENT:
			std::cout << "  UNLOAD_DLL_DEBUG_EVENT" << std::endl;
	        break;
		case CREATE_PROCESS_DEBUG_EVENT:
			std::cout << "  CREATE_PROCESS_DEBUG_EVENT: "
				<< "ProcessId = " << GetProcessId(debugEvent.u.CreateProcessInfo.hProcess)
				<< " ThreadId = " << GetThreadId(debugEvent.u.CreateProcessInfo.hThread) << std::endl;
			CloseHandle(debugEvent.u.CreateProcessInfo.hFile);
			break;
		case EXCEPTION_DEBUG_EVENT: {
			auto exceptionCode = debugEvent.u.Exception.ExceptionRecord.ExceptionCode;
			std::cout << "  EXCEPTION: " << exceptionCode << std::endl;
			if (exceptionCode == EXCEPTION_ACCESS_VIOLATION)
			{
				std::cout << "    This is EXCEPTION_ACCESS_VIOLATION" << std::endl;
				return false;
			}
			break;
		}
		case CREATE_THREAD_DEBUG_EVENT:
			std::cout << "  CREATE_THREAD_DEBUG_EVENT: " << GetThreadId(debugEvent.u.CreateThread.hThread) << std::endl;
			firstThreadCreated = true;
			break;
		case LOAD_DLL_DEBUG_EVENT:
			std::cout << "  LOAD_DLL_DEBUG_EVENT" << std::endl;
			CloseHandle(debugEvent.u.LoadDll.hFile);
			if (firstThreadCreated)
				return true; // success
			break;
		case EXIT_PROCESS_DEBUG_EVENT:
			std::cout << "  EXIT_PROCESS_DEBUG_EVENT" << std::endl;
			// to close all the handles
			ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, DBG_CONTINUE);
			return true;
		default:
			std::cout << "  (UNKNOWN)" << std::endl;
			break;
		}

		const auto result = ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, DBG_EXCEPTION_NOT_HANDLED);
		if (!result)
		{
			std::cout << "ContinueDebugEvent error" << std::endl;
			return false;
		}
	}
}

bool debugProcess(PROCESS_INFORMATION processInformation)
{
	if (!DebugActiveProcess(processInformation.dwProcessId))
	{
		std::cout << "DebugActiveProcess error: " << GetLastError() << std::endl;
		return false;
	}

	const auto result = debugProcessLoop();
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

	std::cout << "All tests finished. Success = " << successCount << ", failure = " << failureCount << std::endl;
}
