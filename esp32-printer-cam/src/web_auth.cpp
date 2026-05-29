#include "web_auth.h"
#include "app_config.h"
#include <cstring>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef WEB_AUTH_USER
#define WEB_AUTH_USER "admin"
#endif
#ifndef WEB_AUTH_PASSWORD
#define WEB_AUTH_PASSWORD ""
#endif

bool webAuthOk(WebServer &server) {
#if !WEB_AUTH_ENABLE
    (void)server;
    return true;
#endif
    if (strlen(WEB_AUTH_PASSWORD) == 0) return true;
    if (server.authenticate(WEB_AUTH_USER, WEB_AUTH_PASSWORD)) {
        return true;
    }
    server.requestAuthentication();
    return false;
}
