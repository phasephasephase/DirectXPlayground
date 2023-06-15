#include "common.h"
#include "hook/hook.h"

void entry() {
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    SetConsoleTitleA("DirectX Playground");

    FILE* in;
    freopen_s(&in, "CONIN$", "r", stdin);
    FILE* out;
    freopen_s(&out, "CONOUT$", "w", stdout);
    FILE* err;
    freopen_s(&err, "CONOUT$", "w", stderr);
    
    printf("Injected.\n");
    install_hook();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        std::thread entryThread(entry);
        entryThread.detach();
    }

    return TRUE;
}