#include "Canvas/ESP.h"
#include "Canvas/Bools.h"
#include "Canvas/StructsCommon.h"

#include "DarkManager.h"

#include "Substrate/SubstrateHook.h"
#include "Substrate/CydiaSubstrate.h"

#if defined(__aarch64__)
#include "And64InlineHook/And64InlineHook.hpp"
#endif

//Target lib here
#define targetLibName OBFUSCATE("libil2cpp.so")

// ============================================================================
// RVAs taken from the ESP Tester report of com.hazegames.fractalspace
// ============================================================================
#if defined(__aarch64__)
// ==================== 64-BIT (arm64-v8a) RVA ====================
#define RVA_BLOC_UPDATE    0x17D6C1C  
#define RVA_HEALTH_UPDATE  0x17FA87C  
#define RVA_CAMERA_MAIN    0x3288470  
#define RVA_CAMERA_CURRENT 0x32884F8  
#define RVA_CAMERA_W2S     0x32881E0  
#define RVA_GET_TRANSFORM  0x32C004C  
#define RVA_GET_POSITION   0x32D1664  
#define RVA_SCREEN_WIDTH   0x32928F4  
#define RVA_SCREEN_HEIGHT  0x329291C  
#else
// ==================== 32-BIT (armeabi-v7a) RVA ====================
#define RVA_BLOC_UPDATE    0xC28A04   // Замените на 32-бит RVA из вашего отчета, если нужно
#define RVA_HEALTH_UPDATE  0xC573AC  
#define RVA_CAMERA_MAIN    0x2B606AC  
#define RVA_CAMERA_CURRENT 0x2B6083C  
#define RVA_CAMERA_W2S     0x2B602C8  
#define RVA_GET_TRANSFORM  0x2BA591C  
#define RVA_GET_POSITION   0x2BBBB6C  
#define RVA_SCREEN_WIDTH   0x2B6D234  
#define RVA_SCREEN_HEIGHT  0x2B6D278  
#endif

// Height of the object model in Unity units (metres).
#define ENEMY_HEIGHT 1.8f

// plain 3 floats: must be a trivial struct so it is passed/returned exactly like Unity's Vector3
struct UVec3 { float x, y, z; };

void *(*get_Transform)(void *instance, void *mi) = NULL;
UVec3 (*get_position)(void *transform, void *mi) = NULL;
void *(*get_camera)(void *mi) = NULL;
void *(*get_camera_current)(void *mi) = NULL;
UVec3 (*get_screenpos)(void *cam, UVec3 pos, void *mi) = NULL;
int (*get_screen_width)(void *mi) = NULL;
int (*get_screen_height)(void *mi) = NULL;

volatile bool espReady = false;

void (*old_Bloc_update)(void *, void *) = NULL;
void (*old_Health_update)(void *, void *) = NULL;

// Separate maps for both active classes
std::map<void *, TrackedEnemy> blocObjectsMap;
std::map<void *, TrackedEnemy> healthObjectsMap;
std::mutex objectsMtx;

// Hook 1: BlocScript Update
void Bloc_update(void *player, void *mi) {
    if (espReady && Esp && player != NULL &&
        *(void **) ((char *) player + 2 * sizeof(void *)) != NULL) {
        TrackedEnemy t;
        t.ok = false;
        t.headOk = false;
        t.hy = 0;
        t.sx = t.sy = t.sz = 0;
        t.seenMs = nowMs();

        static long long lastScreenMs = 0;
        if (get_screen_width && get_screen_height && t.seenMs - lastScreenMs > 500) {
            lastScreenMs = t.seenMs;
            unityScreenW = get_screen_width(NULL);
            unityScreenH = get_screen_height(NULL);
        }

        void *cam = get_camera ? get_camera(NULL) : NULL;
        if (cam == NULL && get_camera_current) cam = get_camera_current(NULL);
        void *tr = get_Transform(player, NULL);
        if (cam != NULL && tr != NULL) {
            UVec3 world = get_position(tr, NULL);
            UVec3 scr = get_screenpos(cam, world, NULL);
            t.sx = scr.x;
            t.sy = scr.y;
            t.sz = scr.z; // Исправлено (используется .z)
            t.ok = true;
            UVec3 head = world;
            head.y += ENEMY_HEIGHT;
            UVec3 hs = get_screenpos(cam, head, NULL);
            if (hs.z > 0) {
                t.hy = hs.y;
                t.headOk = true;
            }
        }
        std::lock_guard<std::mutex> g(objectsMtx);
        blocObjectsMap[player] = t;
    }
    if (old_Bloc_update) old_Bloc_update(player, mi);
}

// Hook 2: Health Update
void Health_update(void *player, void *mi) {
    if (espReady && Esp && player != NULL &&
        *(void **) ((char *) player + 2 * sizeof(void *)) != NULL) {
        TrackedEnemy t;
        t.ok = false;
        t.headOk = false;
        t.hy = 0;
        t.sx = t.sy = t.sz = 0;
        t.seenMs = nowMs();

        static long long lastScreenMs = 0;
        if (get_screen_width && get_screen_height && t.seenMs - lastScreenMs > 500) {
            lastScreenMs = t.seenMs;
            unityScreenW = get_screen_width(NULL);
            unityScreenH = get_screen_height(NULL);
        }

        void *cam = get_camera ? get_camera(NULL) : NULL;
        if (cam == NULL && get_camera_current) cam = get_camera_current(NULL);
        void *tr = get_Transform(player, NULL);
        if (cam != NULL && tr != NULL) {
            UVec3 world = get_position(tr, NULL);
            UVec3 scr = get_screenpos(cam, world, NULL);
            t.sx = scr.x;
            t.sy = scr.y;
            t.sz = scr.z; // Исправлено (используется .z)
            t.ok = true;
            UVec3 head = world;
            head.y += ENEMY_HEIGHT;
            UVec3 hs = get_screenpos(cam, head, NULL);
            if (hs.z > 0) {
                t.hy = hs.y;
                t.headOk = true;
            }
        }
        std::lock_guard<std::mutex> g(objectsMtx);
        healthObjectsMap[player] = t;
    }
    if (old_Health_update) old_Health_update(player, mi);
}

// Runs on the overlay thread: only reads the stored numbers, never calls Unity.
void DrawESP(ESP esp, int screenWidth, int screenHeight) {

    esp.DrawText(Color::Red(), "tojik_proof_93 Esp Mod Menu", Vector2(screenWidth / 2, screenHeight / 1.02f), 25.0f);

    long long now = nowMs();

    if (EspObject) {
        int count = 0;
        {
            std::lock_guard<std::mutex> g(objectsMtx);
            for (std::map<void *, TrackedEnemy>::iterator it = blocObjectsMap.begin(); it != blocObjectsMap.end(); ++it)
                if (now - it->second.seenMs <= 500) count++;
            for (std::map<void *, TrackedEnemy>::iterator it = healthObjectsMap.begin(); it != healthObjectsMap.end(); ++it)
                if (now - it->second.seenMs <= 500) count++;
        }
        std::string Allplayers;
        Allplayers += "All Objects: ";
        Allplayers += std::to_string(count);

        esp.DrawText(Color::Black(), Allplayers.c_str(), Vector2(screenWidth / 2, screenHeight / 2), 35.0f);
    }

    if (Esp) {
        int uw = unityScreenW, uh = unityScreenH;
        float scaleX = uw > 0 ? (float) screenWidth / (float) uw : 1.0f;
        float scaleY = uh > 0 ? (float) screenHeight / (float) uh : 1.0f;

        std::lock_guard<std::mutex> g(objectsMtx);
        
        // Render BlocScript objects (Зеленые линии и кружочки)
        for (std::map<void *, TrackedEnemy>::iterator it = blocObjectsMap.begin(); it != blocObjectsMap.end();) {
            const TrackedEnemy &t = it->second;
            if (now - t.seenMs > 2000) {
                it = blocObjectsMap.erase(it);
                continue;
            }
            ++it;
            if (now - t.seenMs > 500 || !t.ok || t.sz <= 0) continue;

            float x = t.sx * scaleX;
            float y = (float) screenHeight - (t.sy * scaleY); // Исправленный расчет Y
            if (x < -100 || x > screenWidth + 100 || y < -100 || y > screenHeight + 100) continue;

            if (EspLine) {
                // Линия идет ровно в центр кружочка (x, y)
                esp.DrawLine(Color::Green(), 1, Vector2(screenWidth * 0.5f, screenHeight * 0.5f), Vector2(x, y));
            }

            if (EspBox) {
                esp.DrawCircle(Color::Green(), 2, Vector2(x, y), 15.0f);
            }
        }

        // Render Health objects (Красные линии и кружочки)
        for (std::map<void *, TrackedEnemy>::iterator it = healthObjectsMap.begin(); it != healthObjectsMap.end();) {
            const TrackedEnemy &t = it->second;
            if (now - t.seenMs > 2000) {
                it = healthObjectsMap.erase(it);
                continue;
            }
            ++it;
            if (now - t.seenMs > 500 || !t.ok || t.sz <= 0) continue;

            float x = t.sx * scaleX;
            float y = (float) screenHeight - (t.sy * scaleY); // Исправленный расчет Y
            if (x < -100 || x > screenWidth + 100 || y < -100 || y > screenHeight + 100) continue;

            if (EspLine) {
                // Линия идет ровно в центр кружочка (x, y)
                esp.DrawLine(Color::Red(), 1, Vector2(screenWidth * 0.5f, screenHeight * 0.5f), Vector2(x, y));
            }

            if (EspBox) {
                esp.DrawCircle(Color::Red(), 2, Vector2(x, y), 15.0f);
            }
        }
    }
}

void hack_thread() {

    do {
        sleep(1);
    } while (!isLibraryLoaded(targetLibName));
    sleep(5); // let libil2cpp finish loading before patching it

    get_Transform = (void *(*)(void *, void *)) getAbsoluteAddress(targetLibName, RVA_GET_TRANSFORM);
    get_position = (UVec3 (*)(void *, void *)) getAbsoluteAddress(targetLibName, RVA_GET_POSITION);
    get_camera = (void *(*)(void *)) getAbsoluteAddress(targetLibName, RVA_CAMERA_MAIN);
    get_camera_current = (void *(*)(void *)) getAbsoluteAddress(targetLibName, RVA_CAMERA_CURRENT);
    get_screenpos = (UVec3 (*)(void *, UVec3, void *)) getAbsoluteAddress(targetLibName, RVA_CAMERA_W2S);
    get_screen_width = (int (*)(void *)) getAbsoluteAddress(targetLibName, RVA_SCREEN_WIDTH);
    get_screen_height = (int (*)(void *)) getAbsoluteAddress(targetLibName, RVA_SCREEN_HEIGHT);

#if defined(__aarch64__)
    // Для 64-бит (arm64-v8a) используем And64InlineHook
    A64HookFunction((void *) getAbsoluteAddress(targetLibName, RVA_BLOC_UPDATE),
                    (void *) &Bloc_update,
                    (void **) &old_Bloc_update);

    A64HookFunction((void *) getAbsoluteAddress(targetLibName, RVA_HEALTH_UPDATE),
                    (void *) &Health_update,
                    (void **) &old_Health_update);
#else
    // Для 32-бит (armeabi-v7a) используем привычный Substrate
    MSHookFunction((void *) getAbsoluteAddress(targetLibName, RVA_BLOC_UPDATE),
                   (void *) &Bloc_update,
                   (void **) &old_Bloc_update);

    MSHookFunction((void *) getAbsoluteAddress(targetLibName, RVA_HEALTH_UPDATE),
                   (void *) &Health_update,
                   (void **) &old_Health_update);
#endif
                   
    espReady = true;
}

