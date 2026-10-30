// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace sf {
struct Face {
    double x{}, y{}, w{}, h{}, confidence{};
    double cx() const { return x + w * .5; }
    double cy() const { return y + h * .5; }
    bool valid() const {
        return std::isfinite(x+y+w+h+confidence) && w > .005 && h > .005 &&
            w <= 1 && h <= 1 && cx() >= 0 && cx() <= 1 && cy() >= 0 && cy() <= 1;
    }
};
struct Settings {
    double faceSize=.35, maxZoom=2.5, manualZoom=1.6;
    double anchorX=.5, anchorY=.5, response=.35, zoomResponse=1.0;
    double deadZone=.035, maxSpeed=.8, hold=1.5, confidence=.75;
    int mode=0, detectionFps=15; // 0: stick with subject, 1: largest, 2: group
    bool autoZoom=true, returnWide=true, freeze=false;
};
struct Crop { double x=0, y=0, scale=1; };
class Tracker {
    std::optional<Face> subject_;
    double seen_=-1e9, goalX_=.5, goalY_=.5, goalScale_=1;
    double centerX_=.5, centerY_=.5, scale_=1;
    static double move(double v, double target, double dt, double response, double limit) {
        const double step=(target-v)*(-std::expm1(-dt/std::max(.02,response)));
        return v+std::clamp(step,-limit*dt,limit*dt);
    }
public:
    void reset() { *this=Tracker{}; }
    void forget() { subject_.reset(); seen_=-1e9; }
    bool tracked(double now, const Settings &s) const { return subject_ && now-seen_<=s.hold; }
    std::optional<Face> subject() const { return subject_; }
    void observe(const std::vector<Face> &input, double captured, double now, const Settings &s) {
        if (now-captured > .75 || captured > now || captured < seen_) return;
        std::vector<Face> faces;
        for (const auto &f:input) if(f.valid() && f.confidence>=s.confidence) faces.push_back(f);
        if (faces.empty()) return;
        Face selected=faces.front();
        if (s.mode==2) {
            double right=selected.x+selected.w, bottom=selected.y+selected.h;
            for (const auto &f:faces) {
                right=std::max(right,f.x+f.w); bottom=std::max(bottom,f.y+f.h);
                selected.x=std::min(selected.x,f.x); selected.y=std::min(selected.y,f.y);
            }
            selected.w=right-selected.x; selected.h=bottom-selected.y;
        } else if(s.mode==0 && tracked(now,s)) {
            double best=std::numeric_limits<double>::infinity(); bool found=false;
            for (const auto &f:faces) {
                const double distance=std::hypot(f.cx()-subject_->cx(),f.cy()-subject_->cy());
                const double ratio=f.w*f.h/(subject_->w*subject_->h);
                if (distance > .25 || ratio < .3 || ratio > 3.3) continue;
                const double cost=distance+.06*std::abs(std::log(ratio));
                if(cost<best) { best=cost; selected=f; found=true; }
            }
            if(!found) return; // Never jump to a bystander during the loss grace period.
        } else {
            selected=*std::max_element(faces.begin(),faces.end(),[](const Face &a,const Face &b){return a.w*a.h<b.w*b.h;});
        }
        subject_=selected; seen_=captured;
    }
    Crop step(double dt, double now, const Settings &s) {
        dt=std::clamp(dt,0.0,.1);
        if(!s.freeze) {
            if(tracked(now,s)) {
                const Face &f=*subject_;
                double wanted=s.autoZoom ? f.h/std::clamp(s.faceSize,.15,.65) : 1/std::max(1.0,s.manualZoom);
                wanted=std::clamp(wanted,1/std::max(1.0,s.maxZoom),1.0);
                // Do not crop through a detected face, including in group mode.
                wanted=std::min(1.0,std::max({wanted,f.w*1.12,f.h*1.12}));
                if(std::abs(wanted-goalScale_)>.025 || !s.autoZoom) goalScale_=wanted;
                const double desiredX=f.cx()+(.5-s.anchorX)*goalScale_;
                const double desiredY=f.cy()+(.5-s.anchorY)*goalScale_;
                const double dead=s.deadZone*goalScale_;
                auto hysteresis=[dead](double old,double desired) {
                    return old+std::copysign(std::max(0.0,std::abs(desired-old)-dead),desired-old);
                };
                goalX_=hysteresis(goalX_,desiredX); goalY_=hysteresis(goalY_,desiredY);
            } else if(s.returnWide) { goalX_=.5; goalY_=.5; goalScale_=1; }
            scale_=move(scale_,goalScale_,dt,s.zoomResponse,.5);
            scale_=std::clamp(scale_,1/std::max(1.0,s.maxZoom),1.0);
            const double gx=std::clamp(goalX_,scale_*.5,1-scale_*.5);
            const double gy=std::clamp(goalY_,scale_*.5,1-scale_*.5);
            double dx=move(centerX_,gx,dt,s.response,1e6)-centerX_;
            double dy=move(centerY_,gy,dt,s.response,1e6)-centerY_;
            const double distance=std::hypot(dx,dy), maxDistance=s.maxSpeed*dt;
            if(distance>maxDistance && distance>0) { dx*=maxDistance/distance; dy*=maxDistance/distance; }
            centerX_+=dx; centerY_+=dy;
        }
        centerX_=std::clamp(centerX_,scale_*.5,1-scale_*.5);
        centerY_=std::clamp(centerY_,scale_*.5,1-scale_*.5);
        return {centerX_-scale_*.5,centerY_-scale_*.5,scale_};
    }
};
}
