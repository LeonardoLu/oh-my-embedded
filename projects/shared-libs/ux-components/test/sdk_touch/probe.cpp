// Exercises the installed SDK implementation; no SDK state machine is copied.
#include <Touch_Class.hpp>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
struct Probe : m5::Touch_Class { using m5::Touch_Class::update_detail; };
int main(int argc, char** argv) {
    const bool expectLegacy = argc == 2 && !std::strcmp(argv[1], "--expect-legacy-bug");
    Probe probe;
    unsigned lostContacts = 0;
    for (bool hold : {false, true}) {
        m5::Touch_Class::touch_detail_t detail{};
        m5gfx::touch_point_t point;
        point.x = 100; point.y = 100;
        probe.update_detail(&detail, 1000, true, &point);
        assert(detail.wasPressed());
        if (hold) probe.update_detail(&detail, 1700, true, &point);
        point.y = 150;
        probe.update_detail(&detail, 1800, true, &point);
        assert(detail.isPressed());
        probe.update_detail(&detail, 1850, false, nullptr);
        assert(detail.wasReleased() && detail.x == 100 && detail.y == 150);
        unsigned released = detail.state;
        point.x = 130; point.y = 200;
        probe.update_detail(&detail, 1900, true, &point);
        bool lost = !detail.wasPressed() && !detail.isPressed();
        lostContacts += lost;
        std::printf("%s release=%u -> recontact=%u pressed=%d: %s\n",
                    hold ? "drag" : "flick", released, detail.state,
                    detail.isPressed(), lost ? "LOST CONTACT" : "contact recovered");
        if (lost) {
            probe.update_detail(&detail, 1920, true, &point);
            assert(!detail.isPressed());
            probe.update_detail(&detail, 1940, false, nullptr);
            assert(detail.state == m5::none);
            probe.update_detail(&detail, 1960, true, &point);
            assert(detail.wasPressed());
        }
    }
    m5::Touch_Class::touch_detail_t detail{};
    m5gfx::touch_point_t point;
    point.x = 100; point.y = 100;
    probe.update_detail(&detail, 2000, true, &point);
    point.x = 106;
    probe.update_detail(&detail, 2020, true, &point);
    int beforeRelease = detail.x;
    probe.update_detail(&detail, 2040, false, nullptr);
    assert(detail.wasReleased() && detail.x == beforeRelease);
    std::printf("6px raw movement: stored x=%d; release retains x=%d\n", beforeRelease, detail.x);
    if (expectLegacy) {
        if (lostContacts != 2 || beforeRelease != 100) {
            std::fputs("Pinned behavior changed: inspect the SDK and update this historical probe.\n", stderr);
            return 1;
        }
        std::puts("Confirmed both historical lost-contact transitions in the pinned SDK.");
    }
}
