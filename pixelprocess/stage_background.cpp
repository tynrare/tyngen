/*
 * File version: 1.2.0
 * Last updated: 2026-10-05
 * Purpose: Learn border background colors and classify connected regions.
 * Flow PIXEL-01 step 1: border Lab model -> connected mask -> transparent preview.
 */
/* agent: gpt-6.1-sol | 2026-10-05 | Learn connected background colors | v 1.2.0 | 5ecd95 */
#include "common.hpp"
#include <algorithm>
#include <array>
#include <cmath>

struct BackgroundModel { cv::Vec3b center{}; int tolerance=24; int border_samples=0; bool valid=false; };

/* Estimate from RGBA and Lab cv::Mat plus int tolerance; return BackgroundModel. */
static BackgroundModel estimate_background(const cv::Mat &image, const cv::Mat &lab, int base_tolerance) {
    std::array<std::vector<int>,3> samples;
    auto add = [&](int x, int y) {
        if(image.at<cv::Vec4b>(y,x)[3]==0)return;
        auto p=lab.at<cv::Vec3b>(y,x);
        for(int c=0;c<3;c++) samples[(size_t)c].push_back(p[c]);
    };
    for(int x=0;x<lab.cols;x++){add(x,0);if(lab.rows>1)add(x,lab.rows-1);}
    for(int y=1;y+1<lab.rows;y++){add(0,y);if(lab.cols>1)add(lab.cols-1,y);}
    BackgroundModel model; model.border_samples=(int)samples[0].size();model.valid=model.border_samples>0;
    if(!model.valid)return model;
    for(int c=0;c<3;c++){
        auto &v=samples[(size_t)c];auto middle=v.begin()+v.size()/2;
        std::nth_element(v.begin(),middle,v.end());model.center[c]=(uchar)*middle;
    }
    std::vector<int> distances;distances.reserve((size_t)model.border_samples);
    for(int y=0;y<lab.rows;y++)for(int x=0;x<lab.cols;x++)if(x==0||y==0||x==lab.cols-1||y==lab.rows-1){
        auto p=lab.at<cv::Vec3b>(y,x);int d0=(int)p[0]-model.center[0],d1=(int)p[1]-model.center[1],d2=(int)p[2]-model.center[2];
        distances.push_back((int)std::lround(std::sqrt(d0*d0+d1*d1+d2*d2)));
    }
    auto q=distances.begin()+distances.size()*95/100;std::nth_element(distances.begin(),q,distances.end());
    model.tolerance=std::clamp(std::max(base_tolerance,*q+4),8,48);
    return model;
}

/* Process CLI argc and argv, publish artifacts, and return int status. */
int main(int argc, char **argv) {
    auto a=pp::parse_args(argc,argv);
    for(const char *key:{"input","output","debug","mask","meta"})if(!a.has(key))return pp::input_error(std::string("missing --")+key);
    cv::Mat image=pp::read_rgba(a.get("input"));if(image.empty())return pp::input_error("cannot decode input image");
    int base=std::clamp((int)a.number("tolerance",24),1,64);cv::Mat bgr,lab;cv::cvtColor(image,bgr,cv::COLOR_BGRA2BGR);cv::cvtColor(bgr,lab,cv::COLOR_BGR2Lab);
    BackgroundModel model=estimate_background(image,lab,base);cv::Mat candidates(image.rows,image.cols,CV_8UC1,cv::Scalar(0));
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++){
        auto p=image.at<cv::Vec4b>(y,x);if(p[3]==0){candidates.at<uchar>(y,x)=255;continue;}if(!model.valid)continue;
        auto q=lab.at<cv::Vec3b>(y,x);int d0=(int)q[0]-model.center[0],d1=(int)q[1]-model.center[1],d2=(int)q[2]-model.center[2];
        if(d0*d0+d1*d1+d2*d2<=model.tolerance*model.tolerance)candidates.at<uchar>(y,x)=255;
    }
    cv::Mat labels,stats,centers;int components=cv::connectedComponentsWithStats(candidates,labels,stats,centers,4,CV_32S);
    std::vector<uchar> border_component((size_t)components,0);
    auto mark=[&](int x,int y){if(candidates.at<uchar>(y,x))border_component[(size_t)labels.at<int>(y,x)]=1;};
    for(int x=0;x<image.cols;x++){mark(x,0);mark(x,image.rows-1);}
    for(int y=0;y<image.rows;y++){mark(0,y);mark(image.cols-1,y);}
    cv::Mat mask(image.rows,image.cols,CV_8UC1,cv::Scalar(0));int bg=0;
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++)if(border_component[(size_t)labels.at<int>(y,x)]){mask.at<uchar>(y,x)=255;bg++;}
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++)if(image.at<cv::Vec4b>(y,x)[3]==0&&!mask.at<uchar>(y,x)){mask.at<uchar>(y,x)=255;bg++;}
    cv::Mat clean=image.clone(),debug=image.clone();
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++)if(mask.at<uchar>(y,x)){
        clean.at<cv::Vec4b>(y,x)=cv::Vec4b(0,0,0,0);auto &d=debug.at<cv::Vec4b>(y,x);d=cv::Vec4b((d[0]+255)/2,d[1]/2,(d[2]+255)/2,255);
    }
    if(!pp::write_png_atomic(a.get("output"),clean)||!pp::write_png_atomic(a.get("debug"),debug)||!pp::write_png_atomic(a.get("mask"),mask))return pp::input_error("could not publish background artifacts");
    std::string temp;auto fs=pp::json_writer(a.get("meta"),temp);if(!fs.isOpened())return pp::input_error("cannot write stage metadata");
    bool weak=bg==0||bg>(int)(image.total()*.97);
    fs<<"schema_version"<<1<<"stage_id"<<"background"<<"state"<<(weak?"completed_with_warnings":"completed");
    fs<<"model"<<"{"<<"valid"<<model.valid<<"color_space"<<"CIELAB_8U"<<"border_center"<<"["<<(int)model.center[0]<<(int)model.center[1]<<(int)model.center[2]<<"]"<<"border_samples"<<model.border_samples<<"base_tolerance"<<base<<"effective_tolerance"<<model.tolerance<<"connectivity"<<4<<"}";
    fs<<"measurements"<<"{"<<"background_pixels"<<bg<<"background_fraction"<<(double)bg/image.total()<<"candidate_components"<<components-1<<"}";
    fs<<"warnings"<<"[";if(bg==0)fs<<"no_border_connected_background_detected";else if(bg>(int)(image.total()*.97))fs<<"background_model_covers_nearly_entire_image";fs<<"]";
    fs<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"background_mask"<<a.get("mask")<<"}";
    if(!pp::publish_json(fs,temp,a.get("meta")))return pp::input_error("could not publish stage metadata");
    std::cout<<"completed background mask pixels="<<bg<<" tolerance_lab="<<model.tolerance<<'\n';return 0;
}

/* agent: gpt-6.1-sol | 2026-10-05 | Learn connected background colors | v 1.2.0 | 5ecd95 */
