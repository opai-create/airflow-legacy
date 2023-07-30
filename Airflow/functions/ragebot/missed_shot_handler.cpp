#include "ragebot.h"

#include "../features.h"

enum missed_shot_type_t {
	miss_spread,
	miss_occlusion,
	miss_resolver,
	miss_unk,
};

void c_rage_bot::add_shot_record(c_csplayer* player, const point_t& best) {
	aim_shot_record_t* shot = new aim_shot_record_t();
	shot->time = math::ticks_to_time(g_ctx.tick_base);
	shot->init_time = 0.f;
	shot->impact_fire = false;
	shot->fire = false;
	shot->damage = -1;
	shot->start = g_ctx.eye_position;
	shot->hitgroup = -1;
	shot->hitchance = best.hitchance;
	shot->hitbox = best.hitbox;
	shot->record = *best.record;
	shot->resolver = resolver::info[player->index()];
	shot->player = player;
	shot->point = best.position;

	if (g_cfg.visuals.eventlog.logs & 4) {
		const auto& name = player->get_name();

		int diff = math::time_to_ticks(std::abs(best.record->sim_time - player->simulation_time()));

		std::string out = xor_str("SHOT")
			+ xor_str(" to ")
			+ name
			+ xor_str(", ")
			+ xor_str("hitbox: ")
			+ cheat_tools::hitbox_to_string(best.hitbox)
			+ xor_str(", ")
			+ xor_str("hc: ")
			+ std::to_string((int)(best.hitchance * 100.f))
			+ xor_str(", ")
			+ xor_str("dmg: ")
			+ std::to_string(best.damage)
			+ xor_str(", ")
			+ xor_str("bt: ")
			+ std::to_string(diff)
			+ xor_str(", ")
			+ xor_str("priority: ")
			+ std::to_string(best.priority_level)
			+ (shot->resolver.valid ? (xor_str(", res: ")
				+ shot->resolver.mode) : "");

		g_event_logger->add_message(out, -1, true);
	}

	shots.emplace_back(*shot);
	delete shot;
}

void c_rage_bot::on_game_events(c_game_event* event) {
	if (!g_ctx.in_game)
		return;

	if (!std::strcmp(event->get_name(), xor_c("weapon_fire"))) {
		if (!g_ctx.local || !g_ctx.local->is_alive())
			return;

		if (shots.empty())
			return;

		auto& shot = shots.front();
		if (!shot.fire)
			shot.fire = true;
	}

	if (!std::strcmp(event->get_name(), xor_c("bullet_impact"))) {
		if (!g_ctx.local || !g_ctx.local->is_alive())
			return;

		if (shots.empty())
			return;

		auto& shot = shots.front();

		if (interfaces::engine->get_player_for_user_id(event->get_int(xor_c("userid"))) != interfaces::engine->get_local_player())
			return;

		const vector3d vec_impact{
			event->get_float(xor_c("x")),
			event->get_float(xor_c("y")),
			event->get_float(xor_c("z"))
		};

		bool check = false;
		if (shot.impact_fire) {
			if (shot.start.dist_to(vec_impact) > shot.start.dist_to(shot.impact))
				check = true;
		}
		else
			check = true;

		if (!check)
			return;

		shot.impact_fire = true;
		shot.init_time = math::ticks_to_time(g_ctx.tick_base - g_exploits->tickbase_offset());
		shot.impact = vec_impact;
	}

	if (!std::strcmp(event->get_name(), xor_c("player_hurt"))) {
		if (!g_ctx.local || !g_ctx.local->is_alive())
			return;

		if (interfaces::engine->get_player_for_user_id(event->get_int(xor_c("attacker"))) != interfaces::engine->get_local_player())
			return;

		int user_id = interfaces::engine->get_player_for_user_id(event->get_int(xor_c("userid")));

		c_csplayer* player = (c_csplayer*)interfaces::entity_list->get_entity(user_id);
		if (!player)
			return;

		int group = event->get_int(xor_c("hitgroup"));
		int dmg_health = event->get_int(xor_c("dmg_health"));
		int health = event->get_int(xor_c("health"));

		if (!shots.empty()) {
			auto& shot = shots.front();
			shots.erase(shots.begin());
		}

		if (g_cfg.visuals.eventlog.logs & 1) {
			std::string message{};

			if (group != hitgroup_generic && group != hitgroup_gear) {
				message += xor_c("in ");
				message += player->get_name();
				message += xor_c("'s ");
				message += cheat_tools::hitgroup_to_string(group);
			}
			else
				message += player->get_name();

			message += xor_c(" for ");
			message += std::to_string(dmg_health);
			message += xor_c("HP");

			g_event_logger->add_message(message, event_hit);
		}
	}

	if (!std::strcmp(event->get_name(), xor_c("round_start"))) {
		for (auto& m : missed_shots)
			m = 0;

		for (auto& cache : aim_cache) {
			if (!cache.points.empty())
				cache.points.clear();

			if (cache.best_point.filled)
				cache.best_point.reset();

			if (cache.player)
				cache.player = nullptr;
		}

		if (!g_ctx.round_start)
			g_ctx.round_start = true;

		if (g_cfg.visuals.eventlog.logs & 4)
			g_event_logger->add_message(xor_str("===============> ROUND STARTED <==============="), -1, true);
	}
}

void c_rage_bot::on_pre_predict() {
	if (shots.empty())
		return;

	auto time = math::ticks_to_time(g_ctx.tick_base - g_exploits->tickbase_offset());

	auto& shot = shots.front();
	if (std::abs(time - shot.time) > 1.f) {
		shots.erase(shots.begin());
		return;
	}

	if (shot.init_time != -1.f) {
		auto player = shot.record.ptr;

		if (shot.damage == -1 && shot.fire && shot.impact_fire && player) {
			bool spread = false;
			const auto studio_model = interfaces::model_info->get_studio_model(player->get_model());
			int idx = player->index();

			if (studio_model) {
				const auto end = shot.impact;
				if (!cheat_tools::can_hit_hitbox(shot.start, end, player, shot.hitbox, &shot.record))
					spread = true;
			}

			auto get_miss_type = [&]() {
				auto& resolver_info = resolver::info[idx];

				if (spread) {
					float dist = shot.start.dist_to(shot.impact);
					float dist2 = shot.start.dist_to(shot.point);

					if (dist2 > dist)
						return miss_occlusion;

					return miss_spread;
				}
				else if (resolver_info.valid)
					return miss_resolver;
				else
					return miss_unk;
			};

			int miss_type = get_miss_type();
			if (miss_type == miss_resolver || miss_type == miss_unk)
				missed_shots[idx]++;

			if (g_cfg.visuals.eventlog.logs & 2) {
				auto get_miss_message = [&]() {
					switch (miss_type) {
					case miss_spread:
						return xor_str("due to spread");
						break;
					case miss_occlusion:
						return xor_str("due to occlusion");
						break;
					case miss_resolver:
						return xor_str("due to resolver");
						break;
					case miss_unk:
						return xor_str("due to unknown reason");
						break;
					}
				};

				g_event_logger->add_message(get_miss_message(), event_miss);
			}

			shots.erase(shots.begin());
		}
	}
}
