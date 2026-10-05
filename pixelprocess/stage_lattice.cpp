/*
 * File version: 1.2.0
 * Last updated: 2026-10-05
 * Purpose: Fit a fractional straight square lattice independently per sprite.
 * Flow PIXEL-01 step 4: owned crops -> pitch/phase candidates, confidence, and debug overlay.
 */
/* agent: gpt-6.1-sol | 2026-10-05 | Prevent harmonic pitch overfit | v 1.2.0 | b3c2fd */
#include "common.hpp"
#include <filesystem>
#include <cmath>

struct Fit { double pitch=4, phase_x=0, phase_y=0, confidence=0; std::vector<double> candidates, phases_x, phases_y; };

/* Fit cv::Mat inputs and double pitch bounds; return Fit. */
static Fit fit_lattice(const cv::Mat &image, const cv::Mat &background, int source_x, int source_y, double forced, double minimum, double maximum, double precision) {
    Fit best; cv::Mat gray, edge;
    cv::cvtColor(image,gray,cv::COLOR_BGRA2GRAY); cv::Sobel(gray,edge,CV_32F,1,0,3);
    cv::Mat edge_y; cv::Sobel(gray,edge_y,CV_32F,0,1,3);
    cv::Mat alpha; cv::extractChannel(image,alpha,3);
    std::vector<double> ex((size_t)image.cols,0), ey((size_t)image.rows,0); double total_x=0,total_y=0;
    for(int y=1;y<image.rows-1;y++)for(int x=1;x<image.cols-1;x++)if(alpha.at<uchar>(y,x)>0&&!background.at<uchar>(source_y+y,source_x+x)){double gx=std::abs(edge.at<float>(y,x));double gy=std::abs(edge_y.at<float>(y,x));ex[(size_t)x]+=gx;ey[(size_t)y]+=gy;total_x+=gx;total_y+=gy;}
    double step=std::max(.01,precision), start=forced>0?forced:minimum, end=forced>0?forced:maximum;
    double best_score=-1;
    for(double pitch=start;pitch<=end+step*.5;pitch+=step){
        double phase_step=std::max(.1,pitch/32.0), score=0, phase_x=0,phase_y=0;
        auto axis=[&](const std::vector<double>&energy,double total,double &phase){
            if(total<=0){phase=0;return .0;} double local_best=-1;
            for(double ph=0;ph<pitch;ph+=phase_step){double sum=0;for(size_t i=0;i<energy.size();i++){double delta=std::abs(std::remainder((double)i-ph,pitch));if(delta<=.55)sum+=energy[i]*(1.0-delta/1.1);}
                double value=sum/total; if(value>local_best){local_best=value;phase=ph;}}
            return local_best;
        };
        double sx=axis(ex,total_x,phase_x),sy=axis(ey,total_y,phase_y);
        double candidate=(sx+sy)*.5 / std::max(.08,1.1/pitch);
        if(candidate>best_score){best_score=candidate;best.pitch=pitch;best.phase_x=phase_x;best.phase_y=phase_y;best.confidence=std::min(1.0,std::max(0.0,(candidate-0.35)/1.8));}
        if(forced<=0){best.candidates.push_back(candidate);best.phases_x.push_back(phase_x);best.phases_y.push_back(phase_y);}
    }
    if(forced<=0)for(size_t i=0;i<best.candidates.size();i++)if(best.candidates[i]>=best_score*.60){
        best.pitch=start+i*step;best.phase_x=best.phases_x[i];best.phase_y=best.phases_y[i];
        best.confidence=std::min(1.0,std::max(0.0,(best.candidates[i]-.35)/1.8));break;
    }
    if(forced>0)best.confidence=std::max(best.confidence,.45);
    return best;
}

/* Process CLI argc and argv, fit sprites, and return int status. */
int main(int argc,char **argv){
    auto a=pp::parse_args(argc,argv);for(const char*k:{"input","mask","state","output","debug","meta","sprite-dir"})if(!a.has(k))return pp::input_error(std::string("missing --")+k);
    cv::Mat source=pp::read_rgba(a.get("input")),background=cv::imread(a.get("mask"),cv::IMREAD_GRAYSCALE);auto extracted=pp::read_json(a.get("state"));if(source.empty()||background.empty()||source.size()!=background.size()||!extracted.isOpened())return pp::input_error("source, mask, or extraction metadata unavailable");
    auto settings=pp::read_json(a.get("settings"));double minp=4,maxp=16,precision=.1;std::string mode=a.get("pitch","auto"),phase_x="auto",phase_y="auto";
    if(settings.isOpened()){if(!settings["min_pitch"].empty())minp=(double)settings["min_pitch"];if(!settings["max_pitch"].empty())maxp=(double)settings["max_pitch"];if(!settings["pitch_precision"].empty())precision=(double)settings["pitch_precision"];if(!settings["pitch"].empty())mode=(std::string)settings["pitch"];if(!settings["phase_x"].empty())phase_x=(std::string)settings["phase_x"];if(!settings["phase_y"].empty())phase_y=(std::string)settings["phase_y"];}
    double forced=0;try{if(mode!="auto")forced=std::stod(mode);}catch(...){return pp::input_error("pitch must be auto or a positive fractional value");}
    if(!std::isfinite(minp)||!std::isfinite(maxp)||!std::isfinite(precision)||minp<1||maxp>128||minp>maxp||precision<.01||precision>4||!std::isfinite(forced)||forced<0||(forced>0&&forced<1))return pp::input_error("pitch search settings are out of range");
    cv::Mat debug=source.clone();auto sprites=extracted["sprites"];std::string tmp;auto fs=pp::json_writer(a.get("meta"),tmp);
    fs<<"schema_version"<<1<<"stage_id"<<"lattice"<<"state"<<"completed"<<"coordinate_space"<<"source_pixels"<<"sprites"<<"[";
    bool uncertain=false;
    auto overrides=settings["sprite_overrides"];
    for(auto it=sprites.begin();it!=sprites.end();++it){std::string id=(std::string)(*it)["id"],path=(std::string)(*it)["path"];cv::Mat crop=pp::read_rgba(path);if(crop.empty())continue;
        int x0=0,y0=0;std::vector<int> rect;(*it)["crop_bounds"]>>rect;if(rect.size()==4){x0=rect[0];y0=rect[1];}
        std::string sprite_pitch=mode,sprite_phase_x=phase_x,sprite_phase_y=phase_y;
        for(auto oi=overrides.begin();oi!=overrides.end();++oi)if((std::string)(*oi)["id"]==id){if(!(*oi)["pitch"].empty())sprite_pitch=(std::string)(*oi)["pitch"];if(!(*oi)["phase_x"].empty())sprite_phase_x=(std::string)(*oi)["phase_x"];if(!(*oi)["phase_y"].empty())sprite_phase_y=(std::string)(*oi)["phase_y"];}
        double sprite_forced=0;try{if(sprite_pitch!="auto")sprite_forced=std::stod(sprite_pitch);}catch(...){return pp::input_error("sprite pitch override is invalid");}
        if(!std::isfinite(sprite_forced)||sprite_forced<0||(sprite_forced>0&&sprite_forced<1))return pp::input_error("sprite pitch override is invalid");
        Fit f=fit_lattice(crop,background,x0,y0,sprite_forced,minp,maxp,precision);
        try{if(sprite_phase_x!="auto")f.phase_x=std::stod(sprite_phase_x)-x0;if(sprite_phase_y!="auto")f.phase_y=std::stod(sprite_phase_y)-y0;}catch(...){return pp::input_error("sprite phase override is invalid");}
        if(!std::isfinite(f.phase_x)||!std::isfinite(f.phase_y))return pp::input_error("sprite phase override is invalid");
        int x1=x0+crop.cols,y1=y0+crop.rows;
        for(double x=f.phase_x+x0;x<x1;x+=f.pitch)if(x>=x0)cv::line(debug,cv::Point((int)std::lround(x),y0),cv::Point((int)std::lround(x),y1-1),cv::Scalar(255,160,0,200),1);
        for(double y=f.phase_y+y0;y<y1;y+=f.pitch)if(y>=y0)cv::line(debug,cv::Point(x0,(int)std::lround(y)),cv::Point(x1-1,(int)std::lround(y)),cv::Scalar(255,160,0,200),1);
        cv::putText(debug,id+" p="+std::to_string(f.pitch),cv::Point(x0+2,std::max(14,y0+14)),cv::FONT_HERSHEY_SIMPLEX,.35,cv::Scalar(0,0,0,255),2,cv::LINE_AA);
        cv::putText(debug,id+" p="+std::to_string(f.pitch),cv::Point(x0+2,std::max(14,y0+14)),cv::FONT_HERSHEY_SIMPLEX,.35,cv::Scalar(255,255,255,255),1,cv::LINE_AA);
        uncertain|=f.confidence<.25;fs<<"{"<<"id"<<id<<"pitch"<<f.pitch<<"phase_x"<<f.phase_x+x0<<"phase_y"<<f.phase_y+y0<<"confidence"<<f.confidence<<"forced"<<(sprite_forced>0)<<"candidates"<<"[";
        for(size_t i=0;i<f.candidates.size();++i)fs<<"{"<<"pitch"<<minp+i*precision<<"score"<<f.candidates[i]<<"}";fs<<"]"<<"manual_input_required"<<(f.confidence<.25)<<"}";
    }
    fs<<"]"<<"warnings"<<"[";if(uncertain)fs<<"weak_or_ambiguous_pitch_evidence";fs<<"]"<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"}";
    if(!pp::write_png_atomic(a.get("output"),debug)||!pp::write_png_atomic(a.get("debug"),source)||!pp::publish_json(fs,tmp,a.get("meta")))return pp::input_error("could not publish lattice result");
    std::cout<<"completed lattice sprites="<<sprites.size()<<(uncertain?" with warnings":"")<<'\n';return 0;
}

/* agent: gpt-6.1-sol | 2026-10-05 | Prevent harmonic pitch overfit | v 1.2.0 | b3c2fd */
