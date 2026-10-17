// UpdateCheck - "is there a newer TokelaCoop?" Once per run, on a background
// thread, the plugin asks GitHub for the latest release's tag
// (api.github.com/repos/dentroytu/TokelaCoop/releases/latest - only that, no
// data of ours is sent beyond the request itself) and, when it is newer than
// this build, F2 and the banner say so with a button to the download page. No
// answer (offline, GitHub down, rate limit) changes nothing. Off with
// "checkUpdates": false in coop_config.json (or TOKELACOOP_UPDATE_CHECK=0), and
// never in a harness run. The parsing helpers are header-only so prototest
// checks them; the HTTP half is net/UpdateCheck.cpp. VS2010 (v100) compatible.

#ifndef TOKELACOOP_UPDATECHECK_H
#define TOKELACOOP_UPDATECHECK_H

#include <cstdlib>
#include <cstring>
#include <string>

namespace coop {
namespace update {

// "v0.58" / "0.58" / "1.02" -> major * 1000 + minor (58, 1002); -1 when it is
// not a plain "X.YY" version. Versions here always carry two minor digits.
inline int versionKey(const char* s) {
    if (!s) return -1;
    if (*s == 'v' || *s == 'V') ++s;
    if (*s < '0' || *s > '9') return -1;
    int major = 0, minor = 0, digits = 0;
    while (*s >= '0' && *s <= '9') { major = major * 10 + (*s - '0'); ++s; if (major > 999) return -1; }
    if (*s != '.') return -1;
    ++s;
    while (*s >= '0' && *s <= '9') { minor = minor * 10 + (*s - '0'); ++s; if (++digits > 3) return -1; }
    if (digits == 0 || *s != '\0') return -1;
    return major * 1000 + minor;
}

// The "tag_name" of a GitHub release JSON body ("" when absent or odd).
inline std::string tagFromReleaseJson(const std::string& body) {
    const std::string key = "\"tag_name\"";
    std::string::size_type k = body.find(key);
    if (k == std::string::npos) return std::string();
    std::string::size_type q = body.find('"', body.find(':', k + key.size()));
    if (q == std::string::npos) return std::string();
    std::string::size_type e = body.find('"', q + 1);
    if (e == std::string::npos || e - q - 1 > 16) return std::string();
    const std::string tag = body.substr(q + 1, e - q - 1);
    for (size_t i = 0; i < tag.size(); ++i) {
        const char c = tag[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              c == '.' || c == '-' || c == '_'))
            return std::string();
    }
    return tag;
}

// True when 'remoteTag' names a version newer than 'localVersion'.
inline bool isNewer(const std::string& remoteTag, const char* localVersion) {
    const int r = versionKey(remoteTag.c_str()), l = versionKey(localVersion);
    return r >= 0 && l >= 0 && r > l;
}

// Where a player downloads the latest kit.
inline const char* downloadPage() { return "https://github.com/dentroytu/TokelaCoop/releases/latest"; }

// Start the background check (once per run; later calls do nothing).
void start();
// Main thread: the newer version's tag ("v0.59"), or "" while unknown / none.
const char* newerVersion();

} // namespace update
} // namespace coop

#endif // TOKELACOOP_UPDATECHECK_H
