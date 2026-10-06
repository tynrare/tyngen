/*
 * File version: 1.0.0
 * Last updated: 2026-10-06
 * Purpose: Preview reconstructed pixels and magenta candidates before cleanup.
 */
/* agent: gpt-6.1-sol | 2026-10-06 | Preview magenta before cleanup | v 1.0.0 | 6d32ed */
#include "common.hpp"
#include <filesystem>

/* Highlight opaque magenta-like pixels in BGRA image; return void. */
static void highlight_magenta(cv::Mat &image) {
    for (int y=0;y<image.rows;y++) for (int x=0;x<image.cols;x++) {
        auto &p=image.at<cv::Vec4b>(y,x);
        if (p[3]>127 && (int)p[2]-p[1]>=35 && (int)p[0]-p[1]>=20 && (int)p[2]+p[0]-2*(int)p[1]>=90 && p[1]<=190)
            p=cv::Vec4b(0,255,255,255);
    }
}

/* Process reconstruction metadata paths and emit pre-cleanup images; return int status. */
int main(int argc,char **argv) {
    auto a=pp::parse_args(argc,argv);
    for (const char *key:{"state","output","debug","meta","sprite-dir"}) if (!a.has(key)) return pp::input_error(std::string("missing --")+key);
    auto state=pp::read_json(a.get("state")); if (!state.isOpened()) return pp::input_error("reconstruction metadata unavailable");
    namespace fs=std::filesystem; fs::create_directories(a.get("sprite-dir"));
    int rows=1,cols=1,maxw=1,maxh=1; auto grid=state["grid"];
    if(grid.size()==2){rows=std::max(1,(int)grid[0]);cols=std::max(1,(int)grid[1]);}
    struct Item { std::string id,path; int row,col; cv::Mat image; };
    std::vector<Item> items; auto nodes=state["sprites"];
    for(auto it=nodes.begin();it!=nodes.end();++it){Item item;item.id=(std::string)(*it)["id"];item.path=(std::string)(*it)["path"];item.row=(int)(*it)["row"];item.col=(int)(*it)["col"];item.image=pp::read_rgba(item.path);if(item.image.empty())return pp::input_error("reconstructed sprite missing: "+item.id);maxw=std::max(maxw,item.image.cols);maxh=std::max(maxh,item.image.rows);items.push_back(std::move(item));}
    cv::Mat sheet(rows*maxh,cols*maxw,CV_8UC4,cv::Scalar(0,0,0,0)),debug=sheet.clone();
    std::string tmp;auto meta=pp::json_writer(a.get("meta"),tmp);meta<<"schema_version"<<1<<"stage_id"<<"cleanup_preview"<<"state"<<"completed"<<"highlight"<<"opaque magenta-like candidates"<<"sprites"<<"[";
    for(auto &item:items){int x=item.col*maxw+(maxw-item.image.cols)/2,y=item.row*maxh+(maxh-item.image.rows)/2;item.image.copyTo(sheet(cv::Rect(x,y,item.image.cols,item.image.rows)));cv::Mat marked=item.image.clone();highlight_magenta(marked);marked.copyTo(debug(cv::Rect(x,y,marked.cols,marked.rows)));auto path=(fs::path(a.get("sprite-dir"))/(item.id+".png")).string();if(!pp::write_png_atomic(path,marked))return pp::input_error("could not write preview sprite: "+item.id);meta<<"{"<<"id"<<item.id<<"row"<<item.row<<"col"<<item.col<<"path"<<path<<"dimensions"<<"["<<item.image.cols<<item.image.rows<<"]"<<"}";}
    meta<<"]"<<"grid"<<"["<<rows<<cols<<"]"<<"warnings"<<"[]"<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"sprites_dir"<<a.get("sprite-dir")<<"}";
    if(!pp::write_png_atomic(a.get("output"),sheet)||!pp::write_png_atomic(a.get("debug"),debug)||!pp::publish_json(meta,tmp,a.get("meta")))return pp::input_error("could not publish cleanup preview");
    return 0;
}
/* agent: gpt-6.1-sol | 2026-10-06 | Preview magenta before cleanup | v 1.0.0 | 6d32ed */
