/*
 * File version: 1.8.0
 * Last updated: 2026-10-09
 * Purpose: Apply background-supported cleanup and report removed sprite pixels.
 * Flow PIXEL-01 step 9 uses MAGENTA-01 steps 1-3 (see magenta_cleanup.hpp).
 * Evidence requires current cells with foreground_occupancy and support fields.
 * Invariants: no transparent seeds, no recursive expansion, no cross-sprite traversal;
 * missing evidence preserves fringe candidates; all/skip bypass outside detection.
 * CLI tuning: --background-support and --fringe-winner-support.
 */
/* agent: gpt-6.1-sol | 2026-10-09 | Use adjustable magenta support | v 1.7.0 | c7e710 */
/* agent: gpt-6 | 2026-10-05 | Clean source supported magenta fringes | v 1.3.0 | 5abfb5 */
#include "common.hpp"
#include "magenta_cleanup.hpp"
#include <filesystem>
#include <cmath>

using FringeScratch=pp::CleanupScratch;
using FringeCounts=pp::CleanupCounts;
static bool magenta_like(const cv::Vec4b &p,int tolerance) { return pp::shaded_magenta(p,tolerance); }

/* Apply outside cleanup to cv::Mat image using cv::FileNode cells and typed thresholds; reuse FringeScratch and return FringeCounts. */
static FringeCounts clean_outside(cv::Mat &image,const cv::FileNode &cells,double seed_min,
                                  double winner_max,double neighbor_min,int tolerance,FringeScratch &scratch) {
    (void)winner_max;(void)neighbor_min;
    auto counts=pp::decide_magenta_cleanup(image,cells,"outside",tolerance,seed_min,scratch);
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++)
        if(scratch.reason.at<uchar>(y,x)>pp::CleanupKeep)
            image.at<cv::Vec4b>(y,x)=cv::Vec4b(0,0,0,0);
    return counts;
}

/* agent: gpt-6.1-sol | 2026-10-09 | Unify cleanup decisions and reasons | v 1.8.0 | c3178e */
/* Process CLI argc and argv, clean sprites, and return int status. */
int main(int argc,char **argv){
    auto a=pp::parse_args(argc,argv);for(const char*k:{"state","output","debug","meta","sprite-dir"})if(!a.has(k))return pp::input_error(std::string("missing --")+k);
    auto state=pp::read_json(a.get("state"));if(!state.isOpened())return pp::input_error("reconstruction metadata unavailable");
    std::string mode=a.get("mode","outside");if(mode!="outside"&&mode!="all"&&mode!="skip")return pp::input_error("cleanup mode must be outside, all, or skip");
    int tolerance=(int)std::clamp(a.number("tolerance",55),0.0,441.0);std::filesystem::create_directories(a.get("sprite-dir"));
    auto sprite_nodes=state["sprites"];std::vector<cv::Mat> images,originals,reasons;FringeScratch scratch;FringeCounts totals;std::vector<std::string> ids;int maxw=1,maxh=1,removed=0,cleared_cells=0;
    double seed_min=std::clamp(a.number("background-support",20),0.0,100.0)/100.0;
    double winner_max=std::clamp(a.number("fringe-winner-support",.5),0.0,1.0);
    int background_support=(int)std::clamp(a.number("background-support",seed_min*100),0.0,100.0);
    seed_min=background_support/100.0;
    std::string tmp;auto fs=pp::json_writer(a.get("meta"),tmp);fs<<"schema_version"<<1<<"stage_id"<<"cleanup"<<"state"<<"completed"<<"mode"<<mode<<"tolerance"<<tolerance<<"detector"<<"shaded_magenta_line_v1"<<"background_support_min"<<seed_min<<"fringe_policy"<<"{"<<"seed_support_min"<<seed_min<<"winner_support_max_exclusive"<<winner_max<<"neighbor_support_min"<<seed_min*.5<<"radius"<<1<<"}"<<"reason_codes"<<"["<<"keep"<<"background"<<"exterior_seed"<<"exterior_fringe"<<"all_mode_magenta"<<"]"<<"debug_legend"<<"{"<<"background"<<"red"<<"seed"<<"orange"<<"fringe"<<"yellow"<<"retained_magenta"<<"cyan"<<"}"<<"sprites"<<"[";
    for(auto it=sprite_nodes.begin();it!=sprite_nodes.end();++it){std::string id=(std::string)(*it)["id"],source=(std::string)(*it)["path"];cv::Mat img=pp::read_rgba(source);if(img.empty())continue;
        int row=(int)(*it)["row"],col=(int)(*it)["col"];cv::Mat original=img.clone();int local_removed=0,local_cleared=0;
        cv::FileNode cells=(*it)["cells"];
        FringeCounts counts;
        if(mode=="outside")counts=clean_outside(img,cells,seed_min,winner_max,seed_min*.5,tolerance,scratch);
        else {scratch.reason.create(img.size(),CV_8U);scratch.reason.setTo(0);}
        local_cleared=counts.background;local_removed=counts.background+counts.seeds+counts.fringe;
        totals.seeds+=counts.seeds;totals.fringe+=counts.fringe;totals.retained+=counts.retained;
        if(mode=="all")for(int y=0;y<img.rows;y++)for(int x=0;x<img.cols;x++){auto &p=img.at<cv::Vec4b>(y,x);int db=(int)p[0]-255,dg=p[1],dr=(int)p[2]-255;if(p[3]>127&&db*db+dg*dg+dr*dr<=tolerance*tolerance){p=cv::Vec4b(0,0,0,0);scratch.reason.at<uchar>(y,x)=pp::CleanupAll;local_removed++;}}
        reasons.push_back(scratch.reason.clone());
        cv::Mat magenta_mask=cv::Mat::zeros(img.size(),CV_8U);for(int y=0;y<img.rows;y++)for(int x=0;x<img.cols;x++){uchar reason=scratch.reason.at<uchar>(y,x);bool deleted_magenta=reason>0&&magenta_like(original.at<cv::Vec4b>(y,x),tolerance);if(deleted_magenta)magenta_mask.at<uchar>(y,x)=255;if(img.at<cv::Vec4b>(y,x)[3]==0)img.at<cv::Vec4b>(y,x)=cv::Vec4b(0,0,0,0);}
        std::string path=(std::filesystem::path(a.get("sprite-dir"))/(id+".png")).string();if(!pp::write_png_atomic(path,img))return pp::input_error("could not write cleaned sprite "+id);
        std::string mask_path=(std::filesystem::path(a.get("sprite-dir"))/(id+".magenta-mask.png")).string(),reason_path=(std::filesystem::path(a.get("sprite-dir"))/(id+".reason.png")).string();if(!pp::write_png_atomic(mask_path,magenta_mask)||!pp::write_png_atomic(reason_path,scratch.reason))return pp::input_error("could not write cleanup masks "+id);
        maxw=std::max(maxw,img.cols);maxh=std::max(maxh,img.rows);images.push_back(img);originals.push_back(original);ids.push_back(id);removed+=local_removed;cleared_cells+=local_cleared;
        fs<<"{"<<"id"<<id<<"row"<<row<<"col"<<col<<"path"<<path<<"magenta_mask"<<mask_path<<"reason_mask"<<reason_path<<"removed_pixels"<<local_removed<<"cleared_background_supported_cells"<<local_cleared<<"fringe_seed_removed"<<counts.seeds<<"fringe_neighbor_removed"<<counts.fringe<<"retained_magenta"<<counts.retained<<"dimensions"<<"["<<img.cols<<img.rows<<"]"<<"}";
    }
    int rows=(int)state["grid"][0],cols=(int)state["grid"][1];rows=std::max(1,rows);cols=std::max(1,cols);cv::Mat sheet(rows*maxh,cols*maxw,CV_8UC4,cv::Scalar(0,0,0,0)),debug_sheet=sheet.clone();
    for(size_t i=0;i<images.size();i++){auto n=sprite_nodes[(int)i];int row=(int)n["row"],col=(int)n["col"];int x=col*maxw+(maxw-images[i].cols)/2,y=row*maxh+(maxh-images[i].rows)/2;images[i].copyTo(sheet(cv::Rect(x,y,images[i].cols,images[i].rows)));cv::Mat view=debug_sheet(cv::Rect(x,y,images[i].cols,images[i].rows));
        for(int py=0;py<images[i].rows;py++)for(int px=0;px<images[i].cols;px++){const auto before=originals[i].at<cv::Vec4b>(py,px);const auto after=images[i].at<cv::Vec4b>(py,px);uchar reason=reasons[i].at<uchar>(py,px);static const cv::Vec4b colors[]={{0,0,0,0},{0,0,255,255},{0,128,255,255},{0,255,255,255},{255,0,255,255}};
            if(reason)view.at<cv::Vec4b>(py,px)=colors[reason];else if(before[3]&&!after[3])view.at<cv::Vec4b>(py,px)=colors[1];else if(mode=="outside"&&magenta_like(after,tolerance))view.at<cv::Vec4b>(py,px)=cv::Vec4b(255,255,0,255);else view.at<cv::Vec4b>(py,px)=after;}
    }
    /* agent: gpt-6.1-sol | 2026-10-07 | Record cleanup sheet tile dimensions | v 1.6.0 | f9c0a8 */
    fs<<"]"<<"tile_dimensions"<<"["<<maxw<<maxh<<"]"<<"measurements"<<"{"<<"removed_pixels"<<removed<<"cleared_background_supported_cells"<<cleared_cells<<"fringe_seed_removed"<<totals.seeds<<"fringe_neighbor_removed"<<totals.fringe<<"retained_magenta"<<totals.retained<<"}"<<"warnings"<<"["<<"]"<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"sprites_dir"<<a.get("sprite-dir")<<"}";
    if(!pp::write_png_atomic(a.get("output"),sheet)||!pp::write_png_atomic(a.get("debug"),debug_sheet)||!pp::publish_json(fs,tmp,a.get("meta")))return pp::input_error("could not publish cleanup result");
    std::cout<<"completed cleanup mode="<<mode<<" removed="<<removed<<'\n';return 0;
}

/* agent: gpt-6 | 2026-10-05 | Clean source supported magenta fringes | v 1.3.0 | 5abfb5 */
/* agent: gpt-6.1-sol | 2026-10-07 | Record cleanup sheet tile dimensions | v 1.6.0 | f9c0a8 */
/* agent: gpt-6.1-sol | 2026-10-09 | Use adjustable magenta support | v 1.7.0 | c7e710 */

/* agent: gpt-6.1-sol | 2026-10-09 | Unify cleanup decisions and reasons | v 1.8.0 | c3178e */
