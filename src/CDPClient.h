#ifndef CDP_CLIENT_H
#define CDP_CLIENT_H
#include "WinUtils/WinPch.h"

#include "WebSocket.h"

class CDPClient {
public:
    CDPClient();
    ~CDPClient();

    int connect(const char* host, int port);
    int connectTarget(const char* host, int port, const char* target_type);
    int enableRuntime();
    int evaluate(const char* expression, char* result, size_t result_size);

    WebSocket* getWebSocket() const { return m_ws; }
    int getNextIdAndIncrement() { return m_next_id++; }

private:
    WebSocket* m_ws;
    int m_next_id;

    // ¾²Ì¬¸¨Öú·½·¨
    static bool getTargetWebSocketUrl(const char* host, int port, const char* target_type, char* ws_url, size_t url_size);
    static void extractWsInfo(const char* ws_url, char* host, int* port, char* path);
    static int parseWebSocketUrl(const char* json_response, char* ws_url, size_t url_size);
    static int extractResultValue(const char* json_response, char* value, size_t value_size);
};

#endif // CDP_CLIENT_H