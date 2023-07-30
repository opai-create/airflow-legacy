#include <thread>
#include "key_states.h"

#include "../global_context.h"
#include "../../functions/config_vars.h"
#include "../../functions/features.h"

#include "../sdk.h"

create_feature_ptr(key_states);

c_key_states::c_key_states() {
// removed
}

bool c_key_states::proc_key(int idx, int key, bool state) {
	return idx == key && state;
}

c_key_states::key_info_t c_key_states::get_key_state(UINT uMsg, WPARAM wParam) {
	// removed
}

bool c_key_states::key_updated(int key, UINT uMsg, WPARAM wParam) {
	auto key_state = get_key_state(uMsg, wParam);
	return key == key_state.key && key_state.state;
}

void c_key_states::update(UINT uMsg, WPARAM wParam) {
	// removed
}