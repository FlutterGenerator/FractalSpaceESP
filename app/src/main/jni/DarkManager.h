// New Esp Manager by Dark Team!
// Credit : Telegram @dark_fack
// Exclusive to everyone who meets in our community
//
// Reworked: the list of enemies is now a map guarded by a mutex (the game thread writes it, the
// overlay thread reads it) and it stores ready screen positions, so the overlay thread never has to
// call Unity (Unity APIs may only be called from Unity's main thread).

//**********************************************

#include <map>
#include <mutex>
#include <chrono>

struct TrackedEnemy {
    bool ok;         // screen position is valid
    float sx, sy, sz; // Unity screen position of the feet (pixels, origin bottom-left), sz <= 0: behind camera
    bool headOk;      // head position is valid
    float hy;         // Unity screen Y of the head (feet position + ENEMY_HEIGHT)
    long long seenMs; // last time the game updated it
};

std::mutex enemiesMtx;
std::map<void *, TrackedEnemy> enemies;
volatile int unityScreenW = 0, unityScreenH = 0;

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
}

//************************************************
