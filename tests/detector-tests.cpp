// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/detector.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <fstream>
int main(int argc,char **argv) {
    if(argc<2) return 2;
    sf::Detector d(argv[1]);
    cv::Mat input(288,512,CV_8UC4,cv::Scalar(0,0,0,255));
    d.submit(input.data,uint32_t(input.step),input.cols,input.rows,1.0);
    for(int i=0;i<300;++i) {
        auto r=d.latest();
        if(r.sequence) {
            if(!r.faces.empty() || r.captured!=1.0) return 1;
            std::cout<<"PASS: model loads, blank frame has no faces, worker publishes result and stops cleanly ("<<r.milliseconds<<" ms)\n";
            sf::Detector bad("missing-model.onnx");
            for(int j=0;j<100;++j) {
                if(bad.status().find("unavailable")!=std::string::npos) {std::cout<<"PASS: missing-model failure is contained\n"; return 0;}
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return 1;
        }
        if(d.status().find("unavailable")!=std::string::npos) {std::cerr<<d.status(); return 1;}
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::cerr<<"Detector timed out: "<<d.status(); return 1;
}
