// LastFriend - the friend of the last Steam co-op session, remembered so the next
// one is one click on each side: F2 offers "Volver a jugar con <nombre>", which
// connects in the same role as last time (the host goes online waiting for that
// friend, the friend connects to that host) with no invite and no Steam IDs to
// copy. Kept in %LOCALAPPDATA%\kenshi\TokelaCoop_amigo.txt, outside the mod
// folder so a reinstall keeps it. Header-only (prototest checks the text format).
// VS2010 (v100) compatible.

#ifndef TOKELACOOP_LASTFRIEND_H
#define TOKELACOOP_LASTFRIEND_H

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace coop {

struct LastFriend {
    unsigned long long id;       // the other player's steamid64 (0 = none)
    bool               wasHost;  // our role in that session
    std::string        name;     // their Steam name when we last saw it
    LastFriend() : id(0), wasHost(false) {}
};

// A Steam name made safe to keep and show: no control characters (no line
// breaks to break the file), at most 48 bytes, never cut inside a UTF-8 letter.
inline std::string cleanFriendName(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); ++i) {
        const unsigned char c = (unsigned char)in[i];
        if (c < 0x20 || c == 0x7F) continue;
        out += (char)c;
    }
    if (out.size() > 48) {
        size_t cut = 48;
        while (cut > 0 && ((unsigned char)out[cut] & 0xC0) == 0x80) --cut;   // a continuation byte
        out.resize(cut);
    }
    return out;
}

// "id=<steamid64>\nrole=host|join\nname=<name>\n"
inline std::string formatLastFriend(const LastFriend& f) {
    char b[64];
    _snprintf(b, sizeof(b) - 1, "id=%llu\n", f.id);
    b[sizeof(b) - 1] = '\0';
    return std::string(b) + "role=" + (f.wasHost ? "host" : "join") + "\n" +
           "name=" + cleanFriendName(f.name) + "\n";
}

// Parse what formatLastFriend wrote. False (and *out untouched) when there is no
// usable Steam ID: a steamid64 of an individual account is 7656119xxxxxxxxxx.
inline bool parseLastFriend(const std::string& text, LastFriend* out) {
    LastFriend f;
    bool haveRole = false;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) eol = text.size();
        std::string line = text.substr(pos, eol - pos);
        if (!line.empty() && line[line.size() - 1] == '\r') line.resize(line.size() - 1);
        pos = eol + 1;
        if (line.compare(0, 3, "id=") == 0) f.id = _strtoui64(line.c_str() + 3, 0, 10);
        else if (line == "role=host") { f.wasHost = true; haveRole = true; }
        else if (line == "role=join") { f.wasHost = false; haveRole = true; }
        else if (line.compare(0, 5, "name=") == 0) f.name = cleanFriendName(line.substr(5));
    }
    if (f.id < 76561197960265729ULL || f.id > 76561202255233023ULL || !haveRole) return false;
    if (out) *out = f;
    return true;
}

inline std::string lastFriendPath() {
    const char* lad = getenv("LOCALAPPDATA");
    if (!lad || !lad[0]) return std::string();
    return std::string(lad) + "\\kenshi\\TokelaCoop_amigo.txt";
}

inline bool loadLastFriend(LastFriend* out) {
    const std::string p = lastFriendPath();
    if (p.empty()) return false;
    FILE* fp = fopen(p.c_str(), "rb");
    if (!fp) return false;
    char buf[512];
    const size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    return parseLastFriend(std::string(buf, n), out);
}

inline bool saveLastFriend(const LastFriend& f) {
    const std::string p = lastFriendPath();
    if (p.empty() || f.id == 0) return false;
    const std::string dir = p.substr(0, p.find_last_of('\\'));
    CreateDirectoryA(dir.c_str(), 0);
    FILE* fp = fopen(p.c_str(), "wb");
    if (!fp) return false;
    const std::string t = formatLastFriend(f);
    const bool ok = fwrite(t.data(), 1, t.size(), fp) == t.size();
    fclose(fp);
    return ok;
}

} // namespace coop

#endif // TOKELACOOP_LASTFRIEND_H
