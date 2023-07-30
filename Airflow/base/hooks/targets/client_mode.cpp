#include "../hooks.h"
#include "../../../includes.h"

#include "../../sdk.h"
#include "../../global_context.h"
#include "../../../functions/config_vars.h"

#include "../../../functions/features.h"

#include "../../../base/sdk/entity.h"

#include <string>

void call_createmove_features(c_usercmd* cmd) {
	g_ctx.update_local_player();

	interfaces::engine->get_view_angles(g_ctx.base_angle);
	interfaces::engine->get_view_angles(g_ctx.orig_angle);

	g_ctx.lagcomp = cvars::cl_lagcompensation->get_int();
	g_ctx.lerp_time = g_animation_fix->get_lerp_time();

	auto netchannel = interfaces::engine->get_net_channel_info();
	if (netchannel)
		g_ctx.ping = netchannel->get_latency(flow_incoming) + netchannel->get_latency(flow_outgoing);

	g_utils->clantag();
	if (g_ctx.local && g_ctx.local->is_alive()) {
		g_ctx.sidemove = g_ctx.cmd->sidemove;
		g_ctx.forwardmove = g_ctx.cmd->forwardmove;

		if (g_ctx.weapon) {
			if (g_ctx.weapon->is_grenade())
				g_ctx.predicted_curtime = math::ticks_to_time(g_ctx.tick_base);
			else
				g_ctx.predicted_curtime = math::ticks_to_time(g_ctx.tick_base - g_exploits->tickbase_offset());

			g_ctx.abs_origin = g_ctx.local->get_abs_origin();

			g_utils->on_pre_predict();

			g_rage_bot->on_pre_predict();

			g_anti_aim->on_pre_predict();
			g_movement->on_pre_predict();

			g_ctx.modify_eye_pos = true;

			g_rage_bot->start_stop();
			g_movement->on_predict_start();

			g_anti_aim->on_predict_start();
			g_utils->on_predict_start();

			g_local_animation_fix->on_predict_end();
			g_utils->on_predict_end();

			g_ctx.modify_eye_pos = false;
		}

		g_anti_aim->on_predict_end();
	}
}

namespace tr::client_mode {
	void __fastcall override_view(void* ecx, void* edx, c_view_setup* setup) {
		static auto original = vtables[vtables_t::client_mode].original<override_view_fn>(xor_int(18));

		g_local_visuals->on_render_view(setup);
		original(ecx, setup);
		g_local_visuals->on_render_view_after(setup);
	}

	int __fastcall set_view_model_offsets(void* ecx, void* edx, int something, float x, float y, float z) {
		static auto original = hooker.original(&set_view_model_offsets);

		if (g_ctx.local && g_ctx.local->is_alive()) {
			x += g_cfg.misc.viewmodel_pos[0];
			y += g_cfg.misc.viewmodel_pos[1];
			z += g_cfg.misc.viewmodel_pos[2];
		}

		return original(ecx, edx, something, x, y, z);
	}

	bool __fastcall draw_fog(void* ecx, void* edx) {
		static auto original = hooker.original(&draw_fog);

		if ((g_cfg.misc.removals & fog_) && !g_cfg.misc.custom_fog)
			return false;

		return original(ecx, edx);
	}

	bool __fastcall do_post_screen_effects(void* ecx, void* edx, c_view_setup* setup) {
		static auto original = vtables[vtables_t::client_mode].original<do_post_screen_effects_fn>(xor_int(44));
		g_chams->on_post_screen_effects();
		g_glow_esp->on_post_screen_effects();

		return original(ecx, setup);
	}

	bool __stdcall create_move(float a, c_usercmd* cmd) {
		static auto original = vtables[vtables_t::client_mode].original<create_move_fn>(xor_int(24));
		if (!cmd || !cmd->command_number)
			return original(interfaces::client_mode, a, cmd);

		g_ctx.update_local_player();

		if (g_ctx.is_alive && g_exploits->cl_move.shifting) {
			g_ctx.cmd = cmd;

			call_createmove_features(cmd);

			*g_ctx.send_packet = interfaces::client_state->choked_commands >= g_ctx.max_choke;

			return false;
		}

		// update our prediction
		// https://www.unknowncheats.me/forum/counterstrike-global-offensive/261943-fix-prediction-fps-lower-tickrate.html
		g_engine_prediction->update();

		g_ctx.cmd = cmd;
		g_ctx.lagcomp = cvars::cl_lagcompensation->get_int();
		g_ctx.lerp_time = g_animation_fix->get_lerp_time();

		auto netchannel = interfaces::engine->get_net_channel_info();
		if (netchannel)
			g_ctx.ping = netchannel->get_latency(flow_incoming) + netchannel->get_latency(flow_outgoing);	

		g_utils->clantag();
		if (g_ctx.local && g_ctx.local->is_alive()) {
			if (original(interfaces::client_mode, a, cmd)) {
				interfaces::engine->set_view_angles(cmd->viewangles);
				interfaces::prediction->set_local_view_angles(cmd->viewangles);
			}

			interfaces::engine->get_view_angles(g_ctx.base_angle);
			interfaces::engine->get_view_angles(g_ctx.orig_angle);

			g_ctx.sidemove = g_ctx.cmd->sidemove;
			g_ctx.forwardmove = g_ctx.cmd->forwardmove;
			g_ctx.health = g_ctx.local->health();

			// predict ticbase on low fps
			// https://github.com/rollraw/qo0-base/blob/master/base/features/prediction.cpp#L172
			g_ctx.tick_base = [&]() {
				static int tick_base = 0;

				if (cmd != nullptr) {
					static c_usercmd* last_cmd = nullptr;

					// if command was not predicted - increment tickbase
					if (last_cmd == nullptr || last_cmd->predicted)
						tick_base = g_ctx.local->tickbase();
					else
						tick_base++;

					last_cmd = cmd;
				}

				return tick_base;
			}();

			// important note:
			// call fakelags only after engine prediction
			// cuz choking packets change tickbase and everything fucks up

			if (g_exploits->recharging(g_ctx.cmd)) {

				if (g_ctx.weapon)
					g_local_animation_fix->on_predict_end();

				return false;
			}

			if (g_ctx.weapon) {
				if (g_ctx.weapon->is_grenade())
					g_ctx.predicted_curtime = math::ticks_to_time(g_ctx.tick_base);
				else
					g_ctx.predicted_curtime = math::ticks_to_time(g_ctx.tick_base - g_exploits->tickbase_offset());

				g_ctx.abs_origin = g_ctx.local->get_abs_origin();

				g_utils->on_pre_predict();
				g_exploits->on_pre_predict();

				g_rage_bot->on_pre_predict();

				float cone = g_ctx.weapon->get_inaccuracy() + g_ctx.weapon->get_spread();
				cone *= g_render->screen_size.h * 0.7f;

				g_ctx.spread = cone;

				float zoom_substract = 0.f;
				if ((g_ctx.weapon->item_definition_index() == weapon_ssg08 || g_ctx.weapon->item_definition_index() == weapon_awp) && !g_ctx.local->is_scoped())
					zoom_substract = 25.f;
				else
					zoom_substract = 24.f;

				g_ctx.ideal_spread = std::clamp(26.f - zoom_substract, 0.f, 26.f);

				g_anti_aim->on_pre_predict();
				g_movement->on_pre_predict();

				g_ctx.modify_eye_pos = true;

				g_rage_bot->start_stop();
				g_movement->on_predict_start();

				g_engine_prediction->start(g_ctx.local, g_ctx.cmd); {
					g_fake_lag->on_predict_start();
					g_rage_bot->on_predict_start();
					g_exploits->on_predict_start();
					g_anti_aim->on_predict_start();
					g_utils->on_predict_start();

					g_local_animation_fix->on_predict_end();
				}
				g_engine_prediction->finish(g_ctx.local);

				g_utils->on_predict_end();


				g_ctx.modify_eye_pos = false;
			}

			g_anti_aim->on_predict_end();
			g_fake_lag->on_predict_end();

			if (*g_ctx.send_packet)
				g_ctx.sent_tick_count = interfaces::global_vars->tick_count;

			return false;
		}
		else {
			g_rage_bot->on_local_death();
			g_fake_lag->on_local_death();
			g_local_animation_fix->on_local_death();

			g_ctx.predicted_curtime = interfaces::global_vars->cur_time;
			g_ctx.health = 0;
		}

		return original(interfaces::client_mode, a, cmd);
	}
}