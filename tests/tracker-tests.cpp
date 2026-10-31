// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/tracker.hpp"
#include <iostream>
#include <random>
#include <stdexcept>
#define CHECK(x) do { if(!(x)) throw std::runtime_error(#x); } while(0)
using namespace inframe;
Crop follow(Tracker &t,Settings s,Face f,double &time,int frames=600,double dt=1.0/60) {
    Crop c;
    for(int i=0;i<frames;++i) {time+=dt; t.observe({f},time,time,s); c=t.step(dt,time,s);}
    return c;
}
int main() {
    try {
        Settings s; s.deadZone=0;
        Tracker t; double now=0;
        auto c=follow(t,s,{.6,.3,.12,.18,.99},now);
        CHECK(std::abs((.66-c.x)/c.scale-.5)<.002);
        CHECK(std::abs((.39-c.y)/c.scale-.5)<.002);
        CHECK(std::abs(c.scale-.18/.35)<.002);
        // Hold after short dropout; return wide only after the configured grace.
        c=t.step(.016,now+.3,s); CHECK(c.scale<.6);
        for(int i=0;i<600;++i) {now+=1.0/60; c=t.step(1.0/60,now,s);}
        CHECK(c.scale>.99 && c.x<.01);
        // Subject stickiness: a larger distant bystander must not steal the shot.
        t.reset(); now=0;
        t.observe({{.1,.3,.1,.15,.99}},0,0,s);
        t.observe({{.1,.3,.1,.15,.99},{.65,.2,.25,.35,.99}},.1,.1,s);
        CHECK(t.subject()->x==.1);
        t.observe({{.65,.2,.25,.35,.99}},.2,.2,s); CHECK(t.subject()->x==.1);
        t.observe({{.65,.2,.25,.35,.99}},2,2,s); CHECK(t.subject()->x==.65);
        // Stale/invalid/low-confidence detections must not move the camera.
        t.reset(); t.observe({{.2,.2,.2,.2,.99}},0,1,s); CHECK(!t.subject());
        t.observe({{.2,.2,.2,.2,.1},{NAN,0,.1,.1,.99}},1,1,s); CHECK(!t.subject());
        // Freeze and the alternative hold-forever loss policy.
        now=0; c=follow(t,s,{.2,.3,.1,.18,.99},now);
        s.freeze=true; auto frozen=t.step(.1,now+20,s);
        CHECK(c.x==frozen.x && c.y==frozen.y && c.scale==frozen.scale);
        s.freeze=false; s.returnWide=false;
        auto held=t.step(.1,now+30,s); CHECK(std::abs(held.scale-c.scale)<.001);
        s.returnWide=true;
        // Group mode produces a bounding box covering all detected people.
        t.reset(); s.mode=2;
        t.observe({{.1,.2,.1,.2,.99},{.7,.4,.1,.2,.99}},1,1,s);
        CHECK(std::abs(t.subject()->w-.7)<1e-9);
        CHECK(std::abs(t.subject()->h-.4)<1e-9);
        // Motion is effectively frame-rate independent.
        s.mode=0; Tracker a,b; double ta=0,tb=0;
        auto ca=follow(a,s,{.6,.3,.1,.15,.99},ta,60,1.0/30);
        auto cb=follow(b,s,{.6,.3,.1,.15,.99},tb,120,1.0/60);
        CHECK(std::abs(ca.x-cb.x)<.005 && std::abs(ca.scale-cb.scale)<.005);
        // Random motion and settings: crops always stay inside the image.
        std::mt19937 rng(42); std::uniform_real_distribution<double> u(0,1);
        t.reset(); now=0;
        for(int i=0;i<20000;++i) {
            s.mode=1; s.maxZoom=1+3*u(rng); s.anchorX=.15+.7*u(rng); s.anchorY=.15+.7*u(rng);
            const double fw=.02+.4*u(rng),fh=.02+.5*u(rng); now+=.016;
            t.observe({{u(rng)*(1-fw),u(rng)*(1-fh),fw,fh,.99}},now,now,s);
            c=t.step(.016,now,s);
            CHECK(std::isfinite(c.x+c.y+c.scale));
            CHECK(c.scale>0 && c.scale<=1);
            CHECK(c.x>=-1e-10 && c.y>=-1e-10 && c.x+c.scale<=1+1e-10 && c.y+c.scale<=1+1e-10);
        }
        std::cout<<"PASS: centering, zoom, loss, sticky selection, invalid/stale input, freeze, group, frame rates, 20000 bounded crops\n";
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n'; return 1;}
}
