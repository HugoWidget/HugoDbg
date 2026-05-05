#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stddef.h>

class HttpClient {
public:
    static bool init();      // 必须在使用前调用
    static void cleanup();   // 程序结束前调用
    static bool checkPort(const char* host, int port);
    static int get(const char* host, int port, const char* path, char* response, size_t response_size);
};

#endif // HTTP_CLIENT_H