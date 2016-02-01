// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <tuple>
#include <jni.h>
#include <vector>

#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/HidInterface.h"
#include "Common/Timer.h"

// Global java_vm class
extern JavaVM* g_java_vm;

namespace HidInterface
{
static std::string GetJString(JNIEnv *env, jstring jstr)
{
	std::string result = "";
	if (!jstr)
		return result;

	const char *s = env->GetStringUTFChars(jstr, nullptr);
	result = s;
	env->ReleaseStringUTFChars(jstr, s);
	return result;
}
enum class Method
{
	// HID
	HID_Enumerate,
	HID_OpenDevice,

	// Java_List
	Java_List_Size,
	Java_List_Get,

	// USBDevice
	USBDevice_GetProductId,
	USBDevice_GetProductName,
	USBDevice_ToString,
	USBDevice_GetDeviceName,
	USBDevice_GetManufacturerName,
	USBDevice_GetVendorId,
	USBDevice_GetInterfaceCount,
	USBDevice_GetInterface,
	USBDevice_GetConfiguration,
	USBDevice_GetConfigurationCount,
	USBDevice_GetDeviceClass,
	USBDevice_GetDeviceId,
	USBDevice_GetDeviceProtocol,
	USBDevice_GetDeviceSubclass,

	// USBConnection
	USBConnection_BulkTransfer,
	USBConnection_Close,
	USBConnection_ControlTransfer,
	USBConnection_GetFileDescriptor,
	USBConnection_ClaimInterface,
	USBConnection_GetRawDescriptors,
	USBConnection_GetSerial,
	USBConnection_ReleaseInterface,
	USBConnection_SetConfiguration,
	USBConnection_SetInterface,

	// USBConfiguration
	USBConfiguration_GetInterfaceCount,
	USBConfiguration_GetInterface,
	USBConfiguration_ToString,
	USBConfiguration_GetId,
	USBConfiguration_GetMaxPower,
	USBConfiguration_GetName,
	USBConfiguration_IsRemoteWakeup,
	USBConfiguration_IsSelfPowered,

	// USBInterface
	USBInterface_GetEndpoint,
	USBInterface_GetEndpointCount,
	USBInterface_ToString,
	USBInterface_GetAlternateSetting,
	USBInterface_GetId,
	USBInterface_GetInterfaceClass,
	USBInterface_GetInterfaceProtocol,
	USBInterface_GetInterfaceSubclass,
	USBInterface_GetName,

	// USBEndpoint
	USBEndpoint_GetAddress,
	USBEndpoint_GetDirection,
	USBEndpoint_ToString,
	USBEndpoint_GetAttributes,
	USBEndpoint_GetEndpointNumber,
	USBEndpoint_GetInterval,
	USBEndpoint_GetMaxPacketSize,
	USBEndpoint_GetType,
};

class MethodArgsVoid
{
public:
	MethodArgsVoid(Method method_id)
		: m_id(method_id)
	{}

	Method GetId() { return m_id; }

private:
	Method m_id;
};

class MethodArgsJObject : public MethodArgsVoid
{
public:
	MethodArgsJObject(Method method_id, jobject arg0)
		: MethodArgsVoid(method_id), m_arg0(arg0)
	{}

	jobject GetArg() { return m_arg0; }

private:
	jobject m_arg0;
};


class MethodArgsI : public MethodArgsVoid
{
public:
	MethodArgsI(Method method_id, int arg0)
		: MethodArgsVoid(method_id), m_arg0(arg0)
	{}

	int GetArg() { return m_arg0; }

private:
	int m_arg0;
};

class MethodArgsII : public MethodArgsVoid
{
public:
	MethodArgsII(Method method_id, int arg0, int arg1)
		: MethodArgsVoid(method_id), m_arg0(arg0), m_arg1(arg1)
	{}

	int GetArg0() { return m_arg0; }
	int GetArg1() { return m_arg1; }

private:
	int m_arg0;
	int m_arg1;
};

class MethodArgsPB : public MethodArgsVoid
{
public:
	MethodArgsPB(Method method_id, void* arg0, bool arg1)
		: MethodArgsVoid(method_id), m_arg0(arg0), m_arg1(arg1)
	{}

	void* GetArg0() { return m_arg0; }
	bool GetArg1() { return m_arg1; }

private:
	void* m_arg0;
	bool m_arg1;
};

class MethodArgsPPII : public MethodArgsVoid
{
public:
	MethodArgsPPII(Method method_id, void* arg0, void* arg1, int arg2, int arg3)
		: MethodArgsVoid(method_id), m_arg0(arg0), m_arg1(arg1), m_arg2(arg2), m_arg3(arg3)
	{}

	void* GetArg0() { return m_arg0; }
	void* GetArg1() { return m_arg1; }
	int GetArg2() { return m_arg2; }
	int GetArg3() { return m_arg3; }

private:
	void* m_arg0;
	void* m_arg1;
	int m_arg2;
	int m_arg3;
};

class MethodArgsIIIIPII : public MethodArgsVoid
{
public:
	MethodArgsIIIIPII(Method method_id, int arg0, int arg1, int arg2, int arg3, void* arg4, int arg5, int arg6)
		: MethodArgsVoid(method_id),
		  m_arg0(arg0), m_arg1(arg1), m_arg2(arg2), m_arg3(arg3),
		  m_arg4(arg4), m_arg5(arg5), m_arg6(arg6)
	{}

	int GetArg0() { return m_arg0; }
	int GetArg1() { return m_arg1; }
	int GetArg2() { return m_arg2; }
	int GetArg3() { return m_arg3; }
	void* GetArg4() { return m_arg4; }
	int GetArg5() { return m_arg5; }
	int GetArg6() { return m_arg6; }



private:
	int m_arg0;
	int m_arg1;
	int m_arg2;
	int m_arg3;
	void* m_arg4;
	int m_arg5;
	int m_arg6;
};

class MethodRetVoid
{
public:
	MethodRetVoid() {}
};

class MethodRetInt : public MethodRetVoid
{
public:
	MethodRetInt(int ret)
		: m_ret(ret)
	{}

	int GetRet() { return m_ret; }

private:
	int m_ret;
};

class MethodRetBool : public MethodRetVoid
{
public:
	MethodRetBool(bool ret)
		: m_ret(ret)
	{}

	bool GetRet() { return m_ret; }

private:
	bool m_ret;
};

class MethodRetJObject : public MethodRetVoid
{
public:
	MethodRetJObject(jobject ret)
		: m_ret(ret)
	{}

	jobject GetRet() { return m_ret; }

private:
	jobject m_ret;
};

class MethodRetString : public MethodRetVoid
{
public:
	MethodRetString(std::string ret)
		: m_ret(ret)
	{}

	std::string GetRet() { return m_ret; }

private:
	std::string m_ret;
};

template<class T>
class MethodRetUniquePtr : public MethodRetVoid
{
public:
	MethodRetUniquePtr(std::unique_ptr<T> ret)
		: m_ret(std::move(ret))
	{}

	std::unique_ptr<T> GetRet() { return std::move(m_ret); }

private:
	std::unique_ptr<T> m_ret;
};

class Java_Object
{
public:
	Java_Object(JNIEnv* env, jobject raw)
		: m_env(env), m_raw(raw)
	{}

	JNIEnv* GetEnv() { return m_env; }
	jobject GetRaw() { return m_raw; }

private:
	JNIEnv* m_env;
	jobject m_raw;
};

// Handler variables
static Common::Event s_has_method;
static Common::Event s_handled_method;
static std::pair<Java_Object*, std::unique_ptr<MethodArgsVoid>> s_handler_args;
static std::unique_ptr<MethodRetVoid> s_handler_ret;
static std::mutex s_handler_mutex;

// Java classes
static jclass s_handler_class;
static jmethodID s_handler_enumerate;
static jmethodID s_handler_open;

// List class
static jclass s_list_class;
static jmethodID s_list_get;
static jmethodID s_list_size;

// USBEndpoint class
static jclass s_usbendpoint_class;
static jmethodID s_usbendpoint_getAddress;
static jmethodID s_usbendpoint_getDirection;
static jmethodID s_usbendpoint_toString;
static jmethodID s_usbendpoint_getAttributes;
static jmethodID s_usbendpoint_getEndpointNumber;
static jmethodID s_usbendpoint_getInterval;
static jmethodID s_usbendpoint_getMaxPacketSize;
static jmethodID s_usbendpoint_getType;

// USBInterface class
static jclass s_usbinterface_class;
static jmethodID s_usbinterface_getEndpoint;
static jmethodID s_usbinterface_getEndpointCount;
static jmethodID s_usbinterface_toString;
static jmethodID s_usbinterface_getAlternateSetting;
static jmethodID s_usbinterface_getId;
static jmethodID s_usbinterface_getInterfaceClass;
static jmethodID s_usbinterface_getInterfaceProtocol;
static jmethodID s_usbinterface_getInterfaceSubclass;
static jmethodID s_usbinterface_getName;

// USBConfiguration class
static jclass s_usbconfiguration_class;
static jmethodID s_usbconfiguration_getInterfaceCount;
static jmethodID s_usbconfiguration_getInterface;
static jmethodID s_usbconfiguration_toString;
static jmethodID s_usbconfiguration_getId;
static jmethodID s_usbconfiguration_getMaxPower;
static jmethodID s_usbconfiguration_getName;
static jmethodID s_usbconfiguration_isRemoteWakeup;
static jmethodID s_usbconfiguration_isSelfPowered;

// USBConnection class
static jclass s_usbconnection_class;
static jmethodID s_usbconnection_bulkTransfer;
static jmethodID s_usbconnection_close;
static jmethodID s_usbconnection_controlTransfer;
static jmethodID s_usbconnection_getFileDescriptor;
static jmethodID s_usbconnection_claimInterface;
static jmethodID s_usbconnection_getRawDescriptors;
static jmethodID s_usbconnection_getSerial;
static jmethodID s_usbconnection_releaseInterface;
// XXX: static jmethodID s_usbconnection_requestWait;
static jmethodID s_usbconnection_setConfiguration;
static jmethodID s_usbconnection_setInterface;

// USBDevice class
static jclass s_usbdevice_class;
static jmethodID s_usbdevice_getProductId;
static jmethodID s_usbdevice_getProductName;
static jmethodID s_usbdevice_toString;
static jmethodID s_usbdevice_getDeviceName;
static jmethodID s_usbdevice_getManufacturerName;
static jmethodID s_usbdevice_getVendorId;
static jmethodID s_usbdevice_getInterfaceCount;
static jmethodID s_usbdevice_getInterface;
static jmethodID s_usbdevice_getConfiguration;
static jmethodID s_usbdevice_getConfigurationCount;
static jmethodID s_usbdevice_getDeviceClass;
static jmethodID s_usbdevice_getDeviceId;
static jmethodID s_usbdevice_getDeviceProtocol;
static jmethodID s_usbdevice_getDeviceSubclass;

class Java_List : public Java_Object
{
public:
	Java_List(JNIEnv* env, jobject raw)
		: Java_Object(env, raw)
	{}

	jobject Get(int index)
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsI>(Method::Java_List_Get, index);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetJObject* ret = static_cast<MethodRetJObject*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int Size()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::Java_List_Size);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	jobject Get_Impl(int index)
	{
		return GetEnv()->CallObjectMethod(GetRaw(), s_list_get, index);
	}

	int Size_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_list_size);
	}
};

class Java_USBEndpoint : public Java_Object
{
public:
	Java_USBEndpoint(JNIEnv* env, jobject raw)
		: Java_Object(env, raw)
	{}

	int GetAddress()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBEndpoint_GetAddress);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetDirection()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBEndpoint_GetDirection);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	std::string ToString()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBEndpoint_ToString);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetString* ret = static_cast<MethodRetString*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetAttributes()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBEndpoint_GetAttributes);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetEndpointNumber()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBEndpoint_GetEndpointNumber);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetInterval()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBEndpoint_GetInterval);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetMaxPacketSize()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBEndpoint_GetMaxPacketSize);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetType()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBEndpoint_GetType);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	// Implementations!
	int GetAddress_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbendpoint_getAddress);
	}

	int GetDirection_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbendpoint_getDirection);
	}

	std::string ToString_Impl()
	{
		jstring str = (jstring)GetEnv()->CallObjectMethod(GetRaw(), s_usbendpoint_toString);
		return GetJString(GetEnv(), str);
	}

	int GetAttributes_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbendpoint_getAttributes);
	}

	int GetEndpointNumber_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbendpoint_getEndpointNumber);
	}

	int GetInterval_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbendpoint_getInterval);
	}

	int GetMaxPacketSize_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbendpoint_getMaxPacketSize);
	}

	int GetType_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbendpoint_getType);
	}

};

class Java_USBInterface : public Java_Object
{
public:
	Java_USBInterface(JNIEnv* env, jobject raw)
		: Java_Object(env, raw)
	{}

	std::unique_ptr<Java_USBEndpoint> GetEndpoint(int index)
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsI>(Method::USBInterface_GetEndpoint, index);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetUniquePtr<Java_USBEndpoint>* ret = static_cast<MethodRetUniquePtr<Java_USBEndpoint>*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetEndpointCount()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBInterface_GetEndpointCount);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	std::string ToString()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBInterface_ToString);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetString* ret = static_cast<MethodRetString*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetAlternateSetting()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBInterface_GetAlternateSetting);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetId()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBInterface_GetId);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetInterfaceClass()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBInterface_GetInterfaceClass);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetInterfaceProtocol()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBInterface_GetInterfaceProtocol);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetInterfaceSubclass()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBInterface_GetInterfaceSubclass);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	std::string GetName()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBInterface_GetName);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetString* ret = static_cast<MethodRetString*>(s_handler_ret.get());
		return ret->GetRet();
	}

	// Implementations!
	std::unique_ptr<Java_USBEndpoint> GetEndpoint_Impl(int index)
	{
		jobject obj = GetEnv()->CallObjectMethod(GetRaw(), s_usbinterface_getEndpoint, index);
		return std::make_unique<Java_USBEndpoint>(GetEnv(), obj);
	}

	int GetEndpointCount_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbinterface_getEndpointCount);
	}

	std::string ToString_Impl()
	{
		jstring str = (jstring)GetEnv()->CallObjectMethod(GetRaw(), s_usbinterface_toString);
		return GetJString(GetEnv(), str);
	}

	int GetAlternateSetting_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbinterface_getAlternateSetting);
	}

	int GetId_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbinterface_getId);
	}

	int GetInterfaceClass_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbinterface_getInterfaceClass);
	}

	int GetInterfaceProtocol_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbinterface_getInterfaceProtocol);
	}

	int GetInterfaceSubclass_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbinterface_getInterfaceSubclass);
	}

	std::string GetName_Impl()
	{
		jstring str = (jstring)GetEnv()->CallObjectMethod(GetRaw(), s_usbinterface_getName);
		return GetJString(GetEnv(), str);
	}

};

class Java_USBConfiguration : public Java_Object
{
public:
	Java_USBConfiguration(JNIEnv* env, jobject raw)
		: Java_Object(env, raw)
	{}

	int GetInterfaceCount()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBConfiguration_GetInterfaceCount);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	std::unique_ptr<Java_USBInterface> GetInterface(int index)
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsI>(Method::USBConfiguration_GetInterface, index);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetUniquePtr<Java_USBInterface>* ret = static_cast<MethodRetUniquePtr<Java_USBInterface>*>(s_handler_ret.get());
		return ret->GetRet();
	}

	std::string ToString()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBConfiguration_ToString);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetString* ret = static_cast<MethodRetString*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetId()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBConfiguration_GetId);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetMaxPower()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBConfiguration_GetMaxPower);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	std::string GetName()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBConfiguration_GetName);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetString* ret = static_cast<MethodRetString*>(s_handler_ret.get());
		return ret->GetRet();
	}

	bool IsRemoteWakeup()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBConfiguration_IsRemoteWakeup);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetBool* ret = static_cast<MethodRetBool*>(s_handler_ret.get());
		return ret->GetRet();
	}

	bool IsSelfPowered()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBConfiguration_IsSelfPowered);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetBool* ret = static_cast<MethodRetBool*>(s_handler_ret.get());
		return ret->GetRet();
	}

	// Implementations!
	int GetInterfaceCount_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbconfiguration_getInterfaceCount);
	}

	std::unique_ptr<Java_USBInterface> GetInterface_Impl(int index)
	{
		jobject obj = GetEnv()->CallObjectMethod(GetRaw(), s_usbconfiguration_getInterface, index);
		return std::make_unique<Java_USBInterface>(GetEnv(), obj);
	}

	std::string ToString_Impl()
	{
		jstring str = (jstring)GetEnv()->CallObjectMethod(GetRaw(), s_usbconfiguration_toString);
		return GetJString(GetEnv(), str);
	}

	int GetId_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbconfiguration_getId);
	}

	int GetMaxPower_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbconfiguration_getMaxPower);
	}

	std::string GetName_Impl()
	{
		jstring str = (jstring)GetEnv()->CallObjectMethod(GetRaw(), s_usbconfiguration_getName);
		return GetJString(GetEnv(), str);
	}

	bool IsRemoteWakeup_Impl()
	{
		return GetEnv()->CallBooleanMethod(GetRaw(), s_usbconfiguration_isRemoteWakeup);
	}

	bool IsSelfPowered_Impl()
	{
		return GetEnv()->CallBooleanMethod(GetRaw(), s_usbconfiguration_isSelfPowered);
	}
};

class Java_USBConnection : public Java_Object
{
public:
	static const s32 USB_DIR_OUT = 0x00;
	static const s32 USB_DIR_IN  = 0x80;
	Java_USBConnection(JNIEnv* env, jobject raw)
		: Java_Object(env, raw)
	{}

	int BulkTransfer(Java_USBEndpoint* endpoint, u8* buffer, int length, int timeout)
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsPPII>(Method::USBConnection_BulkTransfer, endpoint, buffer, length, timeout);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	void Close()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBConnection_Close);
		s_has_method.Set();
		s_handled_method.Wait();
	}

	int ControlTransfer(int request_type, int request, int value, int index, u8* buffer, int length, int timeout)
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsIIIIPII>(Method::USBConnection_ControlTransfer,
			request_type, request, value, index, buffer, length, timeout);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetFileDescriptor()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBConnection_GetFileDescriptor);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	bool ClaimInterface(Java_USBInterface* intf, bool force)
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsPB>(Method::USBConnection_ClaimInterface, intf, force);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetBool* ret = static_cast<MethodRetBool*>(s_handler_ret.get());
		return ret->GetRet();
	}

	std::vector<u8> GetRawDescriptors()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		// XXX:
	}

	std::string GetSerial()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBConnection_GetSerial);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetString* ret = static_cast<MethodRetString*>(s_handler_ret.get());
		return ret->GetRet();
	}

	bool ReleaseInterface(Java_USBInterface& intf)
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		// XXX:
	}

	/* USBRequest RequestWait()
	 * {
	 * XXX:
	 * }
	 */
	bool SetConfiguration(Java_USBConfiguration& conf)
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		// XXX:
	}

	bool SetInterface(Java_USBInterface& intf)
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		// XXX:
	}

	// Implementations!
	int BulkTransfer_Impl(Java_USBEndpoint* endpoint, u8* buffer, int length, int timeout)
	{
		int dir = endpoint->GetDirection_Impl();
		jbyteArray array = GetEnv()->NewByteArray(length);

		if (dir == USB_DIR_OUT)
			GetEnv()->SetByteArrayRegion(array, 0, length, reinterpret_cast<jbyte*>(buffer));

		int res = GetEnv()->CallIntMethod(GetRaw(), s_usbconnection_bulkTransfer, endpoint->GetRaw(), array, length, timeout);

		if (dir == USB_DIR_IN)
			GetEnv()->GetByteArrayRegion(array, 0, length, reinterpret_cast<jbyte*>(buffer));

		GetEnv()->DeleteLocalRef(array);
		return res;
	}

	void Close_Impl()
	{
		GetEnv()->CallVoidMethod(GetRaw(), s_usbconnection_close);
	}

	int ControlTransfer_Impl(int request_type, int request, int value, int index, u8* buffer, int length, int timeout)
	{
		jbyteArray array = GetEnv()->NewByteArray(length);
		GetEnv()->SetByteArrayRegion(array, 0, length, (jbyte*)buffer);
		return GetEnv()->CallIntMethod(GetRaw(), s_usbconnection_controlTransfer, request_type, request, value, index, array, length, timeout);
	}

	int GetFileDescriptor_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbconnection_getFileDescriptor);
	}

	bool ClaimInterface_Impl(Java_USBInterface* intf, bool force)
	{
		return GetEnv()->CallBooleanMethod(GetRaw(), s_usbconnection_claimInterface, intf->GetRaw(), force);
	}

	std::vector<u8> GetRawDescriptors_Impl()
	{
		// XXX:
	}

	std::string GetSerial_Impl()
	{
		jstring str = (jstring)GetEnv()->CallObjectMethod(GetRaw(), s_usbconnection_getSerial);
		return GetJString(GetEnv(), str);
	}

	bool ReleaseInterface_Impl(Java_USBInterface& intf)
	{
		return GetEnv()->CallBooleanMethod(GetRaw(), s_usbconnection_releaseInterface, intf.GetRaw());
	}

	/* USBRequest RequestWait_Impl()
	 * {
	 * XXX:
	 * }
	 */
	bool SetConfiguration_Impl(Java_USBConfiguration& conf)
	{
		return GetEnv()->CallBooleanMethod(GetRaw(), s_usbconnection_setConfiguration, conf.GetRaw());
	}

	bool SetInterface_Impl(Java_USBInterface& intf)
	{
		return GetEnv()->CallBooleanMethod(GetRaw(), s_usbconnection_setInterface, intf.GetRaw());
	}
};

class Java_USBDevice : public Java_Object
{
public:
	Java_USBDevice(JNIEnv* env, jobject raw)
		: Java_Object(env, raw)
	{}

	int GetProductId()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBDevice_GetProductId);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	std::string GetProductName()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBDevice_GetProductName);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetString* ret = static_cast<MethodRetString*>(s_handler_ret.get());
		return ret->GetRet();
	}

	std::string ToString()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBDevice_ToString);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetString* ret = static_cast<MethodRetString*>(s_handler_ret.get());
		return ret->GetRet();
	}

	std::string GetDeviceName()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBDevice_GetDeviceName);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetString* ret = static_cast<MethodRetString*>(s_handler_ret.get());
		return ret->GetRet();
	}

	std::string GetManufacturerName()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBDevice_GetManufacturerName);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetString* ret = static_cast<MethodRetString*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetVendorId()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBDevice_GetVendorId);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetInterfaceCount()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBDevice_GetInterfaceCount);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	std::unique_ptr<Java_USBInterface> GetInterface(int index)
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsI>(Method::USBDevice_GetInterface, index);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetUniquePtr<Java_USBInterface>* ret = static_cast<MethodRetUniquePtr<Java_USBInterface>*>(s_handler_ret.get());
		return ret->GetRet();
	}

	std::unique_ptr<Java_USBConfiguration> GetConfiguration(int index)
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsI>(Method::USBDevice_GetConfiguration, index);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetUniquePtr<Java_USBConfiguration>* ret = static_cast<MethodRetUniquePtr<Java_USBConfiguration>*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetConfigurationCount()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBDevice_GetConfigurationCount);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetDeviceClass()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBDevice_GetDeviceClass);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetDeviceId()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBDevice_GetDeviceId);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetDeviceProtocol()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBDevice_GetDeviceProtocol);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	int GetDeviceSubclass()
	{
		std::lock_guard<std::mutex> lk(s_handler_mutex);
		s_handler_args.first = this;
		s_handler_args.second = std::make_unique<MethodArgsVoid>(Method::USBDevice_GetDeviceSubclass);
		s_has_method.Set();
		s_handled_method.Wait();
		MethodRetInt* ret = static_cast<MethodRetInt*>(s_handler_ret.get());
		return ret->GetRet();
	}

	// Implementations!
	int GetProductId_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbdevice_getProductId);
	}

	std::string GetProductName_Impl()
	{
		jstring str = (jstring)GetEnv()->CallObjectMethod(GetRaw(), s_usbdevice_getProductName);
		return GetJString(GetEnv(), str);
	}

	std::string ToString_Impl()
	{
		jstring str = (jstring)GetEnv()->CallObjectMethod(GetRaw(), s_usbdevice_toString);
		return GetJString(GetEnv(), str);
	}

	std::string GetDeviceName_Impl()
	{
		jstring str = (jstring)GetEnv()->CallObjectMethod(GetRaw(), s_usbdevice_getDeviceName);
		return GetJString(GetEnv(), str);
	}

	std::string GetManufacturerName_Impl()
	{
		jstring str = (jstring)GetEnv()->CallObjectMethod(GetRaw(), s_usbdevice_getManufacturerName);
		return GetJString(GetEnv(), str);
	}

	int GetVendorId_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbdevice_getVendorId);
	}

	int GetInterfaceCount_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbdevice_getInterfaceCount);
	}

	std::unique_ptr<Java_USBInterface> GetInterface_Impl(int index)
	{
		jobject object = GetEnv()->CallObjectMethod(GetRaw(), s_usbdevice_getInterface, index);
		return std::make_unique<Java_USBInterface>(Java_USBInterface(GetEnv(), object));
	}

	std::unique_ptr<Java_USBConfiguration> GetConfiguration_Impl(int index)
	{
		jobject object = GetEnv()->CallObjectMethod(GetRaw(), s_usbdevice_getConfiguration, index);
		return std::make_unique<Java_USBConfiguration>(Java_USBConfiguration(GetEnv(), object));
	}

	int GetConfigurationCount_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbdevice_getConfigurationCount);
	}

	int GetDeviceClass_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbdevice_getDeviceClass);
	}

	int GetDeviceId_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbdevice_getDeviceId);
	}

	int GetDeviceProtocol_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbdevice_getDeviceProtocol);
	}

	int GetDeviceSubclass_Impl()
	{
		return GetEnv()->CallIntMethod(GetRaw(), s_usbdevice_getDeviceSubclass);
	}

};

static void GetListClassObjects(JNIEnv* env)
{
	s_list_class = env->FindClass("java/util/List");
	s_list_get = env->GetMethodID(s_list_class, "get", "(I)Ljava/lang/Object;");
	s_list_size = env->GetMethodID(s_list_class, "size", "()I");
}

static void GetUSBHandlerClassObjects(JNIEnv* env)
{

	s_handler_enumerate = env->GetStaticMethodID(s_handler_class, "Enumerate", "(II)Ljava/util/List;");
	s_handler_open = env->GetStaticMethodID(s_handler_class, "OpenDevice", "(Landroid/hardware/usb/UsbDevice;)Landroid/hardware/usb/UsbDeviceConnection;");
}

static void GetUSBEndpointClassObjects(JNIEnv* env)
{
	s_usbendpoint_class = env->FindClass("android/hardware/usb/UsbEndpoint");
	s_usbendpoint_getAddress = env->GetMethodID(s_usbendpoint_class, "getAddress", "()I");
	s_usbendpoint_getDirection = env->GetMethodID(s_usbendpoint_class, "getDirection", "()I");
	s_usbendpoint_toString = env->GetMethodID(s_usbendpoint_class, "toString", "()Ljava/lang/String;");
	s_usbendpoint_getAttributes = env->GetMethodID(s_usbendpoint_class, "getAttributes", "()I");
	s_usbendpoint_getEndpointNumber = env->GetMethodID(s_usbendpoint_class, "getEndpointNumber", "()I");
	s_usbendpoint_getInterval = env->GetMethodID(s_usbendpoint_class, "getInterval", "()I");
	s_usbendpoint_getMaxPacketSize = env->GetMethodID(s_usbendpoint_class, "getMaxPacketSize", "()I");
	s_usbendpoint_getType = env->GetMethodID(s_usbendpoint_class, "getType", "()I");
}

static void GetUSBInterfaceClassObjects(JNIEnv* env)
{
	s_usbinterface_class = env->FindClass("android/hardware/usb/UsbInterface");
	s_usbinterface_getEndpoint = env->GetMethodID(s_usbinterface_class, "getEndpoint", "(I)Landroid/hardware/usb/UsbEndpoint;");
	s_usbinterface_getEndpointCount = env->GetMethodID(s_usbinterface_class, "getEndpointCount", "()I");
	s_usbinterface_toString = env->GetMethodID(s_usbinterface_class, "toString", "()Ljava/lang/String;");
	s_usbinterface_getAlternateSetting = env->GetMethodID(s_usbinterface_class, "getAlternateSetting", "()I");
	s_usbinterface_getId = env->GetMethodID(s_usbinterface_class, "getId", "()I");
	s_usbinterface_getInterfaceClass = env->GetMethodID(s_usbinterface_class, "getInterfaceClass", "()I");
	s_usbinterface_getInterfaceProtocol = env->GetMethodID(s_usbinterface_class, "getInterfaceProtocol", "()I");
	s_usbinterface_getInterfaceSubclass = env->GetMethodID(s_usbinterface_class, "getInterfaceSubclass", "()I");
	s_usbinterface_getName = env->GetMethodID(s_usbinterface_class, "getName", "()Ljava/lang/String;");
}

static void GetUSBConfigurationClassObjects(JNIEnv* env)
{
	s_usbconfiguration_class = env->FindClass("android/hardware/usb/UsbConfiguration");
	s_usbconfiguration_getInterfaceCount = env->GetMethodID(s_usbconfiguration_class, "getInterfaceCount", "()I");
	s_usbconfiguration_getInterface = env->GetMethodID(s_usbconfiguration_class, "getInterface", "(I)Landroid/hardware/usb/UsbInterface;");
	s_usbconfiguration_toString = env->GetMethodID(s_usbconfiguration_class, "toString", "()Ljava/lang/String;");
	s_usbconfiguration_getId = env->GetMethodID(s_usbconfiguration_class, "getId", "()I");
	s_usbconfiguration_getMaxPower = env->GetMethodID(s_usbconfiguration_class, "getMaxPower", "()I");
	s_usbconfiguration_getName = env->GetMethodID(s_usbconfiguration_class, "getName", "()Ljava/lang/String;");
	s_usbconfiguration_isRemoteWakeup = env->GetMethodID(s_usbconfiguration_class, "isRemoteWakeup", "()Z");
	s_usbconfiguration_isSelfPowered = env->GetMethodID(s_usbconfiguration_class, "isSelfPowered", "()Z");
}

static void GetUSBConnectionClassObjects(JNIEnv* env)
{
	s_usbconnection_class = env->FindClass("android/hardware/usb/UsbDeviceConnection");
	s_usbconnection_bulkTransfer = env->GetMethodID(s_usbconnection_class, "bulkTransfer", "(Landroid/hardware/usb/UsbEndpoint;[BII)I");
	s_usbconnection_close = env->GetMethodID(s_usbconnection_class, "close", "()V");
	s_usbconnection_controlTransfer = env->GetMethodID(s_usbconnection_class, "controlTransfer", "(IIII[BII)I");
	s_usbconnection_getFileDescriptor = env->GetMethodID(s_usbconnection_class, "getFileDescriptor", "()I");
	s_usbconnection_claimInterface = env->GetMethodID(s_usbconnection_class, "claimInterface", "(Landroid/hardware/usb/UsbInterface;Z)Z");
	s_usbconnection_getRawDescriptors = env->GetMethodID(s_usbconnection_class, "getRawDescriptors", "()[B");
	s_usbconnection_getSerial = env->GetMethodID(s_usbconnection_class, "getSerial", "()Ljava/lang/String;");
	s_usbconnection_releaseInterface = env->GetMethodID(s_usbconnection_class, "releaseInterface", "(Landroid/hardware/usb/UsbInterface;)Z");
	s_usbconnection_setConfiguration = env->GetMethodID(s_usbconnection_class, "setConfiguration", "(Landroid/hardware/usb/UsbConfiguration;)Z");
	s_usbconnection_setInterface = env->GetMethodID(s_usbconnection_class, "setInterface", "(Landroid/hardware/usb/UsbInterface;)Z");
}

static void GetUSBDeviceClassObjects(JNIEnv* env)
{
	s_usbdevice_class = env->FindClass("android/hardware/usb/UsbDevice");
	s_usbdevice_getProductId = env->GetMethodID(s_usbdevice_class, "getProductId", "()I");
	s_usbdevice_getProductName = env->GetMethodID(s_usbdevice_class, "getProductName", "()Ljava/lang/String;");
	s_usbdevice_toString = env->GetMethodID(s_usbdevice_class, "toString", "()Ljava/lang/String;");
	s_usbdevice_getDeviceName = env->GetMethodID(s_usbdevice_class, "getDeviceName", "()Ljava/lang/String;");
	s_usbdevice_getManufacturerName = env->GetMethodID(s_usbdevice_class, "getManufacturerName", "()Ljava/lang/String;");
	s_usbdevice_getVendorId = env->GetMethodID(s_usbdevice_class, "getVendorId", "()I");
	s_usbdevice_getInterfaceCount = env->GetMethodID(s_usbdevice_class, "getInterfaceCount", "()I");
	s_usbdevice_getInterface = env->GetMethodID(s_usbdevice_class, "getInterface", "(I)Landroid/hardware/usb/UsbInterface;");
	s_usbdevice_getConfiguration = env->GetMethodID(s_usbdevice_class, "getConfiguration", "(I)Landroid/hardware/usb/UsbConfiguration;");
	s_usbdevice_getConfigurationCount = env->GetMethodID(s_usbdevice_class, "getConfigurationCount", "()I");
	s_usbdevice_getDeviceClass = env->GetMethodID(s_usbdevice_class, "getDeviceClass", "()I");
	s_usbdevice_getDeviceId = env->GetMethodID(s_usbdevice_class, "getDeviceId", "()I");
	s_usbdevice_getDeviceProtocol = env->GetMethodID(s_usbdevice_class, "getDeviceProtocol", "()I");
	s_usbdevice_getDeviceSubclass = env->GetMethodID(s_usbdevice_class, "getDeviceSubclass", "()I");
}

HidDeviceInfo::HidDeviceInfo(std::string path, std::wstring manufacturer, std::wstring product)
	: m_path(path), m_manufacturer(manufacturer), m_product(product)
{
}

class HidDeviceInfo_Android : public HidDeviceInfo
{
public:
	HidDeviceInfo_Android(Java_USBDevice dev)
		: HidDeviceInfo(dev.GetDeviceName(),
			std::wstring(dev.GetManufacturerName().begin(), dev.GetManufacturerName().end()),
			std::wstring(dev.GetProductName().begin(), dev.GetProductName().end())),
		  m_device(dev)
	{}

	bool IsOpen() override
	{
		return m_con->GetFileDescriptor() != -1 && m_conf->GetInterfaceCount() > 0;
	}

	Java_USBDevice m_device;
	std::unique_ptr<Java_USBConfiguration> m_conf;
	std::unique_ptr<Java_USBInterface> m_intf;
	std::unique_ptr<Java_USBConnection> m_con;
	std::unique_ptr<Java_USBEndpoint> m_in;
	std::unique_ptr<Java_USBEndpoint> m_out;
};

static Common::Flag s_handler_running;
static std::thread s_handler_thread;
static JNIEnv* thread_env;

static void ThreadedJNIHandler()
{

	JNIEnv* env;
	g_java_vm->AttachCurrentThread(&env, nullptr);
	thread_env = env;

	s_handler_enumerate = env->GetStaticMethodID(s_handler_class, "Enumerate", "(II)Ljava/util/List;");
	s_handler_open = env->GetStaticMethodID(s_handler_class, "OpenDevice", "(Landroid/hardware/usb/UsbDevice;)Landroid/hardware/usb/UsbDeviceConnection;");

	GetUSBHandlerClassObjects(env);
	GetListClassObjects(env);
	GetUSBEndpointClassObjects(env);
	GetUSBInterfaceClassObjects(env);
	GetUSBConfigurationClassObjects(env);
	GetUSBConnectionClassObjects(env);
	GetUSBDeviceClassObjects(env);

	while (s_handler_running.IsSet())
	{
		s_has_method.Wait();
		switch (s_handler_args.second->GetId())
		{
		// HID
		case Method::HID_Enumerate:
		{
			MethodArgsII* args = static_cast<MethodArgsII*>(s_handler_args.second.get());
			s_handler_ret = std::make_unique<MethodRetJObject>(
				env->CallStaticObjectMethod(s_handler_class, s_handler_enumerate, args->GetArg0(), args->GetArg1()));
		}
		break;
		case Method::HID_OpenDevice:
		{
			MethodArgsJObject* args = static_cast<MethodArgsJObject*>(s_handler_args.second.get());
			s_handler_ret = std::make_unique<MethodRetJObject>(
				env->CallStaticObjectMethod(s_handler_class, s_handler_open, args->GetArg()));
		}
		break;
		// Java_List
		case Method::Java_List_Size:
		{
			Java_List* dev = static_cast<Java_List*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(dev->Size_Impl());
		}
		break;
		case Method::Java_List_Get:
		{
			MethodArgsI* args = static_cast<MethodArgsI*>(s_handler_args.second.get());
			Java_List* dev = static_cast<Java_List*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetJObject>(dev->Get_Impl(args->GetArg()));
		}
		break;
		// USBDevice
		case Method::USBDevice_GetProductId:
		{
			Java_USBDevice* dev = static_cast<Java_USBDevice*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(dev->GetProductId_Impl());
		}
		break;
		case Method::USBDevice_GetProductName:
		{
			Java_USBDevice* dev = static_cast<Java_USBDevice*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetString>(dev->GetProductName_Impl());
		}
		break;
		case Method::USBDevice_ToString:
		{
			Java_USBDevice* dev = static_cast<Java_USBDevice*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetString>(dev->ToString_Impl());
		}
		break;
		case Method::USBDevice_GetDeviceName:
		{
			Java_USBDevice* dev = static_cast<Java_USBDevice*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetString>(dev->GetDeviceName_Impl());
		}
		break;
		case Method::USBDevice_GetManufacturerName:
		{
			Java_USBDevice* dev = static_cast<Java_USBDevice*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetString>(dev->GetManufacturerName_Impl());
		}
		break;
		case Method::USBDevice_GetVendorId:
		{
			Java_USBDevice* dev = static_cast<Java_USBDevice*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(dev->GetVendorId_Impl());
		}
		break;
		case Method::USBDevice_GetInterfaceCount:
		{
			Java_USBDevice* dev = static_cast<Java_USBDevice*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(dev->GetInterfaceCount_Impl());
		}
		break;
		case Method::USBDevice_GetInterface:
		{
			MethodArgsI* args = static_cast<MethodArgsI*>(s_handler_args.second.get());
			Java_USBDevice* dev = static_cast<Java_USBDevice*>(s_handler_args.first);
			std::unique_ptr<Java_USBInterface> intf = dev->GetInterface_Impl(args->GetArg());
			s_handler_ret = std::make_unique<MethodRetUniquePtr<Java_USBInterface>>(std::move(intf));
		}
		break;
		case Method::USBDevice_GetConfiguration:
		{
			MethodArgsI* args = static_cast<MethodArgsI*>(s_handler_args.second.get());
			Java_USBDevice* dev = static_cast<Java_USBDevice*>(s_handler_args.first);
			std::unique_ptr<Java_USBConfiguration> conf = dev->GetConfiguration_Impl(args->GetArg());
			s_handler_ret = std::make_unique<MethodRetUniquePtr<Java_USBConfiguration>>(std::move(conf));
		}
		break;
		case Method::USBDevice_GetConfigurationCount:
		{
			Java_USBDevice* dev = static_cast<Java_USBDevice*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(dev->GetConfigurationCount_Impl());
		}
		break;
		case Method::USBDevice_GetDeviceClass:
		{
			Java_USBDevice* dev = static_cast<Java_USBDevice*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(dev->GetDeviceClass_Impl());
		}
		break;
		case Method::USBDevice_GetDeviceId:
		{
			Java_USBDevice* dev = static_cast<Java_USBDevice*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(dev->GetDeviceId_Impl());
		}
		break;
		case Method::USBDevice_GetDeviceProtocol:
		{
			Java_USBDevice* dev = static_cast<Java_USBDevice*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(dev->GetDeviceProtocol_Impl());
		}
		break;
		case Method::USBDevice_GetDeviceSubclass:
		{
			Java_USBDevice* dev = static_cast<Java_USBDevice*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(dev->GetDeviceSubclass_Impl());
		}
		break;

		// USBConnection
		case Method::USBConnection_BulkTransfer:
		{
			MethodArgsPPII* args = static_cast<MethodArgsPPII*>(s_handler_args.second.get());
			Java_USBConnection* con = static_cast<Java_USBConnection*>(s_handler_args.first);
			Java_USBEndpoint* usb_end = static_cast<Java_USBEndpoint*>(args->GetArg0());
			u8* buffer = static_cast<u8*>(args->GetArg1());
			s_handler_ret = std::make_unique<MethodRetInt>(con->BulkTransfer_Impl(usb_end, buffer, args->GetArg2(), args->GetArg3()));
		}
		break;
		case Method::USBConnection_Close:
		{
			Java_USBConnection* con = static_cast<Java_USBConnection*>(s_handler_args.first);
			con->Close_Impl();
		}
		break;
		case Method::USBConnection_ControlTransfer:
		{
			MethodArgsIIIIPII* args = static_cast<MethodArgsIIIIPII*>(s_handler_args.second.get());
			Java_USBConnection* con = static_cast<Java_USBConnection*>(s_handler_args.first);
			u8* buffer = static_cast<u8*>(args->GetArg4());
			s_handler_ret = std::make_unique<MethodRetInt>(con->ControlTransfer_Impl(
				args->GetArg0(),
				args->GetArg1(),
				args->GetArg2(),
				args->GetArg3(),
				buffer,
				args->GetArg5(),
				args->GetArg6()
				));
		}
		break;
		case Method::USBConnection_GetFileDescriptor:
		{
			Java_USBConnection* con = static_cast<Java_USBConnection*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(con->GetFileDescriptor_Impl());
		}
		break;
		case Method::USBConnection_ClaimInterface:
		{
			MethodArgsPB* args = static_cast<MethodArgsPB*>(s_handler_args.second.get());
			Java_USBConnection* con = static_cast<Java_USBConnection*>(s_handler_args.first);
			Java_USBInterface* intf = static_cast<Java_USBInterface*>(args->GetArg0());
			s_handler_ret = std::make_unique<MethodRetBool>(con->ClaimInterface_Impl(intf, args->GetArg1()));
		}
		break;
		case Method::USBConnection_GetRawDescriptors:
		case Method::USBConnection_GetSerial:
		{
			Java_USBConnection* con = static_cast<Java_USBConnection*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetString>(con->GetSerial_Impl());
		}
		break;
		case Method::USBConnection_ReleaseInterface:
		case Method::USBConnection_SetConfiguration:
		case Method::USBConnection_SetInterface:
		break;

		// USBConfiguration
		case Method::USBConfiguration_GetInterfaceCount:
		{
			Java_USBConfiguration* conf = static_cast<Java_USBConfiguration*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(conf->GetInterfaceCount_Impl());
		}
		break;
		case Method::USBConfiguration_GetInterface:
		{
			MethodArgsI* args = static_cast<MethodArgsI*>(s_handler_args.second.get());
			Java_USBConfiguration* conf = static_cast<Java_USBConfiguration*>(s_handler_args.first);
			std::unique_ptr<Java_USBInterface> intf = conf->GetInterface_Impl(args->GetArg());
			s_handler_ret = std::make_unique<MethodRetUniquePtr<Java_USBInterface>>(std::move(intf));
		}
		break;
		case Method::USBConfiguration_ToString:
		{
			Java_USBConfiguration* conf = static_cast<Java_USBConfiguration*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetString>(conf->ToString_Impl());
		}
		break;
		case Method::USBConfiguration_GetId:
		{
			Java_USBConfiguration* conf = static_cast<Java_USBConfiguration*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(conf->GetId_Impl());
		}
		break;
		case Method::USBConfiguration_GetMaxPower:
		{
			Java_USBConfiguration* conf = static_cast<Java_USBConfiguration*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(conf->GetMaxPower_Impl());
		}
		break;
		case Method::USBConfiguration_GetName:
		{
			Java_USBConfiguration* conf = static_cast<Java_USBConfiguration*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetString>(conf->GetName_Impl());
		}
		break;
		case Method::USBConfiguration_IsRemoteWakeup:
		{
			Java_USBConfiguration* conf = static_cast<Java_USBConfiguration*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetBool>(conf->IsRemoteWakeup_Impl());
		}
		break;
		case Method::USBConfiguration_IsSelfPowered:
		{
			Java_USBConfiguration* conf = static_cast<Java_USBConfiguration*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetBool>(conf->IsSelfPowered_Impl());
		}
		break;

		// USBInterface
		case Method::USBInterface_GetEndpoint:
		{
			MethodArgsI* args = static_cast<MethodArgsI*>(s_handler_args.second.get());
			Java_USBInterface* intf = static_cast<Java_USBInterface*>(s_handler_args.first);
			std::unique_ptr<Java_USBEndpoint> usb_end = intf->GetEndpoint_Impl(args->GetArg());
			s_handler_ret = std::make_unique<MethodRetUniquePtr<Java_USBEndpoint>>(std::move(usb_end));
		}
		break;
		case Method::USBInterface_GetEndpointCount:
		{
			Java_USBInterface* intf = static_cast<Java_USBInterface*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(intf->GetEndpointCount_Impl());
		}
		break;
		case Method::USBInterface_ToString:
		{
			Java_USBInterface* intf = static_cast<Java_USBInterface*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetString>(intf->ToString_Impl());
		}
		break;
		case Method::USBInterface_GetAlternateSetting:
		{
			Java_USBInterface* intf = static_cast<Java_USBInterface*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(intf->GetAlternateSetting_Impl());
		}
		break;
		case Method::USBInterface_GetId:
		{
			Java_USBInterface* intf = static_cast<Java_USBInterface*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(intf->GetId_Impl());
		}
		break;
		case Method::USBInterface_GetInterfaceClass:
		{
			Java_USBInterface* intf = static_cast<Java_USBInterface*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(intf->GetInterfaceClass_Impl());
		}
		break;
		case Method::USBInterface_GetInterfaceProtocol:
		{
			Java_USBInterface* intf = static_cast<Java_USBInterface*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(intf->GetInterfaceProtocol_Impl());
		}
		break;
		case Method::USBInterface_GetInterfaceSubclass:
		{
			Java_USBInterface* intf = static_cast<Java_USBInterface*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(intf->GetInterfaceSubclass_Impl());
		}
		break;
		case Method::USBInterface_GetName:
		{
			Java_USBInterface* intf = static_cast<Java_USBInterface*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetString>(intf->GetName_Impl());
		}
		break;
		// USBEndpoint
		case Method::USBEndpoint_GetAddress:
		{
			Java_USBEndpoint* usb_end = static_cast<Java_USBEndpoint*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(usb_end->GetAddress_Impl());
		}
		break;
		case Method::USBEndpoint_GetDirection:
		{
			Java_USBEndpoint* usb_end = static_cast<Java_USBEndpoint*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(usb_end->GetDirection_Impl());
		}
		break;
		case Method::USBEndpoint_ToString:
		{
			Java_USBEndpoint* usb_end = static_cast<Java_USBEndpoint*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetString>(usb_end->ToString_Impl());
		}
		break;
		case Method::USBEndpoint_GetAttributes:
		{
			Java_USBEndpoint* usb_end = static_cast<Java_USBEndpoint*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(usb_end->GetAttributes_Impl());
		}
		break;
		case Method::USBEndpoint_GetEndpointNumber:
		{
			Java_USBEndpoint* usb_end = static_cast<Java_USBEndpoint*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(usb_end->GetEndpointNumber_Impl());
		}
		break;
		case Method::USBEndpoint_GetInterval:
		{
			Java_USBEndpoint* usb_end = static_cast<Java_USBEndpoint*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(usb_end->GetInterval_Impl());
		}
		break;
		case Method::USBEndpoint_GetMaxPacketSize:
		{
			Java_USBEndpoint* usb_end = static_cast<Java_USBEndpoint*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(usb_end->GetMaxPacketSize_Impl());
		}
		break;
		case Method::USBEndpoint_GetType:
		{
			Java_USBEndpoint* usb_end = static_cast<Java_USBEndpoint*>(s_handler_args.first);
			s_handler_ret = std::make_unique<MethodRetInt>(usb_end->GetType_Impl());
		}
		break;
		}

		s_handled_method.Set();
	}
}

std::vector<std::unique_ptr<HidDeviceInfo>> Enumerate(u16 vendor, u16 product)
{
	s_handler_args.first = nullptr;
	s_handler_args.second = std::make_unique<MethodArgsII>(Method::HID_Enumerate, vendor, product);
	s_has_method.Set();
	s_handled_method.Wait();
	MethodRetJObject* ret = static_cast<MethodRetJObject*>(s_handler_ret.get());

	Java_List dev_list(thread_env, ret->GetRet());

	int enumerate_size = dev_list.Size();

	std::vector<std::unique_ptr<HidDeviceInfo>> internal_devices;
	ERROR_LOG(COMMON, "Enumeration returned %d devices", enumerate_size);
	for (int i = 0; i < enumerate_size; ++i)
	{
		Java_USBDevice dev(thread_env, dev_list.Get(i));
		internal_devices.emplace_back(std::make_unique<HidDeviceInfo_Android>(dev));
		ERROR_LOG(COMMON, "%d: Vendor/Product 0x%04x:0x%04x", i, dev.GetVendorId(), dev.GetProductId());
		ERROR_LOG(COMMON, "%d: Configs: %d", i, dev.GetConfigurationCount());
		ERROR_LOG(COMMON, "%d: %s", i, dev.ToString().c_str());
		ERROR_LOG(COMMON, "%d: %s", i, dev.GetProductName().c_str());
		ERROR_LOG(COMMON, "%d: %s", i, dev.GetDeviceName().c_str());
		ERROR_LOG(COMMON, "%d: %s", i, dev.GetManufacturerName().c_str());
	}
	return internal_devices;
}

bool Open(HidDeviceInfo* device)
{
	ERROR_LOG(COMMON, "Attempting to open HID device\n");
	HidDeviceInfo_Android* dev = static_cast<HidDeviceInfo_Android*>(device);
	dev->m_conf = dev->m_device.GetConfiguration(0);

	jobject con;
	{
	std::lock_guard<std::mutex> lk(s_handler_mutex);

	s_handler_args.first = nullptr;
	s_handler_args.second = std::make_unique<MethodArgsJObject>(Method::HID_OpenDevice, dev->m_device.GetRaw());
	s_has_method.Set();
	s_handled_method.Wait();
	MethodRetJObject* ret = static_cast<MethodRetJObject*>(s_handler_ret.get());
	con = ret->GetRet();
	}

	dev->m_con = std::make_unique<Java_USBConnection>(Java_USBConnection(dev->m_device.GetEnv(), con));

	int intf_count = dev->m_conf->GetInterfaceCount();
	int usbdev_intf_count = dev->m_device.GetInterfaceCount();

	ERROR_LOG(COMMON, "Interface counts: %d %d", intf_count, usbdev_intf_count);
	if (intf_count > 0)
	{
		dev->m_intf = dev->m_conf->GetInterface(0);
		dev->m_con->ClaimInterface(dev->m_intf.get(), true);
		dev->m_in = dev->m_intf->GetEndpoint(0);
	}

	ERROR_LOG(COMMON, "Have we opened? %s", dev->IsOpen() ? "True" : "False");
	return dev->IsOpen();
}

void Close(HidDeviceInfo* device)
{
	HidDeviceInfo_Android* dev = static_cast<HidDeviceInfo_Android*>(device);
	dev->m_con->Close();
}

s32 Read(HidDeviceInfo* device, u8* data, size_t length, int timeout_ms)
{
	u64 start, end, diff;
	start = Common::Timer::GetTimeUs();
	HidDeviceInfo_Android* dev = static_cast<HidDeviceInfo_Android*>(device);
	int read = dev->m_con->BulkTransfer(dev->m_in.get(), data, length, timeout_ms);
	end = Common::Timer::GetTimeUs();
	ERROR_LOG(COMMON, "Doing a read: %d took %ldus", read, end-start);
	return read;
}

s32 Write(HidDeviceInfo* device, const u8* data, size_t length)
{
	HidDeviceInfo_Android* dev = static_cast<HidDeviceInfo_Android*>(device);
	int res;
	int report_number = data[0];
	int skipped_report_id = 0;

	if (report_number == 0x0) {
		data++;
		length--;
		skipped_report_id = 1;
	}
#define LIBUSB_REQUEST_TYPE_CLASS (1 << 5)
#define LIBUSB_RECIPIENT_INTERFACE 0x1
#define LIBUSB_ENDPOINT_OUT 0
	int write = dev->m_con->ControlTransfer(
		LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE|LIBUSB_ENDPOINT_OUT,
		0x09 /*HID Set_Report*/,
		(2/*HID output*/ << 8) | report_number,
		0,
		(u8*)data, length,
		1000);

	if (skipped_report_id)
		write++;

	ERROR_LOG(COMMON, "Doing a write: %d. Actually wrote %d", length, write);
	return length;
}

void Init()
{
	ERROR_LOG(COMMON, "HIDInterface_Android INIT!\n");
	s_handler_running.Set(true);
	s_handler_thread = std::thread(ThreadedJNIHandler);
}

void InitHandlerClass(JNIEnv* env)
{
	jclass handler_class = env->FindClass("org/dolphinemu/dolphinemu/utils/Java_USBHandler");
	s_handler_class = reinterpret_cast<jclass>(env->NewGlobalRef(handler_class));
}

void Shutdown()
{
	if (s_handler_running.TestAndClear())
		s_handler_thread.join();
}
}


