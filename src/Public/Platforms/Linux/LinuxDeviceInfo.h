#pragma once

#include "GCore/Types/Structs/Context/DeviceContext.h"

#include <cstdint>
#include <vector>

class FLinuxDeviceInfo
{
public:
	static void ProcessAudioHapitc(FDeviceContext* Context);
	static void ConfigureFeatures(FDeviceContext* Context);
	static void Read(FDeviceContext* Context);
	static void Write(FDeviceContext* Context);
	static void Detect(std::vector<FDeviceContext>& Devices);
	static bool CreateHandle(FDeviceContext* Context);
	static void InvalidateHandle(FDeviceContext* Context);

private:
	static int GetFileDescriptor(const FDeviceContext* Context);
	static bool IsDisconnectedError(int Error);
};
