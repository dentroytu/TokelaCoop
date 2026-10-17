// ReplicatorDismiss.cpp - dismissing a character, mirrored between the two games
// (v0.56, protocol 59). The engine half is game/EngineDismiss.cpp.
//
// Author: a dismissal in our squad screen (the engine hook queues it) goes out
// as EVT_DISMISS; the roster sweep ignores the body from then on, so no exit or
// move goes out for it as well. Receiver: repeat Kenshi's dismissal on our copy,
// then forget it as the friend's character. Afterwards the body is an NPC, and NPCs are the host's: a
// host dismissal carries its new NPC hand, a friend's one gets the host's hand
// back in EVT_DISMISS_BIND, so the friend's own body is driven from the host's
// stream instead of getting a duplicate.

#include "Replicator.h"
#include "ReplicatorUtil.h"

namespace coop {

namespace {

// Hands as plain arrays here: Replicator::Key is private to the class.
void fillEvent(EventPacket& ev, u8 type, u32 ownerId, u32 eventId,
               const unsigned int subj[5], const unsigned int actor[5]) {
    memset(&ev, 0, sizeof(ev));
    ev.type = (u8)PKT_EVENT; ev.event = type;
    ev.ownerId = ownerId; ev.eventId = eventId;
    ev.sType = subj[0]; ev.sContainer = subj[1]; ev.sContainerSerial = subj[2];
    ev.sIndex = subj[3]; ev.sSerial = subj[4];
    ev.aType = actor[0]; ev.aContainer = actor[1]; ev.aContainerSerial = actor[2];
    ev.aIndex = actor[3]; ev.aSerial = actor[4];
}

bool handIsZero(const unsigned int h[5]) { return (h[0] | h[1] | h[2] | h[3] | h[4]) == 0; }

} // namespace

#define DSM_KEY(K, H) Key K; K.t = (H)[0]; K.c = (H)[1]; K.cs = (H)[2]; K.i = (H)[3]; K.s = (H)[4]

void Replicator::publishDismissals(NetLink& net, u32 ownerId) {
    engine::DismissEdge de[16];
    const unsigned int n = engine::drainDismissEdges(de, 16);
    const unsigned long now = nowMs();
    for (unsigned int i = 0; i < n; ++i) {
        DSM_KEY(pre, de[i].before);
        DSM_KEY(post, de[i].after);
        // (The roster sweep ignores the body from now on: engine::ignoreRosterChar.)
        pinOwned_.erase(pre); pinPeer_.erase(pre);
        pinOwned_.erase(post); pinPeer_.erase(post);
        exitedOwn_[pre] = now;
        if (!dismissSync_) continue;
        // The host owns the NPC it became: tell the friend its hand. The friend
        // does not know the host's yet - the host answers with a BIND.
        const unsigned int zero[5] = { 0, 0, 0, 0, 0 };
        EventPacket ev;
        fillEvent(ev, (u8)EVT_DISMISS, ownerId, nextEventId_++, de[i].before,
                  isHostRole() ? de[i].after : zero);
        net.queueEvent(ev);
        if (!isHostRole() && de[i].c) ownDismissed_[pre] = de[i].c;
        char b[192]; _snprintf(b, sizeof(b) - 1,
            "[squad] DISMISS send old=%u,%u,%u,%u,%u new=%u,%u,%u,%u,%u",
            pre.t, pre.c, pre.cs, pre.i, pre.s, post.t, post.c, post.cs, post.i, post.s);
        b[sizeof(b) - 1] = '\0'; coop::logLine(b);
    }
    for (size_t i = 0; i < dismissBinds_.size(); ++i) {
        const Key& o = dismissBinds_[i].first;
        const Key& a = dismissBinds_[i].second;
        const unsigned int oh[5] = { o.t, o.c, o.cs, o.i, o.s };
        const unsigned int ah[5] = { a.t, a.c, a.cs, a.i, a.s };
        EventPacket ev;
        fillEvent(ev, (u8)EVT_DISMISS_BIND, ownerId, nextEventId_++, oh, ah);
        net.queueEvent(ev);
        char b[192]; _snprintf(b, sizeof(b) - 1,
            "[squad] DISMISS-BIND send old=%u,%u npc=%u,%u,%u,%u,%u",
            o.i, o.s, a.t, a.c, a.cs, a.i, a.s);
        b[sizeof(b) - 1] = '\0'; coop::logLine(b);
    }
    dismissBinds_.clear();
}

void Replicator::applyDismiss(GameWorld* gw, const Key& k, const Key& actor) {
    char b[256];
    // Never one of ours: the friend cannot dismiss our characters.
    if (ownHands_.count(k) || pinOwned_.count(k)) {
        _snprintf(b, sizeof(b) - 1, "[squad] RECV DISMISS old=%u,%u REFUSED (one of ours)", k.i, k.s);
        b[sizeof(b) - 1] = '\0'; coop::logLine(b);
        return;
    }
    // Never a dead body: a death has its own latch, and a corpse stays where it is.
    std::map<Key, Driven>::iterator t = targets_.find(k);
    const bool dead = (t != targets_.end() && t->second.deathLatched);
    Character* c = dead ? 0 : resolvePeerChar(k);
    if (c && mintedBodies_.count(c)) c = 0;   // a proxy we minted is not in our squad
    // A body we minted for that character (never in our squad): nothing to
    // dismiss here; on the join it follows the host's NPC under its new hand.
    Character* minted = 0;
    if (!c && !dead) {
        std::map<Key, Character*>::iterator mp = proxyByKey_.find(k);
        if (mp != proxyByKey_.end() && mp->second && mintedBodies_.count(mp->second)) minted = mp->second;
    }
    unsigned int lh[5] = { 0, 0, 0, 0, 0 };
    unsigned int post[5] = { 0, 0, 0, 0, 0 };
    int steps = 0;
    bool ok = false;
    if (c) {
        engine::readObjectHand(reinterpret_cast<RootObject*>(c), lh);
        ok = engine::dismissBody(gw, c, post, &steps);
    }
    DSM_KEY(lk, lh);
    DSM_KEY(pk, post);
    // Forget it as the friend's character, and keep its stream tail from
    // re-creating it.
    pinPeer_.erase(k); pinOwned_.erase(k);
    if (!handIsZero(lh)) { pinPeer_.erase(lk); pinOwned_.erase(lk); }
    proxyByKey_.erase(k);
    peerSquadKeys_.erase(k);
    targets_.erase(k);
    spawnReq_.erase(k);
    forceReqHands_.erase(k);
    if (c) canonicalOf_.erase(c);
    rekeyedOld_[k] = nowMs();
    // (dismissBody also takes the body out of our roster sweep.)
    const bool actorZero = (actor.t | actor.c | actor.cs | actor.i | actor.s) == 0;
    if (ok && c) {
        if (isHostRole()) {
            // The friend's dismissed body: tell them our NPC hand for it.
            if (actorZero && !handIsZero(post)) dismissBinds_.push_back(std::make_pair(k, pk));
        } else if (!actorZero) {
            // The host dismissed one of its own: our copy follows the host's NPC
            // stream under its new hand (no duplicate proxy for it).
            std::map<Key, Character*>::iterator ex = proxyByKey_.find(actor);
            if (ex != proxyByKey_.end() && ex->second && ex->second != c) destroyIfMinted(gw, ex->second);
            proxyByKey_[actor] = c;
        }
    } else if (minted && !isHostRole() && !actorZero) {
        std::map<Key, Character*>::iterator ex = proxyByKey_.find(actor);
        if (ex != proxyByKey_.end() && ex->second && ex->second != minted) destroyIfMinted(gw, ex->second);
        proxyByKey_[actor] = minted;
    }
    _snprintf(b, sizeof(b) - 1,
        "[squad] RECV DISMISS old=%u,%u,%u,%u,%u ok=%d steps=%d dead=%d minted=%d local=%u,%u,%u,%u,%u now=%u,%u,%u,%u,%u",
        k.t, k.c, k.cs, k.i, k.s, ok ? 1 : 0, steps, dead ? 1 : 0, minted ? 1 : 0,
        lk.t, lk.c, lk.cs, lk.i, lk.s, pk.t, pk.c, pk.cs, pk.i, pk.s);
    b[sizeof(b) - 1] = '\0'; coop::logLine(b);
}

void Replicator::applyDismissBind(GameWorld* gw, const Key& k, const Key& actor) {
    char b[192];
    std::map<Key, Character*>::iterator it = ownDismissed_.find(k);
    const bool actorZero = (actor.t | actor.c | actor.cs | actor.i | actor.s) == 0;
    if (it == ownDismissed_.end() || actorZero) {
        _snprintf(b, sizeof(b) - 1, "[squad] RECV DISMISS-BIND old=%u,%u bound=0 (not ours or no hand)", k.i, k.s);
        b[sizeof(b) - 1] = '\0'; coop::logLine(b);
        return;
    }
    Character* c = it->second;
    ownDismissed_.erase(it);
    unsigned int lh[5];
    if (!c || !engine::readHand(c, lh)) {   // reaped since
        _snprintf(b, sizeof(b) - 1, "[squad] RECV DISMISS-BIND old=%u,%u bound=0 (body gone)", k.i, k.s);
        b[sizeof(b) - 1] = '\0'; coop::logLine(b);
        return;
    }
    std::map<Key, Character*>::iterator ex = proxyByKey_.find(actor);
    if (ex != proxyByKey_.end() && ex->second && ex->second != c) destroyIfMinted(gw, ex->second);
    proxyByKey_[actor] = c;
    _snprintf(b, sizeof(b) - 1, "[squad] RECV DISMISS-BIND old=%u,%u npc=%u,%u bound=1",
              k.i, k.s, actor.i, actor.s);
    b[sizeof(b) - 1] = '\0'; coop::logLine(b);
}

#undef DSM_KEY

} // namespace coop
