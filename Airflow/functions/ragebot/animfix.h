#pragma once
#include <memory>
#include <deque>
#include <unordered_map>

#include "../../base/sdk/c_animstate.h"
#include "../../base/sdk/entity.h"

#include "../../base/tools/math.h"
#include "../../base/tools/memory/displacement.h"

#include "../../base/other/game_functions.h"

#include "setup_bones_manager.h"

#define backup_globals(name) float prev_##name = interfaces::global_vars->##name
#define restore_globals(name) interfaces::global_vars->##name = prev_##name

struct records_t {
	struct simulated_data_t {
		c_bone_builder		builder{};
		matrix3x4_t			bone[128]{};
		c_animation_layers	layers[13]{};

		__forceinline void reset() {
			std::memset(layers, 0, sizeof(layers));
			std::memset(bone, 0, sizeof(bone));
		}
	};

	bool				valid{};
	bool				dormant{};
	bool				lby_update{};
	c_csplayer*			ptr{};
	int					flags{};
	int					eflags{};
	int					choke{};
	int					last_lerp_update{};
	int					lerp{};
	float				duck_amount{};
	float				lby{};
	float				old_lby{};
	float				thirdperson_recoil{};
	float				interp_time{};
	float				sim_time{};
	float				old_sim_time{};
	float				anim_time{};
	float				anim_speed{};
	float				lby_time{};
	bool				on_ground{};
	bool				real_on_ground{};
	bool				shooting{};
	bool				fake_walking{};
	vector3d			abs_origin{};
	vector3d			origin{};
	vector3d			velocity{};
	vector3d			anim_velocity{};
	vector3d			mins{};
	vector3d			maxs{};
	vector3d			eye_angles{};
	vector3d			abs_angles{};
	simulated_data_t	sim_orig{};
	matrix3x4_t			render_bones[128]{};

	__forceinline void update_record(c_csplayer* player) {
		valid					= true;
		ptr						= player;
		flags					= player->flags();
		eflags					= player->e_flags();
		duck_amount				= player->duck_amount();
		lby						= player->lby();
		thirdperson_recoil		= player->thirdperson_recoil();
		sim_time				= player->simulation_time();
		old_sim_time			= player->old_simtime();
		abs_origin				= player->get_abs_origin();
		origin					= player->origin();
		velocity				= player->velocity();
		mins					= player->bb_mins();
		maxs					= player->bb_maxs();
		eye_angles				= player->eye_angles();
		abs_angles				= player->get_abs_angles();

		float time_delta		= std::max<float>(interfaces::global_vars->interval_per_tick, sim_time - old_sim_time);
		choke					= std::clamp(math::time_to_ticks(time_delta), 0, 17);

		anim_speed				= 0.f;
		fake_walking			= false;
		lby_update				= false;

		anim_time				= interfaces::global_vars->cur_time;

		player->store_layer(sim_orig.layers);

		on_ground				= flags & fl_onground;
	}

	__forceinline void update_shot(records_t* last) {
		auto weapon = ptr->get_active_weapon();
		if (last && weapon) {
			float last_shot_time	= weapon->last_shot_time();
			shooting				= sim_time >= last_shot_time && last_shot_time > last->sim_time;
		}
	}

	__forceinline void update_dormant(int dormant_ticks) {
		dormant = dormant_ticks <= 1;
	}
 
	__forceinline void restore(c_csplayer* player) {
		player->velocity()				= velocity;
		player->flags()					= flags;
		player->duck_amount()			= duck_amount;

		player->set_layer(sim_orig.layers);
	
		player->lby()	= lby;
		player->thirdperson_recoil()		= thirdperson_recoil;
		
		player->set_abs_origin(origin);
	}

	bool is_valid();

	__forceinline void reset() {
		valid				= false;
		dormant				= false;
		ptr					= nullptr;
		flags				= 0;
		eflags				= 0;
		choke				= 0;
		duck_amount			= 0.f;
		lby					= 0.f;
		thirdperson_recoil	= 0.f;
		sim_time			= 0.f;
		old_sim_time		= 0.f;
		anim_time			= 0.f;
		anim_speed			= 0.f;
		interp_time			= 0.f;
		lby_time			= 0.f;
		old_lby				= 0.f;

		on_ground			= false;
		shooting			= false;
		fake_walking		= false;
		real_on_ground		= false;
		lby_update			= false;

		origin.reset();
		velocity.reset();
		mins.reset();
		maxs.reset();
		abs_origin.reset();
		eye_angles.reset();
		abs_angles.reset();
		anim_velocity.reset();

		sim_orig.reset();

		std::memset(render_bones, 0, sizeof(render_bones));
	}
};

class c_animation_fix {
public:
	struct anim_player_t {
		bool					teammate{};
		int						dormant_ticks{};

		float					old_spawn_time{};
		float					next_update_time{};
		float					old_aliveloop_cycle{};

		c_csplayer*				ptr{};

		records_t				backup_record{};
		records_t*				last_record{};
		records_t*				old_record{};

		std::deque<records_t>	records{};

		__forceinline void reset_data() {
			teammate			= false;
			dormant_ticks		= 0;

			old_spawn_time		= 0.f;
			next_update_time	= 0.f;
			old_aliveloop_cycle = 0.f;

			ptr					= nullptr;
			last_record			= nullptr;
			old_record			= nullptr;

			records.clear();
			backup_record.reset();
		}

		// pasted from supremacy and onetap
		// too lazy to work at it
		__forceinline void update_land(records_t* record) {
			if (!last_record)
				return;

			bool on_ground = ptr->flags() & fl_onground;
			record->on_ground = false;
			record->real_on_ground = on_ground;

			if (on_ground && last_record->real_on_ground) {
				record->on_ground = true;
			}
			else {
				if (record->sim_orig.layers[4].weight != 1.f
					&& record->sim_orig.layers[4].weight == 1.f
					&& record->sim_orig.layers[5].weight != 0.f) {
					record->on_ground = true;
				}

				if (on_ground) {
					bool ground = record->on_ground;
					if (!last_record->real_on_ground)
						ground = false;
					record->on_ground = ground;
				}
			}
		}

		__forceinline void update_velocity(records_t* record) {
			if (record->choke > 0 && record->choke < 16 && last_record && !last_record->dormant)
				record->velocity = (record->origin - last_record->origin) * (1.f / math::ticks_to_time(record->choke));

			record->anim_velocity = record->velocity;

			if (record->flags & fl_onground && record->velocity.length(false) > 0.1f && record->sim_orig.layers[12].weight == 0.f && record->sim_orig.layers[6].weight < 0.1f)
				record->fake_walking = true;

			if (record->fake_walking)
				record->anim_velocity = { 0.f, 0.f, 0.f };

			if (!record->fake_walking && last_record) {
				vector3d velo = record->velocity - last_record->velocity;
				vector3d accel = (velo / math::ticks_to_time(record->choke)) * interfaces::global_vars->interval_per_tick;
				record->anim_velocity = last_record->velocity + accel;
			}
		}

		__forceinline void force_update() {
			backup_globals(cur_time);
			backup_globals(frame_time);

			interfaces::global_vars->cur_time = ptr->old_simtime() + interfaces::global_vars->interval_per_tick;
			interfaces::global_vars->frame_time = interfaces::global_vars->interval_per_tick;

			ptr->force_update();

			restore_globals(cur_time);
			restore_globals(frame_time);

			ptr->invalidate_physics_recursive(0x2A);
		}

		void simulate_animation_side(records_t* record);
		void build_bones(records_t* record, records_t::simulated_data_t* sim);
		void update_animations();
	};

	std::array<anim_player_t, 65> players{};

	__forceinline anim_player_t* get_animation_player(int idx) {
		return &players[idx];
	}

	__forceinline float get_lerp_time() {
		const auto update_rate = cvars::cl_updaterate->get_int();
		const auto interp_ratio = cvars::cl_interp->get_float();

		auto lerp = interp_ratio / update_rate;

		if (lerp <= interp_ratio)
			lerp = interp_ratio;

		return lerp;
	}

	__forceinline records_t* get_latest_record(c_csplayer* player) {
		auto anim_player = this->get_animation_player(player->index());
		if (!anim_player || anim_player->records.empty())
			return nullptr;

		auto record = std::find_if(anim_player->records.begin(), anim_player->records.end(),
			[&](records_t& record) {
				return record.is_valid();
			});

		if (record != anim_player->records.end())
			return &*record;

		return nullptr;
	}

	__forceinline records_t* get_oldest_record(c_csplayer* player) {
		auto anim_player = this->get_animation_player(player->index());
		if (!anim_player || anim_player->records.empty() || !g_ctx.lagcomp)
			return nullptr;

		auto record = std::find_if(anim_player->records.rbegin(), anim_player->records.rend(),
			[&](records_t& record) {
				return record.is_valid();
			});

		if (record != anim_player->records.rend())
			return &*record;

		return nullptr;
	}

	__forceinline std::vector<records_t*> get_all_records(c_csplayer* player) {
		auto anim_player = this->get_animation_player(player->index());
		if (!anim_player || anim_player->records.empty())
			return {};

		std::vector<records_t*> out{};
		for (auto it = anim_player->records.begin(); it != anim_player->records.end(); it = next(it))
			if ((it)->is_valid())
				out.emplace_back(&*it);

		return out;
	}

	__forceinline std::pair<records_t*, records_t*> get_interp_record(c_csplayer* player) {
		auto anim_player = this->get_animation_player(player->index());
		if (!anim_player || anim_player->records.empty() || anim_player->records.size() <= 1)
			return std::make_pair(nullptr, nullptr);

		auto it = anim_player->records.begin(), prev = it;

		for (; it != anim_player->records.end(); it = next(it)) {
			if (prev->is_valid() && !it->is_valid()) {
				return std::make_pair(&*prev, &*it);
			}
			prev = it;
		}

		return std::make_pair(nullptr, nullptr);
	}

	void on_update_anims(c_csplayer* player_ptr);
	void on_net_update_end(int stage);
};