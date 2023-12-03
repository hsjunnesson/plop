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
        
        int num_bomps = 8;
        for (int i = 0; i < num_bomps; ++i) {
            int32_t x = rnd_pcg_range(&random_device, 64, window_width - 64);
            int32_t y = rnd_pcg_range(&random_device, 64, window_height - 64);
            float time_offset = rnd_pcg_nextf(&random_device) * 5.0f;
            float speed = rnd_pcg_nextf(&random_device) * 2.5f + 0.5f;
            float radius_minimum = rnd_pcg_range(&random_device, 12, 64);
            float radius_maximum = rnd_pcg_range(&random_device, 95, 128);
            
            array::clear(name);
            printf(name, "Bomp#%d", i);
            
            Bomp bomp;
            bomp.position.x = x;
            bomp.position.y = y;
            bomp.position.z = 0.0f;
            bomp.wwise_game_object_id = wwise::register_game_object(c_str(name));
            bomp.degree = Degree(rnd_pcg_range(&random_device, 1, 7));
            bomp.radius = radius_minimum;
            bomp.radius_min = radius_minimum;
            bomp.radius_max = radius_maximum;
            bomp.speed = speed;
            bomp.time_offset = time_offset;
            
            wwise::set_position(bomp.wwise_game_object_id, bomp.position);
            array::push_back(bomps, bomp);
        }
    }
    
    wwise::set_position(wwise.default_listener_id, math::Vector3f { window_width * 0.5f, window_height * 0.5f, -5.0f });
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

void wavy_circle(engine::Canvas &canvas, int32_t x_center, int32_t y_center, int32_t r, float frequency, float amplitude, float time, math::Color4f col) {
    float angle_increment = 0.5f; // Smaller increment for smoother circle
    const int num_segments = static_cast<int>(360 / angle_increment);
    const float two_pi = 2.0f * M_PI;

    for (int i = 0; i < num_segments; ++i) {
        // Calculate the angle for this segment and the next
        float angle1 = two_pi * i / num_segments;
        float angle2 = two_pi * (i + 1) / num_segments;

        // Calculate the wavy radius for both points
        float wavy_radius1 = r + amplitude * sin(frequency * angle1 + time);
        float wavy_radius2 = r + amplitude * sin(frequency * angle2 + time);

        // Calculate the coordinates for both points
        int32_t x1 = x_center + static_cast<int32_t>(wavy_radius1 * cos(angle1));
        int32_t y1 = y_center + static_cast<int32_t>(wavy_radius1 * sin(angle1));
        int32_t x2 = x_center + static_cast<int32_t>(wavy_radius2 * cos(angle2));
        int32_t y2 = y_center + static_cast<int32_t>(wavy_radius2 * sin(angle2));

        // Draw the line segment
        engine::canvas::line(canvas, x1, y1, x2, y2, col);
    }
}

void game_state_playing_update(engine::Engine &engine, Game &game_object, float t, float dt) {
    using namespace engine::canvas;
    engine::Canvas &c = *game_object.canvas;

    math::Color4f black = game_object.palette[6];
    math::Color4f dark_gray = game_object.palette[7];
    math::Color4f light_gray = game_object.palette[8];
    math::Color4f white = game_object.palette[9];

    rnd_pcg_t random_device;
    rnd_pcg_seed(&random_device, 512);

    clear(c, light_gray);

    for (Bomp *bomp = array::begin(game_object.bomps); bomp != array::end(game_object.bomps); ++bomp) {
        float r = math::lerp(bomp->radius_min, bomp->radius_max, (sinf(t * bomp->speed + bomp->time_offset) + 1.0f) * 0.5f);
        
        float threshold = bomp->radius_max * 0.85f;
        if (bomp->radius < threshold && r >= threshold) {
            bomp->playing_id = wwise::post_event(event_for_degree(bomp->degree), bomp->wwise_game_object_id);
            bomp->playing_time = t;
        }
        
        bomp->radius = r;
    }

    for (Bomp *bomp = array::begin(game_object.bomps); bomp != array::end(game_object.bomps); ++bomp) {
        circle_fill(c, bomp->position.x + 6, bomp->position.y + 6, bomp->radius, dark_gray);
    }

    for (Bomp *bomp = array::begin(game_object.bomps); bomp != array::end(game_object.bomps); ++bomp) {
        circle_fill(c, bomp->position.x, bomp->position.y, bomp->radius, white);
    }
    
    float max_amplitude = 10.0f;
    float max_decay = 1.5f;
    float amplitude_inset = 11.0f;
    
    for (Bomp *bomp = array::begin(game_object.bomps); bomp != array::end(game_object.bomps); ++bomp) {
        float radius = bomp->radius - amplitude_inset;
        if (radius >= 1.0f) {
            float amplitude = 0.0f;
            
            if (bomp->playing_id != AK_INVALID_PLAYING_ID) {
                float time_diff = t - bomp->playing_time;
                if (time_diff <= max_decay) {
                    amplitude = math::lerp(max_amplitude, 0.0f, time_diff / max_decay);
                }
            }
            
            wavy_circle(c, bomp->position.x, bomp->position.y, radius, 40, amplitude, t * 50.0f, black);
            //circle(c, bomp->position.x, bomp->position.y, radius, black);
        }
    }
    
    for (Bomp *bomp = array::begin(game_object.bomps); bomp != array::end(game_object.bomps); ++bomp) {
        circle_fill(c, bomp->position.x, bomp->position.y, bomp->radius - amplitude_inset - max_amplitude, white);
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
        bool repeated = input_command.key_state.trigger_state == engine::TriggerState::Repeated;

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
