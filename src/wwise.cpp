#include "wwise.h"

#include "Wwise_IDs.h"

#pragma warning(push, 0)
#include "memory.h"
#include "temp_allocator.h"
#include "string_stream.h"
#include "murmur_hash.h"
#include "hash.h"

#include <engine/log.h>

#include <AK/SoundEngine/Common/AkMemoryMgr.h>
#include <AK/SoundEngine/Common/AkModule.h>
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/SoundEngine/Common/AkStreamMgrModule.h>
#include <AK/SoundEngine/Common/IAkStreamMgr.h>
#include "SoundEngine/Common/AKJobWorkerMgr.h"
#include "SoundEngine/Common/AkFilePackageLowLevelIODeferred.h"

#if !defined AK_OPTIMIZED
#include <AK/Comm/AkCommunication.h>
#endif

#include <string>
#include <locale>
#include <codecvt>
#include <inttypes.h>

#pragma warning(pop)

namespace wwise {

using namespace foundation;

AkGameObjectID game_object_count = 0;

void local_output_func(AK::Monitor::ErrorCode in_eErrorCode, const AkOSChar *in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel, AkPlayingID in_playingID, AkGameObjectID in_gameObjID) {
    using namespace foundation::string_stream;
    
    TempAllocator128 ta;
    Buffer ss(ta);

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::string bytes = converter.to_bytes(in_pszError);
    const char *message = bytes.c_str();

    printf(ss, "(%d) %s", in_eErrorCode, message);
    
    if (in_playingID != AK_INVALID_PLAYING_ID) {
        printf(ss, " playing id: %d", in_playingID);
    }
    
    if (in_gameObjID != AK_INVALID_GAME_OBJECT) {
        printf(ss, " object id: %d", in_gameObjID);
    }
    
    if (in_eErrorLevel == AK::Monitor::ErrorLevel::ErrorLevel_Error) {
        log_error(c_str(ss));
    } else {
        log_info(c_str(ss));
    }
}

Wwise::Wwise(Allocator &allocator)
: allocator(allocator)
, low_level_io(nullptr)
, default_listener_id(0)
, unpositioned_game_object_id(0)
, loaded_banks(allocator) {
    // MemoryMgr
    {
        AkMemSettings mem_settings;
        AK::MemoryMgr::GetDefaultSettings(mem_settings);
        AKRESULT result = AK::MemoryMgr::Init(&mem_settings);
        if (result != AK_Success) {
            log_fatal("Could not initialize AK::MemoryMgr: %d", result);
        }
    }
    
    // Monitor
    {
        AKRESULT result = AK::Monitor::SetLocalOutput(AK::Monitor::ErrorLevel::ErrorLevel_All, local_output_func); 
        if (result != AK_Success) {
            log_fatal("Could not AK::Monitor::SetLocalOutput: %d", result);
        }
    }
    
    // StreamMgr
    {
        AkStreamMgrSettings stm_settings;
        AK::StreamMgr::GetDefaultSettings(stm_settings);
        AK::IAkStreamMgr *stream_manager = AK::StreamMgr::Create(stm_settings);
        if (!stream_manager) {
            log_fatal("Could not create AK::StreamMgt");
        }
        
        AkDeviceSettings device_settings;
        AK::StreamMgr::GetDefaultDeviceSettings(device_settings);
        device_settings.bUseStreamCache = true;
        
        low_level_io = MAKE_NEW(allocator, CAkFilePackageLowLevelIODeferred);
        AKRESULT result = ((CAkFilePackageLowLevelIODeferred *)low_level_io)->Init(device_settings);
        if (result != AK_Success) {
            log_fatal("Could not initialize CAkFilePackageLowLevelIODeferred: %d", result);
        }
        
        // TODO: Replace this with config.ini setting
        ((CAkFilePackageLowLevelIODeferred *)low_level_io)->SetBasePath(AKTEXT("plop_wwise/GeneratedSoundBanks/Windows"));
        AK::StreamMgr::SetCurrentLanguage(AKTEXT("English(US)"));
    }
    
    // JobWorkerMgr
    {
        AK::JobWorkerMgr::InitSettings job_worker_settings;
        AK::JobWorkerMgr::GetDefaultInitSettings(job_worker_settings);
        job_worker_settings.uNumWorkerThreads = 8;
        
        AKRESULT result = AK::JobWorkerMgr::InitWorkers(job_worker_settings);
        if (result != AK_Success) {
            log_fatal("Could not initialize AK::JobWorkerMgr::InitWorkers: %d", result);
        }
    }
    
    // SoundEngine
    {
        AkInitSettings init_settings;
        AkPlatformInitSettings platform_init_settings;
        AK::SoundEngine::GetDefaultInitSettings(init_settings);
        AK::SoundEngine::GetDefaultPlatformInitSettings(platform_init_settings);
        AKRESULT result = AK::SoundEngine::Init(&init_settings, &platform_init_settings);
        if (result != AK_Success) {
            log_fatal("Could not initialize AK::SoundEngine: %d", result);
        }
    }
    
#if !defined AK_OPTIMIZED
    // Communication
    {
        AkCommSettings comm_settings;
        AK::Comm::GetDefaultInitSettings(comm_settings);
        AKPLATFORM::SafeStrCpy(comm_settings.szAppNetworkName, "Plop", AK_COMM_SETTINGS_MAX_STRING_SIZE);
        AKRESULT result = AK::Comm::Init(comm_settings);
        if (result != AK_Success) {
            log_fatal("Could not initialize AK::Comm: %d", result);
        }
    }
#endif
    
    // Load Init bank
    {
        load_bank(*this, "Init");
    }
    
    // Listener
    {
        default_listener_id = ++game_object_count;

        AKRESULT result = AK::SoundEngine::RegisterGameObj(default_listener_id, "Default listener");
        if (result != AK_Success) {
            log_fatal("Could not AK::SoundEngine::RegisterGameObj %d", result);
        }

        AK::SoundEngine::SetDefaultListeners(&default_listener_id, 1);
    }
    
    // Default objects
    {
        unpositioned_game_object_id = register_game_object("Unpositioned game object");
    }
}

Wwise::~Wwise() {
#if !defined(AK_OPTIMIZED)
    AK::Comm::Term();
#endif

    AK::JobWorkerMgr::TermWorkers();
    
    if (AK::SoundEngine::IsInitialized()) {
        AK::SoundEngine::UnregisterAllGameObj();
        AK::SoundEngine::Term();
    }
    
    if (AK::IAkStreamMgr::Get()) {
        if (low_level_io) {
            ((CAkFilePackageLowLevelIODeferred *)low_level_io)->Term();
            MAKE_DELETE(allocator, CAkFilePackageLowLevelIODeferred, (CAkFilePackageLowLevelIODeferred *)low_level_io);
            low_level_io = nullptr;
        }
        
        AK::IAkStreamMgr::Get()->Destroy();
    }
    
    if (AK::MemoryMgr::IsInitialized()) {
        AK::MemoryMgr::Term();
    }
}

AkBankID load_bank(Wwise &wwise, const char *bank_name) {
    AkBankID out_bank_id;
    AKRESULT result = AK::SoundEngine::LoadBank(bank_name, out_bank_id);
    if (result != AK_Success) {
        log_fatal("Could not load bank: %s: %d", bank_name, result);
    }
    
    uint64_t hash_key = murmur_hash_64(bank_name, strlen(bank_name), 0);
    hash::set(wwise.loaded_banks, hash_key, out_bank_id);
    
    return out_bank_id;
}

void unload_bank(Wwise &wwise, const char *bank_name) {
    uint64_t hash_key = murmur_hash_64(bank_name, strlen(bank_name), 0);

    if (!hash::has(wwise.loaded_banks, hash_key)) {
        log_error("Trying to unload a bank that isn't loaded: %s", bank_name);
        return;
    }
    
    AkBankID bank_id = hash::get(wwise.loaded_banks, hash_key, (AkBankID)0);
    hash::remove(wwise.loaded_banks, hash_key);
    
    AKRESULT result = AK::SoundEngine::UnloadBank(bank_id, NULL);
    if (result != AK_Success) {
        log_fatal("Could not load bank: %d: %d", bank_id, result);
    }
}

AkGameObjectID register_game_object(const char *name) {
    AkGameObjectID id = ++game_object_count;
    
    AKRESULT result = AK::SoundEngine::RegisterGameObj(id, name);
    if (result != AK_Success) {
        log_fatal("Could not AK::SoundEngine::RegisterGameObj: %s: %d", name, result);
    }
    
    return id;
}

void unregister_game_object(AkGameObjectID game_object_id) {
    AKRESULT result = AK::SoundEngine::UnregisterGameObj(game_object_id);
    if (result != AK_Success) {
        log_fatal("Could not AK::SoundEngine::UnregisterGameObj: %d: %d", game_object_id, result);
    }
}

AkPlayingID post_event(AkUniqueID event_id, AkGameObjectID game_object_id) {
    AkPlayingID playing_id = AK::SoundEngine::PostEvent(event_id, game_object_id);
    if (playing_id == AK_INVALID_PLAYING_ID) {
        log_error("Could not AK::SoundEngine::PostEvent %u for game object %" PRIu64 "", event_id, game_object_id);
    }
    
    return playing_id;
}

AkPlayingID post_event(const char *event_name, AkGameObjectID game_object_id) {
    AkPlayingID playing_id = AK::SoundEngine::PostEvent(event_name, game_object_id);
    if (playing_id == AK_INVALID_PLAYING_ID) {
        log_error("Could not AK::SoundEngine::PostEvent %s for game object %" PRIu64 "", event_name, game_object_id);
    }

    return playing_id;
}

void update() {
    if (AK::SoundEngine::IsInitialized()) {
        AK::SoundEngine::RenderAudio();
    }
}

} // namespace wwise
