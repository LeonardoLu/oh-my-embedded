#pragma once
#include <stdint.h>
namespace coralliumclock {
enum class Save { Saved, StorageFailed, ClockFailed };
// NVS commits each key independently. Invalidate the UTC correspondence before
// changing the hardware clock; a reset anywhere in the transaction stays unset.
template<class Storage,class WriteClock>
Save synchronize(Storage& storage,WriteClock writeClock,int16_t requestedOffset,
                 int16_t& liveOffset,bool& offsetKnown) {
    if(storage.putBool("utcValid",false)!=sizeof(bool)) return Save::StorageFailed;
    offsetKnown=false;
    if(!writeClock()) return Save::ClockFailed;
    liveOffset=requestedOffset;
    if(storage.putShort("utcOffset",requestedOffset)!=sizeof(int16_t)) return Save::StorageFailed;
    if(storage.putBool("utcValid",true)!=sizeof(bool)) return Save::StorageFailed;
    offsetKnown=true;
    return Save::Saved;
}
}
