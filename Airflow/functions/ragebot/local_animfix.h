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

class c_local_animation_fix {
private:
	c_animstate*	state{};
	bool			reset{};
	float			old_spawn{};
	uint32_t		old_handle{};

	void update_fake();
	void update_strafe_state();
	void update_viewmodel();
public:
	struct local_data_t {
		bool						on_ground{};
		float						last_lby_time{};
		float						last_lby_tick{};
		float						lby_angle{};

		c_bone_builder				realbuild{};
		c_bone_builder				fakebuild{};

		alignas(16) matrix3x4_t		matrix[256]{};
		alignas(16) matrix3x4_t		matrix_fake[256]{};

		__forceinline void reset() {
			std::memset(matrix, 0, sizeof(matrix));
			std::memset(matrix_fake, 0, sizeof(matrix_fake));

			last_lby_time	= 0.f;
			last_lby_tick	= 0.f;
			lby_angle		= 0.f;
			on_ground		= false;
		}
	} local_info;

	void on_predict_end();

	void on_update_anims(c_csplayer* ecx);

	void on_game_events(c_game_event* event);
	void on_local_death();
};