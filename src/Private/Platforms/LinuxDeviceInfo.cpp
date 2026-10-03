#include "Platforms/Linux/LinuxDeviceInfo.h"

#ifdef __linux__

#include "GCore/Types/Structs/Config/GamepadCalibration.h"
#include "GImplementations/Utils/GamepadSensors.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <linux/hidraw.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

namespace
{
	constexpr unsigned short SonyVendorId = 0x054c;
	constexpr unsigned short DualShock4ProductId = 0x05c4;
	constexpr unsigned short DualShock4SlimProductId = 0x09cc;
	constexpr unsigned short DualSenseProductId = 0x0ce6;
	constexpr unsigned short DualSenseEdgeProductId = 0x0df2;

	int HandleToFd(FPlatformDeviceHandle Handle)
	{
		return static_cast<int>(reinterpret_cast<std::intptr_t>(Handle));
	}

	FPlatformDeviceHandle FdToHandle(int FileDescriptor)
	{
		return reinterpret_cast<FPlatformDeviceHandle>(static_cast<std::intptr_t>(FileDescriptor));
	}

	EDSDeviceType DeviceType(unsigned short ProductId)
	{
		if (ProductId == DualShock4ProductId || ProductId == DualShock4SlimProductId)
		{
			return EDSDeviceType::DualShock4;
		}
		if (ProductId == DualSenseEdgeProductId)
		{
			return EDSDeviceType::DualSenseEdge;
		}
		return EDSDeviceType::DualSense;
	}

	bool IsSupportedProduct(unsigned short ProductId)
	{
		return ProductId == DualShock4ProductId || ProductId == DualShock4SlimProductId ||
			ProductId == DualSenseProductId || ProductId == DualSenseEdgeProductId;
	}

	bool IsBluetoothPath(const std::filesystem::path& DevicePath)
	{
		std::error_code Error;
		const std::filesystem::path SysfsPath =
			std::filesystem::read_symlink("/sys/class/hidraw/" + DevicePath.filename().string(), Error);
		if (Error)
		{
			return false;
		}
		const std::string Path = SysfsPath.string();
		return Path.find("/bluetooth/") != std::string::npos ||
			Path.find("/hidp/") != std::string::npos;
	}

	void ClearContext(FDeviceContext* Context)
	{
		Context->Handle = INVALID_PLATFORM_HANDLE;
		Context->IsConnected = false;
		Context->Path.clear();
		std::memset(Context->Buffer, 0, sizeof(Context->Buffer));
		std::memset(Context->BufferDS4, 0, sizeof(Context->BufferDS4));
		std::memset(Context->BufferOutput, 0, sizeof(Context->BufferOutput));
		std::memset(Context->BufferAudio, 0, sizeof(Context->BufferAudio));
	}
}

int FLinuxDeviceInfo::GetFileDescriptor(const FDeviceContext* Context)
{
	if (!Context || Context->Handle == INVALID_PLATFORM_HANDLE)
	{
		return -1;
	}
	return HandleToFd(Context->Handle);
}

bool FLinuxDeviceInfo::IsDisconnectedError(int Error)
{
	return Error == EBADF || Error == ENODEV || Error == ENXIO || Error == EIO;
}

void FLinuxDeviceInfo::Detect(std::vector<FDeviceContext>& Devices)
{
	Devices.clear();
	std::error_code Error;
	for (const auto& Entry : std::filesystem::directory_iterator("/dev", Error))
	{
		if (Entry.path().filename().string().rfind("hidraw", 0) != 0)
		{
			continue;
		}

		const int FileDescriptor = open(Entry.path().c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
		if (FileDescriptor < 0)
		{
			continue;
		}

		hidraw_devinfo Info{};
		const bool HasInfo = ioctl(FileDescriptor, HIDIOCGRAWINFO, &Info) >= 0;
		if (HasInfo && Info.vendor == SonyVendorId && IsSupportedProduct(Info.product))
		{
			FDeviceContext Context;
			Context.Path = Entry.path().string();
			Context.DeviceType = DeviceType(Info.product);
			Context.ConnectionType = IsBluetoothPath(Entry.path())
				? EDSDeviceConnection::Bluetooth
				: EDSDeviceConnection::Usb;
			Context.IsConnected = true;
			Devices.push_back(Context);
		}
		close(FileDescriptor);
	}
}

bool FLinuxDeviceInfo::CreateHandle(FDeviceContext* Context)
{
	if (!Context || Context->Path.empty())
	{
		return false;
	}

	const int FileDescriptor = open(Context->Path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
	if (FileDescriptor < 0)
	{
		return false;
	}

	Context->Handle = FdToHandle(FileDescriptor);
	Context->IsConnected = true;
	ConfigureFeatures(Context);
	return true;
}

void FLinuxDeviceInfo::InvalidateHandle(FDeviceContext* Context)
{
	if (!Context)
	{
		return;
	}

	const int FileDescriptor = GetFileDescriptor(Context);
	if (FileDescriptor >= 0)
	{
		close(FileDescriptor);
	}
	ClearContext(Context);
}

void FLinuxDeviceInfo::Read(FDeviceContext* Context)
{
	if (!Context || !Context->IsConnected || Context->Handle == INVALID_PLATFORM_HANDLE)
	{
		return;
	}

	const int FileDescriptor = GetFileDescriptor(Context);
	unsigned char* Buffer = Context->Buffer;
	size_t Length = Context->ConnectionType == EDSDeviceConnection::Bluetooth ? 78 : 64;
	if (Context->ConnectionType == EDSDeviceConnection::Bluetooth &&
		Context->DeviceType == EDSDeviceType::DualShock4)
	{
		Buffer = Context->BufferDS4;
		Length = 547;
	}

	const ssize_t BytesRead = read(FileDescriptor, Buffer, Length);
	if (BytesRead < 0 && IsDisconnectedError(errno))
	{
		InvalidateHandle(Context);
	}
}

void FLinuxDeviceInfo::Write(FDeviceContext* Context)
{
	if (!Context || !Context->IsConnected || Context->Handle == INVALID_PLATFORM_HANDLE)
	{
		return;
	}

	const size_t InputReportLength = Context->DeviceType == EDSDeviceType::DualShock4 ? 32 : 74;
	const size_t OutputReportLength =
		Context->ConnectionType == EDSDeviceConnection::Bluetooth ? 78 : InputReportLength;
	if (write(GetFileDescriptor(Context), Context->BufferOutput, OutputReportLength) < 0 &&
		IsDisconnectedError(errno))
	{
		InvalidateHandle(Context);
	}
}

void FLinuxDeviceInfo::ProcessAudioHapitc(FDeviceContext* Context)
{
	if (!Context || !Context->IsConnected || Context->Handle == INVALID_PLATFORM_HANDLE ||
		Context->ConnectionType != EDSDeviceConnection::Bluetooth)
	{
		return;
	}

	if (write(GetFileDescriptor(Context), Context->BufferAudio, 142) < 0 && IsDisconnectedError(errno))
	{
		InvalidateHandle(Context);
	}
}

void FLinuxDeviceInfo::ConfigureFeatures(FDeviceContext* Context)
{
	if (!Context || Context->Handle == INVALID_PLATFORM_HANDLE)
	{
		return;
	}

	unsigned char FeatureBuffer[41] = {};
	FeatureBuffer[0] = 0x05;
	if (ioctl(GetFileDescriptor(Context), HIDIOCGFEATURE(sizeof(FeatureBuffer)), FeatureBuffer) < 0)
	{
		return;
	}

	FGamepadCalibration Calibration;
	FGamepadSensors::DualSenseCalibrationSensors(FeatureBuffer, Calibration);
	Context->Calibration = Calibration;
}

#endif
