#include "CoralliumClockTransaction.h"
#include <cassert>
#include <cstddef>
#include <cstring>
struct Storage {
    bool valid=true;
    int16_t offset=480;
    unsigned calls=0,failAt=0;
    size_t putBool(const char* key,bool value) {
        assert(!strcmp(key,"utcValid"));
        if(++calls==failAt) return 0;
        valid=value; return sizeof(bool);
    }
    size_t putShort(const char* key,int16_t value) {
        assert(!strcmp(key,"utcOffset"));
        if(++calls==failAt) return 0;
        offset=value; return sizeof(int16_t);
    }
};
int main() {
    using namespace coralliumclock;
    for(unsigned failAt=0;failAt<=3;++failAt) {
        Storage storage; storage.failAt=failAt;
        int16_t live=480; bool known=true,written=false;
        auto result=synchronize(storage,[&]() {
            assert(!storage.valid); written=true; return true;
        },-240,live,known);
        if(failAt==1) {
            assert(result==Save::StorageFailed&&!written&&known&&live==480);
            assert(storage.valid&&storage.offset==480); // No RTC change when invalidation fails.
        } else if(failAt) {
            assert(result==Save::StorageFailed&&written&&!known&&live==-240);
            assert(!storage.valid); // Reboot cannot reinterpret new RTC with old offset.
        } else {
            assert(result==Save::Saved&&written&&known&&live==-240);
            assert(storage.valid&&storage.offset==-240);
        }
    }
    Storage storage; int16_t live=480; bool known=true;
    auto result=synchronize(storage,[](){return false;},-240,live,known);
    assert(result==Save::ClockFailed&&!known&&!storage.valid&&storage.offset==480);
    // A later complete sync recovers from either partial transaction.
    result=synchronize(storage,[](){return true;},60,live,known);
    assert(result==Save::Saved&&known&&storage.valid&&storage.offset==60&&live==60);
}
