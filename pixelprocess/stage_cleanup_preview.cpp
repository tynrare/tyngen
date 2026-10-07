/*
 * File version: 1.5.0
 * Last updated: 2026-10-07
 * Purpose: Show removed and preserved pixels using cleanup cell evidence.
 */
/* agent: gpt-6.1-sol | 2026-10-07 | Show removed and preserved cleanup mask | v 1.5.0 | 65b7c2 */
#include "common.hpp"
#include <filesystem>
#include <cmath>

struct Scratch { std::vector<double> background, winner; cv::Mat reason; std::vector<cv::Point> seeds; };

/* Test opaque BGRA pixel p with cleanup's magenta cutoff; return bool. */
static bool magenta_like(const cv::Vec4b &p) {
    int b=p[0],g=p[1],r=p[2];
    return p[3]>127 && r-g>=35 && b-g>=20 && r+b-2*g>=90 && g<=190;
}

/* Mark cleanup's outside-mode removals using recorded support within image bounds; return void. */
static void mark_outside(const cv::Mat &image,const cv::FileNode &cells,Scratch &scratch) {
    scratch.background.assign(image.total(),0);scratch.winner.assign(image.total(),1);
    scratch.reason.create(image.size(),CV_8U);scratch.reason.setTo(0);scratch.seeds.clear();
    for(auto cell:cells){
        auto xy=cell["logical_cell"];if(xy.size()!=2)continue;
        int x=(int)xy[0],y=(int)xy[1];if(x<0||y<0||x>=image.cols||y>=image.rows)continue;
        size_t i=(size_t)y*image.cols+x;double bg=(double)cell["background_support"];
        double fg=cell["foreground_occupancy"].empty()?(double)cell["coverage"]:(double)cell["foreground_occupancy"];scratch.background[i]=bg;
        if(!cell["winning_color_support"].empty())scratch.winner[i]=(double)cell["winning_color_support"];
        if(image.at<cv::Vec4b>(y,x)[3]&&bg>=.5&&fg<.5)scratch.reason.at<uchar>(y,x)=1;
    }
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++){
        if(scratch.reason.at<uchar>(y,x)||!magenta_like(image.at<cv::Vec4b>(y,x)))continue;
        size_t i=(size_t)y*image.cols+x;double bg=scratch.background[i],win=scratch.winner[i];
        if(!std::isfinite(bg)||!std::isfinite(win)||bg<.2||win>=.5||win>bg)continue;
        scratch.seeds.emplace_back(x,y);scratch.reason.at<uchar>(y,x)=2;
    }
    static const cv::Point offsets[]={{-1,0},{1,0},{0,-1},{0,1}};
    for(auto seed:scratch.seeds)for(auto delta:offsets){
        auto q=seed+delta;if(q.x<0||q.y<0||q.x>=image.cols||q.y>=image.rows)continue;
        if(scratch.reason.at<uchar>(q)||!magenta_like(image.at<cv::Vec4b>(q)))continue;
        size_t i=(size_t)q.y*image.cols+q.x;double bg=scratch.background[i],win=scratch.winner[i];
        if(!std::isfinite(bg)||!std::isfinite(win)||bg<.1||win>=.5)continue;
        scratch.reason.at<uchar>(q)=3;
    }
}

/* Process reconstruction sprites with mode/tolerance and emit exact pre-cleanup masks; return int status. */
int main(int argc,char **argv) {
    auto a=pp::parse_args(argc,argv);
    for(const char *key:{"state","output","debug","meta","sprite-dir"})if(!a.has(key))return pp::input_error(std::string("missing --")+key);
    auto state=pp::read_json(a.get("state"));if(!state.isOpened())return pp::input_error("reconstruction metadata unavailable");
    std::string mode=a.get("mode","outside");if(mode!="outside"&&mode!="all"&&mode!="skip")return pp::input_error("cleanup mode must be outside, all, or skip");
    int tolerance=(int)std::clamp(a.number("tolerance",55),0.0,255.0);
    namespace fs=std::filesystem;fs::create_directories(a.get("sprite-dir"));
    int rows=1,cols=1,maxw=1,maxh=1;auto grid=state["grid"];
    if(grid.size()==2){rows=std::max(1,(int)grid[0]);cols=std::max(1,(int)grid[1]);}
    /* agent: gpt-6.1-sol | 2026-10-07 | Show removed and preserved cleanup mask | v 1.5.0 | 65b7c2 */
    struct Item{std::string id,path;int row,col,removed=0,preserved=0;cv::Mat image,debug;};
    std::vector<Item>items;auto nodes=state["sprites"];int total_removed=0,total_preserved=0;
    for(auto it=nodes.begin();it!=nodes.end();++it){
        Item item;item.id=(std::string)(*it)["id"];item.path=(std::string)(*it)["path"];
        item.row=(int)(*it)["row"];item.col=(int)(*it)["col"];item.image=pp::read_rgba(item.path);
        if(item.image.empty())return pp::input_error("reconstructed sprite missing: "+item.id);
        item.debug=item.image.clone();Scratch scratch;
        if(mode=="outside"){auto cells=(*it)["cells"];if(cells.empty())cells=(*it)["uncertain_cells"];mark_outside(item.image,cells,scratch);}
        else scratch.reason=cv::Mat::zeros(item.image.size(),CV_8U);
        for(int y=0;y<item.image.rows;y++)for(int x=0;x<item.image.cols;x++){
            const auto &p=item.image.at<cv::Vec4b>(y,x);uchar reason=scratch.reason.at<uchar>(y,x);
            if(mode=="all"&&p[3]){int db=(int)p[0]-255,dg=(int)p[1],dr=(int)p[2]-255;if(db*db+dg*dg+dr*dr<=tolerance*tolerance)reason=1;}
            if(p[3]){
                auto &out=item.debug.at<cv::Vec4b>(y,x);
                const cv::Vec4b tint=reason?cv::Vec4b(255,0,255,255):cv::Vec4b(180,170,0,255);
                for(int c=0;c<3;c++)out[c]=(uchar)(((int)p[c]+(int)tint[c])/2);
                out[3]=p[3];
                if(reason)item.removed++;else item.preserved++;
            }
        }
        total_removed+=item.removed;total_preserved+=item.preserved;
        /* agent: gpt-6.1-sol | 2026-10-07 | Show removed and preserved cleanup mask | v 1.5.0 | 65b7c2 */
        maxw=std::max(maxw,item.image.cols);maxh=std::max(maxh,item.image.rows);items.push_back(std::move(item));
    }
    cv::Mat sheet(rows*maxh,cols*maxw,CV_8UC4,cv::Scalar(0,0,0,0)),debug=sheet.clone();
    std::string tmp;auto meta=pp::json_writer(a.get("meta"),tmp);
    meta<<"schema_version"<<1<<"stage_id"<<"cleanup_preview"<<"state"<<"completed"<<"mode"<<mode<<"tolerance"<<tolerance
        <<"coordinate_space"<<"logical_pixels"<<"bounds"<<"each reconstructed sprite image; no pixels outside sprite bounds are considered"
        <<"debug_legend"<<"{"<<"magenta"<<"nontransparent pixel selected for removal"<<"teal"<<"nontransparent pixel preserved by cleanup"<<"}"<<"sprites"<<"[";
    for(auto &item:items){
        if(item.row<0||item.col<0||item.row>=rows||item.col>=cols)continue;
        int x=item.col*maxw+(maxw-item.image.cols)/2,y=item.row*maxh+(maxh-item.image.rows)/2;
        item.image.copyTo(sheet(cv::Rect(x,y,item.image.cols,item.image.rows)));
        item.debug.copyTo(debug(cv::Rect(x,y,item.debug.cols,item.debug.rows)));
        auto path=(fs::path(a.get("sprite-dir"))/(item.id+".png")).string();
        if(!pp::write_png_atomic(path,item.debug))return pp::input_error("could not write preview sprite: "+item.id);
        meta<<"{"<<"id"<<item.id<<"row"<<item.row<<"col"<<item.col<<"path"<<path<<"dimensions"<<"["<<item.image.cols<<item.image.rows<<"]"<<"removed_pixels"<<item.removed<<"preserved_pixels"<<item.preserved<<"}";
    }
    /* agent: gpt-6.1-sol | 2026-10-07 | Show removed and preserved cleanup mask | v 1.5.0 | 65b7c2 */
    meta<<"]"<<"grid"<<"["<<rows<<cols<<"]"<<"tile_dimensions"<<"["<<maxw<<maxh<<"]"<<"measurements"<<"{"<<"removed_pixels"<<total_removed<<"preserved_pixels"<<total_preserved<<"}"<<"warnings"<<"[]"<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"sprites_dir"<<a.get("sprite-dir")<<"}";
    if(!pp::write_png_atomic(a.get("output"),sheet)||!pp::write_png_atomic(a.get("debug"),debug)||!pp::publish_json(meta,tmp,a.get("meta")))return pp::input_error("could not publish cleanup preview");
    return 0;
}
/* agent: gpt-6.1-sol | 2026-10-07 | Show removed and preserved cleanup mask | v 1.5.0 | 65b7c2 */
