#include "movement.h"

#include "../config_vars.h"
#include "../features.h"
#include "../anti hit/exploits.h"

#include "../ragebot/engine_prediction.h"
#include "../ragebot/ragebot.h"

#include "../../base/sdk.h"
#include "../../base/global_context.h"

#include "../../base/tools/math.h"

#include "../../base/sdk/c_usercmd.h"
#include "../../base/sdk/c_animstate.h"
#include "../../base/sdk/entity.h"

enum EDirections {
    FORWARDS = 0,
    BACKWARDS = 180,
    LEFT = 90,
    RIGHT = -90,
    BACK_LEFT = 135,
    BACK_RIGHT = -135
};

void c_movement::force_stop() {
    vector3d angle;
    math::vector_to_angles(g_ctx.local->velocity(), angle);

    float speed = g_ctx.local->velocity().length(false);

    angle.y = g_ctx.orig_angle.y - angle.y;

    vector3d direction;
    math::angle_to_vectors(angle, direction);

    vector3d stop = direction * -speed;

    if (speed > 13.f) {
        g_ctx.cmd->forwardmove = stop.x;
        g_ctx.cmd->sidemove = stop.y;
    }
    else {
        g_ctx.cmd->forwardmove = 0.f;
        g_ctx.cmd->sidemove = 0.f;
    }
}

void c_movement::fast_stop() {
    if (!g_cfg.misc.fast_stop)
        return;

    if (peek_move)
        return;

    if (!g_utils->on_ground() || g_rage_bot->stopping || g_exploits->stop_movement || g_cfg.binds[sw_b].toggled || g_rage_bot->should_slide)
        return;

    vector3d velocity = g_ctx.local->velocity();
    float speed = velocity.length(true);

    if (speed < 5.f)
        return;

    bool pressing_move_keys = g_ctx.cmd->buttons & in_moveleft
        || g_ctx.cmd->buttons & in_moveright
        || g_ctx.cmd->buttons & in_back
        || g_ctx.cmd->buttons & in_forward;

    if (pressing_move_keys)
        return;

    this->force_stop();
}

void c_movement::auto_peek() {
    static bool old_move = false;

    bool moving = g_ctx.cmd->buttons & in_moveleft
        || g_ctx.cmd->buttons & in_moveright
        || g_ctx.cmd->buttons & in_forward
        || g_ctx.cmd->buttons & in_back;

    vector3d origin = g_ctx.local->origin();

    auto should_peek = [&]() {
        if (g_ctx.weapon->is_misc_weapon())
            return false;

        if (!g_cfg.binds[ap_b].toggled)
            return false;

        if (!peek_pos.valid())
            peek_pos = origin;

        peek_start = true;

        if (g_utils->on_ground()) {
            if ((g_utils->is_firing() || g_rage_bot->firing) || (g_cfg.misc.retrack_peek && !moving))
                peek_move = true;

            if (g_cfg.misc.retrack_peek && moving && !old_move)
                peek_move = false;
        }

        vector3d origin_delta = peek_pos - origin;
        float distance = origin_delta.length(true);

        if (peek_move) {
            if (g_exploits->cl_move.shifting)
                this->force_stop();

            if (distance > 10.f) {
                vector3d return_position = math::angle_from_vectors(origin, peek_pos);
                g_ctx.base_angle.y = return_position.y;

                g_ctx.cmd->forwardmove = cvars::cl_forwardspeed->get_float();
                g_ctx.cmd->sidemove = 0.f;
            }
            else {
                this->force_stop();
                peek_move = false;
            }

            g_rage_bot->firing = false;
        }

        old_move = moving;
        return true;
    };

    if (!should_peek()) {
        peek_pos.reset();
        peek_start = false;
        peek_move = false;
    }
}

void c_movement::auto_strafe() {
    if (!g_cfg.misc.auto_strafe)
        return;

    if (g_utils->on_ground() || g_exploits->stop_movement)
        return;

    if (g_ctx.local->velocity().length(true) < 10.f)
        return;

    if (g_ctx.cmd->buttons & in_speed)
        return;

    bool holding_w = LI_FN(GetAsyncKeyState).cached()('W');
    bool holding_a = LI_FN(GetAsyncKeyState).cached()('A');
    bool holding_s = LI_FN(GetAsyncKeyState).cached()('S');
    bool holding_d = LI_FN(GetAsyncKeyState).cached()('D');

    bool m_pressing_move = holding_w || holding_a || holding_s || holding_d;

    static auto switch_key = 1.f;
    static auto circle_yaw = 0.f;
    static auto old_yaw = 0.f;

    auto velocity = g_ctx.local->velocity();
    velocity.z = 0.f;

    if (m_pressing_move) {
        float wish_dir{};

        if (holding_w) {
            if (holding_a)
                wish_dir += (EDirections::LEFT / 2);
            else if (holding_d)
                wish_dir += (EDirections::RIGHT / 2);
            else
                wish_dir += EDirections::FORWARDS;
        }
        else if (holding_s) {
            if (holding_a)
                wish_dir += EDirections::BACK_LEFT;
            else if (holding_d)
                wish_dir += EDirections::BACK_RIGHT;
            else
                wish_dir += EDirections::BACKWARDS;

            g_ctx.cmd->forwardmove = 0.f;
        }
        else if (holding_a)
            wish_dir += EDirections::LEFT;
        else if (holding_d)
            wish_dir += EDirections::RIGHT;

        g_ctx.base_angle.y += math::normalize(wish_dir);
    }

    float smooth = (1.f - (0.15f * (1.f - g_cfg.misc.strafe_smooth * 0.01f)));

    auto speed = velocity.length(true);
    if (speed <= 0.5f) {
        g_ctx.cmd->forwardmove = 450.f;
        return;
    }

    const auto diff = math::normalize(g_ctx.base_angle.y - math::rad_to_deg(std::atan2f(velocity.y, velocity.x)));

    g_ctx.cmd->forwardmove = std::clamp((5850.f / speed), -450.f, 450.f);
    g_ctx.cmd->sidemove = (diff > 0.f) ? -450.f : 450.f;

    g_ctx.base_angle.y = math::normalize(g_ctx.base_angle.y - diff * smooth);
}

void c_movement::auto_jump() {
    if (!g_cfg.misc.auto_jump)
        return;
    
    if (g_ctx.local->flags() & fl_onground)
        return;

    g_ctx.cmd->buttons &= ~in_jump;
}

void c_movement::force_speed(float max_speed) {
    // as did in game movement of CSGO:
    // reference: CCSGameMovement::CheckParameters

    if (g_ctx.local->move_type() == movetype_noclip
        || g_ctx.local->move_type() == movetype_isometric
        || g_ctx.local->move_type() == movetype_observer)
        return;

    float sidemove = g_ctx.cmd->sidemove;
    float forwardmove = g_ctx.cmd->forwardmove;

    float move_speed = std::sqrt(std::pow(sidemove, 2) + std::pow(forwardmove, 2));
    if (move_speed > max_speed) {
        bool invalid_speed = max_speed + 1.f <= g_ctx.local->velocity().length(true);

        g_ctx.cmd->sidemove = invalid_speed ? 0.f : (sidemove / move_speed) * max_speed;
        g_ctx.cmd->forwardmove = invalid_speed ? 0.f : (forwardmove / move_speed) * max_speed;
    }
}

float c_movement::get_max_speed() {
    return max_speed;
}

void c_movement::fix_movement(c_usercmd* cmd, vector3d& ang) {
    vector3d  move, dir;
    float   delta, len;
    vector3d   move_angle;

    move = { cmd->forwardmove, cmd->sidemove, 0 };

    len = move.normalized_float();

    if (len == 0.f)
        return;

    math::vector_to_angles(move, move_angle);

    delta = (cmd->viewangles.y - ang.y);

    move_angle.y += delta;

    math::angle_to_vectors(move_angle, dir);

    dir *= len;

    if (g_ctx.local->move_type() == movetype_ladder) {
        if (cmd->viewangles.x >= 45 && ang.x < 45 && std::abs(delta) <= 65)
            dir.x = -dir.x;

        cmd->forwardmove = dir.x;
        cmd->sidemove = dir.y;

        if (cmd->forwardmove > 200)
            cmd->buttons |= in_forward;

        else if (cmd->forwardmove < -200)
            cmd->buttons |= in_back;

        if (cmd->sidemove > 200)
            cmd->buttons |= in_moveright;

        else if (cmd->sidemove < -200)
            cmd->buttons |= in_moveleft;
    }
    else {
        if (cmd->viewangles.x < -90 || cmd->viewangles.x > 90)
            dir.x = -dir.x;

        cmd->forwardmove = dir.x;
        cmd->sidemove = dir.y;
    }

    cmd->forwardmove = std::clamp(cmd->forwardmove, -450.f, 450.f);
    cmd->sidemove = std::clamp(cmd->sidemove, -450.f, 450.f);
    cmd->upmove = std::clamp(cmd->upmove, -320.f, 320.f);
}

void c_movement::on_pre_predict() {
    if (!g_ctx.in_game)
        return;

    if (!g_ctx.local->is_alive())
        return;

    if (g_ctx.local->move_type() == movetype_ladder
        || g_ctx.local->move_type() == movetype_noclip)
        return;

    if (interfaces::game_rules->is_freeze_time()
        || g_ctx.local->flags() & fl_frozen
        || g_ctx.local->gun_game_immunity())
        return;

    if (!g_ctx.weapon_info)
        return;

    max_speed = g_ctx.local->is_scoped() ? g_ctx.weapon_info->max_speed_alt : g_ctx.weapon_info->max_speed;

    this->auto_jump();
    this->auto_strafe();
}

void c_movement::on_predict_start() {
    if (!g_ctx.weapon)
        return;

    if (g_ctx.local->move_type() == movetype_ladder
        || g_ctx.local->move_type() == movetype_noclip)
        return;

    if (interfaces::game_rules->is_freeze_time()
        || g_ctx.local->flags() & fl_frozen
        || g_ctx.local->gun_game_immunity())
        return;

    this->auto_peek();
    this->fast_stop();
}