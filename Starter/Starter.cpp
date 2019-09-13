#include <Windows.h>

int main()
{
	STARTUPINFOW startupInfo = {};
	PROCESS_INFORMATION processInformation = {};
	
	CreateProcessW(L"C:\\work\\NetRuntimeWaiter\\ConsoleApp1\\bin\\Debug\\ConsoleApp1.exe", nullptr, nullptr, nullptr, false, CREATE_SUSPENDED, nullptr, nullptr, &startupInfo, &processInformation);
}
