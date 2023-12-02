#pragma once

#pragma warning(push, 0)
#include <memory_types.h>
#include <stdint.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include "Wwise_IDs.h"
#pragma warning(pop)

#include "util.h"

#pragma warning(push, 0)
#include "collection_types.h"
#include "memory_types.h"
#include <AK/SoundEngine/Common/AkTypes.h>
#pragma warning(pop)

namespace wwise {

struct Wwise {
    Wwise(foundation::Allocator &allocator);
    ~Wwise();
    DELETE_COPY_AND_MOVE(Wwise)

    foundation::Allocator &allocator;
    void *low_level_io;
    AkGameObjectID default_listener_id;
    AkGameObjectID unpositioned_game_object_id;
    foundation::Hash<AkBankID> loaded_banks;
};

AkBankID load_bank(Wwise &wwise, const char *bank_name);
void unload_bank(Wwise &wwise, const char *bank_name);

AkGameObjectID register_game_object(const char *name);
void unregister_game_object(AkGameObjectID game_object_id);

AkPlayingID post_event(AkUniqueID event_id, AkGameObjectID game_object_id);
AkPlayingID post_event(const char *event_name, AkGameObjectID game_object_id);

void update();

} // namespace wwise
