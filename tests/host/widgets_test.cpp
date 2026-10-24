// SPDX-License-Identifier: GPL-3.0-only
#include "widgets.hpp"
#include <cassert>
#include <climits>
#include <cstdio>
#include <limits>
#include <utility>

using dji::Frame;
namespace {
bool text_at(const Frame& frame, unsigned x, unsigned y, const char* text) {
    for (unsigned i=0;text[i];++i) if (frame.at(x+i,y).character != text[i]) return false;
    return true;
}
void arc_tests() {
    playground::WidgetConfig view;
    playground::Scene scene; scene.curvature_per_m=0;
    Frame frame(30,16);
    playground::draw_widgets(frame,view,scene);
    assert(frame.at(15,15).character=='^' && frame.at(15,5).character=='.');
    scene.curvature_per_m=0.5f; Frame left(30,16); playground::draw_widgets(left,view,scene);
    scene.curvature_per_m=-0.5f; Frame right(30,16); playground::draw_widgets(right,view,scene);
    bool bends=false;
    for(unsigned y=3;y<15;++y) for(unsigned dx=1;dx<14;++dx) {
        assert(left.at(15-dx,y)==right.at(15+dx,y));
        if(left.at(15-dx,y).character=='.') bends=true;
        assert(left.at(15+dx,y).character==' ');
    }
    assert(bends);
    scene.curvature_per_m.reset(); playground::draw_widgets(frame,view,scene);
    assert(frame.at(15,14).character=='?');
    scene.curvature_per_m=0; Frame wide(53,20); playground::draw_widgets(wide,view,scene);
    assert(wide.at(26,15).character=='^' && wide.at(15,15).character==' ');
    view.center_turning_region=false; playground::draw_widgets(wide,view,scene);
    assert(wide.at(15,15).character=='^');
    view.center_turning_region=true; view.forward_tilt_deg=60;
    playground::draw_widgets(frame,view,scene);
    assert(frame.at(15,12).character=='.' && frame.at(15,11).character==' ');
    view.forward_tilt_deg=0; view.cell_height_over_width=3;
    playground::draw_widgets(frame,view,scene);
    assert(frame.at(15,10).character=='.' && frame.at(15,9).character==' ');
    view.forward_tilt_deg=80; view.perspective_distance_m=0.1f;
    view.lookahead_m=100; view.horizontal_cells_per_m=100; scene.curvature_per_m=0.5f;
    Frame tiny(8,4); playground::draw_widgets(tiny,view,scene);
    view.forward_tilt_deg=std::numeric_limits<float>::quiet_NaN(); assert(!view.valid());
    playground::draw_widgets(frame,view,scene); assert(frame.at(15,15).character==' ');
}
void readout_and_border_tests() {
    playground::WidgetConfig view; playground::Scene scene;
    scene.speed_mps=10; scene.pack_volts=24; scene.current_amps=1.5f;
    Frame frame(53,20); playground::draw_widgets(frame,view,scene);
    assert(text_at(frame,0,0,"SPD 36.0 km/h") && text_at(frame,0,1,"PACK 24.00 V"));
    scene.speed_mps.reset(); playground::draw_widgets(frame,view,scene);
    assert(text_at(frame,0,0,"SPD -- km/h"));
    view.canvas_test_pattern=true;
    for(auto dimensions : {std::pair<unsigned,unsigned>{53,20},{30,16},{60,22},{8,4},{1,1}}) {
        Frame test(dimensions.first,dimensions.second); playground::draw_widgets(test,view,scene);
        assert(test.at(0,0).character=='+');
        assert(test.at(test.columns()-1,test.rows()-1).character=='+');
        for(unsigned x=1;x+1<test.columns();++x)
            assert(test.at(x,0).character=='-' && test.at(x,test.rows()-1).character=='-');
        for(unsigned y=1;y+1<test.rows();++y)
            assert(test.at(0,y).character=='|' && test.at(test.columns()-1,y).character=='|');
        assert(test.at(test.columns()/2,test.rows()/2).character=='+');
    }
}
}
int main() {
    arc_tests(); readout_and_border_tests();
    std::puts("PASS: example-owned widgets, arc geometry/projection, readouts and calibration");
}
