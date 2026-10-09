/*
 * File version: 1.1.0
 * Last updated: 2026-10-09
 * Purpose: Restore supported pixels from safe magenta deletions.
 * Flow OUTLINE-01:
 * 1. Cleanup -> all-mode deletion provenance and reconstruction evidence -> eligible repair candidates.
 * 2. Outline stage -> foreground support, low background, and a surviving neighbor -> frozen repair list.
 * 3. Outline stage -> strongest adjacent color or valid explicit color -> restore each candidate once.
 * 4. Outline stage -> restored sprites -> exact binary debug area mask and counts.
 * Invariants: no recursive growth, unchanged sprite bounds, and replacement color beyond the configured magenta delta.
 */
/* agent: gpt-6.1-sol | 2026-10-09 | Restore evidence supported outlines | v 1.0.0 | 501b52 */
/* agent: gpt-6.1-sol | 2026-10-09 | Restore supported magenta outline pixels | v 1.0.0 | 7ca9d2 */
#include "common.hpp"
#include "magenta_cleanup.hpp"
#include <algorithm>
#include <filesystem>

struct Repair { int x,y; cv::Vec4b color; };
/* Parse #RRGGBB into BGRA color, or report false. */
static bool parse_color(const std::string &s,cv::Vec4b &out){
    if(s.size()!=7||s[0]!='#')return false;auto hex=[](char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};
    int v[6];for(int i=0;i<6;i++){v[i]=hex(s[i+1]);if(v[i]<0)return false;}out=cv::Vec4b(v[4]*16+v[5],v[2]*16+v[3],v[0]*16+v[1],255);return true;
}
/* Return squared RGB distance of BGRA pixel p from magenta. */
static int magenta_distance2(const cv::Vec4b&p){int b=p[0]-255,g=p[1],r=p[2]-255;return b*b+g*g+r*r;}
/* agent: gpt-6.1-sol | 2026-10-09 | Require safe cleanup provenance | v 1.1.0 | fa87ae */
/* Process original, cleaned, deletion mask, and cell evidence; return status. */
int main(int argc,char **argv){
    auto a=pp::parse_args(argc,argv);for(const char*k:{"state","clean","output","debug","meta","sprite-dir"})if(!a.has(k))return pp::input_error(std::string("missing --")+k);
    auto original_state=pp::read_json(a.get("state")),clean_state=pp::read_json(a.get("clean"));if(!original_state.isOpened()||!clean_state.isOpened())return pp::input_error("outline reconstruction inputs unavailable");
    std::string mode=a.get("mode","weak"),color_setting=a.get("color","auto");if(mode!="off"&&mode!="weak")return pp::input_error("outline mode must be off or weak");
    int support=(int)std::clamp(a.number("foreground-support",35),0.0,100.0),delta=(int)std::clamp(a.number("color-delta",16),0.0,255.0),threshold=(int)std::clamp(a.number("tolerance",55),0.0,441.0);
    cv::Vec4b override_color;if(color_setting!="auto"&&(!parse_color(color_setting,override_color)||magenta_distance2(override_color)<(threshold+delta)*(threshold+delta)||pp::shaded_magenta(override_color,std::min(441,threshold+delta))))return pp::input_error("outline color must be a sufficiently non-magenta #RRGGBB value");
    namespace fs=std::filesystem;fs::create_directories(a.get("sprite-dir"));
    struct Item{std::string id,path,mask_path;int row=0,col=0,restored=0,skipped=0;cv::Mat image,mask;};std::vector<Item>items;int maxw=1,maxh=1,total=0;auto originals=original_state["sprites"],cleaned=clean_state["sprites"];
    for(auto ci=cleaned.begin();ci!=cleaned.end();++ci){std::string id=(std::string)(*ci)["id"];auto oi=originals.begin();for(;oi!=originals.end();++oi)if((std::string)(*oi)["id"]==id)break;if(oi==originals.end())return pp::input_error("original sprite missing: "+id);
        Item item;item.id=id;item.row=(int)(*ci)["row"];item.col=(int)(*ci)["col"];item.image=pp::read_rgba((std::string)(*ci)["path"]);cv::Mat original=pp::read_rgba((std::string)(*oi)["path"]);cv::Mat evidence=cv::Mat::zeros(original.size(),CV_64F),background=cv::Mat::ones(original.size(),CV_64F);cv::Mat mask=cv::imread((std::string)(*ci)["magenta_mask"],cv::IMREAD_GRAYSCALE),reasons=cv::imread((std::string)(*ci)["reason_mask"],cv::IMREAD_GRAYSCALE);
        if(item.image.empty()||original.empty()||item.image.size()!=original.size())return pp::input_error("outline sprite dimensions disagree: "+id);if(mask.empty()||reasons.empty()||reasons.size()!=mask.size())return pp::input_error("cleanup magenta deletion provenance unavailable: "+id);
        cv::Mat color_support=cv::Mat::zeros(original.size(),CV_64F);auto cells=(*oi)["cells"];for(auto cell:cells){auto xy=cell["logical_cell"];if(xy.size()!=2)continue;int x=(int)xy[0],y=(int)xy[1];if(x<0||y<0||x>=original.cols||y>=original.rows)continue;evidence.at<double>(y,x)=cell["foreground_occupancy"].empty()?0:(double)cell["foreground_occupancy"];background.at<double>(y,x)=cell["background_support"].empty()?1:(double)cell["background_support"];color_support.at<double>(y,x)=cell["winning_color_support"].empty()?0:(double)cell["winning_color_support"];}
        item.mask=cv::Mat::zeros(original.size(),CV_8U);std::vector<Repair>repairs;static const cv::Point offsets[]={{0,-1},{-1,0},{1,0},{0,1}};
        if(mode=="weak")for(int y=0;y<original.rows;y++)for(int x=0;x<original.cols;x++)if(mask.at<uchar>(y,x)&&reasons.at<uchar>(y,x)==pp::CleanupAll){
            if(evidence.at<double>(y,x)<support/100.0||background.at<double>(y,x)>=.5){item.skipped++;continue;}bool adjacent=false;cv::Vec4b selected(0,0,0,0);double best=-1;
            for(const auto d:offsets){int nx=x+d.x,ny=y+d.y;if(nx<0||ny<0||nx>=item.image.cols||ny>=item.image.rows)continue;auto p=item.image.at<cv::Vec4b>(ny,nx);if(!p[3]||magenta_distance2(p)<=threshold*threshold||pp::shaded_magenta(p,threshold))continue;adjacent=true;double weight=0;
                weight=color_support.at<double>(ny,nx);
                if(weight>best){best=weight;selected=p;}}
            if(!adjacent){item.skipped++;continue;}if(color_setting!="auto")selected=override_color;else if(magenta_distance2(selected)<(threshold+delta)*(threshold+delta)||pp::shaded_magenta(selected,std::min(441,threshold+delta))){item.skipped++;continue;}repairs.push_back({x,y,selected});}
        for(const auto&r:repairs){item.image.at<cv::Vec4b>(r.y,r.x)=r.color;item.mask.at<uchar>(r.y,r.x)=255;}item.restored=(int)repairs.size();total+=item.restored;
        item.path=(fs::path(a.get("sprite-dir"))/(id+".png")).string();item.mask_path=(fs::path(a.get("sprite-dir"))/(id+".mask.png")).string();if(!pp::write_png_atomic(item.path,item.image)||!pp::write_png_atomic(item.mask_path,item.mask))return pp::input_error("could not publish outline sprite: "+id);
        maxw=std::max(maxw,item.image.cols);maxh=std::max(maxh,item.image.rows);items.push_back(std::move(item));}
    int rows=(int)original_state["grid"][0],cols=(int)original_state["grid"][1];rows=std::max(1,rows);cols=std::max(1,cols);cv::Mat sheet(rows*maxh,cols*maxw,CV_8UC4,cv::Scalar(0,0,0,0)),debug(sheet.size(),CV_8UC4,cv::Scalar(0,0,0,0));
    std::string temp;auto fsj=pp::json_writer(a.get("meta"),temp);fsj<<"schema_version"<<1<<"stage_id"<<"outline_reconstruct"<<"mode"<<mode<<"foreground_support_min"<<support/100.0<<"color_delta"<<delta<<"tolerance"<<threshold<<"cleanup_reason_required"<<"all_mode_color_match"<<"sprites"<<"[";
    for(auto &item:items){int x=item.col*maxw+(maxw-item.image.cols)/2,y=item.row*maxh+(maxh-item.image.rows)/2;item.image.copyTo(sheet(cv::Rect(x,y,item.image.cols,item.image.rows)));cv::Mat colored(item.mask.size(),CV_8UC4,cv::Scalar(0,0,255,255));colored.copyTo(debug(cv::Rect(x,y,item.mask.cols,item.mask.rows)),item.mask);fsj<<"{"<<"id"<<item.id<<"row"<<item.row<<"col"<<item.col<<"path"<<item.path<<"reconstruction_mask"<<item.mask_path<<"dimensions"<<"["<<item.image.cols<<item.image.rows<<"]"<<"restored_pixels"<<item.restored<<"skipped_pixels"<<item.skipped<<"}";}
    fsj<<"]"<<"grid"<<"["<<rows<<cols<<"]"<<"tile_dimensions"<<"["<<maxw<<maxh<<"]"<<"measurements"<<"{"<<"restored_pixels"<<total<<"}"<<"debug_legend"<<"red=restored pixel"<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"sprites_dir"<<a.get("sprite-dir")<<"}";
    if(!pp::write_png_atomic(a.get("output"),sheet)||!pp::write_png_atomic(a.get("debug"),debug)||!pp::publish_json(fsj,temp,a.get("meta")))return pp::input_error("could not publish outline reconstruction");return 0;
}
/* agent: gpt-6.1-sol | 2026-10-09 | Restore supported magenta outline pixels | v 1.0.0 | 7ca9d2 */
/* agent: gpt-6.1-sol | 2026-10-09 | Restore evidence supported outlines | v 1.0.0 | 501b52 */

/* agent: gpt-6.1-sol | 2026-10-09 | Require safe cleanup provenance | v 1.1.0 | fa87ae */
