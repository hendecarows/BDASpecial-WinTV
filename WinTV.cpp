// SPDX-License-Identifier: GPL-2.0-only

#include "IBdaSpecials2.h"
#include <mutex>
#include <new>
#include <string_view>

namespace {

// eMPIA interface used by the installed bdadll64.dll.
struct __declspec(uuid("846a4c4e-76d8-42ec-a333-121f365a4da8")) EmpiaControl : IUnknown {
	virtual HRESULT STDMETHODCALLTYPE Reserved() = 0;
	virtual HRESULT STDMETHODCALLTYPE SetValue(ULONG command, ULONG* value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetValue(ULONG command, ULONG* value) = 0;
	virtual HRESULT STDMETHODCALLTYPE WriteI2c(ULONG address, ULONG length, BYTE* data) = 0;
	virtual HRESULT STDMETHODCALLTYPE ReadI2c(ULONG address, ULONG writeLength, BYTE* command,
		ULONG readLength, BYTE* data) = 0;
};

constexpr ULONG SelectRegister = 0x1005;
constexpr ULONG TransferRegister = 0x1006;
constexpr ULONG TsEnableRegister = 0x5f01; // Register 0x5f, one byte.
constexpr ULONG Ts1DiscardNullPackets = 0x04;
constexpr ULONG Ts2DiscardNullPackets = 0x40;
constexpr ULONG DiscardNullPackets = Ts1DiscardNullPackets | Ts2DiscardNullPackets;

// Multiple capture filters can share the bridge and its register selector.
std::mutex controlMutex;

class WinTV final : public IBdaSpecials2b5 {
public:
	WinTV(EmpiaControl* control, bool readSoloHdCnr)
		: control(control), readSoloHdCnr(readSoloHdCnr) {}

	const HRESULT InitializeHook() override
	{
		return control ? S_OK : E_NOINTERFACE;
	}

	const HRESULT PreLockChannel(TuningParam*) override
	{
		std::lock_guard<std::mutex> lock(controlMutex);
		tuned = false;
		nextSample = 0;
		return S_OK;
	}

	const HRESULT GetSignalStrength(float* level) override
	{
		if (!level)
			return E_POINTER;
		if (!readSoloHdCnr)
			return E_NOINTERFACE;

		std::lock_guard<std::mutex> lock(controlMutex);
		if (!control || !tuned) {
			*level = 0.0f;
			return S_OK;
		}

		const ULONGLONG now = GetTickCount64();
		if (now >= nextSample) {
			BYTE command[] = {0x90, 0x00};
			BYTE response[9] = {};
			HRESULT result = control->WriteI2c(0xc8, sizeof(command), command);
			// The demodulator needs time to complete the status command.
			for (int attempt = 0; attempt < 3 && result == S_OK; ++attempt) {
				Sleep(5);
				result = control->ReadI2c(0xc8, 0, command, sizeof(response), response);
				if (response[0] & 0xc0)
					break;
			}
			sampleValid = result == S_OK && (response[0] & 0xc0) == 0x80;
			if (sampleValid)
				cnr = (response[2] & 0x06) == 0x06 ? response[3] / 4.0f : 0.0f;
			nextSample = now + 1000;
		}
		if (!sampleValid)
			return E_NOINTERFACE;
		*level = cnr;
		return S_OK;
	}


	const HRESULT PostLockChannel(const TuningParam*) override
	{
		std::lock_guard<std::mutex> lock(controlMutex);
		tuned = true;
		if (!control)
			return E_NOINTERFACE;

		ULONG value = 0;
		HRESULT result = ReadTsEnable(value);
		if (FAILED(result) || !(value & DiscardNullPackets))
			return result;

		ULONG updated = value & ~DiscardNullPackets;
		result = SelectTsEnable();
		if (SUCCEEDED(result))
			result = control->SetValue(TransferRegister, &updated);
		if (FAILED(result))
			return result;

		result = ReadTsEnable(value);
		if (SUCCEEDED(result) && (value & DiscardNullPackets))
			return E_FAIL;
		return result;
	}

	void Release() override { delete this; }

private:
	HRESULT SelectTsEnable()
	{
		ULONG selection = TsEnableRegister;
		return control->SetValue(SelectRegister, &selection);
	}

	HRESULT ReadTsEnable(ULONG& value)
	{
		const HRESULT result = SelectTsEnable();
		return FAILED(result) ? result : control->GetValue(TransferRegister, &value);
	}

	CComPtr<EmpiaControl> control;
	const bool readSoloHdCnr;
	bool tuned = false;
	bool sampleValid = false;
	ULONGLONG nextSample = 0;
	float cnr = 0.0f;
};

} // namespace

IBdaSpecials* CreateBdaSpecials2(CComPtr<IBaseFilter>, CComPtr<IBaseFilter> capture,
	const WCHAR* tunerPath, const WCHAR* tunerName, const WCHAR*, const WCHAR*)
{
	if (!capture)
		return nullptr;

	CComQIPtr<EmpiaControl> control(capture);
	if (!control)
		return nullptr;

	// The eMPIA bridge interface does not identify the attached demodulator.
	const bool readSoloHdCnr = tunerPath && tunerName &&
		std::wstring_view(tunerPath).find(L"usb#vid_2040&pid_8268#") != std::wstring_view::npos &&
		std::wstring_view(tunerName) == L"Hauppauge WinTV-soloHD DVBC Tuner";
	return new (std::nothrow) WinTV(control, readSoloHdCnr);
}
