// UpdateCheck.cpp - the HTTP half of core/UpdateCheck.h: one HTTPS GET of the
// latest release on a background thread (WinHTTP, short timeouts), the result
// handed to the main thread through a flag. Never touches the engine.

#include "../core/UpdateCheck.h"
#include "../CoopLog.h"
#include "../../netproto/Version.h"

#include <windows.h>
#include <winhttp.h>
#include <cstdio>
#include <string>

namespace coop {
namespace update {
namespace {

volatile LONG g_state = 0;   // 0 not started, 1 running, 2 done
char          g_newer[24] = "";

// GET https://api.github.com/repos/dentroytu/TokelaCoop/releases/latest.
// Plain data in, the body out; false on any failure (logged by the caller).
bool fetchLatest(std::string* body, DWORD* status) {
    *status = 0;
    bool ok = false;
    HINTERNET ses = WinHttpOpen(L"TokelaCoop-update-check", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return false;
    WinHttpSetTimeouts(ses, 5000, 5000, 5000, 8000);
    HINTERNET con = WinHttpConnect(ses, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", L"/repos/dentroytu/TokelaCoop/releases/latest",
                                             0, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             WINHTTP_FLAG_SECURE)
                        : 0;
    if (req &&
        WinHttpSendRequest(req, L"Accept: application/vnd.github+json\r\n", (DWORD)-1L,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, 0)) {
        DWORD sz = sizeof(*status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, status, &sz, WINHTTP_NO_HEADER_INDEX);
        if (*status == 200) {
            ok = true;
            char buf[4096];
            for (;;) {
                DWORD got = 0;
                if (!WinHttpReadData(req, buf, sizeof(buf), &got)) { ok = false; break; }
                if (got == 0) break;
                body->append(buf, got);
                if (body->size() > 512 * 1024) break;   // a release page is a few KB
            }
        }
    }
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
}

DWORD WINAPI worker(LPVOID) {
    std::string body;
    DWORD status = 0;
    const bool ok = fetchLatest(&body, &status);
    const std::string tag = ok ? tagFromReleaseJson(body) : std::string();
    char b[160];
    if (!ok || tag.empty()) {
        _snprintf(b, sizeof(b) - 1, "[update] could not read the latest version (status %lu)",
                  (unsigned long)status);
    } else if (isNewer(tag, TOKELACOOP_VERSION)) {
        strncpy(g_newer, tag.c_str(), sizeof(g_newer) - 1);
        g_newer[sizeof(g_newer) - 1] = '\0';
        _snprintf(b, sizeof(b) - 1, "[update] newer version available: %s (this is v%s)",
                  g_newer, TOKELACOOP_VERSION);
    } else {
        _snprintf(b, sizeof(b) - 1, "[update] up to date (latest %s, this is v%s)",
                  tag.c_str(), TOKELACOOP_VERSION);
    }
    b[sizeof(b) - 1] = '\0';
    coop::logLine(b);
    InterlockedExchange(&g_state, 2);
    return 0;
}

} // namespace

void start() {
    if (InterlockedCompareExchange(&g_state, 1, 0) != 0) return;
    HANDLE h = CreateThread(0, 0, &worker, 0, 0, 0);
    if (h) CloseHandle(h);
    else InterlockedExchange(&g_state, 2);
}

const char* newerVersion() {
    return (g_state == 2 && g_newer[0]) ? g_newer : "";
}

} // namespace update
} // namespace coop
