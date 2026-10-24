// SPDX-License-Identifier: GPL-3.0-only
#include "canvas.hpp"
#include "dji_protocol.hpp"
#include "engine.hpp"
#include "state_store.hpp"
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>
#include <vector>
using namespace dji;
using namespace dji::detail;
namespace {
Config config() { Config c; c.uart = 0; c.tx_pin = 4; c.rx_pin = 5; return c; }
std::vector<MspFrame> frames(const TxBatch& batch) {
    MspParser parser; MspFrame frame; std::vector<MspFrame> result;
    for (std::size_t i = 0; i < batch.size; ++i)
        if (parser.feed(batch.bytes[i], 0, frame)) result.push_back(frame);
    assert(parser.errors == 0);
    return result;
}
struct Display {
    Frame staged{60,22}, visible{60,22};
    unsigned draws = 0, clears = 0;
    void accept(const TxBatch& batch) {
        for (const auto& frame : frames(batch)) {
            if (frame.command != 182 || frame.size == 0) continue;
            switch (frame.payload[0]) {
            case 2: staged.clear(); ++clears; break;
            case 3:
                assert(frame.size >= 5 && frame.size <= 34);
                for (unsigned i = 4; i < frame.size; ++i) {
                    assert(frame.payload[2] + i - 4 < staged.columns());
                    const auto index = static_cast<std::uint16_t>((frame.payload[3] & 3) * 256 + frame.payload[i]);
                    assert(staged.glyph(frame.payload[2] + i - 4, frame.payload[1], Glyph{index, bool(frame.payload[3] & 0x40)}));
                }
                break;
            case 4: visible = staged; ++draws; break;
            default: break;
            }
        }
    }
};
void equal_cells(const Frame& a, const Frame& b) {
    for (unsigned y=0;y<a.rows();++y) for (unsigned x=0;x<a.columns();++x)
        assert(a.at(x,y)==b.at(x,y));
}
void codec_tests() {
    // Handwritten empty request fixture: '$M<' + length + command + XOR.
    const std::uint8_t request[]{0x24, 0x4d, 0x3c, 0, 101, 101};
    TxBatch batch;
    assert(batch.frame('<', 101, nullptr, 0));
    assert(batch.size == sizeof(request));
    assert(std::memcmp(batch.bytes.data(), request, sizeof(request)) == 0);
    MspParser parser; MspFrame out;
    for (unsigned i = 0; i < 5; ++i) assert(!parser.feed(request[i], i, out));
    assert(parser.feed(request[5], 5, out) && out.command == 101 && out.size == 0);
    assert(!batch.frame('?', 1, nullptr, 0));
    assert(!batch.frame('>', 1, nullptr, 1));
    assert(!batch.frame('>', 1, request, 256));
    for (unsigned i = 0; i < 5; ++i) parser.feed(request[i], 10, out);
    assert(!parser.feed(0xff, 10, out) && parser.errors == 1);
    for (unsigned i = 0; i < 4; ++i) parser.feed(request[i], 100, out);
    // Truncated frame cannot retain parser state indefinitely.
    for (unsigned i = 0; i < 6; ++i) {
        const bool done = parser.feed(request[i], 100000, out);
        assert(done == (i == 5));
    }
    assert(parser.errors == 2);
    std::uint8_t payload[255];
    for (unsigned i = 0; i < 255; ++i) payload[i] = std::uint8_t(i);
    batch.size = 0; assert(batch.frame('>', 200, payload, 255));
    auto decoded = frames(batch); assert(decoded.size() == 1 && decoded[0].size == 255);
    assert(std::memcmp(decoded[0].payload.data(), payload, 255) == 0);
    // Random noise, including arbitrary lengths, must remain bounded and recover.
    std::mt19937 random(42);
    for (unsigned i = 0; i < 100000; ++i) parser.feed(std::uint8_t(random()), 200000, out);
    for (unsigned i = 0; i < 6; ++i) {
        const bool done = parser.feed(request[i], 300000, out);
        assert(done == (i == 5));
    }
}
void state_tests() {
    StateStore s;
    assert(s.put(BatteryVoltage{0,0},0));
    assert(s.voltage.fresh(999999,1000) && !s.voltage.fresh(1000000,1000));
    assert(!s.put(BatteryVoltage{-1,1},1));
    assert(!s.put(BatteryVoltage{std::numeric_limits<float>::quiet_NaN(),1},1));
    assert(s.put(BatteryVoltage{24,100},100));
    assert(!s.put(BatteryVoltage{25,99},100));
    assert(!s.put(BatteryVoltage{25,101},100));
    assert(s.put(BatteryCurrent{-1,1000001},1000001));
    assert(!s.voltage.fresh(1000100,1000));
    assert(s.invalidate(Signal::BatteryCurrent) && !s.current.valid);
    assert(!s.invalidate(static_cast<Signal>(99)));
    for (int i=0;i<1000;++i) assert(s.put(BatteryCurrent{float(i),1000002+i},1001001));
    assert(s.current.value==999 && s.voltage.sampled_at_us==100);
    auto c=config(); assert(c.valid());
    c.tx_pin=c.rx_pin; assert(!c.valid());
    c=config(); c.osd.columns=0; assert(!c.valid());
    c=config(); c.osd.rows=23; assert(!c.valid());
    c=config(); c.osd.columns=1; c.osd.rows=1; assert(c.valid());
}
void frame_tests() {
    Frame frame(8,4); assert(frame.valid());
    assert(frame.text(0,0,"HELLO"));
    assert(frame.at(4,0).character=='O');
    assert(!frame.text(6,1,"ABCD"));
    assert(frame.at(6,1).character=='A' && frame.at(7,1).character=='B');
    assert(!frame.text(-2,2,"ABCD"));
    assert(frame.at(0,2).character=='C');
    assert(!frame.text(INT_MIN,1,"X") && !frame.text(INT_MAX,1,"X"));
    assert(!frame.glyph(-1,0,Glyph{'X'}) && !frame.glyph(8,0,Glyph{'X'}));
    assert(!frame.glyph(0,0,Glyph{1024}) && !frame.glyph(0,0,Glyph{256}));
    assert(frame.glyph(0,0,Glyph{0x141,true}));
    assert(frame.at(0,0).character=='A' && frame.at(0,0).attributes==0x41);
    assert(frame.text(1,0,"Z",TextStyle{3,true}));
    assert(frame.at(1,0).attributes==0x43);
    assert(!frame.text(2,0,"X",TextStyle{4,false}));
    assert(!frame.text(2,0,std::string_view("A\0B",3)));
    assert(frame.at(3,0).character=='L' && frame.at(4,0).character=='B');
    assert(!frame.clear(Glyph{0}));
    assert(frame.clear());
    for(unsigned y=0;y<4;++y) for(unsigned x=0;x<8;++x) assert(frame.at(x,y)==Cell{});
    assert(!Frame(61,22).valid() && !Frame(1,0).valid());
}
void canvas_tests() {
    Frame grid(60,22); grid.clear(Glyph{'X'});
    Canvas canvas; canvas.begin(grid); Display display; unsigned iterations=0;
    while(canvas.active()) {
        TxBatch batch; canvas.append(batch); assert(batch.size>0 && batch.size<=384);
        display.accept(batch); assert(++iterations<20);
    }
    assert(display.draws==1 && display.clears==1); equal_cells(grid,display.visible);
    canvas.begin(grid); TxBatch same; assert(!canvas.append(same) && same.size==0);
    // An attribute-only change is dirty, and is serialized with its font/blink.
    grid.glyph(2,0,Glyph{0x158,true}); canvas.begin(grid); TxBatch attr;
    assert(canvas.append(attr)); display.accept(attr);
    assert(display.visible.at(2,0).attributes==0x41); equal_cells(grid,display.visible);
    grid.clear(); canvas.begin(grid); iterations=0;
    while(canvas.active()) { TxBatch b; canvas.append(b); display.accept(b); assert(++iterations<20); }
    equal_cells(grid,display.visible);
    // Failed output must clear/repaint instead of retaining optimistic diff state.
    grid.glyph(10,10,Glyph{'.'}); canvas.begin(grid); TxBatch lost; canvas.append(lost);
    canvas.invalidate(); canvas.begin(grid); TxBatch retry; canvas.append(retry);
    assert(frames(retry).front().payload[0]==2); display.accept(retry);
    assert(display.visible.at(10,10).character=='.');
}
void submitted_frame_tests() {
    auto c=config(); Engine engine; engine.start(c); StateStore s; TxBatch batch; Display display;
    assert(engine.prepare(s,0,batch));
    for(auto f:frames(batch)) if(f.command==182) assert(f.payload[0]==0); // no hidden widgets
    engine.commit(true);
    Frame frame(53,20); frame.text(2,4,"OLD"); assert(engine.submit(frame));
    frame.clear(); frame.text(2,4,"NEW"); assert(engine.submit(frame));
    frame.text(2,4,"BAD"); // edits after submission cannot mutate the snapshot
    assert(engine.prepare(s,1,batch)); display.accept(batch); engine.commit(true);
    assert(display.visible.at(2,4).character=='N');
    assert(display.visible.at(3,4).character=='E');
    assert(display.draws==1);
    frame.clear(); assert(engine.submit(frame));
    assert(engine.prepare(s,100001,batch)); display.accept(batch); engine.commit(true);
    assert(display.visible.at(2,4).character==' ');
    const std::uint8_t dims[]{50,18}; TxBatch request; request.frame('<',188,dims,2);
    engine.feed(request.bytes.data(),request.size,100002);
    assert(!engine.submit(frame)); // old dimensions rejected after resize
    Frame resized(50,18); resized.text(25,10,"OK"); assert(engine.submit(resized));
    assert(engine.prepare(s,100003,batch)); display.accept(batch); engine.commit(true);
    assert(display.visible.at(25,10).character=='O');
    // Same-size announcements/restarts trigger repaint of retained content.
    engine.feed(request.bytes.data(),request.size,100004);
    assert(engine.prepare(s,100005,batch));
    bool clear=false; for(auto f:frames(batch)) if(f.command==182 && f.payload[0]==2) clear=true;
    assert(clear); engine.commit(true);

    // Submitting during a multi-batch frame must not mix two revisions before
    // DRAW. Dense alternating attributes force a genuinely long transfer.
    c.osd.columns=60; c.osd.rows=22; engine.start(c); Display coherent;
    Frame dense(60,22);
    for(unsigned y=0;y<22;++y) for(unsigned x=0;x<60;++x)
        dense.glyph(x,y,Glyph{static_cast<std::uint16_t>('A'+((x+y)%2)*256)});
    assert(engine.submit(dense));
    assert(engine.prepare(s,0,batch)); coherent.accept(batch); engine.commit(true);
    assert(coherent.draws==0);
    Frame newer(60,22); newer.text(25,10,"LATEST"); assert(engine.submit(newer));
    unsigned attempts=0;
    while(coherent.draws==0) {
        assert(engine.prepare(s,++attempts*1000,batch)); coherent.accept(batch); engine.commit(true);
        assert(attempts<100);
    }
    equal_cells(dense,coherent.visible);
    attempts=0;
    while(coherent.draws==1) {
        assert(engine.prepare(s,200000+attempts++*1000,batch)); coherent.accept(batch); engine.commit(true);
        assert(attempts<100);
    }
    equal_cells(newer,coherent.visible);
}
void engine_tests() {
    auto c = config(); Engine engine; engine.start(c); StateStore s;
    TxBatch batch; assert(engine.prepare(s, 0, batch));
    const auto start = frames(batch);
    assert(start[0].command == 101 && start[0].size == 22 && start[0].payload[6] == 0);
    assert(start[0].payload[16] == 1 && start[0].payload[17] == 1);
    engine.commit(true); assert(!engine.status().reported_armed);
    s.put(Armed{true, 10000}, 10000);
    assert(engine.prepare(s, 10000, batch));
    assert(frames(batch).front().payload[6] == 1);
    engine.commit(false); assert(!engine.status().reported_armed && engine.status().tx_errors == 1);
    assert(engine.prepare(s, 20000, batch));
    assert(frames(batch).front().payload[6] == 1);
    engine.commit(true); assert(engine.status().reported_armed);
    assert(engine.prepare(s, 510000, batch));
    assert(frames(batch).front().payload[6] == 0); // stale arm expires disarmed
    engine.commit(true);
    const std::uint8_t dims[]{50, 18}; TxBatch request;
    request.frame('<', 188, dims, 2); engine.feed(request.bytes.data(), request.size, 520000);
    assert(engine.status().columns == 50 && engine.status().rows == 18 && engine.status().canvas_announced);
    assert(engine.prepare(s, 520000, batch)); engine.commit(true);
    const std::uint8_t invalid[]{255, 255}; request.size = 0;
    request.frame('<', 188, invalid, 2); engine.feed(request.bytes.data(), request.size, 530000);
    assert(engine.status().columns == 50);
    assert(engine.prepare(s, 530000, batch));
    bool error = false;
    for (auto f : frames(batch)) if (f.command == 188 && f.direction == '!') error = true;
    assert(error); engine.commit(true);
    request.size = 0; request.frame('<', 189, nullptr, 0);
    for (int i = 0; i < 100; ++i) engine.feed(request.bytes.data(), request.size, 620000);
    assert(engine.status().dropped_requests >= 84);
    // Input flood still leaves room for periodic armed/status/heartbeat work.
    assert(engine.prepare(s, 620000, batch));
    assert(frames(batch).front().command == 101); engine.commit(true);
    c.osd.enabled = false; s = {}; engine.start(c);
    assert(engine.prepare(s, 0, batch));
    for (auto f : frames(batch)) assert(f.command != 182);
    engine.commit(true);
    assert(!engine.prepare(s, 99999, batch));
    assert(engine.prepare(s, 100000, batch)); engine.commit(true);
}
void native_battery_tests() {
    auto c = config();
    assert(c.osd.columns == 53 && c.osd.rows == 20);
    c.native_battery = true; c.osd.enabled = false;
    Engine engine; engine.start(c); StateStore state;
    assert(state.put(BatteryVoltage{24, 0}, 0));
    assert(state.put(BatteryCurrent{1.5f, 0}, 0));
    assert(state.put(BatteryDetails{6, 5000, 123, BatteryCondition::Ok, 0}, 0));
    TxBatch batch;
    assert(engine.prepare(state, 0, batch));
    bool found = false;
    // Independently handwritten wire payload, including centivolt extension.
    const std::uint8_t expected[]{6, 0x88, 0x13, 240, 123, 0, 150, 0, 0, 0x60, 9};
    for (const auto& frame : frames(batch)) {
        assert(frame.command != 182); // native works with canvas disabled
        if (frame.command == 130) {
            found = true;
            assert(frame.direction == '>' && frame.size == sizeof(expected));
            assert(std::memcmp(frame.payload.data(), expected, sizeof(expected)) == 0);
        }
    }
    assert(found && engine.status().native_battery_ready);
    engine.commit(false); assert(engine.status().battery_tx_frames == 0);
    assert(engine.prepare(state, 10000, batch)); engine.commit(true);
    assert(engine.status().battery_tx_frames == 1 && !engine.status().reported_armed);

    // Requests may arrive after unrelated unsupported commands in a burst.
    TxBatch requests;
    for (auto id : {3, 10, 92, 94, 101, 105, 110, 111, 112, 130, 150})
        assert(requests.frame('<', id, nullptr, 0));
    engine.feed(requests.bytes.data(), requests.size, 20000);
    assert(engine.status().battery_requests == 1 && engine.status().dropped_requests == 0);
    bool battery_reply = false;
    for (int i = 0; i < 3; ++i) {
        assert(engine.prepare(state, 20000 + i * 10000, batch));
        for (const auto& frame : frames(batch))
            if (frame.command == 130 && frame.direction == '>') battery_reply = true;
        engine.commit(true);
    }
    assert(battery_reply);

    // A real zero current is distinct from unavailable. Negative current is
    // signed on wire. Voltage and metadata have independent freshness.
    assert(state.put(BatteryCurrent{-1.5f, 50000}, 50000));
    batch.size = 0; append_battery(batch, state);
    auto decoded = frames(batch);
    assert(decoded[0].payload[6] == 0x6a && decoded[0].payload[7] == 0xff);
    assert(state.put(BatteryCurrent{0, 60000}, 60000));
    assert(native_battery_ready(state, 60000, 1000));
    assert(state.invalidate(Signal::BatteryCurrent));
    assert(!native_battery_ready(state, 60000, 1000));
    requests.size = 0; requests.frame('<', 130, nullptr, 0);
    engine.feed(requests.bytes.data(), requests.size, 60000);
    assert(engine.prepare(state, 60000, batch));
    bool unavailable = false;
    for (const auto& frame : frames(batch)) if (frame.command == 130) {
        assert(frame.direction == '!' && frame.size == 0); unavailable = true;
    }
    assert(unavailable && !engine.status().native_battery_ready); engine.commit(true);
    assert(state.put(BatteryCurrent{1, 999999}, 999999));
    assert(native_battery_ready(state, 999999, 1000));
    assert(!native_battery_ready(state, 1000000, 1000)); // voltage/details expired
    assert(state.put(BatteryVoltage{25.51f, 1000000}, 1000000));
    assert(state.put(BatteryDetails{6, 5000, 123, BatteryCondition::Ok, 1000000}, 1000000));
    assert(!native_battery_ready(state, 1000000, 1000)); // no legacy saturation lie
    assert(state.put(BatteryVoltage{25.5f, 1000001}, 1000001));
    assert(native_battery_ready(state, 1000001, 1000));
    assert(state.put(BatteryCurrent{400, 1000001}, 1000001));
    assert(!native_battery_ready(state, 1000001, 1000));
    assert(!state.put(BatteryDetails{0, 0, 0, BatteryCondition::Ok, 1000002}, 1000002));
    assert(!state.put(BatteryDetails{6, 0, 0, static_cast<BatteryCondition>(99), 1000002}, 1000002));
    assert(state.invalidate(Signal::BatteryDetails));
    assert(!state.battery_details.valid);

    // Opt-out and malformed requests produce errors, not fake data.
    c.native_battery = false; engine.start(c);
    engine.feed(requests.bytes.data(), requests.size, 2000000);
    assert(engine.prepare(state, 2000000, batch));
    found = false;
    for (const auto& frame : frames(batch)) if (frame.command == 130) {
        assert(frame.direction == '!'); found = true;
    }
    assert(found); engine.commit(true);
    c.native_battery = true; engine.start(c);
    requests.size = 0; const std::uint8_t bad[]{1}; requests.frame('<', 130, bad, 1);
    engine.feed(requests.bytes.data(), requests.size, 2000000);
    assert(engine.prepare(state, 2000000, batch));
    for (const auto& frame : frames(batch)) if (frame.command == 130) assert(frame.direction == '!');
    engine.commit(true);
}
}
int main() {
    codec_tests(); state_tests(); frame_tests(); canvas_tests(); submitted_frame_tests();
    engine_tests(); native_battery_tests();
    std::puts("PASS: codecs, typed telemetry, generic frames/attributes, dirty runs, resize, native battery");
}
