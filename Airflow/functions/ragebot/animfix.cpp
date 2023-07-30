#include "animfix.h"
#include "../features.h"
#include "../../additionals/threading/threading.h"

void c_animation_fix::anim_player_t::simulate_animation_side(records_t* record) {
	auto state = ptr->animstate();
	if (!state)
		return;

	if (!last_record || record->choke < 2) {
		ptr->abs_velocity() = ptr->velocity() = record->velocity;

		if (!teammate)
			resolver::start(ptr, record);

		this->force_update();
	}
	else {
		if (record->on_ground)
			ptr->flags() |= fl_onground;
		else
			ptr->flags() &= ~fl_onground;

		ptr->abs_velocity() = ptr->velocity() = record->anim_velocity;

		if (state->on_ground && state->velocity_length_xy <= 0.1f && !state->landing && state->last_update_increment > 0.f) {
			float delta = math::normalize(std::abs(state->abs_yaw - state->abs_yaw_last));
			if ((delta / state->last_update_increment) > 120.f) {
				record->sim_orig.layers[3].cycle = record->sim_orig.layers[3].weight = 0.f;
				record->sim_orig.layers[3].sequence = ptr->get_sequence_activity(979);
			}
		}

		if (!teammate)
			resolver::start(ptr, record);

		this->force_update();
	}

	ptr->set_layer(record->sim_orig.layers);
	this->build_bones(record, &record->sim_orig);
}

bool records_t::is_valid() {
	if (!g_ctx.lagcomp)
		return true;

	if (!valid)
		return false;

	float outgoing = interfaces::engine->get_net_channel_info()->get_latency(flow_outgoing);
	float time = g_ctx.local->is_alive() ? g_ctx.predicted_curtime : interfaces::global_vars->cur_time;
	float correct = std::clamp(g_ctx.lerp_time + g_ctx.ping, 0.f, 1.f);

	float delta_time = std::fabs(correct - (time - sim_time));
	if (delta_time > 0.2f)
		return false;

	return true;
}

void c_animation_fix::anim_player_t::build_bones(records_t* record, records_t::simulated_data_t* sim) {
	ptr->invalidate_bone_cache();
	ptr->disable_interpolation();

	auto old_origin = ptr->get_abs_origin();
	ptr->set_abs_origin(ptr->origin());

	auto old_ik = ptr->ik_context();
	ptr->ik_context() = 0;

	auto old_flags = ptr->ent_flags();
	ptr->ent_flags() |= 2;

	g_ctx.setup_bones[ptr->index()] = true;
	ptr->setup_bones(sim->bone, 128, 0x7FF00, record->sim_time);
	g_ctx.setup_bones[ptr->index()] = false;

	ptr->ik_context() = old_ik;
	ptr->ent_flags() = old_flags;

	ptr->set_abs_origin(old_origin);

	ptr->enable_interpolation();
}

void c_animation_fix::anim_player_t::update_animations() {
	backup_record.update_record(ptr);

	ptr->update_weapon_dispatch_layers();

	if (records.size() > 0) {
		last_record = &records.front();

		if (records.size() >= 3)
			old_record = &records[2];
	}

	auto& record = records.emplace_front();
	record.update_record(ptr);
	record.update_dormant(dormant_ticks);
	record.update_shot(last_record);

	if (dormant_ticks < 3)
		dormant_ticks++;
	
	this->update_land(&record);
	this->update_velocity(&record);
	this->simulate_animation_side(&record);

	backup_record.restore(ptr);

	if (g_ctx.lagcomp && last_record) {
		if (last_record->sim_time > record.sim_time) {
			next_update_time = record.sim_time + std::abs(last_record->sim_time - record.sim_time) + math::ticks_to_time(1);
			record.valid = false;
		}
		else {
			if (math::time_to_ticks(std::abs(next_update_time - record.sim_time)) > 17)
				next_update_time = -1.f;

			if (next_update_time > record.sim_time)
				record.valid = false;
		}
	}

	const auto records_size = teammate ? 3 : g_ctx.tick_rate;
	while (records.size() > records_size)
		records.pop_back();
}

void c_animation_fix::on_update_anims(c_csplayer* player_ptr) {
	if (!g_ctx.in_game || !player_ptr)
		return;

	if (!player_ptr->is_alive()) {
		g_ctx.setup_bones[player_ptr->index()] = true;
		return;
	}

	auto animation_player = this->get_animation_player(player_ptr->index()); 
	if (animation_player->dormant_ticks < 3) {
		g_ctx.setup_bones[player_ptr->index()] = true;
		return;
	}

	auto anim = this->get_animation_player(player_ptr->index());
	if (!anim || anim->records.empty()) {
		g_ctx.setup_bones[player_ptr->index()] = true;
		return;
	}

	auto first_record = &anim->records.front();
	if (!first_record) {
		g_ctx.setup_bones[player_ptr->index()] = true;
		return;
	}

	std::memcpy(first_record->render_bones, first_record->sim_orig.bone, sizeof(first_record->render_bones));
	math::change_matrix_position(first_record->render_bones, 128, first_record->origin, player_ptr->get_render_origin());

	player_ptr->invalidate_bone_cache();

	player_ptr->interpolate_moveparent_pos();

	player_ptr->set_bone_cache(first_record->render_bones);
	player_ptr->attachments_helper();
}

void thread_anim_update(c_animation_fix::anim_player_t* player) {
	player->update_animations();
}

void c_animation_fix::on_net_update_end(int stage) {

	if (!g_ctx.in_game || !g_ctx.local || g_ctx.uninject)
		return;

	if (stage != frame_net_update_postdataupdate_end)
		return;

	auto& players = g_listener_entity->get_entity(ent_player);
	if (players.empty())
		return;

	for (auto& player : players) {
		auto ptr = (c_csplayer*)player.m_entity;
		if (!ptr)
			continue;

		if (ptr == g_ctx.local)
			continue;

		auto anim_player = this->get_animation_player(ptr->index());
		if (anim_player->ptr != ptr) {
			anim_player->reset_data();
			anim_player->ptr = ptr;
			continue;
		}

		if (!ptr->is_alive()) {
			if (!anim_player->teammate) {
				resolver::reset_info(ptr);
				g_rage_bot->missed_shots[ptr->index()] = 0;
			}

			anim_player->ptr = nullptr;
			continue;
		}

		if (g_cfg.misc.force_radar && ptr->team() != g_ctx.local->team())
			ptr->target_spotted() = true;

		if (ptr->dormant()) {
			anim_player->dormant_ticks = 0;

			if (!anim_player->teammate)
				resolver::reset_info(ptr);
			continue;
		}

		if (ptr->simulation_time() == ptr->old_simtime())
			continue;

		auto& layer = ptr->anim_overlay()[11];
		if (layer.cycle == anim_player->old_aliveloop_cycle)
			continue;

		anim_player->old_aliveloop_cycle = layer.cycle;

		auto state = ptr->animstate();
		if (!state)
			continue;

		if (anim_player->old_spawn_time != ptr->spawn_time()) {
			state->player = ptr;
			state->reset();

			anim_player->old_spawn_time = ptr->spawn_time();
		}

		anim_player->teammate = g_ctx.local && g_ctx.local->team() == ptr->team();
		anim_player->update_animations();
	}
}
