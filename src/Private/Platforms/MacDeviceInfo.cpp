#include "Platforms/Mac/MacDeviceInfo.h"

#ifdef __APPLE__

#include "GCore/Types/Structs/Config/GamepadCalibration.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/IOKitLib.h>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
	constexpr long kSonyVendorId = 0x054c;
	constexpr long kDualSenseProductId = 0x0ce6;
	constexpr long kDualSenseEdgeProductId = 0x0df2;
	constexpr long kDualShock4ProductId = 0x05c4;
	constexpr long kDualShock4SlimProductId = 0x09cc;

	IOHIDManagerRef CreateManager()
	{
		IOHIDManagerRef Manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
		if (!Manager)
		{
			return nullptr;
		}

		CFMutableDictionaryRef Matching = CFDictionaryCreateMutable(
			kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFNumberRef Vendor = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &kSonyVendorId);
		CFDictionarySetValue(Matching, CFSTR(kIOHIDVendorIDKey), Vendor);
		IOHIDManagerSetDeviceMatching(Manager, Matching);
		CFRelease(Vendor);
		CFRelease(Matching);
		if (IOHIDManagerOpen(Manager, kIOHIDOptionsTypeNone) != kIOReturnSuccess)
		{
			CFRelease(Manager);
			return nullptr;
		}
		return Manager;
	}

	bool NumberProperty(IOHIDDeviceRef Device, CFStringRef Key, long& Value)
	{
		CFTypeRef Property = IOHIDDeviceGetProperty(Device, Key);
		return Property && CFGetTypeID(Property) == CFNumberGetTypeID() &&
			CFNumberGetValue(static_cast<CFNumberRef>(Property), kCFNumberLongType, &Value);
	}

	bool IsSupported(IOHIDDeviceRef Device, long& ProductId)
	{
		long VendorId = 0;
		if (!NumberProperty(Device, CFSTR(kIOHIDVendorIDKey), VendorId) ||
			VendorId != kSonyVendorId ||
			!NumberProperty(Device, CFSTR(kIOHIDProductIDKey), ProductId))
		{
			return false;
		}

		return ProductId == kDualSenseProductId || ProductId == kDualSenseEdgeProductId ||
			ProductId == kDualShock4ProductId || ProductId == kDualShock4SlimProductId;
	}

	uint64_t LocationId(IOHIDDeviceRef Device)
	{
		long Location = 0;
		NumberProperty(Device, CFSTR(kIOHIDLocationIDKey), Location);
		return static_cast<uint64_t>(Location);
	}

	EDSDeviceType DeviceType(long ProductId)
	{
		if (ProductId == kDualShock4ProductId || ProductId == kDualShock4SlimProductId)
		{
			return EDSDeviceType::DualShock4;
		}
		if (ProductId == kDualSenseEdgeProductId)
		{
			return EDSDeviceType::DualSenseEdge;
		}
		return EDSDeviceType::DualSense;
	}

	bool IsBluetooth(IOHIDDeviceRef Device)
	{
		CFTypeRef Transport = IOHIDDeviceGetProperty(Device, CFSTR(kIOHIDTransportKey));
		if (!Transport || CFGetTypeID(Transport) != CFStringGetTypeID())
		{
			return false;
		}
		return CFStringCompare(static_cast<CFStringRef>(Transport), CFSTR("Bluetooth"),
			kCFCompareCaseInsensitive) == kCFCompareEqualTo;
	}

	CFSetRef Devices(IOHIDManagerRef Manager)
	{
		return IOHIDManagerCopyDevices(Manager);
	}
}

void FMacDeviceInfo::Detect(std::vector<FDeviceContext>& OutDevices)
{
	IOHIDManagerRef Manager = CreateManager();
	if (!Manager)
	{
		return;
	}

	CFSetRef DeviceSet = Devices(Manager);
	if (DeviceSet)
	{
		const CFIndex Count = CFSetGetCount(DeviceSet);
		std::vector<const void*> Values(static_cast<size_t>(Count));
		CFSetGetValues(DeviceSet, Values.data());
		for (const void* Value : Values)
		{
			auto Device = static_cast<IOHIDDeviceRef>(const_cast<void*>(Value));
			long ProductId = 0;
			if (!IsSupported(Device, ProductId))
			{
				continue;
			}

			FDeviceContext Context;
			Context.Path = std::to_string(LocationId(Device));
			Context.DeviceType = DeviceType(ProductId);
			Context.ConnectionType = IsBluetooth(Device) ? EDSDeviceConnection::Bluetooth : EDSDeviceConnection::Usb;
			Context.IsConnected = true;
			OutDevices.push_back(Context);
		}
		CFRelease(DeviceSet);
	}
	IOHIDManagerClose(Manager, kIOHIDOptionsTypeNone);
	CFRelease(Manager);
}

bool FMacDeviceInfo::CreateHandle(FDeviceContext* Context)
{
	if (!Context)
	{
		return false;
	}

	char* End = nullptr;
	const uint64_t ExpectedLocation = std::strtoull(Context->Path.c_str(), &End, 10);
	if (End == Context->Path.c_str() || *End != '\0')
	{
		return false;
	}

	IOHIDManagerRef Manager = CreateManager();
	if (!Manager)
	{
		return false;
	}

	CFSetRef DeviceSet = Devices(Manager);
	IOHIDDeviceRef Found = nullptr;
	if (DeviceSet)
	{
		const CFIndex Count = CFSetGetCount(DeviceSet);
		std::vector<const void*> Values(static_cast<size_t>(Count));
		CFSetGetValues(DeviceSet, Values.data());
		for (const void* Value : Values)
		{
			auto Device = static_cast<IOHIDDeviceRef>(const_cast<void*>(Value));
			long ProductId = 0;
			if (IsSupported(Device, ProductId) && LocationId(Device) == ExpectedLocation)
			{
				Found = Device;
				Context->DeviceType = DeviceType(ProductId);
				Context->ConnectionType = IsBluetooth(Device) ? EDSDeviceConnection::Bluetooth : EDSDeviceConnection::Usb;
				break;
			}
		}
	}

	bool Opened = Found && IOHIDDeviceOpen(Found, kIOHIDOptionsTypeNone) == kIOReturnSuccess;
	if (Opened)
	{
		CFRetain(Found);
		Context->Handle = Found;
		Context->IsConnected = true;
	}
	if (DeviceSet)
	{
		CFRelease(DeviceSet);
	}
	IOHIDManagerClose(Manager, kIOHIDOptionsTypeNone);
	CFRelease(Manager);
	return Opened;
}

void FMacDeviceInfo::Read(FDeviceContext* Context)
{
	if (!Context || !Context->IsConnected || Context->Handle == INVALID_PLATFORM_HANDLE)
	{
		return;
	}

	auto Device = static_cast<IOHIDDeviceRef>(Context->Handle);
	const bool IsBluetoothDualShock = Context->DeviceType == EDSDeviceType::DualShock4 &&
		Context->ConnectionType == EDSDeviceConnection::Bluetooth;
	CFIndex Length = IsBluetoothDualShock ? 547 : 64;
	unsigned char* Buffer = IsBluetoothDualShock ? Context->BufferDS4 : Context->Buffer;
	if (IOHIDDeviceGetReport(Device, kIOHIDReportTypeInput, 0, Buffer, &Length) != kIOReturnSuccess)
	{
		Context->IsConnected = false;
	}
}

void FMacDeviceInfo::Write(FDeviceContext* Context)
{
	if (!Context || Context->Handle == INVALID_PLATFORM_HANDLE)
	{
		return;
	}

	const CFIndex Length = Context->DeviceType == EDSDeviceType::DualShock4 ? 32 : 74;
	auto Device = static_cast<IOHIDDeviceRef>(Context->Handle);
	const uint8_t ReportId = Context->BufferOutput[0];
	if (IOHIDDeviceSetReport(Device, kIOHIDReportTypeOutput, ReportId, Context->BufferOutput + 1, Length - 1) !=
		kIOReturnSuccess)
	{
		Context->IsConnected = false;
	}
}

void FMacDeviceInfo::ProcessAudioHapitc(FDeviceContext* Context)
{
	Write(Context);
}

void FMacDeviceInfo::InvalidateHandle(FDeviceContext* Context)
{
	if (!Context || Context->Handle == INVALID_PLATFORM_HANDLE)
	{
		return;
	}

	auto Device = static_cast<IOHIDDeviceRef>(Context->Handle);
	IOHIDDeviceClose(Device, kIOHIDOptionsTypeNone);
	CFRelease(Device);
	Context->Handle = INVALID_PLATFORM_HANDLE;
	Context->IsConnected = false;
	Context->Path.clear();
}

#endif
