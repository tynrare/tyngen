/*
 * File version: 1.2.0
 * Last updated: 2026-10-09
 * Purpose: Detect drawn sheet dividers for ownership and extraction.
 * Flow SEP-01: 1. Background -> source and mask -> separator candidates.
 * 2. Separators -> full-span and anchored local divider evidence -> masks and per-cell probes.
 * 3. Ownership and extraction -> shared exclusion mask -> consistent components.
 * Invariant: only supported divider pixels are excluded; sprite intersections stay.
 */
/* agent: gpt-6 | 2026-10-09 | Detect local divider continuations | v 1.2.0 | 904a85 */
#include "common.hpp"
#include <algorithm>

/* Check cv::Mat image and background at int x,y; return bool for dark foreground. */
static bool dark_foreground(const cv::Mat&image,const cv::Mat&background,int x,int y){
    auto p=image.at<cv::Vec4b>(y,x);
    return p[3]&&background.at<uchar>(y,x)==0&&(29*p[0]+150*p[1]+77*p[2])<28000;
}

/* Find runs in const vector<int>& counts with int span; return vector<cv::Point>. */
static std::vector<cv::Point> supported_runs(const std::vector<int>&counts,const std::vector<int>&longest,int span){
    std::vector<cv::Point> runs;int start=-1;
    for(int i=0;i<=(int)counts.size();i++){
        bool supported=i<(int)counts.size()&&counts[(size_t)i]>=span*0.75&&longest[(size_t)i]>=span*0.65;
        if(supported&&start<0)start=i;
        if(!supported&&start>=0){if(i-start<=14)runs.emplace_back(start,i);start=-1;}
    }
    return runs;
}

/* Learn color from const cv::Mat& image/mask and const vector<cv::Point>& runs; return cv::Vec3i. */
static cv::Vec3i divider_color(const cv::Mat&image,const cv::Mat&mask,const std::vector<cv::Point>&yruns,const std::vector<cv::Point>&xruns){
    std::vector<int> channels[3];
    auto sample=[&](int x,int y){if(!dark_foreground(image,mask,x,y))return;auto p=image.at<cv::Vec4b>(y,x);for(int c=0;c<3;c++)channels[c].push_back(p[c]);};
    for(auto r:yruns)for(int y=r.x;y<r.y;y++)for(int x=0;x<image.cols;x++)sample(x,y);
    for(auto r:xruns)for(int x=r.x;x<r.y;x++)for(int y=0;y<image.rows;y++)sample(x,y);
    cv::Vec3i color{};for(int c=0;c<3;c++){if(channels[c].empty())return {-1,-1,-1};auto mid=channels[c].begin()+channels[c].size()/2;std::nth_element(channels[c].begin(),mid,channels[c].end());color[c]=*mid;}return color;
}

/* Match const cv::Mat& image/mask, int x/y, const cv::Vec3i& color; return bool. */
static bool divider_pixel(const cv::Mat&image,const cv::Mat&mask,int x,int y,const cv::Vec3i&color){
    if(color[0]<0||!dark_foreground(image,mask,x,y))return false;auto p=image.at<cv::Vec4b>(y,x);int d0=(int)p[0]-color[0],d1=(int)p[1]-color[1],d2=(int)p[2]-color[2];return d0*d0+d1*d1+d2*d2<=2304;
}

/* Mark const cv::Mat& image/mask using const vector<cv::Point>& runs, const cv::Vec3i&, cv::Mat&; return vector<cv::Rect>. */
static std::vector<cv::Rect> local_vertical_dividers(const cv::Mat&image,const cv::Mat&mask,const std::vector<cv::Point>&yruns,const std::vector<cv::Point>&xruns,const cv::Vec3i&color,cv::Mat&proposed){
    std::vector<cv::Rect> found;if(color[0]<0)return found;std::vector<int>edges{0};for(auto r:yruns)edges.push_back((r.x+r.y)/2);edges.push_back(image.rows);
    for(size_t row=0;row+1<edges.size();row++){int y0=edges[row],y1=edges[row+1],span=y1-y0;if(span<2)continue;std::vector<int>counts((size_t)image.cols),longest((size_t)image.cols);
        for(int x=0;x<image.cols;x++){int run=0;for(int y=y0;y<y1;y++){bool line=divider_pixel(image,mask,x,y,color);run=line?run+1:0;counts[(size_t)x]+=line;longest[(size_t)x]=std::max(longest[(size_t)x],run);}}
        for(auto band:supported_runs(counts,longest,span)){if(std::any_of(xruns.begin(),xruns.end(),[&](cv::Point r){return band.x<r.y&&band.y>r.x;}))continue;
            int matched=0,total=0;for(int y=y0;y<y1;y++)for(int x=band.x;x<band.y;x++){if(!dark_foreground(image,mask,x,y))continue;total++;matched+=divider_pixel(image,mask,x,y,color);}
            if(total==0||matched<total*.75)continue;for(int y=y0;y<y1;y++)for(int x=band.x;x<band.y;x++)if(divider_pixel(image,mask,x,y,color))proposed.at<uchar>(y,x)=255;found.emplace_back(band.x,y0,band.y-band.x,y1-y0);
        }
    }return found;
}

/* Process int argc and char** argv, publish masks/metadata, and return int status. */
int main(int argc,char**argv){
    auto a=pp::parse_args(argc,argv);
    for(const char*k:{"input","mask","output","debug","separator-mask","refined-mask","meta"})if(!a.has(k))return pp::input_error(std::string("missing --")+k);
    cv::Mat image=pp::read_rgba(a.get("input")),background=cv::imread(a.get("mask"),cv::IMREAD_GRAYSCALE);
    auto background_state=a.has("background-state")?pp::read_json(a.get("background-state")):cv::FileStorage();
    if(image.empty()||background.empty()||image.size()!=background.size())return pp::input_error("source and background mask must be readable and equal-sized");
    std::vector<int> rows((size_t)image.rows),cols((size_t)image.cols),row_longest((size_t)image.rows),col_longest((size_t)image.cols),col_run((size_t)image.cols);
    for(int y=0;y<image.rows;y++){
        int row_run=0;
        for(int x=0;x<image.cols;x++){
            bool dark=dark_foreground(image,background,x,y);row_run=dark?row_run+1:0;col_run[(size_t)x]=dark?col_run[(size_t)x]+1:0;
            if(!dark)continue;
            rows[(size_t)y]++;cols[(size_t)x]++;row_longest[(size_t)y]=std::max(row_longest[(size_t)y],row_run);col_longest[(size_t)x]=std::max(col_longest[(size_t)x],col_run[(size_t)x]);
        }
    }
    auto yruns=supported_runs(rows,row_longest,image.cols),xruns=supported_runs(cols,col_longest,image.rows);
    auto divider=divider_color(image,background,yruns,xruns);
    cv::Mat separators(image.size(),CV_8UC1,cv::Scalar(0)),debug=image.clone();int removed=0;
    for(auto run:yruns)for(int y=run.x;y<run.y;y++)for(int x=0;x<image.cols;x++){
        if(divider_pixel(image,background,x,y,divider)){separators.at<uchar>(y,x)=255;removed++;}
    }
    for(auto run:xruns)for(int x=run.x;x<run.y;x++)for(int y=0;y<image.rows;y++){
        if(divider_pixel(image,background,x,y,divider)){separators.at<uchar>(y,x)=255;removed++;}
    }
    cv::Mat proposed=separators.clone();auto local_vertical=local_vertical_dividers(image,background,yruns,xruns,divider,proposed);
    cv::Mat transposed_image,transposed_mask,transposed_proposed;cv::transpose(image,transposed_image);cv::transpose(background,transposed_mask);cv::transpose(proposed,transposed_proposed);
    auto local_horizontal_transposed=local_vertical_dividers(transposed_image,transposed_mask,xruns,yruns,divider,transposed_proposed);cv::transpose(transposed_proposed,proposed);
    std::vector<cv::Rect>local_horizontal;for(auto r:local_horizontal_transposed)local_horizontal.emplace_back(r.y,r.x,r.height,r.width);
    int ambiguous=0;cv::Mat rejected(image.size(),CV_8UC1,cv::Scalar(0));
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++)if(proposed.at<uchar>(y,x)){
        int nearby_foreground=0;for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++){
            int nx=x+dx,ny=y+dy;if(nx<0||ny<0||nx>=image.cols||ny>=image.rows||(dx==0&&dy==0))continue;
            if(background.at<uchar>(ny,nx)==0&&!proposed.at<uchar>(ny,nx))nearby_foreground++;
        }
        if(nearby_foreground>=16){rejected.at<uchar>(y,x)=255;ambiguous++;}
    }
    cv::subtract(proposed,rejected,separators);removed=cv::countNonZero(separators);
    cv::Mat refined=background.clone(),bgr,lab,candidates(image.size(),CV_8UC1,cv::Scalar(0));cv::cvtColor(image,bgr,cv::COLOR_BGRA2BGR);cv::cvtColor(bgr,lab,cv::COLOR_BGR2Lab);
    std::vector<int> mass;int tolerance=24;if(background_state.isOpened()){background_state["model"]["mass_center"]>>mass;tolerance=(int)background_state["model"]["base_tolerance"];}
    if(mass.size()==3){for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++){
        auto p=lab.at<cv::Vec3b>(y,x);int d0=(int)p[0]-mass[0],d1=(int)p[1]-mass[1],d2=(int)p[2]-mass[2];
        if(image.at<cv::Vec4b>(y,x)[3]&&d0*d0+d1*d1+d2*d2<=tolerance*tolerance)candidates.at<uchar>(y,x)=255;
    }}
    cv::Mat candidate_labels,stats,centers;int candidate_count=cv::connectedComponentsWithStats(candidates,candidate_labels,stats,centers,4,CV_32S);
    cv::Mat near_separators;cv::dilate(separators,near_separators,cv::getStructuringElement(cv::MORPH_RECT,{5,5}));int filled_regions=0,filled_pixels=0;
    std::vector<uchar> touches((size_t)candidate_count,0),fill((size_t)candidate_count,0);
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++)if(near_separators.at<uchar>(y,x)){int label=candidate_labels.at<int>(y,x);if(label>0)touches[(size_t)label]=1;}
    auto settings=a.has("settings")?pp::read_json(a.get("settings")):cv::FileStorage();std::vector<std::string>disabled;for(auto it=settings["disabled_background_probes"].begin();it!=settings["disabled_background_probes"].end();++it)disabled.push_back((std::string)*it);
    std::vector<int>x_edges{0},y_edges{0};for(auto r:xruns)x_edges.push_back((r.x+r.y)/2);for(auto r:yruns)y_edges.push_back((r.x+r.y)/2);x_edges.push_back(image.cols);y_edges.push_back(image.rows);
    struct Probe{int row,col,x,y,region,area;};std::vector<Probe>probes;size_t slot_count=(x_edges.size()-1)*(y_edges.size()-1);std::vector<int>probe_label(slot_count,0);std::vector<double>probe_distance(slot_count,1e30);
    for(int label=1;label<candidate_count;label++)if(touches[(size_t)label]&&stats.at<int>(label,cv::CC_STAT_AREA)>=image.total()*.003){
        int cx=(int)centers.at<double>(label,0),cy=(int)centers.at<double>(label,1);int col=std::clamp((int)(std::upper_bound(x_edges.begin(),x_edges.end(),cx)-x_edges.begin())-1,0,(int)x_edges.size()-2),row=std::clamp((int)(std::upper_bound(y_edges.begin(),y_edges.end(),cy)-y_edges.begin())-1,0,(int)y_edges.size()-2);
        std::string id="auto-r"+std::to_string(row)+"-c"+std::to_string(col);if(std::find(disabled.begin(),disabled.end(),id)!=disabled.end())continue;
        int x0=x_edges[(size_t)col],x1=x_edges[(size_t)col+1],y0=y_edges[(size_t)row],y1=y_edges[(size_t)row+1];double dx=cx-(x0+x1)*.5,dy=cy-(y0+y1)*.5,d=dx*dx+dy*dy;size_t slot=(size_t)row*(x_edges.size()-1)+(size_t)col;
        fill[(size_t)label]=1;filled_regions++;if(d<probe_distance[slot]){probe_distance[slot]=d;probe_label[slot]=label;}
    }
    for(size_t slot=0;slot<slot_count;slot++)if(probe_label[slot]){
        int row=(int)(slot/(x_edges.size()-1)),col=(int)(slot%(x_edges.size()-1)),label=probe_label[slot],x0=x_edges[(size_t)col],x1=x_edges[(size_t)col+1],y0=y_edges[(size_t)row],y1=y_edges[(size_t)row+1],tx=(x0+x1)/2,ty=(y0+y1)/2,px=-1,py=-1;double best=1e30;
        int bx=stats.at<int>(label,cv::CC_STAT_LEFT),by=stats.at<int>(label,cv::CC_STAT_TOP),bw=stats.at<int>(label,cv::CC_STAT_WIDTH),bh=stats.at<int>(label,cv::CC_STAT_HEIGHT);
        for(int y=std::max(by,y0);y<std::min(by+bh,y1);y++)for(int x=std::max(bx,x0);x<std::min(bx+bw,x1);x++)if(candidate_labels.at<int>(y,x)==label){double dx=x-tx,dy=y-ty,d=dx*dx+dy*dy;if(d<best){best=d;px=x;py=y;}}
        if(px>=0)probes.push_back({row,col,px,py,label,stats.at<int>(label,cv::CC_STAT_AREA)});
    }
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++){int label=candidate_labels.at<int>(y,x);if(label>0&&fill[(size_t)label]){refined.at<uchar>(y,x)=255;filled_pixels++;}}
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++)if(separators.at<uchar>(y,x)){refined.at<uchar>(y,x)=255;debug.at<cv::Vec4b>(y,x)=cv::Vec4b(0,220,255,255);}
    std::string temp;auto fs=pp::json_writer(a.get("meta"),temp);if(!fs.isOpened())return pp::input_error("cannot write separator metadata");
    fs<<"schema_version"<<1<<"stage_id"<<"separators"<<"state"<<(yruns.empty()&&xruns.empty()?"skipped":"completed")<<"coordinate_space"<<"source_pixels"<<"bounds_convention"<<"half_open";
    fs<<"measurements"<<"{"<<"horizontal_bands"<<(int)yruns.size()<<"vertical_bands"<<(int)xruns.size()<<"excluded_pixels"<<removed<<"ambiguous_intersection_pixels"<<ambiguous<<"filled_background_regions"<<filled_regions<<"filled_background_pixels"<<filled_pixels<<"}";
    fs<<"horizontal_bands"<<"[";for(auto r:yruns)fs<<"["<<r.x<<r.y<<"]";fs<<"]"<<"vertical_bands"<<"[";for(auto r:xruns)fs<<"["<<r.x<<r.y<<"]";fs<<"]"<<"local_vertical_dividers"<<"[";for(auto r:local_vertical)fs<<"["<<r.x<<r.y<<r.width<<r.height<<"]";fs<<"]"<<"local_horizontal_dividers"<<"[";for(auto r:local_horizontal)fs<<"["<<r.x<<r.y<<r.width<<r.height<<"]";fs<<"]";
    fs<<"probes"<<"[";for(const auto&p:probes)fs<<"{"<<"id"<<("auto-r"+std::to_string(p.row)+"-c"+std::to_string(p.col))<<"x"<<p.x<<"y"<<p.y<<"row"<<p.row<<"col"<<p.col<<"origin"<<"automatic"<<"region_id"<<p.region<<"region_area"<<p.area<<"}";fs<<"]";
    fs<<"warnings"<<"[";if(yruns.empty()&&xruns.empty())fs<<"no_confident_separator_evidence";if(ambiguous)fs<<"ambiguous_separator_sprite_intersections";fs<<"]"<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"separator_mask"<<a.get("separator-mask")<<"background_mask"<<a.get("refined-mask")<<"}";
    if(!pp::write_png_atomic(a.get("output"),debug)||!pp::write_png_atomic(a.get("debug"),debug)||!pp::write_png_atomic(a.get("separator-mask"),separators)||!pp::write_png_atomic(a.get("refined-mask"),refined)||!pp::publish_json(fs,temp,a.get("meta")))return pp::input_error("could not publish separator artifacts");
    return 0;
}
/* agent: gpt-6 | 2026-10-09 | Detect local divider continuations | v 1.2.0 | 904a85 */
