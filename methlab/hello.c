#include<stdio.h>
#include<windows.h>

// https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa
// https://learn.microsoft.com/en-gb/archive/blogs/twistylittlepassagesallalike/everyone-quotes-command-line-arguments-the-wrong-way
int main() {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));


    // BOOL CreateProcessA(
    // [in, optional]      LPCSTR                lpApplicationName,
    // [in, out, optional] LPSTR                 lpCommandLine,
    // [in, optional]      LPSECURITY_ATTRIBUTES lpProcessAttributes,
    // [in, optional]      LPSECURITY_ATTRIBUTES lpThreadAttributes,
    // [in]                BOOL                  bInheritHandles,
    // [in]                DWORD                 dwCreationFlags,
    // [in, optional]      LPVOID                lpEnvironment,
    // [in, optional]      LPCSTR                lpCurrentDirectory,
    // [in]                LPSTARTUPINFOA        lpStartupInfo,
    // [out]               LPPROCESS_INFORMATION lpProcessInformation
    // );

    BOOL cp = CreateProcessA
	(NULL, "notepad\0", NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if(!cp) {
        printf("failed!\n");
        printf("%d\n", GetLastError());
        return 1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
