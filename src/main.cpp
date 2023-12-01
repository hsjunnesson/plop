#define RND_IMPLEMENTATION
#include "rnd.h"

#include <engine/engine.h>
#include <engine/input.h>
#include <engine/log.h>

#include <backward.hpp>
#include <memory.h>

#if defined(_WIN32)
#include <Windows.h>

#include <fileapi.h>
#endif

#if defined(LIVE_PP)
#include "LPP_API_x64_CPP.h"
#endif

#if defined(SUPERLUMINAL)
#include <Superluminal/PerformanceAPI.h>
#endif

#include "game.h"

#if defined(_WIN32)
NOINLINE static LONG WINAPI CrashExceptionHandler(EXCEPTION_POINTERS *pExceptionInfo) {
    HANDLE hFile = CreateFile(L"CrashDump.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
        dumpInfo.ThreadId = GetCurrentThreadId();
        dumpInfo.ExceptionPointers = pExceptionInfo;
        dumpInfo.ClientPointers = FALSE;

        // Write the dump
        BOOL success = MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            hFile,
            MiniDumpNormal,
            &dumpInfo,
            NULL,
            NULL);
        CloseHandle(hFile);

        if (success) {
            log_info("Wrote CrashDump.dmp");
        } else {
            log_info("Could not write CrashDump.dmp");
        }
    } else {
        log_info("Could not write CrashDump.dmp");
    }

    log_fatal("Unhandled exception");
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

#if defined(SUPERLUMINAL)
    PerformanceAPI_SetCurrentThreadName("main");
#endif

    // Validate platform
    {
        unsigned int x = 1;
        char *c = (char *)&x;
        if ((int)*c == 0) {
            log_fatal("Unsupported platform: big endian");
        }

        if constexpr (sizeof(char) != sizeof(uint8_t)) {
            log_fatal("Unsupported platform: invalid char size");
        }

        if constexpr (sizeof(float) != 4) {
            log_fatal("Unsupported platform: invalid float size");
        }
    }

#if defined(LIVE_PP)
    lpp::LppDefaultAgent lpp_agent = lpp::LppCreateDefaultAgent(nullptr, L"LivePP");
    lpp_agent.EnableModule(lpp::LppGetCurrentModulePath(), lpp::LPP_MODULES_OPTION_ALL_IMPORT_MODULES, nullptr, nullptr);
#endif

#if defined(_WIN32)
    SetUnhandledExceptionFilter(CrashExceptionHandler);
#else
    backward::SignalHandling sh;
#endif

    int status = 0;
    foundation::memory_globals::init();
    foundation::Allocator &allocator = foundation::memory_globals::default_allocator();

    {
        const char *config_path = "assets/config.ini";

        engine::Engine engine(allocator, config_path);
        plop::Game game(allocator, config_path);

        engine::EngineCallbacks engine_callbacks;
        engine_callbacks.on_input = plop::on_input;
        engine_callbacks.update = plop::update;
        engine_callbacks.render = plop::render;
        engine_callbacks.render_imgui = plop::render_imgui;
        engine_callbacks.on_shutdown = plop::on_shutdown;
        engine.engine_callbacks = &engine_callbacks;
        engine.game_object = &game;

        status = engine::run(engine);
    }

    foundation::memory_globals::shutdown();

#if defined(LIVE_PP)
    lpp::LppDestroyDefaultAgent(&lpp_agent);
#endif

    return status;
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    return main(__argc, __argv);
}
#endif
