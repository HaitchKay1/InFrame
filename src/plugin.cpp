// SPDX-License-Identifier: GPL-3.0-or-later
#include <obs-module.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <util/platform.h>
#include "detector.hpp"
#include <atomic>
#include <memory>
#include <mutex>

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_description(void) { return "InFrame: smooth, local face-follow framing"; }
namespace {
double nowSeconds() { return double(os_gettime_ns())/1e9; }
struct Filter {
    obs_source_t *source=nullptr;
    gs_effect_t *effect=nullptr;
    gs_texrender_t *full=nullptr, *small=nullptr;
    gs_stagesurf_t *stage=nullptr;
    std::unique_ptr<inframe::Detector> detector;
    std::mutex settingsMutex;
    inframe::Settings settings;
    inframe::Tracker tracker;
    inframe::Crop crop;
    std::atomic<bool> freezeHotkey{false}, reset{false};
    obs_hotkey_id freezeKey=OBS_INVALID_HOTKEY_ID, resetKey=OBS_INVALID_HOTKEY_ID;
    uint32_t width=0,height=0,sw=0,sh=0;
    uint64_t sequence=0;
    bool rendered=false, staged=false, validTexture=false;
    double stagedAt=0, nextCapture=0, lastFrame=0, resizedAt=0;
};
const char *name(void *) {return "InFrame — Face Tracking";}
void update(void *data, obs_data_t *d) {
    auto *f=static_cast<Filter*>(data); inframe::Settings s;
    s.faceSize=std::clamp(obs_data_get_double(d,"face_size")/100,.15,.65);
    s.maxZoom=std::clamp(obs_data_get_double(d,"max_zoom"),1.0,4.0);
    s.manualZoom=std::clamp(obs_data_get_double(d,"manual_zoom"),1.0,4.0);
    s.anchorX=std::clamp(obs_data_get_double(d,"position_x")/100,.15,.85);
    s.anchorY=std::clamp(obs_data_get_double(d,"position_y")/100,.15,.85);
    s.response=std::clamp(obs_data_get_double(d,"response")/1000,.02,2.0);
    s.zoomResponse=std::clamp(obs_data_get_double(d,"zoom_response")/1000,.1,5.0);
    s.deadZone=std::clamp(obs_data_get_double(d,"dead_zone")/100,0.0,.2);
    s.hold=std::clamp(obs_data_get_double(d,"hold"),0.0,10.0);
    s.maxSpeed=std::clamp(obs_data_get_double(d,"speed")/100,.05,3.0);
    s.confidence=std::clamp(obs_data_get_double(d,"confidence")/100,.5,.99);
    s.detectionFps=int(std::clamp(obs_data_get_int(d,"detection_fps"),10LL,30LL));
    s.mode=int(std::clamp(obs_data_get_int(d,"subject"),0LL,2LL));
    s.autoZoom=obs_data_get_bool(d,"auto_zoom");
    s.returnWide=obs_data_get_bool(d,"return_wide");
    s.freeze=obs_data_get_bool(d,"freeze");
    std::lock_guard<std::mutex> lock(f->settingsMutex);
    if(f->settings.mode!=s.mode) f->reset=true;
    f->settings=s;
}
void defaults(obs_data_t *d) {
    obs_data_set_default_int(d,"preset",0);
    obs_data_set_default_double(d,"face_size",35);
    obs_data_set_default_double(d,"max_zoom",2.5);
    obs_data_set_default_double(d,"manual_zoom",1.6);
    obs_data_set_default_double(d,"position_x",50);
    obs_data_set_default_double(d,"position_y",50);
    obs_data_set_default_double(d,"response",350);
    obs_data_set_default_double(d,"zoom_response",1000);
    obs_data_set_default_double(d,"dead_zone",3.5);
    obs_data_set_default_double(d,"hold",1.5);
    obs_data_set_default_double(d,"speed",80);
    obs_data_set_default_double(d,"confidence",75);
    obs_data_set_default_int(d,"detection_fps",15);
    obs_data_set_default_int(d,"subject",0);
    obs_data_set_default_bool(d,"auto_zoom",true);
    obs_data_set_default_bool(d,"return_wide",true);
}
bool presetChanged(obs_properties_t *,obs_property_t *,obs_data_t *d) {
    const int p=int(obs_data_get_int(d,"preset"));
    if(p==3) return false;
    obs_data_set_double(d,"face_size",p==1?40:p==2?30:35);
    obs_data_set_double(d,"response",p==1?160:p==2?650:350);
    obs_data_set_double(d,"zoom_response",p==1?700:p==2?1800:1000);
    obs_data_set_double(d,"dead_zone",p==1?1.5:p==2?6:3.5);
    obs_data_set_double(d,"speed",p==1?120:p==2?45:80);
    return true;
}
bool applyPresetClicked(obs_properties_t *,obs_property_t *,void *data) {
    if(!data) return false;
    auto *f=static_cast<Filter*>(data);
    auto *settings=obs_source_get_settings(f->source);
    presetChanged(nullptr,nullptr,settings);
    obs_source_update(f->source,settings);
    obs_data_release(settings);
    obs_source_update_properties(f->source);
    return true;
}
bool zoomChanged(obs_properties_t *p,obs_property_t *,obs_data_t *d) {
    const bool automatic=obs_data_get_bool(d,"auto_zoom");
    obs_property_set_visible(obs_properties_get(p,"face_size"),automatic);
    obs_property_set_visible(obs_properties_get(p,"manual_zoom"),!automatic);
    return true;
}
bool resetClicked(obs_properties_t *,obs_property_t *,void *data) {
    if(data) static_cast<Filter*>(data)->reset=true;
    return false;
}
bool refreshClicked(obs_properties_t *,obs_property_t *,void *data) {
    if(data) obs_source_update_properties(static_cast<Filter*>(data)->source);
    return false;
}
void tooltip(obs_property_t *p,const char *text) { obs_property_set_long_description(p,text); }
obs_properties_t *properties(void *data) {
    auto *f=static_cast<Filter*>(data);
    auto *p=obs_properties_create();
    obs_properties_add_text(p,"help","Start with Balanced. Keep space around your head in the original camera view.\nTracking is local; no camera images are saved or uploaded. SDR video only in this alpha.",OBS_TEXT_INFO);
    auto *v=obs_properties_add_list(p,"preset","Starting preset",OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(v,"Balanced",0); obs_property_list_add_int(v,"Responsive",1);
    obs_property_list_add_int(v,"Cinematic",2); obs_property_list_add_int(v,"Custom",3);
    obs_properties_add_button2(p,"apply_preset","Apply preset",applyPresetClicked,data);
    tooltip(v,"Choose a starting point, then click Apply preset. Changing this list alone does not overwrite your tuning. Custom leaves your values untouched.");
    v=obs_properties_add_bool(p,"auto_zoom","Automatically adjust zoom");
    obs_property_set_modified_callback(v,zoomChanged);
    tooltip(obs_properties_add_float_slider(p,"face_size","Face size (% of picture height)",15,65,1),"Higher = closer framing. 35% is a useful starting point for a webcam. Limited by Maximum zoom and available image edges.");
    obs_properties_add_float_slider(p,"manual_zoom","Fixed zoom",1,4,.05);
    tooltip(obs_properties_add_float_slider(p,"response","Follow smoothing (ms)",20,2000,10),"Lower follows faster. Higher makes gentler camera movements. This controls smoothing, not an added video delay.");
    obs_properties_add_float_slider(p,"position_x","Face position — left / right (%)",15,85,1);
    tooltip(obs_properties_add_float_slider(p,"position_y","Face position — top / bottom (%)",15,85,1),"50 centers your face. 40 places it a little higher. Image edges may prevent exact positioning.");
    obs_properties_add_bool(p,"freeze","Freeze framing");
    obs_properties_add_button2(p,"reacquire","Find me again",resetClicked,data);
    auto *a=obs_properties_create();
    v=obs_properties_add_list(a,"subject","Who to follow",OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(v,"Stay with my face",0); obs_property_list_add_int(v,"Largest face",1);
    obs_property_list_add_int(v,"Keep all faces in view",2);
    tooltip(v,"Stay with my face uses position and size continuity, not facial recognition. Find me again selects the largest face. Crossing or overlapping faces can still confuse it.");
    tooltip(obs_properties_add_float_slider(a,"max_zoom","Maximum zoom",1,4,.05),"Caps enlargement to protect image quality. A 4K source cropped to 1080p gives more room than a 1080p source. Digital zoom cannot add detail.");
    tooltip(obs_properties_add_float_slider(a,"dead_zone","Ignore small movements (%)",0,20,.5),"A calm zone measured against the cropped picture. Set to 0 for the closest centering; increase to reduce fidget-driven movement.");
    obs_properties_add_float_slider(a,"zoom_response","Zoom smoothing (ms)",100,5000,50);
    obs_properties_add_float_slider(a,"speed","Maximum pan speed (% of source / second)",5,300,5);
    obs_properties_add_float_slider(a,"hold","Hold shot after face disappears (seconds)",0,10,.1);
    tooltip(obs_properties_add_bool(a,"return_wide","Then slowly return to the full camera view"),"Turn off to keep the last framing until a face is found again.");
    tooltip(obs_properties_add_int_slider(a,"detection_fps","Face checks per second",10,30,1),"15 is a starting point. Rendering still runs at your OBS frame rate. Actual detection rate depends on CPU performance.");
    obs_properties_add_float_slider(a,"confidence","Minimum detection confidence (%)",50,99,1);
    obs_properties_add_group(p,"advanced","Advanced",OBS_GROUP_NORMAL,a);
    std::string status=f && f->detector ? f->detector->status():"Detector starts when this filter is added.";
    if(f && f->freezeHotkey) status+=" | Hotkey freeze ON";
    obs_properties_add_text(p,"status",status.c_str(),OBS_TEXT_INFO);
    obs_properties_add_button2(p,"refresh","Refresh detector status",refreshClicked,data);
    obs_properties_add_text(p,"hotkeys","Optional hotkeys: OBS Settings > Hotkeys > InFrame.\nHotkey freeze and the Freeze framing checkbox are independent; turn both off to resume.",OBS_TEXT_INFO);
    return p;
}
void freeGraphics(Filter *f) {
    gs_stagesurface_destroy(f->stage); f->stage=nullptr;
    gs_texrender_destroy(f->full); f->full=nullptr;
    gs_texrender_destroy(f->small); f->small=nullptr;
    f->staged=false; f->validTexture=false;
}
void destroy(void *data) {
    auto *f=static_cast<Filter*>(data); if(!f) return;
    if(f->freezeKey!=OBS_INVALID_HOTKEY_ID) obs_hotkey_unregister(f->freezeKey);
    if(f->resetKey!=OBS_INVALID_HOTKEY_ID) obs_hotkey_unregister(f->resetKey);
    f->detector.reset();
    obs_enter_graphics(); freeGraphics(f); gs_effect_destroy(f->effect); obs_leave_graphics();
    delete f;
}
void *create(obs_data_t *d,obs_source_t *source) {
    auto *f=new Filter; f->source=source;
    char *path=obs_module_file("crop.effect");
    obs_enter_graphics();
    if(path) f->effect=gs_effect_create_from_file(path,nullptr);
    obs_leave_graphics(); bfree(path);
    if(!f->effect) {destroy(f); return nullptr;}
    path=obs_module_file("models/face_detection_yunet_2023mar.onnx");
    try { f->detector=std::make_unique<inframe::Detector>(path?path:""); }
    catch(const std::exception &e) {blog(LOG_ERROR,"[InFrame] %s",e.what()); bfree(path); destroy(f); return nullptr;}
    bfree(path); update(f,d);
    f->freezeKey=obs_hotkey_register_source(source,"inframe.freeze","InFrame: Toggle freeze framing",[](void *data,obs_hotkey_id,obs_hotkey_t *,bool pressed){
        auto *f=static_cast<Filter*>(data); if(pressed) f->freezeHotkey=!f->freezeHotkey.load();
    },f);
    f->resetKey=obs_hotkey_register_source(source,"inframe.reacquire","InFrame: Find me again",[](void *data,obs_hotkey_id,obs_hotkey_t *,bool pressed){if(pressed) static_cast<Filter*>(data)->reset=true;},f);
    return f;
}
void tick(void *data,float) {static_cast<Filter*>(data)->rendered=false;}
bool capture(Filter *f,obs_source_t *target,obs_source_t *parent) {
    gs_texrender_reset(f->full);
    if(!gs_texrender_begin(f->full,f->width,f->height)) return false;
    vec4 clear; vec4_zero(&clear); gs_clear(GS_CLEAR_COLOR,&clear,0,0);
    gs_ortho(0,float(f->width),0,float(f->height),-100,100);
    gs_blend_state_push();
    gs_blend_function_separate(GS_BLEND_SRCALPHA,GS_BLEND_INVSRCALPHA,GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
    const auto flags=obs_source_get_output_flags(parent);
    if(target==parent && !(flags&OBS_SOURCE_CUSTOM_DRAW) && !(flags&OBS_SOURCE_ASYNC)) obs_source_default_render(target);
    else obs_source_video_render(target);
    gs_blend_state_pop(); gs_texrender_end(f->full);
    return true;
}
void analyze(Filter *f,double now) {
    // Read last frame's small staging texture, never the full-resolution camera.
    if(f->staged) {
        uint8_t *bytes=nullptr; uint32_t stride=0;
        if(gs_stagesurface_map(f->stage,&bytes,&stride)) {
            try {f->detector->submit(bytes,stride,int(f->sw),int(f->sh),f->stagedAt);}
            catch(const std::exception &e) {blog(LOG_WARNING,"[InFrame] frame copy failed: %s",e.what());}
            gs_stagesurface_unmap(f->stage);
        }
        f->staged=false;
    }
    if(now<f->nextCapture) return;
    gs_texrender_reset(f->small);
    if(!gs_texrender_begin(f->small,f->sw,f->sh)) return;
    vec4 clear; vec4_zero(&clear); gs_clear(GS_CLEAR_COLOR,&clear,0,0);
    gs_ortho(0,float(f->width),0,float(f->height),-100,100);
    gs_blend_state_push(); gs_blend_function(GS_BLEND_ONE,GS_BLEND_ZERO);
    auto *effect=obs_get_base_effect(OBS_EFFECT_DEFAULT);
    gs_effect_set_texture(gs_effect_get_param_by_name(effect,"image"),gs_texrender_get_texture(f->full));
    while(gs_effect_loop(effect,"Draw")) gs_draw_sprite(gs_texrender_get_texture(f->full),0,f->width,f->height);
    gs_blend_state_pop(); gs_texrender_end(f->small);
    gs_stage_texture(f->stage,gs_texrender_get_texture(f->small)); f->staged=true; f->stagedAt=now;
}
void render(void *data,gs_effect_t *) {
    auto *f=static_cast<Filter*>(data);
    auto *target=obs_filter_get_target(f->source),*parent=obs_filter_get_parent(f->source);
    if(!target||!parent) {obs_source_skip_video_filter(f->source); return;}
    // Deliberate passthrough for HDR; do not silently clip or reinterpret HDR pixels.
    const gs_color_space preferred[]={GS_CS_SRGB,GS_CS_SRGB_16F,GS_CS_709_EXTENDED,GS_CS_709_SCRGB};
    if(obs_source_get_color_space(target,4,preferred)!=GS_CS_SRGB || gs_get_color_space()!=GS_CS_SRGB) {
        obs_source_skip_video_filter(f->source); return;
    }
    const uint32_t w=obs_source_get_base_width(target),h=obs_source_get_base_height(target);
    if(!w||!h) {obs_source_skip_video_filter(f->source); return;}
    const double now=nowSeconds();
    if(w!=f->width||h!=f->height) {
        freeGraphics(f); f->width=w; f->height=h;
        const double ratio=512.0/std::max(w,h);
        f->sw=std::max(32u,uint32_t(std::round(w*ratio/32))*32);
        f->sh=std::max(32u,uint32_t(std::round(h*ratio/32))*32);
        f->full=gs_texrender_create(GS_RGBA,GS_ZS_NONE);
        f->small=gs_texrender_create(GS_RGBA,GS_ZS_NONE);
        f->stage=gs_stagesurface_create(f->sw,f->sh,GS_RGBA);
        f->tracker.reset(); f->resizedAt=now; f->rendered=false;
    }
    if(!f->full||!f->small||!f->stage) {obs_source_skip_video_filter(f->source); return;}
    if(!f->rendered) {
        inframe::Settings s; {std::lock_guard<std::mutex> lock(f->settingsMutex); s=f->settings;}
        s.freeze=s.freeze||f->freezeHotkey.load();
        if(f->reset.exchange(false)) {f->tracker.forget(); f->resizedAt=now;}
        f->validTexture=capture(f,target,parent);
        if(!f->validTexture) {obs_source_skip_video_filter(f->source); return;}
        analyze(f,now);
        if(now>=f->nextCapture) f->nextCapture=now+1.0/s.detectionFps;
        auto result=f->detector->latest();
        if(result.sequence!=f->sequence) {
            f->sequence=result.sequence;
            if(result.captured>=f->resizedAt) f->tracker.observe(result.faces,result.captured,now,s);
        }
        f->crop=f->tracker.step(f->lastFrame?now-f->lastFrame:1.0/60,now,s);
        f->lastFrame=now; f->rendered=true;
    }
    if(!f->validTexture) {obs_source_skip_video_filter(f->source); return;}
    vec2 offset; vec2_set(&offset,float(f->crop.x),float(f->crop.y));
    auto *texture=gs_texrender_get_texture(f->full);
    gs_effect_set_texture(gs_effect_get_param_by_name(f->effect,"image"),texture);
    gs_effect_set_vec2(gs_effect_get_param_by_name(f->effect,"crop_offset"),&offset);
    gs_effect_set_float(gs_effect_get_param_by_name(f->effect,"crop_scale"),float(f->crop.scale));
    gs_blend_state_push(); gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
    while(gs_effect_loop(f->effect,"Draw")) gs_draw_sprite(texture,0,w,h);
    gs_blend_state_pop();
}
}
bool obs_module_load(void) {
    obs_source_info info={}; info.id="inframe_filter"; info.type=OBS_SOURCE_TYPE_FILTER;
    info.output_flags=OBS_SOURCE_VIDEO; info.get_name=name; info.create=create; info.destroy=destroy;
    info.update=update; info.get_defaults=defaults; info.get_properties=properties;
    info.video_tick=tick; info.video_render=render;
    obs_register_source(&info);
    return true;
}
