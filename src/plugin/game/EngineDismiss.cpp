// EngineDismiss.cpp - dismissing a character, mirrored between the two games
// (v0.56, protocol 59).
//
// Before this, a dismissal reached the other game as a squad-move edge with a
// zeroed actor - the same thing a death sends - so the receiver only dropped its
// pins: the host kept the friend's dismissed character in the player squad, its
// next save kept it, and after a reload it was back (the own-characters guard
// left the host no way to dismiss it by hand).
//
// Kenshi has no engine call that dismisses a character outside the squad screen:
// SquadManagementScreen::dismissCharacter(2) does it inline (disassembled for
// Steam 1.0.65): move the body to the "Adventurers Guild" faction (or an empty
// one), into a new "Wandering Squad" platoon at its position, unselect it, give
// it that squad's AI packages (the wandering) and clear its player jobs and
// orders. dismissBody repeats those calls; the hook on
// dismissCharacter tells the replicator about a local dismissal (and refuses one
// of the friend's characters - a second barrier behind the squad-screen drag
// guard). Every engine read and call is SEH-guarded on the main thread.

#include "EngineInternal.h"
#include <kenshi/AI/AI.h>
#include <kenshi/AI/AITaskSystem.h>
#include <kenshi/AI/Blackboard.h>
#include <kenshi/gui/SquadManagementScreen.h>
#include <kenshi/Faction.h>
#include <kenshi/GameDataManager.h>
#include <kenshi/Platoon.h>

#include <vector>

#ifdef TOKELACOOP_HARNESS
// ManagementScreen.h does not compile under VC100 (see EngineInventory.cpp
// probeCurrentResearchSid); the dismiss_sync scenario only needs its singleton
// (KenshiLib export ?getSingleton@ManagementScreen@@SAPEAV1@XZ). The squad screen
// is its member at +0xA0, built the first time the squad tab (index 4 in
// tabCallback's switch, Steam 1.0.65) is shown: refreshSquads creates it only
// while the screen is visible.
class ManagementScreen {
public:
    static ManagementScreen* getSingleton();
    void setVisible(bool on, int tab);
};
#endif

namespace coop {
namespace engine {

namespace {

typedef void        (__fastcall* DismissCharFn)(SquadManagementScreen* self, int result);
typedef Faction*    (__fastcall* EmptyFacFn)(FactionManager* self);
typedef Platoon*    (__fastcall* NewPlatoonFn)(Faction* self, GameData* squadTemplate,
                                               bool permanent, const Ogre::Vector3* p);
typedef GameData*   (__fastcall* GdGetFn)(GameDataContainer* self, const std::string* sid);
typedef void        (__fastcall* CharVoidFn)(Character* self);
typedef void        (__fastcall* AddPackagesFn)(Blackboard* self, GameData* packageCollection);
typedef AIPackage*  (__fastcall* CurPackageFn)(Blackboard* self);

DismissCharFn g_dismissOrig       = 0;
EmptyFacFn    g_emptyFacFn        = 0;
NewPlatoonFn  g_newPlatoonFn      = 0;
GdGetFn       g_gdGetFn           = 0;
CharVoidFn    g_clearPermajobsFn  = 0;
AddPackagesFn g_addPackagesFn     = 0;
CurPackageFn  g_curPackageFn      = 0;   // diagnostics (harness probe)
bool          g_dismissResolved   = false;
unsigned int  g_lastDismissed[5]  = { 0, 0, 0, 0, 0 };   // hand of the last dismissed body
bool          g_haveLastDismissed = false;

void noteDismissed(const unsigned int h[5]) {
    for (int i = 0; i < 5; ++i) g_lastDismissed[i] = h[i];
    g_haveLastDismissed = true;
}

DismissOwnerFn            g_dismissOwner = 0;   // 0 unknown, 1 ours, 2 the friend's
bool                      g_dismissTrack = true;
std::vector<DismissEdge>  g_dismissEdges;       // main thread only
unsigned int              g_dismissRefused = 0;

// dismissCharacter and dismissChar are protected: reach them from a derived class.
struct DismissAccess : SquadManagementScreen {
    static intptr_t dismissAddr() {
        return KenshiLib::GetRealAddress(&DismissAccess::dismissCharacter);
    }
    static Character* pendingOf(SquadManagementScreen* s) {
        return static_cast<DismissAccess*>(s)->dismissChar;
    }
    static void setPending(SquadManagementScreen* s, Character* c) {
        static_cast<DismissAccess*>(s)->dismissChar = c;
    }
};

Character* pendingSeh(SquadManagementScreen* s) {
    __try { return s ? DismissAccess::pendingOf(s) : 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

void __fastcall dismissCharacter_hook(SquadManagementScreen* self, int result) {
    Character* c = (result == 2) ? pendingSeh(self) : 0;
    unsigned int pre[5] = { 0, 0, 0, 0, 0 };
    const bool havePre = c && readObjectHand(reinterpret_cast<RootObject*>(c), pre);
    if (c && havePre && g_dismissOwner && g_dismissOwner(pre) == 2) {
        // One of the friend's characters: never dismissed from here. A result
        // other than 2 makes the original only clear its pending pointer.
        ++g_dismissRefused;
        g_dismissOrig(self, 0);
        return;
    }
    g_dismissOrig(self, result);
    if (!c || !havePre || !g_dismissTrack) return;
    ignoreRosterChar(c);   // the event below is the whole story, not a squad move
    DismissEdge e;
    for (int i = 0; i < 5; ++i) { e.before[i] = pre[i]; e.after[i] = 0; }
    readObjectHand(reinterpret_cast<RootObject*>(c), e.after);
    noteDismissed(e.after);
    e.c = c;
    if (g_dismissEdges.size() < 16) g_dismissEdges.push_back(e);
}

void resolveDismissFns() {
    if (g_dismissResolved) return;
    g_dismissResolved = true;
    g_emptyFacFn  = (EmptyFacFn)KenshiLib::GetRealAddress(&FactionManager::getEmptyFaction);
    g_newPlatoonFn = (NewPlatoonFn)KenshiLib::GetRealAddress(&Faction::createNewEmptyActivePlatoon);
    g_gdGetFn = (GdGetFn)KenshiLib::GetRealAddress(
        static_cast<GameData* (GameDataContainer::*)(const std::string&)>(&GameDataContainer::getData));
    g_clearPermajobsFn = (CharVoidFn)KenshiLib::GetRealAddress(&Character::clearPermajobs);
    // Blackboard::addPackages (the wandering AI packages Kenshi gives a dismissed
    // body) is not in our link library, but KenshiLib.dll exports it from
    // RE_Kenshi 0.3.5 on. The export is a jump through KenshiLib's own address
    // table, so it is called like any linked function. With an older RE_Kenshi
    // it is missing and the body is left a plain NPC, as before v0.61.
    HMODULE klib = GetModuleHandleA("KenshiLib.dll");
    if (klib) {
        g_addPackagesFn = (AddPackagesFn)GetProcAddress(klib, "?addPackages@Blackboard@@QEAAXPEAVGameData@@@Z");
        g_curPackageFn  = (CurPackageFn)GetProcAddress(klib, "?getCurrentPackage@Blackboard@@QEAAPEAVAIPackage@@XZ");
    }
#ifdef TOKELACOOP_HARNESS
    // dismiss_ai_off (negative control): TOKELACOOP_DISMISS_AI=0 repeats a
    // dismissal without the packages, as up to v0.60.
    const char* aiEnv = getenv("TOKELACOOP_DISMISS_AI");
    if (aiEnv && aiEnv[0] == '0') {
        g_addPackagesFn = 0;
        coop::logLine("[squad] dismiss: AI packages off (TOKELACOOP_DISMISS_AI=0)");
        return;
    }
#endif
    coop::logLine(g_addPackagesFn ? "[squad] dismiss: Blackboard::addPackages found"
                                  : "[squad] dismiss: Blackboard::addPackages missing (RE_Kenshi < 0.3.5)");
}

// The engine half of dismissBody: plain data only (no C++ object needing
// unwinding may live in an SEH frame). 'steps' reports how far it got: 4 done,
// 5 done and the body got the "Wandering Squad" AI packages.
bool dismissBodySeh(GameWorld* gw, Character* c, const std::string* facSid,
                    const std::string* squadSid, unsigned int post[5], int* steps) {
    *steps = 0;
    __try {
        if (!isPlayerSquad(gw, reinterpret_cast<RootObject*>(c))) return false;
        Faction* f = g_facBySidFn ? g_facBySidFn(gw->factionMgr, facSid) : 0;
        if (!f && g_emptyFacFn) f = g_emptyFacFn(gw->factionMgr);
        if (!f) return false;
        *steps = 1;
        PlayerInterface* pl = gw->player;
        RootObject* obj = reinterpret_cast<RootObject*>(c);
        if (pl && pl->isObjectSelected(obj)) pl->unselectPlayerCharacter(obj);
        c->setFaction(f, 0);
        *steps = 2;
        GameData* squad = g_gdGetFn ? g_gdGetFn(static_cast<GameDataContainer*>(&gw->gamedata), squadSid) : 0;
        bool moved = false;
        if (squad && g_newPlatoonFn) {
            Ogre::Vector3 pos = c->getPosition();
            Platoon* p = g_newPlatoonFn(f, squad, true, &pos);
            if (p && p->activePlatoon) { p->activePlatoon->addActiveObject(obj); moved = true; }
            *steps = 3;
        }
        // Kenshi's next steps, in its order: the new squad's blackboard drops
        // the old squad's jobs and takes the template's AI packages, then the
        // body's own orders go. Only once it is in the new squad: before that
        // the blackboard is still the player squad's.
        bool wander = false;
        AI* ai = c->ai;
        if (moved && ai) {
            Blackboard* bb = ai->getBlackboard();
            if (bb) {
                bb->_flagCleared = true;
                bb->leaderJobs.clear();
                bb->squadJobs.clear();
                bb->squadJobs2.clear();
                bb->slaveJobs.clear();
                if (g_addPackagesFn) { g_addPackagesFn(bb, squad); wander = true; }
            }
        }
        if (g_clearPermajobsFn) g_clearPermajobsFn(c);   // = ai->taskSystemAI->clearPermajobs()
        AITaskSytem* ts = ai ? ai->taskSystemAI : 0;
        if (ts) {
            ts->actions.clearAndDelete();
            ts->clearSlaveJobs();
        }
        *steps = wander ? 5 : 4;
        readObjectHand(obj, post);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace

bool installDismissHook(DismissOwnerFn owner, bool track) {
    resolveDismissFns();
    g_dismissOwner = owner;
    g_dismissTrack = track;
    intptr_t addr = DismissAccess::dismissAddr();
    if (!addr) return false;
    return KenshiLib::AddHook(addr, (void*)&dismissCharacter_hook,
                              (void**)&g_dismissOrig) == KenshiLib::SUCCESS;
}

unsigned int drainDismissEdges(DismissEdge* out, unsigned int maxOut) {
    unsigned int n = 0;
    for (size_t i = 0; i < g_dismissEdges.size() && n < maxOut; ++i) out[n++] = g_dismissEdges[i];
    g_dismissEdges.clear();
    return n;
}

unsigned int takeDismissRefusals() {
    unsigned int n = g_dismissRefused;
    g_dismissRefused = 0;
    return n;
}

bool dismissBody(GameWorld* gw, Character* c, unsigned int post[5], int* steps) {
    int s = 0;
    if (steps) *steps = 0;
    for (int i = 0; i < 5; ++i) post[i] = 0;
    if (!gw || !c) return false;
    resolveDismissFns();
    // The records Kenshi's own dismissal uses: the "Adventurers Guild" faction
    // and the "Wandering Squad" squad template.
    const std::string facSid("1233-gamedata.base");
    const std::string squadSid("5006-squad.mod");
    const bool ok = dismissBodySeh(gw, c, &facSid, &squadSid, post, &s);
    if (steps) *steps = s;
    if (s >= 2) ignoreRosterChar(c);   // it left the player faction
    if (ok) noteDismissed(post);
    return ok;
}

#ifdef TOKELACOOP_HARNESS
namespace {

// The player's own path: the squad screen's pending character, then the
// confirmation's "yes" (2), through the detoured entry so our hook runs.
int dismissViaScreenSeh(Character* c) {
    const int SQUAD_TAB = 4;
    __try {
        ManagementScreen* ms = ManagementScreen::getSingleton();
        if (!ms) return -2;
        SquadManagementScreen** slot =
            reinterpret_cast<SquadManagementScreen**>(reinterpret_cast<char*>(ms) + 0xA0);
        bool opened = false;
        if (!*slot) {
            // Never opened this session: open the squad tab, as the player does.
            ms->setVisible(true, SQUAD_TAB);
            opened = true;
        }
        SquadManagementScreen* sq = *slot;
        if (!sq) {
            if (opened) ms->setVisible(false, SQUAD_TAB);
            return -3;
        }
        DismissCharFn fn = (DismissCharFn)DismissAccess::dismissAddr();
        if (!fn) return -4;
        DismissAccess::setPending(sq, c);
        fn(sq, 2);
        if (opened) ms->setVisible(false, SQUAD_TAB);
        return opened ? 2 : 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -5;
    }
}

unsigned int rosterPtrsSeh(GameWorld* gw, Character** out, unsigned int cap) {
    __try {
        PlayerInterface* pl = gw ? gw->player : 0;
        if (!pl) return 0;
        unsigned int n = (unsigned int)pl->playerCharacters.size();
        if (n > cap) n = cap;
        for (unsigned int i = 0; i < n; ++i) out[i] = pl->playerCharacters[i];
        return n;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

} // namespace

int probeDismiss(GameWorld* gw, const unsigned int hand[5], unsigned int post[5], int* via,
                 bool allowFallback) {
    if (via) *via = 0;
    for (int i = 0; i < 5; ++i) post[i] = 0;
    if (!gw || !hand) return -1;
    Character* c = resolveCharByHand(hand[3], hand[4], hand[0], hand[1], hand[2]);
    if (!c) return -1;
    const size_t queued = g_dismissEdges.size();
    const unsigned int refusedBefore = g_dismissRefused;
    const int rc = dismissViaScreenSeh(c);
    if (rc == 1 || rc == 2) {
        if (via) *via = 1;
        if (rc == 2) coop::logLine("[squad] dismiss probe: opened the squad tab to reach the squad screen");
        if (g_dismissRefused != refusedBefore) return 3;   // the hook refused: the friend's
        readObjectHand(reinterpret_cast<RootObject*>(c), post);
        // With tracking on, the hook queued the edge: the proof it ran.
        return (!g_dismissTrack || g_dismissEdges.size() > queued) ? 1 : 0;
    }
    if (!allowFallback) return 0;
    // The screen is not reachable: the same engine calls, edge queued by hand.
    if (via) *via = 2;
    {
        char b[96];
        _snprintf(b, sizeof(b) - 1, "[squad] dismiss probe: squad screen unreachable rc=%d", rc);
        b[sizeof(b) - 1] = '\0'; coop::logLine(b);
    }
    unsigned int pre[5] = { 0, 0, 0, 0, 0 };
    readObjectHand(reinterpret_cast<RootObject*>(c), pre);
    int steps = 0;
    if (!dismissBody(gw, c, post, &steps)) return 0;
    if (g_dismissTrack) {
        DismissEdge e;
        for (int i = 0; i < 5; ++i) { e.before[i] = pre[i]; e.after[i] = post[i]; }
        e.c = c;
        g_dismissEdges.push_back(e);
    }
    return 1;
}

bool probeFriendMember(GameWorld* gw, unsigned int outHand[5], char* outName, unsigned int nameCap) {
    for (int i = 0; i < 5; ++i) outHand[i] = 0;
    if (outName && nameCap) outName[0] = '\0';
    if (!g_dismissOwner) return false;
    static Character* cs[160];   // main thread only
    const unsigned int n = rosterPtrsSeh(gw, cs, 160);
    for (unsigned int i = 0; i < n; ++i) {
        unsigned int h[5] = { 0, 0, 0, 0, 0 };
        if (!readObjectHand(reinterpret_cast<RootObject*>(cs[i]), h)) continue;
        if (g_dismissOwner(h) != 2) continue;
        for (int k = 0; k < 5; ++k) outHand[k] = h[k];
        if (outName && nameCap) charName(cs[i], outName, nameCap);
        return true;
    }
    return false;
}

unsigned int probeSquadNames(GameWorld* gw, char* out, unsigned int cap) {
    if (!out || cap == 0) return 0;
    out[0] = '\0';
    static Character* cs[160];   // main thread only
    const unsigned int n = rosterPtrsSeh(gw, cs, 160);
    size_t len = 0;
    for (unsigned int i = 0; i < n; ++i) {
        char nm[48];
        charName(cs[i], nm, sizeof(nm));
        const size_t l = strlen(nm);
        if (len + l + 2 >= cap) break;
        if (len) out[len++] = '|';
        memcpy(out + len, nm, l);
        len += l;
        out[len] = '\0';
    }
    return n;
}

namespace {
// Plain data only: the SEH half of probeDismissedAi.
int dismissedAiSeh(Character* c, char* sid, unsigned int sidCap) {
    __try {
        if (!c) return 0;
        AI* ai = c->ai;
        Blackboard* bb = ai ? ai->getBlackboard() : 0;
        if (!bb) return 1;
        AIPackage* p = g_curPackageFn ? g_curPackageFn(bb) : 0;
        // AIPackage::packageData (0x68 in AIPackage.h, which does not compile
        // next to Blackboard.h: both define BlackboardSignalFunctions).
        GameData* d = p ? *reinterpret_cast<GameData**>(reinterpret_cast<char*>(p) + 0x68) : 0;
        if (!d) return 2;
        strncpy(sid, d->stringID.c_str(), sidCap - 1);
        sid[sidCap - 1] = '\0';
        return 3;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
} // namespace

int probeDismissedAi(GameWorld* gw, unsigned int hand[5], char* sid, unsigned int sidCap) {
    for (int i = 0; i < 5; ++i) hand[i] = g_lastDismissed[i];
    if (sid && sidCap) sid[0] = '\0';
    if (!gw || !sid || sidCap < 2) return 0;
    if (!g_haveLastDismissed) return -1;
    resolveDismissFns();
    Character* c = resolveCharByHand(hand[3], hand[4], hand[0], hand[1], hand[2]);
    return dismissedAiSeh(c, sid, sidCap);
}
#endif

} // namespace engine
} // namespace coop
