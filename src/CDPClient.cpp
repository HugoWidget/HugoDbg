#include "CDPClient.h"
#include "HttpClient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 8192

void CDPClient::extractWsInfo(const char* ws_url, char* host, int* port, char* path) {
    const char* url = ws_url;
    if (strncmp(url, "ws://", 5) == 0) url += 5;

    const char* colon = strchr(url, ':');
    const char* slash = strchr(url, '/');

    if (colon && (!slash || colon < slash)) {
        size_t host_len = colon - url;
        strncpy_s(host, strlen(url) + 1, url, host_len);
        host[host_len] = '\0';
        *port = atoi(colon + 1);
        const char* path_start = strchr(colon, '/');
        if (path_start) {
            strcpy_s(path, strlen(path_start) + 1, path_start);
        }
        else {
            strcpy_s(path, 2, "/");
        }
    }
    else if (slash) {
        size_t host_len = slash - url;
        strncpy_s(host, strlen(url) + 1, url, host_len);
        host[host_len] = '\0';
        *port = 80;
        strcpy_s(path, strlen(slash) + 1, slash);
    }
    else {
        strcpy_s(host, strlen(url) + 1, url);
        *port = 80;
        strcpy_s(path, 2, "/");
    }
}

int CDPClient::parseWebSocketUrl(const char* json_response, char* ws_url, size_t url_size) {
    const char* body_start = strstr(json_response, "\r\n\r\n");
    if (!body_start) return -1;
    body_start += 4;

    const char* url_start = strstr(body_start, "webSocketDebuggerUrl");
    if (!url_start) return -1;

    url_start = strchr(url_start, ':');
    if (!url_start) return -1;
    url_start++;
    while (*url_start == ' ' || *url_start == '"') url_start++;

    const char* url_end = strchr(url_start, '"');
    if (!url_end) return -1;

    size_t url_len = url_end - url_start;
    if (url_len >= url_size) url_len = url_size - 1;
    strncpy_s(ws_url, url_size, url_start, url_len);
    ws_url[url_len] = '\0';
    return 0;
}

bool CDPClient::getTargetWebSocketUrl(const char* host, int port, const char* target_type, char* ws_url, size_t url_size) {
    const char* path = "/json";
    char response[BUFFER_SIZE];
    if (!HttpClient::checkPort(host, port)) return false;

    int result = HttpClient::get(host, port, path, response, sizeof(response));
    if (result <= 0) return false;

    const char* body_start = strstr(response, "\r\n\r\n");
    if (!body_start) return false;
    body_start += 4;

    const char* current = body_start;
    while (current && *current) {
        const char* type_start = strstr(current, "\"type\":");
        if (!type_start) break;

        const char* type_value = strchr(type_start, ':');
        if (type_value) {
            type_value++;
            while (*type_value == ' ' || *type_value == '"') type_value++;
            if (strncmp(type_value, target_type, strlen(target_type)) == 0) {
                const char* ws_start = strstr(type_start, "webSocketDebuggerUrl");
                if (ws_start) {
                    const char* url_start = strchr(ws_start, ':');
                    if (url_start) {
                        url_start++;
                        while (*url_start == ' ' || *url_start == '"') url_start++;
                        const char* url_end = strchr(url_start, '"');
                        if (url_end) {
                            size_t url_len = url_end - url_start;
                            if (url_len < url_size) {
                                strncpy_s(ws_url, url_size, url_start, url_len);
                                ws_url[url_len] = '\0';
                                return true;
                            }
                        }
                    }
                }
            }
        }
        current = strchr(current + 1, '{');
    }

    return parseWebSocketUrl(response, ws_url, url_size) == 0;
}

int CDPClient::extractResultValue(const char* json_response, char* value, size_t value_size) {
    const char* result_start = strstr(json_response, "\"result\":");
    if (!result_start) return -1;

    const char* value_start = strstr(result_start, "\"value\":");
    if (value_start) {
        value_start = strchr(value_start, ':');
        if (value_start) {
            value_start++;
            while (*value_start == ' ') value_start++;
            if (*value_start == '"') {
                value_start++;
                const char* value_end = strchr(value_start, '"');
                if (value_end) {
                    size_t len = value_end - value_start;
                    if (len < value_size) {
                        strncpy_s(value, value_size, value_start, len);
                        value[len] = '\0';
                        return 0;
                    }
                }
            }
            else {
                const char* value_end = strpbrk(value_start, ",}");
                if (value_end) {
                    size_t len = value_end - value_start;
                    if (len < value_size) {
                        strncpy_s(value, value_size, value_start, len);
                        value[len] = '\0';
                        return 0;
                    }
                }
            }
        }
    }

    const char* desc_start = strstr(result_start, "\"description\":");
    if (desc_start) {
        desc_start = strchr(desc_start, ':');
        if (desc_start) {
            desc_start++;
            while (*desc_start == ' ' || *desc_start == '"') desc_start++;
            const char* desc_end = strchr(desc_start, '"');
            if (desc_end) {
                size_t len = desc_end - desc_start;
                if (len < value_size) {
                    strncpy_s(value, value_size, desc_start, len);
                    value[len] = '\0';
                    return 0;
                }
            }
        }
    }
    return -1;
}

CDPClient::CDPClient() : m_ws(nullptr), m_next_id(1) {}

CDPClient::~CDPClient() {
    if (m_ws) {
        m_ws->close();
        delete m_ws;
    }
}

int CDPClient::connect(const char* host, int port) {
    return connectTarget(host, port, "page");
}

int CDPClient::connectTarget(const char* host, int port, const char* target_type) {
    char ws_url[512];
    char ws_host[256];
    int ws_port;
    char ws_path[256];

    if (!getTargetWebSocketUrl(host, port, target_type, ws_url, sizeof(ws_url))) {
        return -1;
    }

    extractWsInfo(ws_url, ws_host, &ws_port, ws_path);
    m_ws = new WebSocket();
    if (!m_ws->connect(ws_host, ws_port, ws_path)) {
        delete m_ws;
        m_ws = nullptr;
        return -1;
    }
    return 0;
}

int CDPClient::enableRuntime() {
    if (!m_ws || !m_ws->isConnected()) return -1;

    char command[256];
    char response[BUFFER_SIZE];
    int id = m_next_id++;

    snprintf(command, sizeof(command), "{\"id\":%d,\"method\":\"Runtime.enable\"}", id);
    if (m_ws->sendText(command) != 0) return -1;
    if (m_ws->receiveResponse(id, response, sizeof(response), 5000) <= 0) return -1;

    return 0;
}

int CDPClient::evaluate(const char* expression, char* result, size_t result_size) {
    if (!m_ws || !m_ws->isConnected() || !expression) return -1;

    char command[4096] = {};
    char response[BUFFER_SIZE] = {};
    int id = m_next_id++;

    snprintf(command, sizeof(command),
        "{\"id\":%d,\"method\":\"Runtime.evaluate\",\"params\":{\"expression\":\"%s\",\"returnByValue\":true,\"includeCommandLineAPI\":true,\"contextId\":1}}",
        id, expression);

    if (m_ws->sendText(command) != 0) return -1;
    if (m_ws->receiveResponse(id, response, sizeof(response), 5000) <= 0) return -1;

    if (result && result_size > 0) {
        if (extractResultValue(response, result, result_size) != 0) {
            strncpy_s(result, result_size, response, result_size - 1);
            result[result_size - 1] = '\0';
        }
    }
    return 0;
}