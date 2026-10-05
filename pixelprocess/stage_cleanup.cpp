/*
 * File version: 1.3.0
 * Last updated: 2026-10-05
 * Purpose: Apply background-supported cleanup and report removed sprite pixels.
 * Flow PIXEL-01 step 6 owns FRINGE-01 (outside mode only):
 * 1. Reconstruction -> original source footprints/support -> immutable per-cell evidence.
 * 2. Cleanup -> majority removal, then magenta with background >= .20, winner < .50
 *    and winner <= background -> frozen seed mask.
 * 3. Cleanup -> one four-neighbor step with background >= .10 and winner < .50
 *    through magenta only -> bounded fringe removals.
 * 4. Diagnostics -> red background, orange seed, yellow fringe, cyan retained -> review.
 * Invariants: no transparent seeds, no recursive expansion, no cross-sprite traversal;
 * missing winner evidence preserves fringe candidates; all/skip bypass FRINGE-01.
 * CLI tuning: --fringe-seed-support, --fringe-winner-support, --fringe-neighbor-support.
 */
/* agent: gpt-6 | 2026-10-05 | Clean source supported magenta fringes | v 1.3.0 | 5abfb5 */
#include "common.hpp"
#include <filesystem>
#include <cmath>

struct FringeScratch { std::vector<double> support, winner; cv::Mat reasons; std::vector<cv::Point> seeds; };
struct FringeCounts { int background=0, seeds=0, fringe=0, retained=0; };

/* Test opaque BGRA cv::Vec4b p for tolerant magenta; return bool. */
static bool magenta_like(const cv::Vec4b &p) {
    int b=p[0],g=p[1],r=p[2];
    return p[3]>127 && r-g>=35 && b-g>=20 && r+b-2*g>=90 && g<=190;
}

/* Clean cv::Mat image using FileNode cells and double seed/winner/neighbor limits;
 * reuse FringeScratch scratch, record reasons, and return FringeCounts. */
static FringeCounts clean_outside(cv::Mat &image, const cv::FileNode &cells, double seed_min,
                                  double winner_max, double neighbor_min, FringeScratch &scratch) {
    scratch.support.assign(image.total(),0);
    scratch.winner.assign(image.total(),1);
    scratch.reasons.create(image.size(),CV_8U);scratch.reasons.setTo(0);scratch.seeds.clear();
    FringeCounts counts;
    for(auto cell:cells){
        auto xy=cell["logical_cell"];if(xy.size()!=2)continue;
        int x=(int)xy[0],y=(int)xy[1];if(x<0||y<0||x>=image.cols||y>=image.rows)continue;
        double bg=(double)cell["background_support"],fg=(double)cell["coverage"];
        scratch.support[(size_t)y*image.cols+x]=bg;
        if(!cell["winning_color_support"].empty())scratch.winner[(size_t)y*image.cols+x]=(double)cell["winning_color_support"];
        auto &p=image.at<cv::Vec4b>(y,x);
        if(p[3] && bg>=.5 && fg<.5){p=cv::Vec4b(0,0,0,0);scratch.reasons.at<uchar>(y,x)=1;counts.background++;}
    }
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++){
        if(!magenta_like(image.at<cv::Vec4b>(y,x)))continue;
        double bg=scratch.support[(size_t)y*image.cols+x],win=scratch.winner[(size_t)y*image.cols+x];
        if(!std::isfinite(bg)||!std::isfinite(win)||bg<seed_min||win>=winner_max||win>bg)continue;
        scratch.seeds.emplace_back(x,y);scratch.reasons.at<uchar>(y,x)=2;
    }
    static const cv::Point offsets[]={{-1,0},{1,0},{0,-1},{0,1}};
    for(auto seed:scratch.seeds)for(auto delta:offsets){
        auto q=seed+delta;if(q.x<0||q.y<0||q.x>=image.cols||q.y>=image.rows)continue;
        if(scratch.reasons.at<uchar>(q)||!magenta_like(image.at<cv::Vec4b>(q)))continue;
        double bg=scratch.support[(size_t)q.y*image.cols+q.x],win=scratch.winner[(size_t)q.y*image.cols+q.x];
        if(!std::isfinite(bg)||!std::isfinite(win)||bg<neighbor_min||win>=winner_max)continue;
        scratch.reasons.at<uchar>(q)=3;
    }
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++){
        uchar reason=scratch.reasons.at<uchar>(y,x);
        if(reason==2||reason==3){image.at<cv::Vec4b>(y,x)=cv::Vec4b(0,0,0,0);if(reason==2)counts.seeds++;else counts.fringe++;}
        else if(magenta_like(image.at<cv::Vec4b>(y,x)))counts.retained++;
    }
    return counts;
}

/* agent: gpt-6 | 2026-10-05 | Clean source supported magenta fringes | v 1.3.0 | 5abfb5 */
/* Process CLI argc and argv, clean sprites, and return int status. */
int main(int argc,char **argv){
    auto a=pp::parse_args(argc,argv);for(const char*k:{"state","output","debug","meta","sprite-dir"})if(!a.has(k))return pp::input_error(std::string("missing --")+k);
    auto state=pp::read_json(a.get("state"));if(!state.isOpened())return pp::input_error("reconstruction metadata unavailable");
    std::string mode=a.get("mode","outside");if(mode!="outside"&&mode!="all"&&mode!="skip")return pp::input_error("cleanup mode must be outside, all, or skip");
    int tolerance=(int)std::clamp(a.number("tolerance",55),0.0,255.0);std::filesystem::create_directories(a.get("sprite-dir"));
    auto sprite_nodes=state["sprites"];std::vector<cv::Mat> images,originals,reasons;FringeScratch scratch;FringeCounts totals;std::vector<std::string> ids;int maxw=1,maxh=1,removed=0,cleared_cells=0;
    double seed_min=std::clamp(a.number("fringe-seed-support",.2),.01,1.0);
    double winner_max=std::clamp(a.number("fringe-winner-support",.5),0.0,1.0);
    double neighbor_min=std::clamp(a.number("fringe-neighbor-support",.1),.01,1.0);
    std::string tmp;auto fs=pp::json_writer(a.get("meta"),tmp);fs<<"schema_version"<<1<<"stage_id"<<"cleanup"<<"state"<<"completed"<<"mode"<<mode<<"fringe_policy"<<"{"<<"seed_support_min"<<seed_min<<"winner_support_max_exclusive"<<winner_max<<"neighbor_support_min"<<neighbor_min<<"radius"<<1<<"}"<<"debug_legend"<<"{"<<"background"<<"red"<<"seed"<<"orange"<<"fringe"<<"yellow"<<"retained_magenta"<<"cyan"<<"}"<<"sprites"<<"[";
    for(auto it=sprite_nodes.begin();it!=sprite_nodes.end();++it){std::string id=(std::string)(*it)["id"],source=(std::string)(*it)["path"];cv::Mat img=pp::read_rgba(source);if(img.empty())continue;
        int row=(int)(*it)["row"],col=(int)(*it)["col"];cv::Mat original=img.clone();int local_removed=0,local_cleared=0;
        cv::FileNode cells=(*it)["uncertain_cells"];
        FringeCounts counts;
        if(mode=="outside")counts=clean_outside(img,cells,seed_min,winner_max,neighbor_min,scratch);
        else {scratch.reasons.create(img.size(),CV_8U);scratch.reasons.setTo(0);}
        local_cleared=counts.background;local_removed=counts.background+counts.seeds+counts.fringe;
        totals.seeds+=counts.seeds;totals.fringe+=counts.fringe;totals.retained+=counts.retained;
        reasons.push_back(scratch.reasons.clone());
        if(mode=="all")for(int y=0;y<img.rows;y++)for(int x=0;x<img.cols;x++){auto &p=img.at<cv::Vec4b>(y,x);if(!p[3])continue;int db=(int)p[0]-255,dg=(int)p[1],dr=(int)p[2]-255;if(db*db+dg*dg+dr*dr<=tolerance*tolerance){p=cv::Vec4b(0,0,0,0);local_removed++;}}
        for(int y=0;y<img.rows;y++)for(int x=0;x<img.cols;x++)if(img.at<cv::Vec4b>(y,x)[3]==0)img.at<cv::Vec4b>(y,x)=cv::Vec4b(0,0,0,0);
        std::string path=(std::filesystem::path(a.get("sprite-dir"))/(id+".png")).string();if(!pp::write_png_atomic(path,img))return pp::input_error("could not write cleaned sprite "+id);
        maxw=std::max(maxw,img.cols);maxh=std::max(maxh,img.rows);images.push_back(img);originals.push_back(original);ids.push_back(id);removed+=local_removed;cleared_cells+=local_cleared;
        fs<<"{"<<"id"<<id<<"row"<<row<<"col"<<col<<"path"<<path<<"removed_pixels"<<local_removed<<"cleared_background_supported_cells"<<local_cleared<<"fringe_seed_removed"<<counts.seeds<<"fringe_neighbor_removed"<<counts.fringe<<"retained_magenta"<<counts.retained<<"dimensions"<<"["<<img.cols<<img.rows<<"]"<<"}";
    }
    int rows=(int)state["grid"][0],cols=(int)state["grid"][1];rows=std::max(1,rows);cols=std::max(1,cols);cv::Mat sheet(rows*maxh,cols*maxw,CV_8UC4,cv::Scalar(0,0,0,0)),debug_sheet=sheet.clone();
    for(size_t i=0;i<images.size();i++){auto n=sprite_nodes[(int)i];int row=(int)n["row"],col=(int)n["col"];int x=col*maxw+(maxw-images[i].cols)/2,y=row*maxh+(maxh-images[i].rows)/2;images[i].copyTo(sheet(cv::Rect(x,y,images[i].cols,images[i].rows)));cv::Mat view=debug_sheet(cv::Rect(x,y,images[i].cols,images[i].rows));
        for(int py=0;py<images[i].rows;py++)for(int px=0;px<images[i].cols;px++){const auto before=originals[i].at<cv::Vec4b>(py,px);const auto after=images[i].at<cv::Vec4b>(py,px);uchar reason=reasons[i].at<uchar>(py,px);static const cv::Vec4b colors[]={{0,0,0,0},{0,0,255,255},{0,128,255,255},{0,255,255,255}};
            if(reason)view.at<cv::Vec4b>(py,px)=colors[reason];else if(before[3]&&!after[3])view.at<cv::Vec4b>(py,px)=colors[1];else if(mode=="outside"&&magenta_like(after))view.at<cv::Vec4b>(py,px)=cv::Vec4b(255,255,0,255);else view.at<cv::Vec4b>(py,px)=after;}
    }
    fs<<"]"<<"measurements"<<"{"<<"removed_pixels"<<removed<<"cleared_background_supported_cells"<<cleared_cells<<"fringe_seed_removed"<<totals.seeds<<"fringe_neighbor_removed"<<totals.fringe<<"retained_magenta"<<totals.retained<<"}"<<"warnings"<<"[]"<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"sprites_dir"<<a.get("sprite-dir")<<"}";
    if(!pp::write_png_atomic(a.get("output"),sheet)||!pp::write_png_atomic(a.get("debug"),debug_sheet)||!pp::publish_json(fs,tmp,a.get("meta")))return pp::input_error("could not publish cleanup result");
    std::cout<<"completed cleanup mode="<<mode<<" removed="<<removed<<'\n';return 0;
}

/* agent: gpt-6 | 2026-10-05 | Clean source supported magenta fringes | v 1.3.0 | 5abfb5 */
