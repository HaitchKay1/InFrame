// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tracker.hpp"
#include <opencv2/core.hpp>
#include <opencv2/objdetect.hpp>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
namespace inframe {
struct Detection { std::vector<Face> faces; double captured=0, milliseconds=0; uint64_t sequence=0; };
class Detector {
    std::mutex mutex_;
    std::condition_variable wake_;
    std::thread thread_;
    bool stopping_=false, pending_=false;
    cv::Mat frame_;
    double captured_=0;
    Detection latest_;
    std::string status_="Starting face detector...";
    void run(const std::string &model);
public:
    explicit Detector(const std::string &model) { thread_=std::thread([this,model]{run(model);}); }
    ~Detector();
    void submit(const uint8_t *rgba, uint32_t stride, int width, int height, double captured);
    Detection latest();
    std::string status();
};
}
