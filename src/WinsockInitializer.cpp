#include "WinsockInitializer.h"

WinsockInitializer::WinsockInitializer() : m_valid(false) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        m_valid = true;
    }
}

WinsockInitializer::~WinsockInitializer() {
    if (m_valid) {
        WSACleanup();
    }
}