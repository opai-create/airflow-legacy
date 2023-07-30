#include "local_animfix.h"

#include "../features.h"

#include "../config_vars.h"

void c_local_animation_fix::update_fake() {
	// don't need to create state and other shit when chams disabled
	// affects on performance
	if (!g_cfg.visuals.chams[c_fake].enable) {
		if (state) {
			interfaces::memory->free(state);
			state = nullptr;
		}
		reset = false;
		return;
	}

	// force recreate animstate when player spawned
	if (old_spawn != g_ctx.local->spawn_time()) {
		interfaces::memory->free(state);
		state = nullptr;

		old_spawn = g_ctx.local->spawn_time();
		reset = false;
		return;
	}

	// force recreate animstate when handle changed (disconnect / map change)
	if (old_handle != g_ctx.local->get_ref_handle()) {
		interfaces::memory->free(state);
		state = nullptr;

		old_handle = g_ctx.local->get_ref_handle();
		reset = false;
		return;
	}

	// don't do anything when game is frozen
	if (interfaces::client_state->delta_tick == -1)
		return;

	if (!reset) {
		if (!state)
			state = (c_animstate*)interfaces::memory->alloc(sizeof(c_animstate));

		state->create(g_ctx.local);
		reset = true;
	}

	if (!reset || !state)
		return;

	if (state->last_update_time == interfaces::global_vars->cur_time)
		state->last_update_time = interfaces::global_vars->cur_time - 1.f;

	if (state->last_update_frame == interfaces::global_vars->frame_count)
		state->last_update_frame = interfaces::global_vars->frame_count - 1;

	auto old_angle = g_ctx.local->render_angles();

	g_ctx.local->render_angles() = g_ctx.cur_angle;

	state->update(g_ctx.fake_angle);

	local_info.fakebuild.store(g_ctx.local, local_info.matrix_fake, 0x7FF00);
	local_info.fakebuild.setup();

	vector3d render_origin = g_ctx.local->get_render_origin();
	math::change_matrix_position(local_info.matrix_fake, 128, render_origin, vector3d());

	g_ctx.local->render_angles() = old_angle;
}

void c_local_animation_fix::update_strafe_state() {
	auto state = g_ctx.local->animstate();
	if (!state)
		return;

	int buttons = g_ctx.cmd->buttons;

	vector3d forward;
	vector3d right;
	vector3d up;

	math::angle_to_vectors(vector3d(0, state->abs_yaw, 0), forward, right, up);
	right = right.normalized();

	auto velocity = state->velocity_normalized_non_zero;
	auto speed = state->speed_as_portion_of_walk_top_speed;

	float vel_to_right_dot = velocity.dot(right);
	float vel_to_foward_dot = velocity.dot(forward);

	bool move_right = (buttons & (in_moveright)) != 0;
	bool move_left = (buttons & (in_moveleft)) != 0;
	bool move_forward = (buttons & (in_forward)) != 0;
	bool move_backward = (buttons & (in_back)) != 0;

	bool strafe_right = (speed >= 0.73f && move_right && !move_left && vel_to_right_dot < -0.63f);
	bool strafe_left = (speed >= 0.73f && move_left && !move_right && vel_to_right_dot > 0.63f);
	bool strafe_forward = (speed >= 0.65f && move_forward && !move_backward && vel_to_foward_dot < -0.55f);
	bool strafe_backward = (speed >= 0.65f && move_backward && !move_forward && vel_to_foward_dot > 0.55f);

	g_ctx.local->strafing() = (strafe_right || strafe_left || strafe_forward || strafe_backward);
}

void c_local_animation_fix::update_viewmodel() {
	auto viewmodel = g_ctx.local->get_view_model();
	if (viewmodel)
		func_ptrs::update_all_viewmodel_addons(viewmodel);
}

void c_local_animation_fix::on_predict_end() {
	if (!g_ctx.cmd || !g_ctx.local || !g_ctx.local->is_alive())
		return;

	auto state = g_ctx.local->animstate();
	if (!state)
		return;

	if (interfaces::client_state->delta_tick == -1) {
		g_ctx.local->force_update();
		g_ctx.setup_bones[g_ctx.local->index()] = true;
		return;
	}

	if ((g_exploits->recharge && !g_exploits->recharge_finish) || g_exploits->hs_works || g_exploits->lag_shift)
		return;

	if (interfaces::client_state->choked_commands)
		return;

	g_ctx.setup_bones[g_ctx.local->index()] = false;

	this->update_fake();

	//g_ctx.local->draw_server_hitbox();

	static float anim_time = 0.f;

	anim_time = interfaces::global_vars->cur_time;

	auto old_angle = g_ctx.local->render_angles();

	g_ctx.local->render_angles() = g_ctx.cur_angle;
	g_ctx.local->lby() = local_info.lby_angle;

	// force update
	this->update_strafe_state();
	this->update_viewmodel();

	g_ctx.local->force_update();

	local_info.realbuild.store(g_ctx.local, local_info.matrix, 0x7FF00);

	// custom animation changers for visual purposes
	static float last_air_time = 0.f;
	if (!g_utils->on_ground()) {
		// ghetto method but animstate broken idk 
		last_air_time = interfaces::global_vars->cur_time;
	}
	// zero pitch on land
	else if (g_cfg.misc.animation_changes & 1 && std::abs(interfaces::global_vars->cur_time - last_air_time) < 1.f)
		local_info.realbuild.poses[12] = 0.5f;

	// break leg movement
	if (g_cfg.misc.animation_changes & 2)
		local_info.realbuild.poses[0] = 1.f;

	local_info.realbuild.setup();

	// start correcting bone pos
	vector3d render_origin = g_ctx.local->get_render_origin();
	math::change_matrix_position(local_info.matrix, 128, render_origin, vector3d());

	if (!local_info.on_ground && state->on_ground) {
		local_info.lby_angle = g_ctx.cur_angle.y;
		local_info.last_lby_time = anim_time;
	}
	else if (state->velocity_length_xy > 0.1f) {
		if (state->on_ground)
			local_info.lby_angle = g_ctx.cur_angle.y;

		local_info.last_lby_time = anim_time + 0.22f;
	}
	else if (anim_time > local_info.last_lby_time) {
		local_info.lby_angle = g_ctx.cur_angle.y;
		local_info.last_lby_time = anim_time + 1.1f;
	}

	local_info.on_ground = state->on_ground;
	local_info.last_lby_tick = interfaces::global_vars->interval_per_tick;

	g_ctx.local->render_angles() = old_angle;
}

void c_local_animation_fix::on_update_anims(c_csplayer* ecx) {
	if (!g_ctx.in_game || !ecx || interfaces::client_state->delta_tick == -1)
		return;

	if (!ecx->is_alive()) {
		g_ctx.setup_bones[ecx->index()] = true;
		return;
	}

	ecx->invalidate_bone_cache();

	vector3d render_origin = ecx->get_render_origin();
	math::change_matrix_position(local_info.matrix, 128, vector3d(), render_origin);

	ecx->interpolate_moveparent_pos();
	ecx->set_bone_cache(local_info.matrix);
	ecx->attachments_helper();

	math::change_matrix_position(local_info.matrix, 128, render_origin, vector3d());
}

void c_local_animation_fix::on_game_events(c_game_event* event) {
	if (std::strcmp(event->get_name(), xor_c("round_start")))
		return;

	local_info.reset();
}

void c_local_animation_fix::on_local_death() {
	local_info.reset();
}