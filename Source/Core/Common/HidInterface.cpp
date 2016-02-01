// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <hidapi.h>

#include "Common/Logging/Log.h"
#include "Common/HidInterface.h"

namespace HidInterface
{
HidDeviceInfo::HidDeviceInfo(std::string path, std::wstring manufacturer, std::wstring product)
	: m_path(path), m_manufacturer(manufacturer), m_product(product)
{
}

class HidDeviceInfo_hidapi : public HidDeviceInfo
{
public:
	HidDeviceInfo_hidapi(std::string path, std::wstring manufacturer, std::wstring product)
		: HidDeviceInfo(path, manufacturer, product), m_handle(nullptr)
	{}

	bool IsOpen() override
	{
		return !!m_handle;
	}
	hid_device* m_handle;
};

template<class... types>
std::tuple<types...> threaded_arguments;


std::vector<std::unique_ptr<HidDeviceInfo>> Enumerate(u16 vendor, u16 product)
{
	hid_device_info* list = hid_enumerate(vendor, product);
	hid_device_info* item = list;
	std::vector<std::unique_ptr<HidDeviceInfo>> internal_devices;
	while (item)
	{
		internal_devices.emplace_back
			(std::make_unique<HidDeviceInfo_hidapi>(
				item->path,
				item->manufacturer_string,
				item->product_string
			));

		item = item->next;
	}
	hid_free_enumeration(list);
	return internal_devices;
}

bool Open(HidDeviceInfo* device)
{
	HidDeviceInfo_hidapi* dev = static_cast<HidDeviceInfo_hidapi*>(device);
	dev->m_handle = hid_open_path(dev->m_path.c_str());
	return !!dev->m_handle;
}
void Close(HidDeviceInfo* device)
{
	HidDeviceInfo_hidapi* dev = static_cast<HidDeviceInfo_hidapi*>(device);
	hid_close(dev->m_handle);
}

s32 Read(HidDeviceInfo* device, u8* data, size_t length, int timeout_ms)
{
	HidDeviceInfo_hidapi* dev = static_cast<HidDeviceInfo_hidapi*>(device);
	return hid_read_timeout(dev->m_handle, data, length, timeout_ms);
}

s32 Write(HidDeviceInfo* device, const u8* data, size_t length)
{
	HidDeviceInfo_hidapi* dev = static_cast<HidDeviceInfo_hidapi*>(device);
	return hid_write(dev->m_handle, data, length);
}

void Init()
{
	if (hid_init() == -1)
	{
		ERROR_LOG(COMMON, "Failed to initialize hidapi.");
	}
}

void Shutdown()
{
	if (hid_exit() == -1)
	{
		ERROR_LOG(COMMON, "Failed to clean up hidapi.");
	}
}
}


