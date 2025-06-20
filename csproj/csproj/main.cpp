#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <thread>
#include <vector>
#include <iomanip>
#include <cmath>
#include <cfloat>
#include <algorithm>

#include "RadarTypes.h"

constexpr uintptr_t dwEntityList = 0x1A020A8;
constexpr uintptr_t dwLocalPlayerController = 0x1A50AD0;
constexpr uintptr_t m_hPlayerPawn = 0x824;
constexpr uintptr_t m_iTeamNum = 0x3E3;
constexpr uintptr_t m_iHealth = 0x344;
constexpr uintptr_t m_vOldOrigin = 0x1324;
constexpr uintptr_t m_angEyeAngles = 0x1438;  // dein Blickwinkel (Yaw)

Vector3 g_localPos = {};
std::vector<Vector3> g_enemyPositions;
float g_viewYaw = 0.0f;

float Distance(const Vector3& a, const Vector3& b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) +
        (a.y - b.y) * (a.y - b.y) +
        (a.z - b.z) * (a.z - b.z));
}

int MapDistanceToIntensity(float dist, float minDist = 100.0f, float maxDist = 2000.0f) {
    if (dist <= minDist) return 100;
    if (dist >= maxDist) return 0;
    return static_cast<int>(100 * (1.0f - (dist - minDist) / (maxDist - minDist)));
}

DWORD GetProcessIdByName(const wchar_t* processName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry = { sizeof(entry) };
    while (Process32NextW(snap, &entry)) {
        if (_wcsicmp(entry.szExeFile, processName) == 0) {
            CloseHandle(snap);
            return entry.th32ProcessID;
        }
    }
    CloseHandle(snap);
    return 0;
}

uintptr_t GetModuleBaseAddress(DWORD procId, const wchar_t* modName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
    MODULEENTRY32W modEntry = { sizeof(modEntry) };
    while (Module32NextW(snap, &modEntry)) {
        if (_wcsicmp(modEntry.szModule, modName) == 0) {
            CloseHandle(snap);
            return (uintptr_t)modEntry.modBaseAddr;
        }
    }
    CloseHandle(snap);
    return 0;
}

uintptr_t GetPawnFromHandle(HANDLE hProc, uintptr_t entityList, uint32_t handle) {
    uint16_t index = handle & 0x7FF;
    uintptr_t listEntryPtr;
    uintptr_t listEntry = entityList + 0x8 * (index >> 9) + 0x10;
    if (!ReadProcessMemory(hProc, (void*)listEntry, &listEntryPtr, sizeof(listEntryPtr), nullptr) || !listEntryPtr)
        return 0;

    uintptr_t pawnPtr;
    uintptr_t pawnAddr = listEntryPtr + 0x78 * (index & 0x1FF);
    if (!ReadProcessMemory(hProc, (void*)pawnAddr, &pawnPtr, sizeof(pawnPtr), nullptr) || !pawnPtr)
        return 0;

    return pawnPtr;
}

bool IsValidPosition(const Vector3& pos) {
    return !(std::isnan(pos.x) || std::isnan(pos.y) || std::isnan(pos.z)) &&
        !(pos.x == 0.0f && pos.y == 0.0f && pos.z == 0.0f);
}

void RunRadarWindow(); // forward declaration

int main() {
    std::cout << "[INFO] CS2 Radar + Sound ESP + Rotation + FOV Starting...\n";

    std::thread radarThread(RunRadarWindow);
    radarThread.detach();

    while (true) {
        DWORD pid = GetProcessIdByName(L"cs2.exe");
        if (!pid) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        HANDLE hProc = OpenProcess(PROCESS_VM_READ, FALSE, pid);
        if (!hProc) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        uintptr_t clientBase = GetModuleBaseAddress(pid, L"client.dll");
        if (!clientBase) {
            CloseHandle(hProc);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        uintptr_t entityList, localController;
        ReadProcessMemory(hProc, (void*)(clientBase + dwEntityList), &entityList, sizeof(entityList), nullptr);
        ReadProcessMemory(hProc, (void*)(clientBase + dwLocalPlayerController), &localController, sizeof(localController), nullptr);

        if (!entityList || !localController) {
            CloseHandle(hProc);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        while (true) {
            uint32_t localPawnHandle;
            if (!ReadProcessMemory(hProc, (void*)(localController + m_hPlayerPawn), &localPawnHandle, sizeof(localPawnHandle), nullptr)
                || localPawnHandle == 0 || localPawnHandle == 0xFFFFFFFF) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            uintptr_t localPawn = GetPawnFromHandle(hProc, entityList, localPawnHandle);
            if (!localPawn) break;

            uint8_t localTeam;
            ReadProcessMemory(hProc, (void*)(localPawn + m_iTeamNum), &localTeam, sizeof(localTeam), nullptr);
            if (localTeam < 2 || localTeam > 3) break;

            // Position setzen
            ReadProcessMemory(hProc, (void*)(localPawn + m_vOldOrigin), &g_localPos, sizeof(g_localPos), nullptr);

            // Blickwinkel setzen
            struct QAngle { float pitch, yaw, roll; };
            QAngle eyeAngles{};
            ReadProcessMemory(hProc, (void*)(localPawn + m_angEyeAngles), &eyeAngles, sizeof(eyeAngles), nullptr);
            g_viewYaw = eyeAngles.yaw;

            // Gegner sammeln
            g_enemyPositions.clear();
            float closestDist = FLT_MAX;

            for (int i = 0; i < 1024; i++) {
                uintptr_t entryBase = entityList + 0x8 * (i >> 9) + 0x10;
                uintptr_t entryPtr = 0;
                ReadProcessMemory(hProc, (void*)entryBase, &entryPtr, sizeof(entryPtr), nullptr);
                if (!entryPtr) continue;

                uintptr_t pawnAddr = entryPtr + 0x78 * (i & 0x1FF);
                uintptr_t pawn = 0;
                ReadProcessMemory(hProc, (void*)pawnAddr, &pawn, sizeof(pawn), nullptr);
                if (!pawn || pawn == localPawn) continue;

                uint8_t team = 0;
                int health = 0;
                ReadProcessMemory(hProc, (void*)(pawn + m_iTeamNum), &team, sizeof(team), nullptr);
                ReadProcessMemory(hProc, (void*)(pawn + m_iHealth), &health, sizeof(health), nullptr);

                if (team != 2 && team != 3 || team == localTeam || health <= 0 || health > 100) continue;

                Vector3 pos{};
                if (ReadProcessMemory(hProc, (void*)(pawn + m_vOldOrigin), &pos, sizeof(pos), nullptr)) {
                    if (IsValidPosition(pos)) {
                        g_enemyPositions.push_back(pos);
                        float dist = Distance(g_localPos, pos);
                        if (dist < closestDist) closestDist = dist;
                    }
                }
            }

            // Sound-ESP
            int intensity = MapDistanceToIntensity(closestDist);
            int freq = 400 + (intensity * 3);
            if (intensity > 0) Beep(freq, 100);

            int delay = 2000 - (intensity * 18);
            delay = std::clamp(delay, 200, 2000);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }

        CloseHandle(hProc);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
