#include "game.h"
#include "wwise.h"
#include "palette.h"

#include <assert.h>
#include <functional>
#include <time.h>

#include <engine/action_binds.h>
#include <engine/config.h>
#include <engine/engine.h>
#include <engine/file.h>
#include <engine/ini.h>
#include <engine/input.h>
#include <engine/log.h>
#include <engine/canvas.h>
#include <engine/math.inl>

#include <hash.h>
#include <memory.h>
#include <string_stream.h>
#include <temp_allocator.h>

#include "rnd.h"
 
namespace plop {

using namespace foundation;

math::Vector3f right_handed_screen_to_left_handed_world(const Game &game, const math::Vector3f v) {
    return math::Vector3f { v.x, game.canvas->height - v.y, v.z };
}

Game::Game(Allocator &allocator, const char *config_path)
: allocator(allocator)
, config(nullptr)
, app_state(AppState::None)
, action_binds(nullptr)
, canvas(nullptr)
, palette(allocator)
, wwise(allocator)
, bomps(allocator) {
    using namespace foundation::string_stream;

    action_binds = MAKE_NEW(allocator, engine::ActionBinds, allocator, config_path);
    canvas = MAKE_NEW(allocator, engine::Canvas, allocator);

    if (!grunka::load_palette("assets/resurrect-64.pal", this->palette)) {
        log_fatal("Could not load palette.");
    }
    
    uint32_t window_width = 0;
    uint32_t window_height = 0;
    
    // Config
    {
        TempAllocator1024 ta;
        Buffer buffer(ta);

        if (!engine::file::read(buffer, config_path)) {
            log_fatal("Could not open config file %s", config_path);
        }

        config = ini_load(string_stream::c_str(buffer), nullptr);

        if (!config) {
            log_fatal("Could not parse config file %s", config_path);
        }
        
        auto read_property = [](ini_t *config, const char *section, const char *property, const std::function<void(const char *)> success) {
            const char *s = engine::config::read_property(config, section, property);
            if (s) {
                success(s);
            } else {
                if (!section) {
                    section = "global property";
                }

                log_fatal("Invalid config file, missing [%s] %s", section, property);
            }
        };
        
        read_property(config, "engine", "window_width", [this, &window_width](const char *property) {
            int i = atoi(property);
            if (i > UINT32_MAX || i < 0) {
                log_fatal("Invalid [engine] window_width %d", i);
            }

            window_width = static_cast<uint32_t>(i);
        });
        
        read_property(config, "engine", "window_height", [this, &window_height](const char *property) {
            int i = atoi(property);
            if (i > UINT32_MAX || i < 0) {
                log_fatal("Invalid [engine] window_height %d", i);
            }

            window_height = static_cast<uint32_t>(i);
        });
    }
    
    // bomps
    {
        time_t seconds;
        time(&seconds);

        rnd_pcg_t random_device;
        rnd_pcg_seed(&random_device, (RND_U32) seconds);
        
        TempAllocator128 ta;
        Buffer name(ta);
        
        int num_bomps = 6;
        for (int i = 0; i < num_bomps; ++i) {
            float time_offset = rnd_pcg_nextf(&random_device) * 5.0f;
            float speed = rnd_pcg_nextf(&random_device) * 2.5f + 0.5f;
            float radius_minimum = (float)rnd_pcg_range(&random_device, 12, 64);
            float radius_maximum = (float)rnd_pcg_range(&random_device, 76, 128);
            
            array::clear(name);
            printf(name, "Bomp#%d", i);

            Bomp bomp;

            bool valid_pos = false;
            while (!valid_pos) {
                bomp.position.x = (float)rnd_pcg_range(&random_device, 64, window_width - 64);;
                bomp.position.y = (float)rnd_pcg_range(&random_device, 64, window_height - 64);
                bomp.position.z = 0.0f;
                
                valid_pos = true;
                for (Bomp *other_bomp = array::begin(bomps); other_bomp != array::end(bomps); ++other_bomp) {
                    float distance = sqrtf(powf(other_bomp->position.x - bomp.position.x, 2) + powf(other_bomp->position.y - bomp.position.y, 2));
                    if (distance <= 128) {
                        valid_pos = false;
                        break;
                    }
                }
            }
            
            bomp.wwise_game_object_id = wwise::register_game_object(c_str(name));
            bomp.degree = Degree(rnd_pcg_range(&random_device, 1, 7));
            bomp.radius = radius_minimum;
            bomp.radius_min = radius_minimum;
            bomp.radius_max = radius_maximum;
            bomp.speed = speed;
            bomp.time_offset = time_offset;
            bomp.rotation = bomp.time_offset;

            wwise::set_game_parameter(AK::GAME_PARAMETERS::BOMPMAXSIZE, bomp.wwise_game_object_id, radius_maximum);
            array::push_back(bomps, bomp);
        }
    }
    
    // Default listener
    {
        math::Vector3f position = { window_width * 0.5f, window_height * 0.5f, -100.0f };
        math::Vector3f front = { 0, 0, 1 };
        math::Vector3f top = { 0, 1, 0 };
        wwise::set_pose(wwise.default_listener_id, position, front, top);
    }
}

Game::~Game() {
    MAKE_DELETE(allocator, ActionBinds, action_binds);
    MAKE_DELETE(allocator, Canvas, canvas);

    if (config) {
        ini_destroy(config);
    }
}

AkUniqueID event_for_degree(Degree degree) {
    switch (degree) {
    case Degree::NONE:
        return AK_INVALID_UNIQUE_ID;
    case Degree::FIRST:
        return AK::EVENTS::PLAY_PIANO_D_001;
    case Degree::SECOND:
        return AK::EVENTS::PLAY_PIANO_E_001;
    case Degree::THIRD:
        return AK::EVENTS::PLAY_PIANO_F_001;
    case Degree::FOURTH:
        return AK::EVENTS::PLAY_PIANO_G_001;
    case Degree::FIFTH:
        return AK::EVENTS::PLAY_PIANO_A_001;
    case Degree::SIXTH:
        return AK::EVENTS::PLAY_PIANO_BB_001;
    case Degree::SEVENTH:
        return AK::EVENTS::PLAY_PIANO_C_001;
    default:
        return AK_INVALID_UNIQUE_ID;
    }
}

void wavy_circle(engine::Canvas &canvas, const int32_t x_center, const int32_t y_center, const float r, const math::Color4f col, const float frequency, float amplitude, float offset = 0.0f, const int num_segments = 152) {
    static float TWO_PI = 2.0f * (float)M_PI;

    for (int i = 1; i <= num_segments; ++i) {
        float angle1 = TWO_PI * i / num_segments;
        float angle2 = TWO_PI * (i + 1) / num_segments;

        float wavy_radius1 = r + amplitude * sin(frequency * angle1 + offset);
        float wavy_radius2 = r + amplitude * sin(frequency * angle2 + offset);

        float x1 = x_center + wavy_radius1 * cos(angle1);
        float y1 = y_center + wavy_radius1 * sin(angle1);
        float x2 = x_center + wavy_radius2 * cos(angle2);
        float y2 = y_center + wavy_radius2 * sin(angle2);

        engine::canvas::line(canvas, (int32_t)x1, (int32_t)y1, (int32_t)x2, (int32_t)y2, col);
    }
}

void wavy_circle_fill(engine::Canvas &canvas, const float x_center, const float y_center, const float r, math::Color4f col, const float frequency, const float amplitude, const float offset = 0.0f, const int num_segments = 152) {
    static float TWO_PI = 2.0f * (float)M_PI;

    math::Vector2f center { x_center, y_center };
    
    for (int i = 1; i <= num_segments; ++i) {
        float angle1 = TWO_PI * i / num_segments;
        float angle2 = TWO_PI * (i + 1) / num_segments;

        float wavy_radius1 = r + amplitude * sin(frequency * angle1 + offset);
        float wavy_radius2 = r + amplitude * sin(frequency * angle2 + offset);

        float x1 = x_center + wavy_radius1 * cos(angle1);
        float y1 = y_center + wavy_radius1 * sin(angle1);
        float x2 = x_center + wavy_radius2 * cos(angle2);
        float y2 = y_center + wavy_radius2 * sin(angle2);
        
        math::Vector2f v0 { x1, y1 };
        math::Vector2f v1 { x2, y2 };
        
        engine::canvas::triangle_fill(canvas, v0, v1, center, col);
    }
}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {
    (void)engine;
    using namespace engine::canvas;
    
    engine::Canvas &c = *game.canvas;

    math::Color4f black = game.palette[6];
    math::Color4f dark_gray = game.palette[7];
    math::Color4f light_gray = game.palette[8];
    math::Color4f white = game.palette[9];
    math::Color4f green = game.palette[37];
    math::Color4f orange = game.palette[17];
    
    math::Color4f background = light_gray;
    math::Color4f shadow = dark_gray;
    
//  autumn.pal
//    math::Color4f black = game.palette[0];
//    math::Color4f shadow = game.palette[1];
//    math::Color4f white = game.palette[2];
//    math::Color4f pale = game.palette[3];
//    math::Color4f red = game.palette[4];
//    math::Color4f yellow = game.palette[5];
//    math::Color4f orange = game.palette[6];
        
    rnd_pcg_t random_device;
    rnd_pcg_seed(&random_device, 512);

    clear(c, background);
    
    float jump_time = 1.0f;
    
    auto vertical_bomp = [t, jump_time](const Bomp *bomp, int32_t &height_offset, int32_t &shadow_rad_offset) {
        if (bomp->playing_id != AK_INVALID_PLAYING_ID) {
            float time_since = t - bomp->playing_time;
            if (time_since < jump_time) {
                float ratio = sinf((time_since / jump_time) * (float)M_PI);
                height_offset = (int32_t)math::lerp(6.0f, 32.0f, ratio);
                shadow_rad_offset = (int32_t)math::lerp(0.0f, 24.0f, ratio);
                return;
            }
        }
        
        height_offset = 6;
        shadow_rad_offset = 0;
    };
    
    auto speed_rot_bomp = [t, jump_time](const Bomp *bomp, float &time_offset) {
        if (bomp->playing_id != AK_INVALID_PLAYING_ID) {
            float time_since = t - bomp->playing_time;
            if (time_since < jump_time) {
                float ratio = cosf((time_since / jump_time) * ((float)M_PI / 2.0f));
                time_offset = math::lerp<float>(0.0f, 5.0f, ratio);
                return;
            }
        }
        
        time_offset = 0.0f;
    };
    
    auto amplitude_bomp = [t, jump_time](const Bomp *bomp, float &amplitude) {
        if (bomp->playing_id != AK_INVALID_PLAYING_ID) {
            float time_since = t - bomp->playing_time;
            if (time_since < jump_time) {
                float ratio = cosf((time_since / jump_time) * ((float)M_PI / 2.0f));
                amplitude = math::lerp(0.0f, 8.0f, ratio);
                return;
            }
        }
        
        amplitude = 0.0f;
    };
    
    auto num_segments_bomp = [t, jump_time](const Bomp *bomp, int &num_segments) {
        if (bomp->playing_id != AK_INVALID_PLAYING_ID) {
            float time_since = t - bomp->playing_time;
            if (time_since < jump_time) {
                num_segments = 128;
                return;
            }
        }
        
        num_segments = 48;
    };
    
    // Update bomps
    for (Bomp *bomp = array::begin(game.bomps); bomp != array::end(game.bomps); ++bomp) {
        float r = math::lerp(bomp->radius_min, bomp->radius_max, (sinf(t * bomp->speed + bomp->time_offset) + 1.0f) * 0.5f);
        
        float threshold = bomp->radius_max * 0.85f;
        if (bomp->radius < threshold && r >= threshold) {
            bomp->playing_id = wwise::post_event(event_for_degree(bomp->degree), bomp->wwise_game_object_id);
            bomp->playing_time = t;
        }
        
        bomp->radius = r;
        wwise::set_position(bomp->wwise_game_object_id, right_handed_screen_to_left_handed_world(game, bomp->position));
        
        float rotation_incr = bomp->speed * 5.0f;
        float rotation_jump_incr = 0.0f;
        speed_rot_bomp(bomp, rotation_jump_incr);
        bomp->rotation += dt * (rotation_incr + rotation_jump_incr);
    }

    // Draw bomp shadow
    for (Bomp *bomp = array::begin(game.bomps); bomp != array::end(game.bomps); ++bomp) {
        int32_t height_offset = 0;
        int32_t shadow_rad_offset = 0;
        vertical_bomp(bomp, height_offset, shadow_rad_offset);

        float frequency = 20.0f;
        float amplitude = 0.0f;
        amplitude_bomp(bomp, amplitude);
        
        if (amplitude >= 0.5f) {
            int num_segments = 0;
            num_segments_bomp(bomp, num_segments);
            wavy_circle_fill(c, bomp->position.x, bomp->position.y, bomp->radius - shadow_rad_offset, dark_gray, frequency, amplitude, bomp->rotation, num_segments);
        } else {
            circle_fill(c, (int32_t)bomp->position.x, (int32_t)bomp->position.y, (int32_t)bomp->radius - shadow_rad_offset, dark_gray);
        }
    }

    // Draw bomp
    for (Bomp *bomp = array::begin(game.bomps); bomp != array::end(game.bomps); ++bomp) {
        int32_t height_offset = 0;
        int32_t shadow_rad_offset = 0;
        vertical_bomp(bomp, height_offset, shadow_rad_offset);

        float frequency = 20.0f;
        float amplitude = 0.0f;
        amplitude_bomp(bomp, amplitude);

        if (amplitude >= 0.5f) {
            int num_segments = 0;
            num_segments_bomp(bomp, num_segments);
            wavy_circle_fill(c, bomp->position.x - height_offset, bomp->position.y - height_offset, bomp->radius, white, frequency, amplitude, bomp->rotation, num_segments);
        } else {
            circle_fill(c, (int32_t)bomp->position.x - height_offset, (int32_t)bomp->position.y - height_offset, (int32_t)bomp->radius, white);
        }
    }
}

void update(engine::Engine &engine, void *game_object, float t, float dt) {
    if (!game_object) {
        return;
    }

    Game *game = static_cast<Game *>(game_object);

    switch (game->app_state) {
    case AppState::None: {
        transition(engine, game_object, AppState::Initializing);
        break;
    }
    case AppState::Playing: {
        game_state_playing_update(engine, *game, t, dt);
        break;
    }
    case AppState::Quitting: {
        transition(engine, game_object, AppState::Terminate);
        break;
    }
    default: {
        break;
    }
    }
    
    wwise::update();
}

void on_input(engine::Engine &engine, void *game_object, engine::InputCommand &input_command) {
    if (!game_object) {
        return;
    }

    Game *game = static_cast<Game *>(game_object);
    assert(game->action_binds != nullptr);

    if (game->app_state != AppState::Playing) {
        return;
    }

    if (input_command.input_type == engine::InputType::Key) {
        bool pressed = input_command.key_state.trigger_state == engine::TriggerState::Pressed;
//        bool repeated = input_command.key_state.trigger_state == engine::TriggerState::Repeated;

        uint64_t bind_action_key = action_key_for_input_command(input_command);
        if (bind_action_key == 0) {
            return;
        }

        ActionHash action_hash = ActionHash(foundation::hash::get(game->action_binds->bind_actions, bind_action_key, (uint64_t)0));

        switch (action_hash) {
        case (ActionHash::QUIT): {
            if (pressed) {
                transition(engine, game_object, AppState::Quitting);
            }
            break;
        }
        case (ActionHash::PLAY_DEBUG): {
            break;
        }
        }
    }
}

void render(engine::Engine &engine, void *game_object) {
    if (!game_object) {
        return;
    }

    Game *game = static_cast<Game *>(game_object);
    if (game->app_state != AppState::Playing) {
        return;
    }
    
    engine::render_canvas(engine, *game->canvas);
}

void render_imgui(engine::Engine &engine, void *game_object) {
    (void)engine;
    
    if (!game_object) {
        return;
    }

    Game *game = static_cast<Game *>(game_object);
    if (game->app_state != AppState::Playing) {
        return;
    }
}

bool on_shutdown(engine::Engine &engine, void *game_object) {
    transition(engine, game_object, AppState::Quitting);
    return true;
}

void transition(engine::Engine &engine, void *game_object, AppState app_state) {
    if (!game_object) {
        return;
    }

    Game *game = static_cast<Game *>(game_object);

    if (game->app_state == app_state) {
        return;
    }

    // When leaving an game state
    switch (game->app_state) {
    case AppState::Terminate: {
        return;
    }
    case AppState::Playing: {
        break;
    }
    default:
        break;
    }

    game->app_state = app_state;

    // When entering a new game state
    switch (game->app_state) {
    case AppState::None: {
        break;
    }
    case AppState::Initializing: {
        log_info("Initializing");
        wwise::load_bank(game->wwise, "Mix_Master");
        wwise::load_bank(game->wwise, "Debug_Sounds");
        wwise::load_bank(game->wwise, "Player");
        engine::init_canvas(engine, *game->canvas, game->config);
        
        transition(engine, game_object, AppState::Playing);
        break;
    }
    case AppState::Playing: {
        log_info("Playing");
        break;
    }
    case AppState::Quitting: {
        log_info("Quitting");
        break;
    }
    case AppState::Terminate: {
        log_info("Terminating");
        engine::terminate(engine);
        break;
    }
    }
}

} // namespace plop
