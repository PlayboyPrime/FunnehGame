#pragma once

#include <d3d9.h>
#include <windows.h>

#define APP_NAME "Funneh Game"
#define APP_VERSION "1.0.0"

inline bool g_running = true;
inline LPDIRECT3D9              g_pD3D = nullptr;
inline LPDIRECT3DDEVICE9        g_pd3dDevice = nullptr;
inline UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
inline D3DPRESENT_PARAMETERS    g_d3dpp = {};