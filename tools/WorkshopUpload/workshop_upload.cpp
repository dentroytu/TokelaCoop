// workshop_upload.cpp - publish or update TokelaCoop's Steam Workshop item from
// the player's own Steam client. No password: the running, logged-in Steam
// client authenticates the upload (the same way Kenshi's own mod uploader does).
//
// It drives Kenshi's own steam_api64.dll through its flat C API, as app 233860.
// The flat functions call fixed vtable slots (disassembled 2026-10-20:
// CreateItem = 30, SubmitItemUpdate = 47, GetNumSubscribedItems = 55,
// GetSubscribedItems = 56), so the interface must be the one that DLL was built
// with: STEAMUGC_INTERFACE_VERSION008, the version Kenshi itself asks for (same
// SDK; the DLL also carries Kenshi's SteamUtils008). 009 has one more call
// before those slots: with it, slot 55 read garbage and slot 56 returned the
// subscription COUNT. `check` proves the layout before anything is written:
// the subscribed items read through slots 55/56 must equal the folders in
// workshop\content\233860.
//
// Build: scripts\build_workshop_upload.cmd (any modern MSVC, x64; standalone,
// not linked against KenshiLib). Driven by scripts\publish_workshop.ps1.
//
//   workshop_upload check  --kenshi <dir> [--workshop-dir <dir>]
//   workshop_upload create --kenshi <dir>
//   workshop_upload update --kenshi <dir> --item <id> --content <dir> --preview <file>
//                          --title-file <f> --desc-file <f> --note-file <f>
//                          --visibility <0 public|1 friends|2 private|3 unlisted> [--tag <t>]
//
// Exit code 0 = done. Text files are UTF-8 (a BOM is dropped).

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>

namespace {

typedef uint64_t SteamAPICall_t;
typedef uint64_t PublishedFileId_t;
typedef uint64_t UGCUpdateHandle_t;
typedef int32_t  HSteamUser;
typedef int32_t  HSteamPipe;

const uint32_t KENSHI_APP_ID = 233860;
const int CB_CREATE_ITEM = 3403;   // k_iClientUGCCallbacks + 3: CreateItemResult_t
const int CB_SUBMIT_ITEM = 3404;   // k_iClientUGCCallbacks + 4: SubmitItemUpdateResult_t

#pragma pack(push, 8)
struct CreateItemResult {          // CreateItemResult_t
    int32_t           result;      // EResult, 1 = OK
    PublishedFileId_t id;
    bool              needsLegalAgreement;
};
struct SubmitItemResultOld {       // SubmitItemUpdateResult_t up to SDK 1.4x
    int32_t result;
    bool    needsLegalAgreement;
};
struct SubmitItemResultNew {       // ... and with the item id appended (later SDKs)
    int32_t           result;
    bool              needsLegalAgreement;
    PublishedFileId_t id;
};
#pragma pack(pop)

struct SteamParamStringArray { const char** strings; int32_t count; };

typedef bool        (*InitFn)();
typedef void        (*VoidFn)();
typedef HSteamUser  (*GetUserFn)();
typedef HSteamPipe  (*GetPipeFn)();
typedef void*       (*ClientFn)();
typedef void*       (*GetUserIfaceFn)(void* client, HSteamUser, HSteamPipe, const char*);
typedef void*       (*GetPipeIfaceFn)(void* client, HSteamPipe, const char*);
typedef bool        (*CallDoneFn)(void* utils, SteamAPICall_t, bool* failed);
typedef bool        (*CallResultFn)(void* utils, SteamAPICall_t, void* out, int cub, int expected, bool* failed);
typedef uint32_t    (*AppIdFn)(void* utils);
typedef const char* (*PersonaFn)(void* friends);
typedef SteamAPICall_t    (*CreateItemFn)(void* ugc, uint32_t app, int fileType);
typedef UGCUpdateHandle_t (*StartUpdateFn)(void* ugc, uint32_t app, PublishedFileId_t);
typedef bool        (*SetStrFn)(void* ugc, UGCUpdateHandle_t, const char*);
typedef bool        (*SetIntFn)(void* ugc, UGCUpdateHandle_t, int);
typedef bool        (*SetTagsFn)(void* ugc, UGCUpdateHandle_t, const SteamParamStringArray*);
typedef SteamAPICall_t (*SubmitFn)(void* ugc, UGCUpdateHandle_t, const char* note);
typedef int         (*ProgressFn)(void* ugc, UGCUpdateHandle_t, uint64_t* done, uint64_t* total);
typedef uint32_t    (*NumSubsFn)(void* ugc);
typedef uint32_t    (*SubsFn)(void* ugc, PublishedFileId_t* out, uint32_t max);

struct Api {
    HMODULE dll;
    InitFn init; VoidFn shutdown; VoidFn runCallbacks;
    void* ugc; void* utils; void* friends;
    CallDoneFn callDone; CallResultFn callResult; AppIdFn appId; PersonaFn persona;
    CreateItemFn createItem; StartUpdateFn startUpdate;
    SetStrFn setTitle, setDesc, setContent, setPreview; SetIntFn setVisibility; SetTagsFn setTags;
    SubmitFn submit; ProgressFn progress; NumSubsFn numSubs; SubsFn subs;
};

std::string arg(int argc, char** argv, const char* name, const char* def = "") {
    for (int i = 2; i + 1 < argc; ++i)
        if (strcmp(argv[i], name) == 0) return argv[i + 1];
    return def;
}

bool readText(const std::string& path, std::string* out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    std::string s;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    fclose(f);
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
        s.erase(0, 3);
    while (!s.empty() && (s[s.size() - 1] == '\n' || s[s.size() - 1] == '\r')) s.erase(s.size() - 1);
    *out = s;
    return true;
}

template <typename T> bool load(HMODULE m, const char* name, T* fn) {
    *fn = (T)GetProcAddress(m, name);
    if (!*fn) fprintf(stderr, "steam_api64.dll: falta %s\n", name);
    return *fn != 0;
}

bool openSteam(const std::string& kenshi, Api* a) {
    memset(a, 0, sizeof(*a));
    // Act as Kenshi (app 233860) without a steam_appid.txt next to this exe.
    SetEnvironmentVariableA("SteamAppId", "233860");
    SetEnvironmentVariableA("SteamGameId", "233860");
    const std::string dll = kenshi + "\\steam_api64.dll";
    a->dll = LoadLibraryExA(dll.c_str(), 0, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!a->dll) { fprintf(stderr, "No se pudo cargar %s\n", dll.c_str()); return false; }
    HMODULE m = a->dll;
    GetUserFn getUser; GetPipeFn getPipe; ClientFn client;
    GetUserIfaceFn getUgc, getFriends; GetPipeIfaceFn getUtils;
    bool ok = load(m, "SteamAPI_Init", &a->init) && load(m, "SteamAPI_Shutdown", &a->shutdown) &&
              load(m, "SteamAPI_RunCallbacks", &a->runCallbacks) &&
              load(m, "SteamAPI_GetHSteamUser", &getUser) && load(m, "SteamAPI_GetHSteamPipe", &getPipe) &&
              load(m, "SteamClient", &client) &&
              load(m, "SteamAPI_ISteamClient_GetISteamUGC", &getUgc) &&
              load(m, "SteamAPI_ISteamClient_GetISteamUtils", &getUtils) &&
              load(m, "SteamAPI_ISteamClient_GetISteamFriends", &getFriends) &&
              load(m, "SteamAPI_ISteamUtils_IsAPICallCompleted", &a->callDone) &&
              load(m, "SteamAPI_ISteamUtils_GetAPICallResult", &a->callResult) &&
              load(m, "SteamAPI_ISteamUtils_GetAppID", &a->appId) &&
              load(m, "SteamAPI_ISteamFriends_GetPersonaName", &a->persona) &&
              load(m, "SteamAPI_ISteamUGC_CreateItem", &a->createItem) &&
              load(m, "SteamAPI_ISteamUGC_StartItemUpdate", &a->startUpdate) &&
              load(m, "SteamAPI_ISteamUGC_SetItemTitle", &a->setTitle) &&
              load(m, "SteamAPI_ISteamUGC_SetItemDescription", &a->setDesc) &&
              load(m, "SteamAPI_ISteamUGC_SetItemVisibility", &a->setVisibility) &&
              load(m, "SteamAPI_ISteamUGC_SetItemTags", &a->setTags) &&
              load(m, "SteamAPI_ISteamUGC_SetItemContent", &a->setContent) &&
              load(m, "SteamAPI_ISteamUGC_SetItemPreview", &a->setPreview) &&
              load(m, "SteamAPI_ISteamUGC_SubmitItemUpdate", &a->submit) &&
              load(m, "SteamAPI_ISteamUGC_GetItemUpdateProgress", &a->progress) &&
              load(m, "SteamAPI_ISteamUGC_GetNumSubscribedItems", &a->numSubs) &&
              load(m, "SteamAPI_ISteamUGC_GetSubscribedItems", &a->subs);
    if (!ok) return false;
    if (!a->init()) {
        fprintf(stderr, "SteamAPI_Init fallo: abre Steam con tu cuenta y vuelve a probar.\n");
        return false;
    }
    void* c = client();
    const HSteamUser u = getUser();
    const HSteamPipe p = getPipe();
    a->ugc     = getUgc(c, u, p, "STEAMUGC_INTERFACE_VERSION008");
    a->utils   = getUtils(c, p, "SteamUtils008");
    a->friends = getFriends(c, u, p, "SteamFriends015");
    if (!a->ugc || !a->utils || !a->friends) {
        fprintf(stderr, "Steam no dio las interfaces (ugc=%p utils=%p friends=%p)\n", a->ugc, a->utils, a->friends);
        return false;
    }
    return true;
}

// Wait for an async Steam call; 'tick' runs once a second (progress output).
bool waitCall(Api* a, SteamAPICall_t call, unsigned timeoutSec, void (*tick)(Api*, void*), void* ctx) {
    bool failed = false;
    DWORD start = GetTickCount(), lastTick = start;
    while (!a->callDone(a->utils, call, &failed)) {
        a->runCallbacks();
        Sleep(100);
        if (tick && GetTickCount() - lastTick >= 1000) { lastTick = GetTickCount(); tick(a, ctx); }
        if (GetTickCount() - start > timeoutSec * 1000u) { fprintf(stderr, "Steam no contesta (tiempo agotado)\n"); return false; }
    }
    if (failed) { fprintf(stderr, "La llamada a Steam fallo (IO)\n"); return false; }
    return true;
}

int cmdCheck(Api* a, const std::string& workshopDir) {
    printf("Cuenta de Steam: %s\n", a->persona(a->friends));
    printf("App: %u\n", a->appId(a->utils));
    const uint32_t n = a->numSubs(a->ugc);
    std::vector<PublishedFileId_t> ids(n ? n : 1);
    const uint32_t got = n ? a->subs(a->ugc, &ids[0], n) : 0;
    printf("Suscripciones de Kenshi: %u (leidas %u)\n", n, got);
    std::set<uint64_t> subs(ids.begin(), ids.begin() + got);
    if (workshopDir.empty()) return 0;
    // Every downloaded Workshop folder must be one of the subscriptions, and
    // the counts must agree: a wrong vtable would return garbage here.
    std::set<uint64_t> folders;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((workshopDir + "\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.')
                folders.insert(_strtoui64(fd.cFileName, 0, 10));
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    unsigned matched = 0;
    for (std::set<uint64_t>::const_iterator i = folders.begin(); i != folders.end(); ++i)
        if (subs.count(*i)) ++matched;
    printf("Carpetas descargadas: %u, de ellas suscritas: %u\n", (unsigned)folders.size(), matched);
    const bool ok = n == got && got > 0 && matched == folders.size() && matched == got;
    printf(ok ? "CHECK OK: la interfaz del Workshop es la correcta\n"
              : "CHECK FALLO: no coincide; no se sube nada\n");
    return ok ? 0 : 3;
}

int cmdCreate(Api* a) {
    printf("Cuenta de Steam: %s\n", a->persona(a->friends));
    const SteamAPICall_t call = a->createItem(a->ugc, KENSHI_APP_ID, 0 /*community*/);
    if (!call || !waitCall(a, call, 120, 0, 0)) return 4;
    CreateItemResult r;
    memset(&r, 0, sizeof(r));
    bool failed = false;
    if (!a->callResult(a->utils, call, &r, sizeof(r), CB_CREATE_ITEM, &failed) || failed) {
        fprintf(stderr, "No se pudo leer el resultado de CreateItem\n");
        return 4;
    }
    if (r.result != 1) { fprintf(stderr, "CreateItem: EResult %d\n", r.result); return 4; }
    printf("ITEM %llu\n", (unsigned long long)r.id);
    if (r.needsLegalAgreement) printf("LEGAL: falta aceptar el acuerdo del Workshop\n");
    return 0;
}

struct ProgressCtx { UGCUpdateHandle_t handle; };
void printProgress(Api* a, void* ctx) {
    uint64_t done = 0, total = 0;
    const int st = a->progress(a->ugc, static_cast<ProgressCtx*>(ctx)->handle, &done, &total);
    static const char* const names[] = { "-", "preparando", "leyendo", "subiendo contenido", "subiendo vista previa", "guardando cambios" };
    printf("  %s %llu/%llu\n", (st >= 0 && st <= 5) ? names[st] : "?", (unsigned long long)done, (unsigned long long)total);
    fflush(stdout);
}

int cmdUpdate(Api* a, int argc, char** argv) {
    const std::string itemS = arg(argc, argv, "--item");
    const std::string content = arg(argc, argv, "--content");
    const std::string preview = arg(argc, argv, "--preview");
    const std::string tag = arg(argc, argv, "--tag");
    const int visibility = atoi(arg(argc, argv, "--visibility", "2").c_str());
    std::string title, desc, note;
    if (!readText(arg(argc, argv, "--title-file"), &title) || !readText(arg(argc, argv, "--desc-file"), &desc) ||
        !readText(arg(argc, argv, "--note-file"), &note)) {
        fprintf(stderr, "Faltan --title-file, --desc-file o --note-file\n");
        return 2;
    }
    const PublishedFileId_t item = _strtoui64(itemS.c_str(), 0, 10);
    if (!item || content.empty() || preview.empty()) { fprintf(stderr, "Faltan --item, --content o --preview\n"); return 2; }
    if (desc.size() > 7900) { fprintf(stderr, "La descripcion pasa de 8000 caracteres\n"); return 2; }
    printf("Cuenta de Steam: %s\n", a->persona(a->friends));

    const UGCUpdateHandle_t h = a->startUpdate(a->ugc, KENSHI_APP_ID, item);
    if (!h) { fprintf(stderr, "StartItemUpdate fallo\n"); return 5; }
    bool ok = a->setTitle(a->ugc, h, title.c_str()) && a->setDesc(a->ugc, h, desc.c_str()) &&
              a->setContent(a->ugc, h, content.c_str()) && a->setPreview(a->ugc, h, preview.c_str());
    if (!ok) { fprintf(stderr, "Steam rechazo el titulo, la descripcion, la carpeta o la imagen\n"); return 5; }
    if (!a->setVisibility(a->ugc, h, visibility)) {
        // "Unlisted" (3) is newer than this interface; the owner can still pick
        // it on the item's page. Private until then.
        printf("VISIBILITY: Steam no acepto %d; queda privado (2)\n", visibility);
        a->setVisibility(a->ugc, h, 2);
    } else {
        printf("VISIBILITY: %d\n", visibility);
    }
    if (!tag.empty()) {
        const char* tags[1] = { tag.c_str() };
        SteamParamStringArray arr = { tags, 1 };
        a->setTags(a->ugc, h, &arr);
    }
    const SteamAPICall_t call = a->submit(a->ugc, h, note.c_str());
    ProgressCtx pc = { h };
    if (!call || !waitCall(a, call, 1800, &printProgress, &pc)) return 6;
    bool failed = false;
    SubmitItemResultNew rn;
    memset(&rn, 0, sizeof(rn));
    int result = -1;
    bool legal = false;
    if (a->callResult(a->utils, call, &rn, sizeof(rn), CB_SUBMIT_ITEM, &failed) && !failed) {
        result = rn.result; legal = rn.needsLegalAgreement;
    } else {
        SubmitItemResultOld ro;
        memset(&ro, 0, sizeof(ro));
        failed = false;
        if (a->callResult(a->utils, call, &ro, sizeof(ro), CB_SUBMIT_ITEM, &failed) && !failed) {
            result = ro.result; legal = ro.needsLegalAgreement;
        }
    }
    if (result != 1) { fprintf(stderr, "SubmitItemUpdate: EResult %d\n", result); return 6; }
    printf("SUBMIT OK item %llu\n", (unsigned long long)item);
    if (legal) printf("LEGAL: falta aceptar el acuerdo del Workshop\n");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "uso: workshop_upload check|create|update --kenshi <dir> ...\n");
        return 2;
    }
    const std::string cmd = argv[1];
    const std::string kenshi = arg(argc, argv, "--kenshi", "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Kenshi");
    Api a;
    if (!openSteam(kenshi, &a)) return 1;
    int rc = 2;
    if (cmd == "check") rc = cmdCheck(&a, arg(argc, argv, "--workshop-dir"));
    else if (cmd == "create") rc = cmdCreate(&a);
    else if (cmd == "update") rc = cmdUpdate(&a, argc, argv);
    else fprintf(stderr, "orden desconocida: %s\n", cmd.c_str());
    a.shutdown();
    return rc;
}
