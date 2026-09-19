#pragma once

#include "GCore/Interfaces/IPlatformHardwareInfo.h"
#include "GCore/Types/Structs/Context/DeviceContext.h"

class FMacDeviceInfo
{
public:
	static void ProcessAudioHapitc(FDeviceContext* Context);
	static void Read(FDeviceContext* Context);
	static void Write(FDeviceContext* Context);
	static void Detect(std::vector<FDeviceContext>& Devices);
	static bool CreateHandle(FDeviceContext* Context);
	static void InvalidateHandle(FDeviceContext* Context);
};
