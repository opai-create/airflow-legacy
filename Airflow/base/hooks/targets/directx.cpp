#pragma once
#include "../hooks.h"
#include "../../../includes.h"

#include "../../sdk.h"
#include "../../global_context.h"

#include "../../tools/render.h"
#include "../../tools/key_states.h"

#include "../../../functions/features.h"

#include "../../../functions/config_vars.h"

#include <string>

namespace tr::direct {
	HRESULT __stdcall present(IDirect3DDevice9* device, const RECT* src_rect, const RECT* dest_rect,
		HWND window_override, const RGNDATA* dirty_region) {
		if (g_ctx.uninject)
			return original_present(device, src_rect, dest_rect, window_override, dirty_region);

		DWORD colorwrite, srgbwrite;

		device->GetRenderState(D3DRS_COLORWRITEENABLE, &colorwrite);
		device->GetRenderState(D3DRS_SRGBWRITEENABLE, &srgbwrite);
		device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);
		device->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, FALSE);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xffffffff);
		device->SetRenderState(D3DRS_SRGBWRITEENABLE, false);
		device->SetSamplerState(NULL, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		device->SetSamplerState(NULL, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
		device->SetSamplerState(NULL, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
		device->SetSamplerState(NULL, D3DSAMP_SRGBTEXTURE, NULL);

		if (g_ctx.cheat_init && !g_cfg.misc.menu) {
			g_cfg.misc.menu = true;
			g_ctx.cheat_init = false;
			g_ctx.cheat_init2 = true;
		}

		g_ctx.update_animations();

		g_render->init(device);
		g_render->start_render(device); {
			imgui_blur::set_device(device);
			imgui_blur::new_frame();

			g_visuals_wrapper->on_directx();
			g_menu->draw();
		}
		g_render->end_render(device);

		device->SetRenderState(D3DRS_COLORWRITEENABLE, colorwrite);
		device->SetRenderState(D3DRS_SRGBWRITEENABLE, srgbwrite);

		return original_present(device, src_rect, dest_rect, window_override, dirty_region);
	}

	HRESULT __stdcall reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) {
		if (g_ctx.uninject)
			return original_reset(device, params);

		imgui_blur::on_device_reset();

		g_render->invalidate_objects();

		auto hr = original_reset(device, params);
		if (hr >= 0) {
			imgui_blur::set_device(device);
			imgui_blur::create_textures();

			g_render->create_objects();
		}

		return hr;
	}
}