#include "Managers/DualSenseManager.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/color.hpp>
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
        UtilityFunctions::print("Shutdown triggers");
        gamepad->StopTrigger(
            static_cast<EDSGamepadHand>(GamepadDefs::GamepadHand::RIGHT_HAND)
        );
        gamepad->StopTrigger(
            static_cast<EDSGamepadHand>(GamepadDefs::GamepadHand::LEFT_HAND)
        );
    } else {
        UtilityFunctions::print("Not found trigger gamepad");
    }

    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(1)) {
        UtilityFunctions::print("Shutdown effects");
        gamepad->SetVibration(0, 0);
        gamepad->ResetLights();
    } else {
        UtilityFunctions::print("Not found gamepad");
    }

    FGodotDeviceRegistry::Shutdown();
}

void DualSenseManager::_bind_methods() {
	ClassDB::bind_static_method("DualSenseManager", D_METHOD("is_connected"), &DualSenseManager::is_connected);
	ClassDB::bind_static_method("DualSenseManager", D_METHOD("reset_trigger", "hand", "device_id"), &DualSenseManager::reset_trigger);
	ClassDB::bind_static_method("DualSenseManager", D_METHOD("reset_lights", "device_id"), &DualSenseManager::reset_lights);

	ClassDB::bind_static_method("DualSenseManager", D_METHOD("test_rumble"), &DualSenseManager::test_rumble);
	ClassDB::bind_static_method("DualSenseManager", D_METHOD("test_lightbar"), &DualSenseManager::test_lightbar);
	ClassDB::bind_static_method("DualSenseManager", D_METHOD("test_weapon"), &DualSenseManager::test_weapon);
	ClassDB::bind_static_method("DualSenseManager", D_METHOD("test_custom_trigger"), &DualSenseManager::test_custom_trigger);

    ClassDB::bind_static_method("DualSenseManager", D_METHOD("set_trigger_resistance", "hand", "start_zone", "strength", "device_id"), &DualSenseManager::set_trigger_resistance);
    ClassDB::bind_static_method("DualSenseManager", D_METHOD("set_trigger_weapon", "hand", "start_zone", "amplitude", "behavior", "trigger", "device_id"), &DualSenseManager::set_trigger_weapon);
    ClassDB::bind_static_method("DualSenseManager", D_METHOD("set_trigger_custom", "hand", "buffer", "device_id"), &DualSenseManager::set_trigger_custom);
	ClassDB::bind_static_method("DualSenseManager", D_METHOD("rumble", "left", "right", "duration", "device_id"), &DualSenseManager::rumble);
    ClassDB::bind_static_method("DualSenseManager", D_METHOD("set_lightbar", "color", "device_id"), &DualSenseManager::set_lightbar);
    ClassDB::bind_static_method("DualSenseManager", D_METHOD("set_lightbar_flash", "color", "brightness_time", "toggle_time", "device_id"), &DualSenseManager::set_lightbar_flash);
    ClassDB::bind_static_method("DualSenseManager", D_METHOD("set_player_led", "led", "brightness", "device_id"), &DualSenseManager::set_player_led);
    ClassDB::bind_static_method("DualSenseManager", D_METHOD("set_player_led_direct", "bitmask", "brightness", "device_id"), &DualSenseManager::set_player_led_direct);
    ClassDB::bind_static_method("DualSenseManager", D_METHOD("set_audio_haptic", "buffer", "device_id"), &DualSenseManager::set_audio_haptic);
    ClassDB::bind_static_method("DualSenseManager", D_METHOD("set_mic_settings", "mic_enabled", "mic_volume", "device_id"), &DualSenseManager::set_mic_settings);
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

void DualSenseManager::reset_lights(int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(device_id)) {
        UtilityFunctions::print("Lightbar reset...");
        gamepad->ResetLights();
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

void DualSenseManager::rumble(int left, int right, float duration, int device_id) {
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

void DualSenseManager::set_lightbar(Color color, int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(device_id)) {
        // Convert Godot Color (0.0-1.0 range) to byte values (0-255)
        std::uint8_t r = static_cast<std::uint8_t>(color.r * 255.0f);
        std::uint8_t g = static_cast<std::uint8_t>(color.g * 255.0f);
        std::uint8_t b = static_cast<std::uint8_t>(color.b * 255.0f);
        std::uint8_t a = static_cast<std::uint8_t>(color.a * 255.0f);
        gamepad->SetLightbar({r, g, b, a});
    } else {
        UtilityFunctions::print("Not found gamepad");
    }
}

void DualSenseManager::set_lightbar_flash(Color color, float brightness_time, float toggle_time, int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(device_id)) {
        // Convert Godot Color (0.0-1.0 range) to byte values (0-255)
        std::uint8_t r = static_cast<std::uint8_t>(color.r * 255.0f);
        std::uint8_t g = static_cast<std::uint8_t>(color.g * 255.0f);
        std::uint8_t b = static_cast<std::uint8_t>(color.b * 255.0f);
        std::uint8_t a = static_cast<std::uint8_t>(color.a * 255.0f);
        gamepad->SetLightbarFlash({r, g, b, a}, brightness_time, toggle_time);
    } else {
        UtilityFunctions::print("Not found gamepad");
    }
}

void DualSenseManager::set_player_led(int led, int brightness, int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(device_id)) {
        // Map simple LED values to EDSPlayer enum
        EDSPlayer player_led;
        switch(led) {
            case 0: player_led = EDSPlayer::Off; break;
            case 1: player_led = EDSPlayer::One; break;
            case 2: player_led = EDSPlayer::Two; break;
            case 3: player_led = EDSPlayer::Three; break;
            case 4: player_led = EDSPlayer::All; break;
            default: player_led = EDSPlayer::Off; break;
        }
        gamepad->SetPlayerLed(player_led, clamp_byte(brightness));
    } else {
        UtilityFunctions::print("Not found gamepad");
    }
}

void DualSenseManager::set_player_led_direct(int bitmask, int brightness, int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(device_id)) {
        // Use bitmask value directly for granular LED control
        // Bitmask values: 0x01=Left, 0x02=MiddleLeft, 0x04=Middle, 0x08=MiddleRight, 0x10=Right
        // Can combine: 0x01|0x04 for Left+Middle, etc.
        gamepad->SetPlayerLedBitmask(clamp_byte(bitmask), clamp_byte(brightness));
    } else {
        UtilityFunctions::print("Not found gamepad");
    }
}

void DualSenseManager::set_audio_haptic(PackedByteArray buffer, int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(device_id)) {
        // Convert PackedByteArray to std::vector, cap at 64 bytes
        size_t data_size = std::min(static_cast<size_t>(buffer.size()), static_cast<size_t>(64));
        std::vector<std::uint8_t> haptic_data(buffer.ptr(), buffer.ptr() + data_size);
        
        auto haptics = gamepad->GetIGamepadHaptics();
        if (haptics) {
            haptics->AudioHapticUpdate(haptic_data);
        } else {
            UtilityFunctions::print("Haptics interface not available");
        }
    } else {
        UtilityFunctions::print("Not found gamepad");
    }
}

void DualSenseManager::set_mic_settings(int mic_enabled, int mic_volume, int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(device_id)) {
        // Clamp values to valid ranges
        std::uint8_t mic_enabled_clamped = (mic_enabled != 0) ? 1 : 0;
        std::uint8_t mic_volume_clamped = clamp_byte(mic_volume);
        
        // Call DualSenseSettings with microphone parameters
        // (bIsMic, bIsHeadset, bIsSpeaker, MicVolume, AudioVolume, RumbleMode, RumbleReduce, TriggerReduce)
        gamepad->DualSenseSettings(mic_enabled_clamped, 1, 1, mic_volume_clamped, 200, 0, 0, 0);
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
