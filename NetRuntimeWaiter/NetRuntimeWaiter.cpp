#include <iostream>
#include <string>
#include <vector>
#include <cstddef>

#include <Windows.h>

void printUsage()
{
	std::cout << "Usage: NetRuntimeWaiter <suspended-process-pid> <thread-id> <timeout-milliseconds>" << std::endl;
}

int waitForRuntime(DWORD, DWORD, int);

#define APPLICATION_PATH L"C:\\Windows\\SYSWOW64\\notepad.exe"

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

int main(int argc, char* argv[])
{
	try {
		SetProcessDPIAware();

		DWORD suspendedProcessPid = 0UL, threadId = 0UL;
		HANDLE hProcess = nullptr;
		int timeOutMs = 0;

		if (argc == 4)
		{
			suspendedProcessPid = std::stoul(argv[1]);
			threadId = std::stoul(argv[2]);
			timeOutMs = std::stoi(argv[3]);
		}
		else if (argc == 2 && *(argv[1]) == 'a')
		{
			STARTUPINFOW startupInfo = {};
			PROCESS_INFORMATION processInformation = {};

			CreateProcessW(APPLICATION_PATH, nullptr, nullptr,
			               nullptr, false, CREATE_SUSPENDED, nullptr, nullptr, &startupInfo, &processInformation);
			suspendedProcessPid = processInformation.dwProcessId;
			threadId = processInformation.dwThreadId;
			timeOutMs = 60000;

			hProcess = processInformation.hProcess;
		}
		else if (argc == 2 && *(argv[1]) == 'b')
		{
			STARTUPINFOW startupInfo = {};
			PROCESS_INFORMATION processInformation = {};

			CreateProcessW(APPLICATION_PATH, nullptr, nullptr,
			               nullptr, false, CREATE_SUSPENDED, nullptr, nullptr, &startupInfo, &processInformation);
			threadId = processInformation.dwThreadId;
			hProcess = processInformation.hProcess;

			resumeThread(threadId);
			WaitForSingleObject(hProcess, INFINITE);
			DWORD exitCode = 0UL;
			GetExitCodeProcess(hProcess, &exitCode);
			if (exitCode != 0UL)
			{
				MessageBoxW(nullptr, L"GOZ GOZ", L"GOZ", MB_OK);
			}
			return exitCode;
		}
		else
		{
			printUsage();
			return 11;
		}

		std::cout << "Process id: " << suspendedProcessPid << "\n"
			<< "Thread id: " << threadId << "\n"
			<< "Timeout: " << timeOutMs << std::endl;

		if (!DebugActiveProcess(suspendedProcessPid)) {
			std::cout << "DebugActiveProcess error: " << GetLastError() << std::endl;
			return 1;
		}

		resumeThread(threadId);
		auto result = waitForRuntime(suspendedProcessPid, threadId, timeOutMs);

		if (!DebugActiveProcessStop(suspendedProcessPid))
		{
			std::cout << "DebugActiveProcessStop error: " << GetLastError() << std::endl;
			return 3;
		}

		if (argc == 2 && *(argv[1]) == 'a' && result != -1)
		{
			TerminateProcess(hProcess, 0);
		}

		return result;
	}
	catch (const std::exception &ex)
	{
		std::cout << ex.what();
		return 6;
	}
}

long long getCurrentTimeMs()
{
	FILETIME ft = {};
	GetSystemTimeAsFileTime(&ft);
	return (long long(ft.dwLowDateTime) + (long long(ft.dwHighDateTime) << 32LL)) / 10000;
}

std::vector<std::byte> readProcessMemory(HANDLE processHandle, void *ptr, unsigned long size)
{
	std::vector<std::byte> result(size);
	SIZE_T read = 0;
	if (ReadProcessMemory(processHandle, ptr, result.data(), size, &read))
	{
		if (read != size)
			std::cout << "warn: ReadProcessMemory read only " << read << " bytes" << std::endl;
	}
	else
	{
		std::cout << "RMP ERROR" << std::endl;
	}

	return result;
}

std::wstring readDllName(HANDLE processHandle, LOAD_DLL_DEBUG_INFO debugInfo)
{
	if (debugInfo.lpImageName == nullptr) return std::wstring();
	auto pointer = readProcessMemory(processHandle, debugInfo.lpImageName, sizeof(wchar_t*));
	if (pointer.empty()) return std::wstring();
	auto ptr = *(void**)pointer.data();
	std::wstring result = {};
	wchar_t lastCharRead = L'\0';
	do
	{
		auto charData = readProcessMemory(processHandle, ptr, debugInfo.fUnicode ? 2 : 1);
		lastCharRead = debugInfo.fUnicode ? *(wchar_t*)charData.data() : *(char*)charData.data();
		ptr = (char*)ptr + (debugInfo.fUnicode ? 2 : 1);
		if (lastCharRead != L'\0') result.push_back(lastCharRead);
	} while (lastCharRead != L'\0');

	return result;
}

void suspendThreadHandle(HANDLE threadHandle)
{
	if (SuspendThread(threadHandle) == (DWORD)-1)
		throw std::runtime_error(std::string("SuspendThread error ") + std::to_string(GetLastError()));
}

void suspendThread(DWORD threadId)
{
	auto handle = OpenThread(THREAD_SUSPEND_RESUME, false, threadId);
	if (handle == nullptr)
		throw std::runtime_error(std::string("OpenThread error ") + std::to_string(GetLastError()));
	if (SuspendThread(handle) == (DWORD)-1)
		throw std::runtime_error(std::string("SuspendThread error ") + std::to_string(GetLastError()));
	if (CloseHandle(handle) == 0)
		throw std::runtime_error(std::string("CloseHandle error ") + std::to_string(GetLastError()));
}

void debugActiveProcessStop(DWORD processId)
{
	if (!DebugActiveProcessStop(processId))
		throw std::runtime_error(std::string("DebugActiveProcessStop error ") + std::to_string(GetLastError()));
}

int waitForRuntime(DWORD processId, DWORD threadId, int timeoutMs)
{
	std::vector<HANDLE> threadHandles {};

	auto startTime = getCurrentTimeMs();
	DEBUG_EVENT debugEvent = {};
	HANDLE processHandle = nullptr, threadHandle;
	while (WaitForDebugEvent(&debugEvent, timeoutMs))
	{
		// Sleep(1);
		std::cout << "DebugEvent: " << debugEvent.dwDebugEventCode << " (thread: " << debugEvent.dwThreadId << ")" << std::endl;
		switch (debugEvent.dwDebugEventCode)
		{
		case EXCEPTION_DEBUG_EVENT: {
			auto exceptionCode = debugEvent.u.Exception.ExceptionRecord.ExceptionCode;
			std::cout << "EXCEPTION: " << exceptionCode << std::endl;
			if (exceptionCode == EXCEPTION_ACCESS_VIOLATION)
			{
				MessageBoxW(nullptr, L"Press OK to suspend all process threads and exit", L"ZOG", MB_OK);
				for (auto handle : threadHandles)
					suspendThreadHandle(handle);

				return -1;
			}
			break;
		}
		case CREATE_PROCESS_DEBUG_EVENT:
			processHandle = debugEvent.u.CreateProcessInfo.hProcess;
			threadHandle = debugEvent.u.CreateProcessInfo.hThread;
			threadHandles.push_back(threadHandle);
			if (CloseHandle(debugEvent.u.CreateProcessInfo.hFile) == 0)
				throw std::runtime_error(std::string("CREATE_PROCESS_DEBUG_EVENT->CloseHandle error ") + std::to_string(GetLastError()));
			break;
		case CREATE_THREAD_DEBUG_EVENT:
			{
				auto threadHandle = debugEvent.u.CreateThread.hThread;
				std::cout << "[CREATE_THREAD_DEBUG_EVENT] Thread started: " << GetThreadId(threadHandle) << std::endl;
				threadHandles.push_back(threadHandle);
				break;
			}
		case LOAD_DLL_DEBUG_EVENT: {
			auto dllName = readDllName(processHandle, debugEvent.u.LoadDll);
			std::wcout << "LOAD_DLL_DEBUG_EVENT: " << dllName << std::endl;
			if (dllName.size() == wcslen(L"C:\\WINDOWS\\SysWOW64\\msvcrt.dll")
				&& _wcsnicmp(dllName.c_str(), L"C:\\WINDOWS\\SysWOW64\\msvcrt.dll", dllName.size()) == 0)
			{
				suspendThread(threadId);
				return 0;
			}
			
			if (CloseHandle(debugEvent.u.LoadDll.hFile) == 0)
				throw std::runtime_error(std::string("LOAD_DLL_DEBUG_EVENT->CloseHandle error ") + std::to_string(GetLastError()));
			break;
		}
		case EXIT_PROCESS_DEBUG_EVENT:
			if (!ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, DBG_EXCEPTION_NOT_HANDLED))
				throw std::runtime_error(std::string("EXIT_PROCESS_DEBUG_EVENT->ContinueDebugEvent error ") + std::to_string(GetLastError()));
			return 4;
		}

		// timeoutMs = startTime + timeoutMs - getCurrentTimeMs();
		// TODO if (timeoutMs < 0) return 5;

		if (!ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, DBG_EXCEPTION_NOT_HANDLED))
			throw std::runtime_error(std::string("ContinueDebugEvent error ") + std::to_string(GetLastError()));
	}

	return 6;
}