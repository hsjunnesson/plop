#pragma once

#include "collection_types.h"
#include "memory_types.h"
#include <engine/math.inl>
#include "wwise.h"

typedef struct ini_t ini_t;

namespace engine {
struct Engine;
struct InputCommand;
struct ActionBinds;
struct Canvas;
} // namespace engine

namespace plop {
   
/// Murmur hashed actions.
enum class ActionHash : uint64_t {
    NONE = 0x0ULL,
    QUIT = 0x387bbb994ac3551ULL,
    PLAY_DEBUG = 0x4fda3bbaea57358fULL,
};

/**
 * @brief An enum that describes a specific game state.
 *
 */
enum class AppState {
    // No state.
    None,

    // game state is creating, or loading from a save.
    Initializing,

    // The user is using the game
    Playing,

    // Shutting down, saving and closing the game.
    Quitting,

    // Final state that signals the engine to terminate the game.
    Terminate,
};

enum class Degree : int {
    NONE = 0,
    FIRST = 1,
    SECOND = 2,
    THIRD = 3,
    FOURTH = 4,
    FIFTH = 5,
    SIXTH = 6,
    SEVENTH = 7,
    OCTAVE = 8,
};

struct Game {
    Game(foundation::Allocator &allocator, const char *config_path);
    ~Game();

    foundation::Allocator &allocator;
    ini_t *config;
    AppState app_state;

    engine::ActionBinds *action_binds;
    engine::Canvas *canvas;
    
    foundation::Array<math::Color4f> palette;
    wwise::Wwise wwise;
};

/**
 * @brief Updates the game
 *
 * @param engine The engine which calls this function
 * @param game The game to update
 * @param t The current time
 * @param dt The delta time since last update
 */
void update(engine::Engine &engine, void *game, float t, float dt);

/**
 * @brief Callback to the game that an input has ocurred.
 *
 * @param engine The engine which calls this function
 * @param game The game to signal.
 * @param input_command The input command.
 */
void on_input(engine::Engine &engine, void *game, engine::InputCommand &input_command);

/**
 * @brief Renders the game
 *
 * @param engine The engine which calls this function
 * @param game The game to render
 */
void render(engine::Engine &engine, void *game);

/**
 * @brief Renders the imgui
 *
 * @param engine The engine which calls this function
 * @param game The game to render
 */
void render_imgui(engine::Engine &engine, void *game);

/**
 * @brief Asks the game to quit.
 *
 * @param engine The engine which calls this function
 * @param game The game to render
 * @return Whether we should quit or clear the should close flag.
 */
bool on_shutdown(engine::Engine &engine, void *game);

/**
 * @brief Transition an game to another app state.
 *
 * @param engine The engine which calls this function
 * @param game The game to transition
 * @param app_state The AppState to transition to.
 */
void transition(engine::Engine &engine, void *game, AppState app_state);

} // namespace plop
