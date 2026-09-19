#pragma once

#include "Platforms/Mac/MacDeviceInfo.h"
#include "GCore/Templates/TGenericHardwareInfo.h"

namespace FMacPlatform
{
	struct FMacHardwarePolicy
	{
		void Read(FDeviceContext* Context) { FMacDeviceInfo::Read(Context); }
		void Write(FDeviceContext* Context) { FMacDeviceInfo::Write(Context); }
		void Detect(std::vector<FDeviceContext>& Devices) { FMacDeviceInfo::Detect(Devices); }
		bool CreateHandle(FDeviceContext* Context) { return FMacDeviceInfo::CreateHandle(Context); }
		void InvalidateHandle(FDeviceContext* Context) { FMacDeviceInfo::InvalidateHandle(Context); }
		void ProcessAudioHaptic(FDeviceContext* Context) { FMacDeviceInfo::ProcessAudioHapitc(Context); }
	};

	using FMacHardware = GamepadCore::TGenericHardwareInfo<FMacHardwarePolicy>;
}
