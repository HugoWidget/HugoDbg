#ifndef DEBUGGER_H
#define DEBUGGER_H
#include "WinUtils/WinPch.h"

#include "CDPClient.h"

typedef enum {
    DEBUGGER_SUCCESS = 0,
    DEBUGGER_ERROR_OPEN_PROCESS = -1,
    DEBUGGER_ERROR_MAPPING_NAME = -2,
    DEBUGGER_ERROR_OPEN_MAPPING = -3,
    DEBUGGER_ERROR_MAP_VIEW = -4,
    DEBUGGER_ERROR_CREATE_THREAD = -5,
    DEBUGGER_ERROR_THREAD_TIMEOUT = -6,
    DEBUGGER_ERROR_INVALID_CLIENT = -7,
    DEBUGGER_ERROR_SEND_COMMAND = -8
} dbg_result_t;

class Debugger {
public:
    static dbg_result_t connect(DWORD pid);
    static dbg_result_t disconnect(CDPClient* cdp_client);
    static const char* getErrorString(dbg_result_t result);
};

#endif // DEBUGGER_H