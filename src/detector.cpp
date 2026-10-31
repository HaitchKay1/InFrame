// SPDX-License-Identifier: GPL-3.0-or-later
#include "detector.hpp"
#include <opencv2/imgproc.hpp>
#include <chrono>
namespace inframe {
Detector::~Detector() {
    {std::lock_guard<std::mutex> lock(mutex_); stopping_=true;}
    wake_.notify_one();
    if(thread_.joinable()) thread_.join();
}
void Detector::submit(const uint8_t *rgba,uint32_t stride,int width,int height,double captured) {
    // A single replaceable mailbox: inference never accumulates a queue of old video.
    cv::Mat copy(height,width,CV_8UC4,const_cast<uint8_t*>(rgba),stride);
    {std::lock_guard<std::mutex> lock(mutex_); if(stopping_) return;
        frame_=copy.clone(); captured_=captured; pending_=true;}
    wake_.notify_one();
}
Detection Detector::latest() {std::lock_guard<std::mutex> lock(mutex_); return latest_;}
std::string Detector::status() {std::lock_guard<std::mutex> lock(mutex_); return status_;}
void Detector::run(const std::string &model) {
    try {
        auto detector=cv::FaceDetectorYN::create(model,"",cv::Size(320,320),.5f,.3f,1000);
        {std::lock_guard<std::mutex> lock(mutex_); status_="Ready — YuNet CPU / GPU framing";}
        for(;;) {
            cv::Mat rgba; double captured;
            {std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock,[this]{return stopping_||pending_;});
                if(stopping_) return;
                rgba=std::move(frame_); captured=captured_; pending_=false;}
            const auto start=std::chrono::steady_clock::now();
            cv::Mat bgr,faces;
            cv::cvtColor(rgba,bgr,cv::COLOR_RGBA2BGR);
            detector->setInputSize(bgr.size()); detector->detect(bgr,faces);
            Detection result; result.captured=captured;
            for(int i=0;i<faces.rows;++i) {
                const float *f=faces.ptr<float>(i);
                result.faces.push_back({f[0]/bgr.cols,f[1]/bgr.rows,f[2]/bgr.cols,f[3]/bgr.rows,f[14]});
            }
            result.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            {std::lock_guard<std::mutex> lock(mutex_); result.sequence=latest_.sequence+1; latest_=std::move(result);
                status_="YuNet CPU: "+std::to_string(latest_.faces.size())+" face(s), "+std::to_string(int(latest_.milliseconds))+" ms inference";}
        }
    } catch(const std::exception &e) {
        std::lock_guard<std::mutex> lock(mutex_);
        status_=std::string("Detector unavailable: ")+e.what();
        stopping_=true;
    }
}
}
