// Derived from VR Reins, Copyright (C) 2026 SollyStones.
// Non-VR adaptation. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <SKSE/InputMap.h>
#include <mutex>
#include <sstream>
namespace Config {
struct Data {
 bool debugLog = false, dumpHorseNodes = false, tieToTerrain = true;
 std::string headNode;
 float mouthForward = 35, mouthDown = 18, restUp = 22, restForward = 10;
 float reinLength = 150, leadFollowDistance = 200, leadCatchUpRadius = 360;
 float tieReach = 32;
};
inline const Data& Get() { static Data data; return data; }
}
namespace NeckConfig {
// Forward/right/up, then longitudinal/lateral/vertical radii and pitch in degrees.
inline constexpr std::array<float,7> defaults={-6.f,0.f,9.25f,57.f,28.75f,29.75f,61.25f};
inline std::array<float,7> values=defaults;
inline std::atomic<bool> enabled{true},preview{false};
inline std::mutex mutex;
inline std::vector<std::pair<RE::NiPoint3,RE::NiPoint3>> wire;
inline auto Get(){std::scoped_lock lock(mutex);return values;}
inline void Set(const std::array<float,7>& value){std::scoped_lock lock(mutex);values=value;}
}
namespace EndpointConfig {
inline constexpr std::array<float,3> defaults={85.f,0.f,0.f}; // original camera-relative floating tip
inline constexpr std::array<float,3> handDefaults={10.f,0.f,0.f};
inline std::atomic<bool> hand{false};
inline std::array<float,3> values=defaults;
inline std::mutex mutex;
inline auto Get(){std::scoped_lock lock(mutex);return values;}
inline void Set(const std::array<float,3>& value){std::scoped_lock lock(mutex);values=value;}
}
namespace Translations {
inline std::atomic<unsigned> language{0};
inline constexpr std::array<const char*,10> codes={"EN","PTBR","RU","ZH","JA","FR","ES","PL","DE","IT"};
inline constexpr std::array<const char*,10> labels={"English (EN)","Português Brasileiro (PTBR)","Русский (RU)","简体中文 (ZH)","日本語 (JA)","Français (FR)","Español (ES)","Polski (PL)","Deutsch (DE)","Italiano (IT)"};
const char* Text(const char* en,const char* pt="");
}
namespace Controls {
inline std::atomic<unsigned> key{34};
inline std::atomic<bool> automaticReins{false};
inline unsigned ButtonCode(const RE::ButtonEvent* button) {
 if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard)return button->GetIDCode();
 if(button->GetDevice()==RE::INPUT_DEVICE::kGamepad)return SKSE::InputMap::GamepadMaskToKeycode(button->GetIDCode());
 return 0xffffffffu;
}
inline bool ValidKey(unsigned code) {return (code>0 && code<256) || (code>=266 && code<=281);}

inline std::atomic<float> grabProgress{-1.f};
inline std::atomic<bool> capture{false};
inline std::atomic<bool> disableNotifications{false};
inline void Notify(const char* text) {if(!disableNotifications.load()) RE::SendHUDMessage::ShowHUDMessage(text);}
void Load();
void Save();
void RegisterMenu();
bool Blocked();
}
namespace Reins {
bool BlockHorseCall(RE::InputEvent*);
bool Init();
void BeforeUpdate();
void Update(float dt);
void OnGameLoad();
void Save(SKSE::SerializationInterface*);
void Load(SKSE::SerializationInterface*);
void Revert();
}
