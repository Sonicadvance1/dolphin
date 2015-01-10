// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Assert.h"
#include "Common/Common.h"
#include "Common/HidInterface.h"
#include "Common/Logging/Log.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

namespace WiimoteReal
{

class WiimoteHidapi final : public Wiimote
{
public:
	WiimoteHidapi(HidInterface::HidDeviceInfo* device);
	~WiimoteHidapi() override;

protected:
	bool ConnectInternal() override;
	void DisconnectInternal() override;
	bool IsConnected() const override;
	void IOWakeup() override;
	int IORead(u8* buf) override;
	int IOWrite(u8 const* buf, size_t len) override;

private:
	HidInterface::HidDeviceInfo* m_device;
};

WiimoteScanner::WiimoteScanner()
	: m_want_wiimotes()
{
	HidInterface::Init();
}

bool WiimoteScanner::IsReady() const
{
	return true;
}

WiimoteScanner::~WiimoteScanner()
{
	HidInterface::Shutdown();
}

void WiimoteScanner::Update()
{}

void WiimoteScanner::FindWiimotes(std::vector<Wiimote*>& found_wiimotes, Wiimote*& found_board)
{
	// Search for both old and new Wiimotes.
	for (uint16_t product_id : {0x0306, 0x0330})
	{
		const auto& list = HidInterface::Enumerate(0x057e, product_id);
		for (const auto& item : list)
		{
			NOTICE_LOG(WIIMOTE, "Found Wiimote at %s: %ls %ls", item->m_path.c_str(), item->m_manufacturer.c_str(), item->m_product.c_str());
			Wiimote* wiimote = new WiimoteHidapi(item.get());
			found_wiimotes.push_back(wiimote);
		}
	}
}

WiimoteHidapi::WiimoteHidapi(HidInterface::HidDeviceInfo* device) : m_device(device)
{}

WiimoteHidapi::~WiimoteHidapi()
{
	Shutdown();
}

// Connect to a wiimote with a known address.
bool WiimoteHidapi::ConnectInternal()
{
	bool result = HidInterface::Open(m_device);
	if (!result)
	{
		ERROR_LOG(WIIMOTE, "Could not connect to Wiimote at \"%s\". "
		                   "Do you have permission to access the device?", m_device->m_path.c_str());
	}

	return result;
}

void WiimoteHidapi::DisconnectInternal()
{
	HidInterface::Close(m_device);
}

bool WiimoteHidapi::IsConnected() const
{
	return m_device->IsOpen();
}

void WiimoteHidapi::IOWakeup()
{}

// positive = read packet
// negative = didn't read packet
// zero = error
int WiimoteHidapi::IORead(u8* buf)
{
	int timeout = 200; // ms
	int result = HidInterface::Read(m_device, buf + 1, MAX_PAYLOAD - 1, timeout);
	// TODO: If and once we use hidapi across plaforms, change our internal API to clean up this mess.
	if (result == -1)
	{
		ERROR_LOG(WIIMOTE, "Failed to read from %s.", m_device->m_path.c_str());
		result = 0;
	}
	else if (result == 0)
	{
		result = -1;
	}
	else
	{
		buf[0] = WM_SET_REPORT | WM_BT_INPUT;
		result += 1;
	}
	return result;
}

int WiimoteHidapi::IOWrite(u8 const* buf, size_t len)
{
	_dbg_assert_(WIIMOTE, buf[0] == (WM_SET_REPORT | WM_BT_OUTPUT));
	int result = HidInterface::Write(m_device, buf + 1, len - 1);
	if (result == -1)
	{
		ERROR_LOG(WIIMOTE, "Failed to write to %s.", m_device->m_path.c_str());
		result = 0;
	}
	else
	{
		result += 1;
	}
	return result;
}

}; // WiimoteReal
