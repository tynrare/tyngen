/*
 * File version: 1.4.0
 * Last updated: 2026-10-09
 * Purpose: Learn background color mass and fill probed regions.
 * Flow PIXEL-01 step 1: mass model + probes -> connected mask -> preview.
 * Flow BPROBE-01 runner.cpp steps 1-3 owns bootstrap, probe selection, and final ownership ordering.
 */
/* agent: gpt-6.1-sol | 2026-10-09 | Record probed background region evidence | v 1.4.0 | 1f9442 */
#include "common.hpp"
#include <algorithm>
#include <array>
#include <cmath>

struct BackgroundModel { cv::Vec3b center{}; int tolerance=24; int border_samples=0; bool valid=false; };

/* Infer slot count from candidate mask and return source boundaries. */
static std::vector<int> grid_edges(const cv::Mat &candidates,bool vertical,int requested) {
    int length=vertical?candidates.cols:candidates.rows;std::vector<int> edges{0};
    if(requested>0){for(int i=1;i<requested;i++)edges.push_back(length*i/requested);edges.push_back(length);return edges;}
    std::vector<int> foreground((size_t)length,0);
    for(int i=0;i<length;i++)for(int j=0;j<(vertical?candidates.rows:candidates.cols);j++)
        foreground[(size_t)i]+=candidates.at<uchar>(vertical?j:i,vertical?i:j)==0;
    int begin=-1;
    for(int i=0;i<=length;i++){bool gap=i<length&&foreground[(size_t)i]==0;if(gap&&begin<0)begin=i;
        if(!gap&&begin>=0){if(begin>0&&i<length&&i-begin>=2)edges.push_back((begin+i)/2);begin=-1;}}
    edges.push_back(length);int count=(int)edges.size()-1;edges.clear();edges.push_back(0);
    for(int i=1;i<count;i++)edges.push_back(length*i/count);edges.push_back(length);return edges;
}

/* Find the most frequent quantized Lab color and return its center. */
static cv::Vec3b mass_color(const cv::Mat &image,const cv::Mat &lab) {
    std::array<int,32768> counts{};
    for(int y=0;y<lab.rows;y++)for(int x=0;x<lab.cols;x++)if(image.at<cv::Vec4b>(y,x)[3]){
        auto p=lab.at<cv::Vec3b>(y,x);int key=(p[0]>>3)<<10|(p[1]>>3)<<5|(p[2]>>3);counts[(size_t)key]++;
    }
    auto best=std::max_element(counts.begin(),counts.end());
    int key=(int)(best-counts.begin());
    return cv::Vec3b((key>>10&31)*8+4,(key>>5&31)*8+4,(key&31)*8+4);
}

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
    BackgroundModel model=estimate_background(image,lab,base);cv::Vec3b mass=mass_color(image,lab);cv::Mat candidates(image.rows,image.cols,CV_8UC1,cv::Scalar(0));int mass_pixels=0;
    auto settings=a.has("settings")?pp::read_json(a.get("settings")):cv::FileStorage();
    std::vector<cv::Point> manual;std::vector<cv::Vec3b> manual_colors;std::vector<std::string>manual_ids;auto manual_nodes=settings["background_probes"];
    for(auto it=manual_nodes.begin();it!=manual_nodes.end();++it){int x=(int)(*it)["x"],y=(int)(*it)["y"];
        if(x<0||y<0||x>=image.cols||y>=image.rows)continue;
        manual.emplace_back(x,y);if(image.at<cv::Vec4b>(y,x)[3])manual_colors.push_back(lab.at<cv::Vec3b>(y,x));std::string id=(std::string)(*it)["id"];manual_ids.push_back(id.empty()?"manual-"+std::to_string(manual.size()-1):id);}
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++){
        auto p=image.at<cv::Vec4b>(y,x);if(p[3]==0){candidates.at<uchar>(y,x)=255;continue;}
        auto q=lab.at<cv::Vec3b>(y,x);int m0=(int)q[0]-mass[0],m1=(int)q[1]-mass[1],m2=(int)q[2]-mass[2];
        int b0=(int)q[0]-model.center[0],b1=(int)q[1]-model.center[1],b2=(int)q[2]-model.center[2];
        bool selected=m0*m0+m1*m1+m2*m2<=base*base||(model.valid&&b0*b0+b1*b1+b2*b2<=model.tolerance*model.tolerance);
        for(const auto &center:manual_colors){int d0=(int)q[0]-center[0],d1=(int)q[1]-center[1],d2=(int)q[2]-center[2];if(d0*d0+d1*d1+d2*d2<=base*base){selected=true;break;}}
        if(m0*m0+m1*m1+m2*m2<=base*base)mass_pixels++;if(selected)candidates.at<uchar>(y,x)=255;
    }
    cv::Mat labels,stats,centers;int components=cv::connectedComponentsWithStats(candidates,labels,stats,centers,4,CV_32S);
    std::vector<uchar> border_component((size_t)components,0);
    auto mark=[&](int x,int y){if(candidates.at<uchar>(y,x))border_component[(size_t)labels.at<int>(y,x)]=1;};
    for(int x=0;x<image.cols;x++){mark(x,0);mark(x,image.rows-1);}
    for(int y=0;y<image.rows;y++){mark(0,y);mark(image.cols-1,y);}
    cv::Mat mask(image.rows,image.cols,CV_8UC1,cv::Scalar(0));int bg=0;
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++)if(border_component[(size_t)labels.at<int>(y,x)]){mask.at<uchar>(y,x)=255;bg++;}
    /* Automatic probes seed one central candidate point in every inferred sprite slot. */
    int requested_rows=(int)a.number("grid_rows",0),requested_cols=(int)a.number("grid_cols",0);
    auto x_edges=grid_edges(candidates,true,requested_cols),y_edges=grid_edges(candidates,false,requested_rows);
    if(x_edges.size()<2)x_edges={0,image.cols};if(y_edges.size()<2)y_edges={0,image.rows};
    int rows=(int)y_edges.size()-1,cols=(int)x_edges.size()-1;
    struct Probe{std::string id;cv::Point point;bool automatic;int region,area;cv::Rect bounds;};std::vector<Probe> probes;
    std::vector<std::string> disabled;auto disabled_nodes=settings["disabled_background_probes"];
    for(auto it=disabled_nodes.begin();it!=disabled_nodes.end();++it)disabled.push_back((std::string)*it);
    for(int slot=0;slot<rows*cols;slot++){
        int row=slot/cols,col=slot%cols,x0=x_edges[(size_t)col],x1=x_edges[(size_t)col+1];
        int y0=y_edges[(size_t)row],y1=y_edges[(size_t)row+1];
        int cx=(x0+x1)/2,cy=(y0+y1)/2,best_label=0;double nearest=1e30;cv::Point chosen;
        for(int yy=y0;yy<y1;yy++)for(int xx=x0;xx<x1;xx++)if(candidates.at<uchar>(yy,xx)){
            double dx=xx-cx,dy=yy-cy,d=dx*dx+dy*dy;if(d<nearest){nearest=d;chosen={xx,yy};best_label=labels.at<int>(yy,xx);}
        }
        if(best_label){std::string id="auto-r"+std::to_string(row)+"-c"+std::to_string(col);
            if(std::find(disabled.begin(),disabled.end(),id)==disabled.end()){int lx=stats.at<int>(best_label,cv::CC_STAT_LEFT),ly=stats.at<int>(best_label,cv::CC_STAT_TOP),w=stats.at<int>(best_label,cv::CC_STAT_WIDTH),h=stats.at<int>(best_label,cv::CC_STAT_HEIGHT);probes.push_back({id,chosen,true,best_label,stats.at<int>(best_label,cv::CC_STAT_AREA),cv::Rect(lx,ly,w,h)});border_component[(size_t)best_label]=1;}}
    }
    for(size_t i=0;i<manual.size();i++){int label=labels.at<int>(manual[i]);if(label>0){int lx=stats.at<int>(label,cv::CC_STAT_LEFT),ly=stats.at<int>(label,cv::CC_STAT_TOP),w=stats.at<int>(label,cv::CC_STAT_WIDTH),h=stats.at<int>(label,cv::CC_STAT_HEIGHT);probes.push_back({manual_ids[i],manual[i],false,label,stats.at<int>(label,cv::CC_STAT_AREA),cv::Rect(lx,ly,w,h)});border_component[(size_t)label]=1;}}
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++)if(border_component[(size_t)labels.at<int>(y,x)]&&!mask.at<uchar>(y,x)){mask.at<uchar>(y,x)=255;bg++;}
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++)if(image.at<cv::Vec4b>(y,x)[3]==0&&!mask.at<uchar>(y,x)){mask.at<uchar>(y,x)=255;bg++;}
    cv::Mat clean=image.clone(),debug=image.clone();
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++)if(mask.at<uchar>(y,x)){
        clean.at<cv::Vec4b>(y,x)=cv::Vec4b(0,0,0,0);auto &d=debug.at<cv::Vec4b>(y,x);d=cv::Vec4b((d[0]+255)/2,d[1]/2,(d[2]+255)/2,255);
    }
    for(const auto &probe:probes){cv::circle(debug,probe.point,6,cv::Scalar(0,0,0,255),2,cv::LINE_8);cv::circle(debug,probe.point,3,probe.automatic?cv::Scalar(0,255,255,255):cv::Scalar(255,128,0,255),-1,cv::LINE_8);}
    if(!pp::write_png_atomic(a.get("output"),clean)||!pp::write_png_atomic(a.get("debug"),debug)||!pp::write_png_atomic(a.get("mask"),mask))return pp::input_error("could not publish background artifacts");
    std::string temp;auto fs=pp::json_writer(a.get("meta"),temp);if(!fs.isOpened())return pp::input_error("cannot write stage metadata");
    bool weak=bg==0||bg>(int)(image.total()*.97);
    fs<<"schema_version"<<1<<"stage_id"<<"background"<<"state"<<(weak?"completed_with_warnings":"completed");
    fs<<"model"<<"{"<<"valid"<<model.valid<<"color_space"<<"CIELAB_8U"<<"mass_center"<<"["<<(int)mass[0]<<(int)mass[1]<<(int)mass[2]<<"]"<<"border_center"<<"["<<(int)model.center[0]<<(int)model.center[1]<<(int)model.center[2]<<"]"<<"border_samples"<<model.border_samples<<"dominant_mass_pixels"<<mass_pixels<<"dominant_mass_fraction"<<(double)mass_pixels/image.total()<<"base_tolerance"<<base<<"effective_tolerance"<<model.tolerance<<"connectivity"<<4<<"}";
    fs<<"grid"<<"{"<<"rows"<<rows<<"cols"<<cols<<"x_edges"<<"[";for(int edge:x_edges)fs<<edge;fs<<"]"<<"y_edges"<<"[";for(int edge:y_edges)fs<<edge;fs<<"]"<<"}";
    fs<<"probes"<<"[";for(const auto &probe:probes)fs<<"{"<<"id"<<probe.id<<"x"<<probe.point.x<<"y"<<probe.point.y<<"row"<<(int)(std::upper_bound(y_edges.begin(),y_edges.end(),probe.point.y)-y_edges.begin())-1<<"col"<<(int)(std::upper_bound(x_edges.begin(),x_edges.end(),probe.point.x)-x_edges.begin())-1<<"origin"<<(probe.automatic?"automatic":"manual")<<"region_id"<<probe.region<<"region_area"<<probe.area<<"region_bounds"<<"["<<probe.bounds.x<<probe.bounds.y<<probe.bounds.x+probe.bounds.width<<probe.bounds.y+probe.bounds.height<<"]"<<"}";fs<<"]";
    int automatic_count=0;for(const auto &probe:probes)automatic_count+=probe.automatic;
    fs<<"measurements"<<"{"<<"background_pixels"<<bg<<"background_fraction"<<(double)bg/image.total()<<"candidate_components"<<components-1<<"automatic_probe_count"<<automatic_count<<"manual_probe_count"<<(int)probes.size()-automatic_count<<"}";
    fs<<"warnings"<<"[";if(bg==0)fs<<"no_background_detected";else if(bg>(int)(image.total()*.97))fs<<"background_model_covers_nearly_entire_image";if(automatic_count<rows*cols)fs<<"some_grid_slots_have_no_background_probe";fs<<"]";
    fs<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"background_mask"<<a.get("mask")<<"}";
    if(!pp::publish_json(fs,temp,a.get("meta")))return pp::input_error("could not publish stage metadata");
    std::cout<<"completed background mask pixels="<<bg<<" tolerance_lab="<<model.tolerance<<'\n';return 0;
}

/* agent: gpt-6.1-sol | 2026-10-09 | Record probed background region evidence | v 1.4.0 | 1f9442 */
