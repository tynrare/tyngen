/*
 * File version: 1.9.0
 * Last updated: 2026-10-09
 * Purpose: Overlay cleanup decisions on reconstructed sprites.
 */
/* agent: gpt-6.1-sol | 2026-10-09 | Overlay cleanup decisions on source | v 1.8.0 | 857177 */
/* agent: gpt-6.1-sol | 2026-10-09 | Render separate cleanup masks | v 1.7.0 | eac8b0 */
#include "common.hpp"
#include "magenta_cleanup.hpp"
#include <filesystem>
#include <cmath>

using Scratch=pp::CleanupScratch;

/* agent: gpt-6.1-sol | 2026-10-09 | Share cleanup decisions and reasons | v 1.9.0 | ecb59e */
/* Process reconstruction sprites with mode/tolerance and emit exact pre-cleanup masks; return int status. */
int main(int argc,char **argv) {
    auto a=pp::parse_args(argc,argv);
    for(const char *key:{"state","output","debug","meta","sprite-dir"})if(!a.has(key))return pp::input_error(std::string("missing --")+key);
    auto state=pp::read_json(a.get("state"));if(!state.isOpened())return pp::input_error("reconstruction metadata unavailable");
    std::string mode=a.get("mode","outside");if(mode!="outside"&&mode!="all"&&mode!="skip")return pp::input_error("cleanup mode must be outside, all, or skip");
    int tolerance=(int)std::clamp(a.number("tolerance",55),0.0,441.0);
    double support_min=std::clamp(a.number("background-support",20),0.0,100.0)/100.0;
    namespace fs=std::filesystem;fs::create_directories(a.get("sprite-dir"));
    int rows=1,cols=1,maxw=1,maxh=1;auto grid=state["grid"];
    if(grid.size()==2){rows=std::max(1,(int)grid[0]);cols=std::max(1,(int)grid[1]);}
    /* agent: gpt-6.1-sol | 2026-10-09 | Render separate cleanup masks | v 1.7.0 | eac8b0 */
    struct Item{std::string id,path;int row,col,removed=0,preserved=0;cv::Mat image,debug,clear,keep,reason;};
    std::vector<Item>items;auto nodes=state["sprites"];int total_removed=0,total_preserved=0;
    for(auto it=nodes.begin();it!=nodes.end();++it){
        Item item;item.id=(std::string)(*it)["id"];item.path=(std::string)(*it)["path"];
        item.row=(int)(*it)["row"];item.col=(int)(*it)["col"];item.image=pp::read_rgba(item.path);
        if(item.image.empty())return pp::input_error("reconstructed sprite missing: "+item.id);
        item.debug=item.image.clone();item.clear=cv::Mat::zeros(item.image.size(),CV_8UC4);item.keep=item.clear.clone();Scratch scratch;
        auto cells=(*it)["cells"];
        pp::decide_magenta_cleanup(item.image,cells,mode,tolerance,support_min,scratch);item.reason=scratch.reason.clone();
        for(int y=0;y<item.image.rows;y++)for(int x=0;x<item.image.cols;x++){
            const auto &p=item.image.at<cv::Vec4b>(y,x);uchar reason=scratch.reason.at<uchar>(y,x);
            if(mode=="all"&&p[3]){int db=(int)p[0]-255,dg=(int)p[1],dr=(int)p[2]-255;if(db*db+dg*dg+dr*dr<=tolerance*tolerance)reason=1;}
            if(p[3]){
                item.debug.at<cv::Vec4b>(y,x)=reason?cv::Vec4b(255,0,255,255):cv::Vec4b(180,170,0,255);
                (reason?item.clear:item.keep).at<cv::Vec4b>(y,x)=reason?cv::Vec4b(255,0,255,255):cv::Vec4b(180,170,0,255);
                if(reason)item.removed++;else item.preserved++;
            }
        }
        total_removed+=item.removed;total_preserved+=item.preserved;
        /* agent: gpt-6.1-sol | 2026-10-09 | Render separate cleanup masks | v 1.7.0 | eac8b0 */
        maxw=std::max(maxw,item.image.cols);maxh=std::max(maxh,item.image.rows);items.push_back(std::move(item));
    }
    cv::Mat sheet(rows*maxh,cols*maxw,CV_8UC4,cv::Scalar(0,0,0,0)),debug=sheet.clone();
    std::string tmp;auto meta=pp::json_writer(a.get("meta"),tmp);
    meta<<"schema_version"<<1<<"stage_id"<<"cleanup_preview"<<"state"<<"completed"<<"mode"<<mode<<"tolerance"<<tolerance<<"detector"<<"shaded_magenta_line_v1"<<"background_support_min"<<support_min
        <<"coordinate_space"<<"logical_pixels"<<"bounds"<<"each reconstructed sprite image; no pixels outside sprite bounds are considered"
        <<"debug_legend"<<"{"<<"magenta"<<"opaque pixel selected for removal"<<"teal"<<"opaque pixel preserved by cleanup"<<"}"<<"sprites"<<"[";
    for(auto &item:items){
        if(item.row<0||item.col<0||item.row>=rows||item.col>=cols)continue;
        int x=item.col*maxw+(maxw-item.image.cols)/2,y=item.row*maxh+(maxh-item.image.rows)/2;
        auto dst=sheet(cv::Rect(x,y,item.image.cols,item.image.rows));
        auto dbg=debug(cv::Rect(x,y,item.image.cols,item.image.rows));
        for(int py=0;py<item.image.rows;py++)for(int px=0;px<item.image.cols;px++){
            auto source=item.image.at<cv::Vec4b>(py,px);uchar reason=item.clear.at<cv::Vec4b>(py,px)[3]?1:0;
            if(source[3]){
                auto overlay=reason?cv::Vec4b(255,0,255,255):cv::Vec4b(180,170,0,255);
                for(int c=0;c<3;c++){dst.at<cv::Vec4b>(py,px)[c]=(uchar)((source[c]*3+overlay[c])/4);dbg.at<cv::Vec4b>(py,px)[c]=overlay[c];}
                dst.at<cv::Vec4b>(py,px)[3]=255;dbg.at<cv::Vec4b>(py,px)[3]=255;
            }
        }
        auto path=(fs::path(a.get("sprite-dir"))/(item.id+".clear.png")).string(),keep_path=(fs::path(a.get("sprite-dir"))/(item.id+".keep.png")).string(),reason_path=(fs::path(a.get("sprite-dir"))/(item.id+".reason.png")).string();
        if(!pp::write_png_atomic(path,item.clear)||!pp::write_png_atomic(keep_path,item.keep)||!pp::write_png_atomic(reason_path,item.reason))return pp::input_error("could not write preview masks: "+item.id);
        meta<<"{"<<"id"<<item.id<<"row"<<item.row<<"col"<<item.col<<"clear_mask"<<path<<"keep_mask"<<keep_path<<"reason_mask"<<reason_path<<"dimensions"<<"["<<item.image.cols<<item.image.rows<<"]"<<"removed_pixels"<<item.removed<<"preserved_pixels"<<item.preserved<<"}";
    }
    /* agent: gpt-6.1-sol | 2026-10-09 | Render separate cleanup masks | v 1.7.0 | eac8b0 */
    meta<<"]"<<"grid"<<"["<<rows<<cols<<"]"<<"tile_dimensions"<<"["<<maxw<<maxh<<"]"<<"mask_order"<<"["<<"clear"<<"keep"<<"]"<<"reason_codes"<<"["<<"keep"<<"background"<<"exterior_seed"<<"exterior_fringe"<<"all_mode_magenta"<<"]"<<"overlay_legend"<<"{"<<"magenta"<<"selected for removal"<<"teal"<<"preserved"<<"}"<<"measurements"<<"{"<<"removed_pixels"<<total_removed<<"preserved_pixels"<<total_preserved<<"}"<<"warnings"<<"[]"<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"sprites_dir"<<a.get("sprite-dir")<<"}";
    if(!pp::write_png_atomic(a.get("output"),sheet)||!pp::write_png_atomic(a.get("debug"),debug)||!pp::publish_json(meta,tmp,a.get("meta")))return pp::input_error("could not publish cleanup preview");
    return 0;
}
/* agent: gpt-6.1-sol | 2026-10-09 | Overlay cleanup decisions on source | v 1.8.0 | 857177 */
/* agent: gpt-6.1-sol | 2026-10-09 | Render separate cleanup masks | v 1.7.0 | eac8b0 */

/* agent: gpt-6.1-sol | 2026-10-09 | Share cleanup decisions and reasons | v 1.9.0 | ecb59e */
