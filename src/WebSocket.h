#ifndef WEB_SOCKET_H
#define WEB_SOCKET_H
#include "WinUtils/WinPch.h"

class WebSocket {
public:
    WebSocket();
    ~WebSocket();

    bool connect(const char* host, int port, const char* path);
    void close();
    bool isConnected() const { return m_connected; }
    void setConnected(bool connected) { m_connected = connected; }

    int sendText(const char* message);
    int receive(char* buffer, size_t buffer_size);
    int receiveResponse(int expected_id, char* buffer, size_t buffer_size, int timeout_ms);

private:
    UINT_PTR m_sockfd;
    bool m_connected;
};

#endif // WEB_SOCKET_H