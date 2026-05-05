#ifndef WINSOCK_INITIALIZER_H
#define WINSOCK_INITIALIZER_H

#include <winsock2.h>
#include <windows.h>

class WinsockInitializer {
public:
    WinsockInitializer();
    ~WinsockInitializer();
    bool isValid() const { return m_valid; }
private:
    bool m_valid;
};

#endif // WINSOCK_INITIALIZER_H