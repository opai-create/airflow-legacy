#pragma once
#include "../../base/sdk.h"

#include "../../base/tools/math.h"

#include "../../base/other/game_functions.h"

#include "../../base/sdk/c_usercmd.h"
#include "../../base/sdk/entity.h"

#include <memory>
#undef local

class c_engine_prediction {
private:
	struct net_data_t {
		int			cmd_number{};

		float		vel_modifier{};
		float		fall_velocity{};
		float		duck_amt{};
		float		duck_speed{};
		float		thirdperson_recoil{};

		vector3d	punch{};
		vector3d	punch_vel{};
		vector3d	view_offset{};
		vector3d	view_punch{};
		vector3d	velocity{};

		bool		filled{};

		__forceinline void reset() {
			cmd_number = 0;

			vel_modifier = 0.f;
			fall_velocity = 0.f;
			duck_amt = 0.f;
			duck_speed = 0.f;
			thirdperson_recoil = 0.f;

			punch.reset();
			punch_vel.reset();
			view_offset.reset();
			view_punch.reset();
			velocity.reset();

			filled = false;
		}
	};

	bool		reset_net_data{};
	bool		old_in_prediction{};
	bool		old_first_time_predicted{};

	int			old_tick_base{};
	int			old_tick_count{};

	float		old_cur_time{};
	float		old_frame_time{};

	float		old_recoil_index{};
	float		old_accuracy_penalty{};

	uint32_t	old_seed{};

	c_usercmd* old_cmd{};

	int* prediction_player{};
	int* prediction_random_seed{};

	c_movedata	move_data{};

	std::array<net_data_t, 150> net_data{};

	__forceinline void reset() {
		if (!reset_net_data)
			return;

		for (auto& d : net_data)
			d.reset();

		reset_net_data = false;
	}
public:
	vector3d	unprediced_velocity{};
	int			unpredicted_flags{};

	float		predicted_inaccuracy{};
	float		predicted_spread{};

	float		interp_amount{};

	void on_render_start(int stage, bool after);

	void net_compress_store(int tick);
	void net_compress_apply(int tick);

	void init();
	void update();

	void start(c_csplayer* local, c_usercmd* cmd);
	void force_update_eyepos(const float& pitch);
	void repredict(c_csplayer* local, c_usercmd* cmd, bool real_cmd = false);
	void finish(c_csplayer* local);
};