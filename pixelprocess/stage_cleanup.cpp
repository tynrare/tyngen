/*
 * File version: 1.1.0
 * Last updated: 2026-10-05
 * Purpose: Apply background-supported, all-magenta, or skipped cleanup policy.
 * Flow PIXEL-01 step 6: reconstruction evidence -> cleaned isolated sprites without new flood passes.
 */
/* agent: gpt-6.1-sol | 2026-10-05 | Apply evidence-based sprite cleanup | v 1.1.0 | b1b8c2 */
#include "common.hpp"
#include <filesystem>

/* Apply one cleanup policy to reconstructed sprites and return process status. */
int main(int argc,char **argv){
    auto a=pp::parse_args(argc,argv);for(const char*k:{"state","output","debug","meta","sprite-dir"})if(!a.has(k))return pp::input_error(std::string("missing --")+k);
    auto state=pp::read_json(a.get("state"));if(!state.isOpened())return pp::input_error("reconstruction metadata unavailable");
    std::string mode=a.get("mode","outside");if(mode!="outside"&&mode!="all"&&mode!="skip")return pp::input_error("cleanup mode must be outside, all, or skip");
    int tolerance=(int)std::clamp(a.number("tolerance",55),0.0,255.0);std::filesystem::create_directories(a.get("sprite-dir"));
    auto sprite_nodes=state["sprites"];std::vector<cv::Mat> images;std::vector<std::string> ids;int maxw=1,maxh=1,removed=0,cleared_cells=0;
    std::string tmp;auto fs=pp::json_writer(a.get("meta"),tmp);fs<<"schema_version"<<1<<"stage_id"<<"cleanup"<<"state"<<"completed"<<"mode"<<mode<<"sprites"<<"[";
    for(auto it=sprite_nodes.begin();it!=sprite_nodes.end();++it){std::string id=(std::string)(*it)["id"],source=(std::string)(*it)["path"];cv::Mat img=pp::read_rgba(source);if(img.empty())continue;
        int row=(int)(*it)["row"],col=(int)(*it)["col"];int local_removed=0,local_cleared=0;
        cv::FileNode cells=(*it)["uncertain_cells"];
        if(mode=="outside")for(auto ci=cells.begin();ci!=cells.end();++ci){double bg=(double)(*ci)["background_support"],fg=(double)(*ci)["coverage"];std::vector<int> xy;(*ci)["logical_cell"]>>xy;if(xy.size()!=2)continue;
            if(bg>=.5&&fg<.5&&xy[0]>=0&&xy[1]>=0&&xy[0]<img.cols&&xy[1]<img.rows){auto &p=img.at<cv::Vec4b>(xy[1],xy[0]);if(p[3]){p=cv::Vec4b(0,0,0,0);local_removed++;local_cleared++;}}
        }
        if(mode=="all")for(int y=0;y<img.rows;y++)for(int x=0;x<img.cols;x++){auto &p=img.at<cv::Vec4b>(y,x);if(!p[3])continue;int db=(int)p[0]-255,dg=(int)p[1],dr=(int)p[2]-255;if(db*db+dg*dg+dr*dr<=tolerance*tolerance){p=cv::Vec4b(0,0,0,0);local_removed++;}}
        for(int y=0;y<img.rows;y++)for(int x=0;x<img.cols;x++)if(img.at<cv::Vec4b>(y,x)[3]==0)img.at<cv::Vec4b>(y,x)=cv::Vec4b(0,0,0,0);
        std::string path=(std::filesystem::path(a.get("sprite-dir"))/(id+".png")).string();if(!pp::write_png_atomic(path,img))return pp::input_error("could not write cleaned sprite "+id);
        maxw=std::max(maxw,img.cols);maxh=std::max(maxh,img.rows);images.push_back(img);ids.push_back(id);removed+=local_removed;cleared_cells+=local_cleared;
        fs<<"{"<<"id"<<id<<"row"<<row<<"col"<<col<<"path"<<path<<"removed_pixels"<<local_removed<<"cleared_background_supported_cells"<<local_cleared<<"dimensions"<<"["<<img.cols<<img.rows<<"]"<<"}";
    }
    int rows=(int)state["grid"][0],cols=(int)state["grid"][1];rows=std::max(1,rows);cols=std::max(1,cols);cv::Mat sheet(rows*maxh,cols*maxw,CV_8UC4,cv::Scalar(0,0,0,0));
    for(size_t i=0;i<images.size();i++){auto n=sprite_nodes[(int)i];int row=(int)n["row"],col=(int)n["col"];int x=col*maxw+(maxw-images[i].cols)/2,y=row*maxh+(maxh-images[i].rows)/2;images[i].copyTo(sheet(cv::Rect(x,y,images[i].cols,images[i].rows)));}
    fs<<"]"<<"measurements"<<"{"<<"removed_pixels"<<removed<<"cleared_background_supported_cells"<<cleared_cells<<"}"<<"warnings"<<"[]"<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"sprites_dir"<<a.get("sprite-dir")<<"}";
    if(!pp::write_png_atomic(a.get("output"),sheet)||!pp::write_png_atomic(a.get("debug"),sheet)||!pp::publish_json(fs,tmp,a.get("meta")))return pp::input_error("could not publish cleanup result");
    std::cout<<"completed cleanup mode="<<mode<<" removed="<<removed<<'\n';return 0;
}

/* agent: gpt-6.1-sol | 2026-10-05 | Apply evidence-based sprite cleanup | v 1.1.0 | b1b8c2 */
