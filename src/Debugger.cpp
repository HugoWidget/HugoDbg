#include "Debugger.h"
#include <stdio.h>
#include <wchar.h>
#include <winternl.h>
#include "CDPClient.h"

#ifndef OBJECT_ATTRIBUTES_DEFINED
#define OBJECT_ATTRIBUTES_DEFINED
typedef struct _UNICODE_STRING_CUSTOM {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING_CUSTOM;

typedef struct _OBJECT_ATTRIBUTES_CUSTOM {
    ULONG           Length;
    HANDLE          RootDirectory;
    UNICODE_STRING_CUSTOM* ObjectName;
    ULONG           Attributes;
    PVOID           SecurityDescriptor;
    PVOID           SecurityQualityOfService;
} OBJECT_ATTRIBUTES_CUSTOM;
#endif

typedef NTSTATUS(NTAPI* PNT_OPEN_SECTION)(
    PHANDLE SectionHandle,
    ACCESS_MASK DesiredAccess,
    OBJECT_ATTRIBUTES_CUSTOM* ObjectAttributes
    );

static BOOL EnableDebugPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tp;
    LUID luid;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) return FALSE;
    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
        CloseHandle(hToken);
        return FALSE;
    }
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
        CloseHandle(hToken);
        return FALSE;
    }
    CloseHandle(hToken);
    return TRUE;
}

static HANDLE OpenMappingFromSession(DWORD pid, DWORD sessionId) {
    HMODULE hNtDll = GetModuleHandleA("ntdll.dll");
    if (!hNtDll) return NULL;

    PNT_OPEN_SECTION NtOpenSection = (PNT_OPEN_SECTION)(void*)GetProcAddress(hNtDll, "NtOpenSection");
    if (!NtOpenSection) return NULL;

    wchar_t pathBuffer[MAX_PATH];
    swprintf(pathBuffer, MAX_PATH, L"\\Sessions\\%lu\\BaseNamedObjects\\node-debug-handler-%lu", sessionId, pid);

    UNICODE_STRING_CUSTOM usName;
    usName.Buffer = pathBuffer;
    usName.Length = (USHORT)(wcslen(pathBuffer) * sizeof(wchar_t));
    usName.MaximumLength = usName.Length + sizeof(wchar_t);

    OBJECT_ATTRIBUTES_CUSTOM objAttr = { 0 };
    objAttr.Length = sizeof(objAttr);
    objAttr.ObjectName = &usName;
    objAttr.Attributes = 0x00000040;
    objAttr.RootDirectory = NULL;

    HANDLE hSection = NULL;
    NTSTATUS status = NtOpenSection(&hSection, SECTION_MAP_READ, &objAttr);
    if (status != 0) {
        printf("NtOpenSection failed. Status: 0x%08lX\n", status);
        return NULL;
    }
    return hSection;
}

dbg_result_t Debugger::connect(DWORD pid) {
    HANDLE process = NULL;
    HANDLE thread = NULL;
    HANDLE mapping = NULL;
    LPTHREAD_START_ROUTINE* handler = NULL;
    dbg_result_t result = DEBUGGER_ERROR_OPEN_PROCESS;
    DWORD sessionId = 0;

    EnableDebugPrivilege();

    if (!ProcessIdToSessionId(pid, &sessionId)) {
        printf("ProcessIdToSessionId failed. Err: %lu\n", GetLastError());
        sessionId = 1;
    }

    process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, pid);

    if (process == NULL) {
        result = DEBUGGER_ERROR_OPEN_PROCESS;
        goto cleanup;
    }

    mapping = OpenMappingFromSession(pid, sessionId);
    if (mapping == NULL) {
        wchar_t localName[64];
        swprintf(localName, 64, L"node-debug-handler-%u", pid);
        mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, localName);
        if (mapping == NULL) {
            result = DEBUGGER_ERROR_OPEN_MAPPING;
            goto cleanup;
        }
    }

    handler = (LPTHREAD_START_ROUTINE*)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(*handler));
    if (handler == NULL || *handler == NULL) {
        result = DEBUGGER_ERROR_MAP_VIEW;
        goto cleanup;
    }

    thread = CreateRemoteThread(process, NULL, 0, *handler, NULL, 0, NULL);
    if (thread == NULL) {
        result = DEBUGGER_ERROR_CREATE_THREAD;
        goto cleanup;
    }

    if (WaitForSingleObject(thread, 10000) != WAIT_OBJECT_0) {
        result = DEBUGGER_ERROR_THREAD_TIMEOUT;
        goto cleanup;
    }

    result = DEBUGGER_SUCCESS;

cleanup:
    if (process != NULL) CloseHandle(process);
    if (thread != NULL) CloseHandle(thread);
    if (handler != NULL) UnmapViewOfFile(handler);
    if (mapping != NULL) CloseHandle(mapping);
    return result;
}

dbg_result_t Debugger::disconnect(CDPClient* cdp_client) {
    if (!cdp_client) return DEBUGGER_ERROR_INVALID_CLIENT;

    CDPClient* client = cdp_client;
    WebSocket* ws = client->getWebSocket();
    if (!ws || !ws->isConnected()) return DEBUGGER_ERROR_INVALID_CLIENT;

    char command[256];
    int id = client->getNextIdAndIncrement();
    snprintf(command, sizeof(command), "{\"id\":%d,\"method\":\"Runtime.evaluate\",\"params\":{\"expression\":\"process._debugEnd()\",\"contextId\":1}}", id);

    if (ws->sendText(command) == 0) {
        ws->setConnected(false);
        return DEBUGGER_SUCCESS;
    }
    else {
        return DEBUGGER_ERROR_SEND_COMMAND;
    }
}

const char* Debugger::getErrorString(dbg_result_t result) {
    switch (result) {
    case DEBUGGER_SUCCESS: return "Success";
    case DEBUGGER_ERROR_OPEN_PROCESS: return "Failed to open process";
    case DEBUGGER_ERROR_MAPPING_NAME: return "Failed to create mapping name";
    case DEBUGGER_ERROR_OPEN_MAPPING: return "Failed to open mapping (NtOpenSection failed)";
    case DEBUGGER_ERROR_MAP_VIEW: return "Failed to map view";
    case DEBUGGER_ERROR_CREATE_THREAD: return "Failed to create remote thread";
    case DEBUGGER_ERROR_THREAD_TIMEOUT: return "Remote thread start timeout";
    case DEBUGGER_ERROR_INVALID_CLIENT: return "Invalid connection";
    case DEBUGGER_ERROR_SEND_COMMAND: return "Failed to send exit command";
    default: return "Unknown error";
    }
}