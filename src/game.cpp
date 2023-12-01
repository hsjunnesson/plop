#include "game.h"

#include <assert.h> 

#include <engine/action_binds.h>
#include <engine/config.h>
#include <engine/engine.h>
#include <engine/input.h>
#include <engine/ini.h>
#include <engine/log.h>
#include <engine/file.h>

#include <memory.h>
#include <temp_allocator.h> 
#include <string_stream.h>
#include <hash.h>

namespace plop {

using namespace foundation;

Game::Game(Allocator &allocator, const char *config_path)
: allocator(allocator)
, config(nullptr)
, app_state(AppState::None)
, action_binds(nullptr) {
    action_binds = MAKE_NEW(allocator, engine::ActionBinds, allocator, config_path);

    // Config
    {
        TempAllocator1024 ta;

        string_stream::Buffer buffer(ta);
        if (!engine::file::read(buffer, config_path)) {
            log_fatal("Could not open config file %s", config_path);
        }

        config = ini_load(string_stream::c_str(buffer), nullptr);

        if (!config) {
            log_fatal("Could not parse config file %s", config_path);
        }
    }
}

Game::~Game() {
    MAKE_DELETE(allocator, ActionBinds, action_binds);

    if (config) {
        ini_destroy(config);
    }
}

void game_state_playing_update(engine::Engine &engine, Game &game_object, float t, float dt) {
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
