#include "Managers/DualSenseManager.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>
#include "Adapter/GodotDeviceRegistry.h"
#include "API/GamepadDefs.h"
#include "Platforms/Windows/WindowsHardwarePolicy.h"
#include "GCore/Interfaces/IPlatformHardwareInfo.h"

using namespace godot;

DualSenseManager *DualSenseManager::singleton = nullptr;
float DualSenseManager::vibration_remaining_duration = 0.0f;
int DualSenseManager::vibration_left = 0;
int DualSenseManager::vibration_right = 0;
int DualSenseManager::vibration_device_id = 1;

DualSenseManager::DualSenseManager() {
    singleton = this;
}

DualSenseManager::~DualSenseManager() {
    if (singleton == this) {
        singleton = nullptr;
    }
}

void DualSenseManager::_ready() {
    UtilityFunctions::print("[DualSenseManager] Initialize GamepadCore...");

    // 1.Hardware (Windows)
#ifdef _WIN32
    std::unique_ptr<IPlatformHardwareInfo> WindowsInstance = std::make_unique<FWindowsPlatform::FWindowsHardware>();
    IPlatformHardwareInfo::SetInstance(std::move(WindowsInstance));
#endif

    FGodotDeviceRegistry::Initialize();
    UtilityFunctions::print("[DualSenseManager] GamepadCore initialized!");
}

void DualSenseManager::_process(double delta) {
    FGodotDeviceRegistry::DiscoverDevices(delta);
    
    // Handle vibration timing
    if (vibration_remaining_duration > 0.0f) {
        vibration_remaining_duration -= static_cast<float>(delta);
        if (vibration_remaining_duration <= 0.0f) {
            // Stop vibration when duration expires
            if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(vibration_device_id)) {
                gamepad->SetVibration(0, 0);
            }
            vibration_remaining_duration = 0.0f;
        }
    }
}

void DualSenseManager::_exit_tree() {
    if (const auto gamepad = FGodotDeviceRegistry::GetTriggerGamepad(1)) {
        UtilityFunctions::print("Shutdown effects");
        gamepad->StopTrigger(
            static_cast<EDSGamepadHand>(GamepadDefs::GamepadHand::RIGHT_HAND)
        );
        gamepad->StopTrigger(
            static_cast<EDSGamepadHand>(GamepadDefs::GamepadHand::LEFT_HAND)
        );
    } else {
        UtilityFunctions::print("Not found gamepad");
    }

    FGodotDeviceRegistry::Shutdown();
}

void DualSenseManager::_bind_methods() {
	ClassDB::bind_static_method("DualSenseManager", D_METHOD("is_connected"), &DualSenseManager::is_connected);
	ClassDB::bind_static_method("DualSenseManager", D_METHOD("reset_trigger", "hand", "device_id"), &DualSenseManager::reset_trigger);
    
	ClassDB::bind_static_method("DualSenseManager", D_METHOD("test_rumble"), &DualSenseManager::test_rumble);
	ClassDB::bind_static_method("DualSenseManager", D_METHOD("test_lightbar"), &DualSenseManager::test_lightbar);
	ClassDB::bind_static_method("DualSenseManager", D_METHOD("test_weapon"), &DualSenseManager::test_weapon);
	ClassDB::bind_static_method("DualSenseManager", D_METHOD("test_custom_trigger"), &DualSenseManager::test_custom_trigger);

    ClassDB::bind_static_method("DualSenseManager", D_METHOD("set_trigger_resistance", "hand", "start_zone", "strength", "device_id"), &DualSenseManager::set_trigger_resistance);
    ClassDB::bind_static_method("DualSenseManager", D_METHOD("set_trigger_weapon", "hand", "start_zone", "amplitude", "behavior", "trigger", "device_id"), &DualSenseManager::set_trigger_weapon);
    ClassDB::bind_static_method("DualSenseManager", D_METHOD("set_trigger_custom", "hand", "buffer", "device_id"), &DualSenseManager::set_trigger_custom);
	ClassDB::bind_static_method("DualSenseManager", D_METHOD("rumble", "left", "right", "duration", "device_id"), &DualSenseManager::rumble);
}

bool DualSenseManager::is_connected() {
    return FGodotDeviceRegistry::GetGamepad(1) != nullptr;
}

static std::uint8_t clamp_byte(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return static_cast<std::uint8_t>(v);
}

static EDSGamepadHand to_hand(int hand) {
    return hand == 0 ? EDSGamepadHand::Left : EDSGamepadHand::Right;
}

void DualSenseManager::reset_trigger(int hand, int device_id = 1) {
    if (const auto gamepad = FGodotDeviceRegistry::GetTriggerGamepad(device_id)) {
        UtilityFunctions::print("Trigger reset...");
        gamepad->StopTrigger(
            static_cast<EDSGamepadHand>(to_hand(hand))
        );
    } else {
        UtilityFunctions::print("Not found gamepad");
    }
}

void DualSenseManager::set_trigger_resistance(int hand, int start_zone, int strength, int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetTriggerGamepad(device_id)) {
        gamepad->SetResistance(clamp_byte(start_zone), clamp_byte(strength), to_hand(hand));
    }
    else {
        UtilityFunctions::print("Not found gamepad");
    }
}

void DualSenseManager::set_trigger_weapon(int hand, int start_zone, int amplitude, int behavior, int trigger, int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetTriggerGamepad(device_id)) {
        gamepad->SetWeapon25(clamp_byte(start_zone), clamp_byte(amplitude), clamp_byte(behavior), clamp_byte(trigger), to_hand(hand));
    }
    else {
        UtilityFunctions::print("Not found gamepad");
    }
}

void DualSenseManager::set_trigger_custom(int hand, PackedByteArray buffer, int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetTriggerGamepad(device_id)) {
        // Convert PackedByteArray to std::vector for internal API
        std::vector<std::uint8_t> vec(buffer.ptr(), buffer.ptr() + buffer.size());
        gamepad->SetCustomTrigger(to_hand(hand), vec);
    }
    else {
        UtilityFunctions::print("Not found gamepad");
    }
}

void DualSenseManager::rumble(int left, int right, float duration, int device_id = 1) {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(device_id)) {
        gamepad->SetVibration(clamp_byte(left), clamp_byte(right));
        // Store vibration state for timing
        vibration_left = clamp_byte(left);
        vibration_right = clamp_byte(right);
        vibration_device_id = device_id;
        vibration_remaining_duration = (duration > 0.0f) ? duration : 0.0f;
    } else {
        UtilityFunctions::print("Not found gamepad");
    }
}

void DualSenseManager::test_rumble() {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(1)) {
        UtilityFunctions::print("test_rumble vibration...");
        gamepad->SetVibration(255, 255);
    } else {
        UtilityFunctions::print("Not found gamepad");
    }
}

void DualSenseManager::test_lightbar() {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(1)) {
        gamepad->SetLightbar({255, 0, 0, 0});
    } else {
        UtilityFunctions::print("Not found gamepad");
    }
}

void DualSenseManager::test_weapon() {
    if (const auto gamepad = FGodotDeviceRegistry::GetTriggerGamepad(1)) {
        UtilityFunctions::print("test_weapon (weapon 0x25) effect...");
        gamepad->SetWeapon25(
            GamepadDefs::POS_START,
            GamepadDefs::AMP_HIGH,
            GamepadDefs::SUSTAINED,
            GamepadDefs::TriggerForceMask::MASK_FORCE_HIGH,
            static_cast<EDSGamepadHand>(GamepadDefs::GamepadHand::RIGHT_HAND)
        );
    } else {
        UtilityFunctions::print("Not found gamepad");
    }
}
void DualSenseManager::test_custom_trigger() {
    if (const auto gamepad = FGodotDeviceRegistry::GetTriggerGamepad(1)) {
        UtilityFunctions::print("test_trigger_custom (machine 0x27) effect...");
        // machine effects...
        // 27 02 02 3a 0a 05
        // 27 40 01 3a 0a 05
        // 27 80 02 32 19 02
        std::vector<std::uint8_t> Buffer = {0};
        Buffer.resize(10);
        Buffer[0] = 0x27;
        Buffer[1] = 0x80;
        Buffer[2] = 0x02;
        Buffer[3] = 0x32;
        Buffer[4] = 0x19;
        Buffer[5] = 0x02;
        Buffer[6] = 0;
        Buffer[7] = 0;
        Buffer[8] = 0;
        Buffer[9] = 0;
        gamepad->SetCustomTrigger(EDSGamepadHand::Left, Buffer);
    } else {
        UtilityFunctions::print("Not found gamepad");
    }
}
