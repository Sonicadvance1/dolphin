// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifdef ANDROID
#include <jni.h>
#endif

#include <memory>
#include <vector>

namespace HidInterface
{
	class HidDeviceInfo
	{
	public:
		HidDeviceInfo() = delete;
		virtual bool IsOpen() = 0;

		std::string m_path;
		std::wstring m_manufacturer;
		std::wstring m_product;

	protected:
		HidDeviceInfo(std::string path, std::wstring manufacturer, std::wstring product);
	};

	std::vector<std::unique_ptr<HidDeviceInfo>> Enumerate(u16 vendor, u16 product);
	bool Open(HidDeviceInfo* device);
	void Close(HidDeviceInfo* device);

	s32 Read(HidDeviceInfo* device, u8* data, size_t length, int timeout_ms);
	s32 Write(HidDeviceInfo* device, const u8* data, size_t length);

	void Init();
	void Shutdown();

#ifdef ANDROID
	void InitHandlerClass(JNIEnv* env);
#endif
}
