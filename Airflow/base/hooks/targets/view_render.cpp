#include "../hooks.h"
#include "../../../includes.h"

#include "../../sdk.h"
#include "../../global_context.h"
#include "../../../functions/config_vars.h"
#include "../../../functions/extra/world_modulation.h"

#include "../../../functions/anti hit/exploits.h"

#include "../../../functions/ragebot/engine_prediction.h"
#include "../../../functions/ragebot/ragebot.h"

#include "../../../functions/visuals/visuals.h"
#include "../../../functions/visuals/event/event_visuals.h"

#include "../../../functions/ragebot/animfix.h"

#include <string>

namespace tr::view_render {
	void __fastcall on_render_start(void* ecx, void* edx) {
		static auto original = vtables[vtables_t::view_render_].original<on_render_start_fn>(xor_int(4));
		original(ecx);

		auto ptr = (c_view_render*)ecx;
		if (!g_ctx.in_game || !g_ctx.local || !g_ctx.weapon || !g_ctx.weapon_info || !g_ctx.local->is_alive()) {
			ptr->view.fov = 90.f + g_cfg.misc.fovs[world];
			ptr->view.fov_viewmodel = cvars::viewmodel_fov->get_float() + g_cfg.misc.fovs[arms];

			g_ctx.current_fov = ptr->view.fov;
			return;
		}

		float fov = 90.f + g_cfg.misc.fovs[world];
		bool invalid_wpn = g_ctx.weapon->item_definition_index() == weapon_sg556 ||
			g_ctx.weapon->item_definition_index() == weapon_aug;

		int m_zoomLevel = g_ctx.weapon->zoom_level();
		if (m_zoomLevel > 1 && g_cfg.misc.skip_second_zoom)
			m_zoomLevel = 1;

		// we calc difference between in-game fov and custom
		// and then we add it to current fov and calc it in percentage
		float zoom_fov = g_ctx.weapon->get_zoom_fov(m_zoomLevel) + g_cfg.misc.fovs[world];
		float fov_delta = fov - zoom_fov;
		float total_fov = fov_delta * (1.f - g_cfg.misc.fovs[zoom] * 0.01f);

		if (g_ctx.local->is_scoped() && !invalid_wpn) {
			float out = zoom_fov + total_fov;
			if (m_zoomLevel > 0)
				fov = out / m_zoomLevel;
		}

		ptr->view.fov = fov;
		ptr->view.fov_viewmodel = cvars::viewmodel_fov->get_float() + g_cfg.misc.fovs[arms];

		g_ctx.current_fov = ptr->view.fov;
	}

	void __fastcall render_2d_effects_post_hud(void* ecx, void* edx, const c_view_setup& setup) {
		static auto original = vtables[vtables_t::view_render_].original<render_2d_effects_post_hud_fn>(xor_int(39));

		if (g_cfg.misc.removals & flash)
			return;

		original(ecx, setup);
	}

	void __fastcall render_smoke_overlay(void* ecx, void* edx, bool unk) {
		static auto original = vtables[vtables_t::view_render_].original<render_smoke_overlay_fn>(xor_int(40));

		if (g_cfg.misc.removals & flash)
			return;

		original(ecx, unk);
	}
}