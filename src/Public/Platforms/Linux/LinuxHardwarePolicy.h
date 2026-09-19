#pragma once

#include "Platforms/Linux/LinuxDeviceInfo.h"
#include "GCore/Templates/TGenericHardwareInfo.h"

namespace FLinuxPlatform
{
	struct FLinuxHardwarePolicy
	{
		void Read(FDeviceContext* Context) { FLinuxDeviceInfo::Read(Context); }
		void Write(FDeviceContext* Context) { FLinuxDeviceInfo::Write(Context); }
		void Detect(std::vector<FDeviceContext>& Devices) { FLinuxDeviceInfo::Detect(Devices); }
		bool CreateHandle(FDeviceContext* Context) { return FLinuxDeviceInfo::CreateHandle(Context); }
		void InvalidateHandle(FDeviceContext* Context) { FLinuxDeviceInfo::InvalidateHandle(Context); }
		void ProcessAudioHaptic(FDeviceContext* Context)
		{
			FLinuxDeviceInfo::ProcessAudioHapitc(Context);
		}
	};

	using FLinuxHardware = GamepadCore::TGenericHardwareInfo<FLinuxHardwarePolicy>;
}
