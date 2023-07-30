#pragma once
#include <string>
#include <vector>

enum message_prefix_t {
	event_hit,
	event_miss,
	event_plant,
	event_buy
};

class color;

class c_event_logger {
private:
	struct message_t {
		int					msgtype{};

		float				time{};

		float alpha			= -1.f;

		std::string			text{};
	};

	std::vector<message_t> messages{};

	std::pair<std::string, color> get_message_prefix_type(int type);
public:
	void add_message(const std::string& text, int message_type = -1, bool debug = false);
	void on_directx();
};