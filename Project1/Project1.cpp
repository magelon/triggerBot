#include <wrl/client.h>
#include <windows.h>
#include <iostream>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <stdlib.h>

#pragma comment(lib,"d3d11.lib")

using namespace std;

int x;
int y;

const int lookSize = 4;

const int Shade_Variation = 35;
const int Formatted_Color_Red = 235;
const int Formatted_Color_Green = 80;
const int Formatted_Color_Blue = 235;

using Microsoft::WRL::ComPtr;

ComPtr<ID3D11Device> lDevice;
ComPtr<ID3D11DeviceContext> lImmediateContext;
ComPtr<IDXGIOutputDuplication> lDeskDupl;
ComPtr<ID3D11Texture2D> lAcquiredDesktopImage;
ComPtr<ID3D11Texture2D> lGDIImage;
DXGI_OUTPUT_DESC lOutputDesc;
DXGI_OUTDUPL_DESC lOutputDuplDesc;
D3D11_TEXTURE2D_DESC desc;


D3D_DRIVER_TYPE gDriverTypes[] = {
	D3D_DRIVER_TYPE_HARDWARE
};
UINT gNumDriverTypes = ARRAYSIZE(gDriverTypes);

// Feature levels supported
D3D_FEATURE_LEVEL gFeatureLevels[] = {
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_1
};
UINT gNumFeatureLevels = ARRAYSIZE(gFeatureLevels);


void ToggleKey(BYTE key, bool set) {
	keybd_event(key,
		0x45,
		set ? (KEYEVENTF_EXTENDEDKEY | 0) : (KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP),
		0);
}

void MouseDown() {
	ToggleKey(VK_LBUTTON, true);
	//gun like vandal's first three shots are accurate
	Sleep(300);
	ToggleKey(VK_LBUTTON, false);
}

bool GoodTriggerbotInit() {
	int lresult(-1);

	D3D_FEATURE_LEVEL lFeatureLevel;

	HRESULT hr(E_FAIL);

	// Create device
	for (UINT DriverTypeIndex = 0; DriverTypeIndex < gNumDriverTypes; ++DriverTypeIndex)
	{
		hr = D3D11CreateDevice(
			nullptr,
			gDriverTypes[DriverTypeIndex],
			nullptr,
			0,
			gFeatureLevels,
			gNumFeatureLevels,
			D3D11_SDK_VERSION,
			&lDevice,
			&lFeatureLevel,
			&lImmediateContext);

		if (SUCCEEDED(hr))
		{
			// Device creation success, no need to loop anymore
			break;
		}

		lDevice.Reset();

		lImmediateContext.Reset();
	}

	if (FAILED(hr))
		return false;

	if (lDevice == nullptr)
		return false;

	// Get DXGI device
	ComPtr<IDXGIDevice> lDxgiDevice;
	hr = lDevice.As(&lDxgiDevice);

	if (FAILED(hr))
		return false;

	// Get DXGI adapter
	ComPtr<IDXGIAdapter> lDxgiAdapter;
	hr = lDxgiDevice->GetParent(
		__uuidof(IDXGIAdapter), &lDxgiAdapter);

	if (FAILED(hr))
		return false;

	lDxgiDevice.Reset();

	UINT Output = 0;

	// Get output
	ComPtr<IDXGIOutput> lDxgiOutput;
	hr = lDxgiAdapter->EnumOutputs(
		Output,
		&lDxgiOutput);

	if (FAILED(hr))
		return false;

	lDxgiAdapter.Reset();

	hr = lDxgiOutput->GetDesc(
		&lOutputDesc);

	if (FAILED(hr))
		return false;

	// QI for Output 1
	ComPtr<IDXGIOutput1> lDxgiOutput1;
	hr = lDxgiOutput.As(&lDxgiOutput1);

	if (FAILED(hr))
		return false;

	lDxgiOutput.Reset();

	// Create desktop duplication
	hr = lDxgiOutput1->DuplicateOutput(
		lDevice.Get(), //TODO what im i doing here
		&lDeskDupl);

	if (FAILED(hr))
		return false;

	lDxgiOutput1.Reset();

	// Create GUI drawing texture
	lDeskDupl->GetDesc(&lOutputDuplDesc);
	desc.Width = lOutputDuplDesc.ModeDesc.Width;
	desc.Height = lOutputDuplDesc.ModeDesc.Height;
	desc.Format = lOutputDuplDesc.ModeDesc.Format;
	desc.ArraySize = 1;
	desc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET;
	desc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.MipLevels = 1;
	desc.CPUAccessFlags = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;


	hr = lDevice->CreateTexture2D(&desc, NULL, &lGDIImage);

	if (FAILED(hr))
		return false;

	if (lGDIImage == nullptr)
		return false;

	// Create CPU access texture
	desc.Width = lOutputDuplDesc.ModeDesc.Width;
	desc.Height = lOutputDuplDesc.ModeDesc.Height;
	desc.Format = lOutputDuplDesc.ModeDesc.Format;
	std::cout << "Starting at " << desc.Width << "x" << desc.Height << endl;
	desc.ArraySize = 1;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.MipLevels = 1;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	desc.Usage = D3D11_USAGE_STAGING;

	return true;
}

bool CaptureNextFrame() {
	HRESULT hr(E_FAIL);
	ComPtr<IDXGIResource> lDesktopResource = nullptr;
	DXGI_OUTDUPL_FRAME_INFO lFrameInfo;
	ID3D11Texture2D* currTexture;

	hr = lDeskDupl->AcquireNextFrame(
		INFINITE,
		&lFrameInfo,
		&lDesktopResource);

	if (FAILED(hr)) {
		cout << "Failed to acquire new frame, Try restarting the program" << endl;
		return false;
	}

	hr = lDesktopResource.As(&lAcquiredDesktopImage);

	hr = lDevice->CreateTexture2D(&desc, NULL, &currTexture);
	if (FAILED(hr))
		return false;
	if (currTexture == nullptr)
		return false;

	lImmediateContext->CopyResource(currTexture, lAcquiredDesktopImage.Get());
	D3D11_MAPPED_SUBRESOURCE* pRes = new D3D11_MAPPED_SUBRESOURCE;
	UINT subresource = D3D11CalcSubresource(0, 0, 0);

	lImmediateContext->Map(currTexture, subresource, D3D11_MAP_READ_WRITE, 0, pRes);

	void* d = pRes->pData;
	char* data = reinterpret_cast<char*>(d);

	int hWidth = desc.Width >> 1;
	int hHeight = desc.Height >> 1;

	for (UINT x = hWidth - lookSize; x < hWidth + lookSize; ++x) {
		for (UINT y = hHeight - lookSize; y < hHeight + lookSize; ++y) {
			int base = (x + y * desc.Width) << 2;
			byte red = data[base + 2];
			if (red >= Formatted_Color_Red - Shade_Variation && red <= Formatted_Color_Red + Shade_Variation) {
				byte green = data[base + 1];
				if (green >= Formatted_Color_Green - Shade_Variation && green <= Formatted_Color_Green + Shade_Variation) {
					byte blue = data[base];
					if (blue >= Formatted_Color_Blue - Shade_Variation && blue <= Formatted_Color_Blue + Shade_Variation) {
						MouseDown();
						currTexture->Release();
						hr = lDeskDupl->ReleaseFrame();
						return true;
					}
				}
			}
		}
	}

	currTexture->Release();
	hr = lDeskDupl->ReleaseFrame();
	return true;
}

void BadTriggerbotInit() { // USED TO GET MONITOR REZ
	MONITORINFO target;
	target.cbSize = sizeof(MONITORINFO);

	HMONITOR hMon = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
	GetMonitorInfo(hMon, &target);
	x = target.rcMonitor.right / 2;
	y = target.rcMonitor.bottom / 2;

	HDC hdcScreen = ::GetDC(NULL);

	// THIS MUST BE USED IF WINDOWS SCALING IS NOT 100%
	int LogicalScreenHeight = GetDeviceCaps(hdcScreen, VERTRES);
	int PhysicalScreenHeight = GetDeviceCaps(hdcScreen, DESKTOPVERTRES);
	float ScreenScalingFactor = (float)PhysicalScreenHeight / (float)LogicalScreenHeight;

	::ReleaseDC(NULL, hdcScreen);

	x *= ScreenScalingFactor;
	y *= ScreenScalingFactor;
}

void BadTriggerbot() {
	HDC _hdc = GetDC(NULL);
	if (_hdc)
	{
		COLORREF _color = GetPixel(_hdc, x, y); // EACH CALL WILL COST YOU 16ms (1/hz of Monitor)
		int r = GetRValue(_color);
		int g = GetGValue(_color);
		int b = GetBValue(_color);

		if ((r < Formatted_Color_Red + Shade_Variation) && (r > Formatted_Color_Red - Shade_Variation)) {
			if ((b < Formatted_Color_Blue + Shade_Variation) && (b > Formatted_Color_Blue - Shade_Variation)) {
				if ((g < Formatted_Color_Green + Shade_Variation) && (g > Formatted_Color_Green - Shade_Variation)) {
					MouseDown();
				}
			}
		}
	}
}

void GoodTriggerBot() {
	while (!CaptureNextFrame()) {}
}

int main()
{
	GoodTriggerbotInit();
	while (true)
	{
		// https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
		// GetAsyncKeyState(VK_MENU) // WHEN HELD DOWN
		//GetKeyState(VK_MENU) // TOGGLE

		if (GetKeyState(VK_CAPITAL)) {
			GoodTriggerBot();
		}
		else { // TO KEEP CPU UNDER 1%
			Sleep(1);
		}
	}
}