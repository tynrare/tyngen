/*
 * File version: 1.2.0
 * Last updated: 2026-10-05
 * Purpose: Reconstruct independent logical sprites from fitted source lattices.
 * Flow PIXEL-01 step 5: sample shared rounded boundaries; preserve partial cells and uncertain color.
 */
/* agent: gpt-6 | 2026-10-05 | Record winning foreground color support | v 1.2.0 | c0007c */
#include "common.hpp"
#include <algorithm>
#include <filesystem>
#include <map>
#include <tuple>

struct SpriteImage { std::string id, path; int row=0,col=0; cv::Mat image; double confidence=0; };

/* agent: gpt-6 | 2026-10-05 | Record winning foreground color support | v 1.2.0 | c0007c */
/* Sample cv::Mat crop/mask with int origin, double lattice/threshold, bool modes,
 * and vector<double>/vector<cv::Rect> evidence outputs; return BGRA cv::Mat. */
static cv::Mat reconstruct_one(const cv::Mat &crop, const cv::Mat &background, int x0, int y0,
                               double pitch, double phase_x, double phase_y, double threshold, bool keep_background, bool preserve_background,
                               std::vector<double> &coverage, std::vector<double> &background_support, std::vector<double> &winning_support,
                               std::vector<cv::Rect> &source_cells) {
    auto boundaries=[](int lo,int hi,double pitch_value,double phase){
        std::vector<int> result{lo}; if(pitch_value<.25){result.push_back(hi);return result;}
        long first=(long)std::floor((lo-phase)/pitch_value);
        for(long k=first+1;;++k){int edge=(int)std::lround(phase+k*pitch_value);if(edge>=hi)break;if(edge>result.back())result.push_back(edge);}
        if(result.back()!=hi)result.push_back(hi);return result;
    };
    int x1=x0+crop.cols,y1=y0+crop.rows;auto xs=boundaries(x0,x1,pitch,phase_x),ys=boundaries(y0,y1,pitch,phase_y);
    int cols=(int)xs.size()-1,rows=(int)ys.size()-1;cv::Mat out(rows,cols,CV_8UC4,cv::Scalar(0,0,0,0));
    winning_support.clear();winning_support.reserve((size_t)(rows*cols));coverage.clear();background_support.clear();source_cells.clear();coverage.reserve((size_t)(rows*cols));background_support.reserve((size_t)(rows*cols));source_cells.reserve((size_t)(rows*cols));
    for(int r=0;r<rows;r++)for(int c=0;c<cols;c++){
        cv::Rect src(xs[(size_t)c]-x0,ys[(size_t)r]-y0,xs[(size_t)c+1]-xs[(size_t)c],ys[(size_t)r+1]-ys[(size_t)r]);
        winning_support.push_back(0);
        source_cells.push_back(cv::Rect(xs[(size_t)c],ys[(size_t)r],src.width,src.height));
        if(src.width<=0||src.height<=0){coverage.push_back(0);background_support.push_back(0);continue;}
        std::map<std::tuple<int,int,int>,std::pair<int,cv::Vec4b>> colors,bg_colors;int opaque=0,bg=0;
        for(int y=src.y;y<src.y+src.height;y++)for(int x=src.x;x<src.x+src.width;x++){
            const auto p=crop.at<cv::Vec4b>(y,x);if(p[3]==0)continue;int sx=x0+x,sy=y0+y;
            if(background.at<uchar>(sy,sx)){bg++;if(!preserve_background)continue;if(!keep_background){auto key=std::make_tuple(p[0]>>3,p[1]>>3,p[2]>>3);auto &entry=bg_colors[key];if(entry.first==0)entry.second=p;entry.first++;continue;}}
            opaque++;auto key=std::make_tuple(p[0]>>3,p[1]>>3,p[2]>>3);auto &entry=colors[key];if(entry.first==0)entry.second=p;entry.first++;
        }
        double ratio=(double)opaque/(src.width*src.height);coverage.push_back(ratio);background_support.push_back((double)bg/(src.width*src.height));
        if(ratio<threshold||colors.empty()){if(preserve_background&&!keep_background&&(double)bg/(src.width*src.height)>=.5&&!bg_colors.empty()){auto best_bg=bg_colors.begin();for(auto it=bg_colors.begin();it!=bg_colors.end();++it)if(it->second.first>best_bg->second.first)best_bg=it;out.at<cv::Vec4b>(r,c)=best_bg->second.second;}continue;}
        auto best=colors.begin();for(auto it=colors.begin();it!=colors.end();++it)if(it->second.first>best->second.first)best=it;
        winning_support.back()=(double)best->second.first/(src.width*src.height);
        out.at<cv::Vec4b>(r,c)=best->second.second;
    }
    return out;
}

/* agent: gpt-6 | 2026-10-05 | Record winning foreground color support | v 1.2.0 | c0007c */
/* Process int argc/char** argv into logical sprites/evidence; return int status. */
int main(int argc,char **argv){
    auto a=pp::parse_args(argc,argv);for(const char*k:{"input","mask","state","lattice","output","debug","meta","sprite-dir"})if(!a.has(k))return pp::input_error(std::string("missing --")+k);
    cv::Mat source=pp::read_rgba(a.get("input")),background=cv::imread(a.get("mask"),cv::IMREAD_GRAYSCALE);auto extracted=pp::read_json(a.get("state")),fits=pp::read_json(a.get("lattice"));
    if(source.empty()||background.empty()||!extracted.isOpened()||!fits.isOpened())return pp::input_error("source, mask, extraction, or lattice metadata unavailable");
    double threshold=std::clamp(a.number("coverage",.35),0.0,1.0);std::string cleanup_mode=a.get("cleanup-mode","outside");bool keep_background=cleanup_mode=="skip",preserve_background=cleanup_mode!="all";auto fit_nodes=fits["sprites"];std::map<std::string,cv::FileNode> fit_by_id;
    for(auto it=fit_nodes.begin();it!=fit_nodes.end();++it)fit_by_id[(std::string)(*it)["id"]]=*it;
    std::filesystem::create_directories(a.get("sprite-dir"));std::vector<SpriteImage> outputs;cv::Mat debug=source.clone();auto nodes=extracted["sprites"];
    std::string tmp;auto fs=pp::json_writer(a.get("meta"),tmp);fs<<"schema_version"<<1<<"stage_id"<<"reconstruct"<<"coordinate_space"<<"logical_pixels"<<"sprites"<<"[";
    int maxw=1,maxh=1;bool warning=false;
    std::vector<double> coverage,bg_support,winning_support;std::vector<cv::Rect> cells;
    for(auto it=nodes.begin();it!=nodes.end();++it){std::string id=(std::string)(*it)["id"],crop_path=(std::string)(*it)["path"];cv::Mat crop=pp::read_rgba(crop_path);if(crop.empty())continue;
        std::vector<int> cb;(*it)["crop_bounds"]>>cb;if(cb.size()!=4)continue;int x0=cb[0],y0=cb[1];auto fn=fit_by_id.find(id);double pitch=4,phase_x=x0,phase_y=y0,confidence=0;
        if(fn!=fit_by_id.end()){pitch=(double)fn->second["pitch"];phase_x=(double)fn->second["phase_x"];phase_y=(double)fn->second["phase_y"];confidence=(double)fn->second["confidence"];}
        cv::Mat logical=reconstruct_one(crop,background,x0,y0,pitch,phase_x,phase_y,threshold,keep_background,preserve_background,coverage,bg_support,winning_support,cells);
        std::string outpath=(std::filesystem::path(a.get("sprite-dir"))/(id+".png")).string();if(!pp::write_png_atomic(outpath,logical))return pp::input_error("could not publish reconstructed sprite "+id);
        int row=(int)(*it)["row"],col=(int)(*it)["col"];maxw=std::max(maxw,logical.cols);maxh=std::max(maxh,logical.rows);outputs.push_back({id,outpath,row,col,logical,confidence});
        cv::rectangle(debug,cv::Rect(x0,y0,crop.cols,crop.rows),cv::Scalar(0,255,255,255),1);
        for(size_t i=0;i<cells.size();i++){auto r=cells[i];cv::rectangle(debug,r,coverage[i]<threshold?cv::Scalar(255,0,255,255):cv::Scalar(255,180,0,180),1);}
        fs<<"{"<<"id"<<id<<"row"<<row<<"col"<<col<<"path"<<outpath<<"dimensions"<<"["<<logical.cols<<logical.rows<<"]"<<"pitch"<<pitch<<"pitch_units"<<"source_pixels_per_logical_pixel"<<"phase_source_pixels"<<"["<<phase_x<<phase_y<<"]"<<"confidence"<<confidence<<"occupancy_threshold"<<threshold<<"uncertain_cells"<<"[";
        for(size_t i=0;i<coverage.size();i++){fs<<"{"<<"logical_cell"<<"["<<(int)(i%(size_t)logical.cols)<<(int)(i/(size_t)logical.cols)<<"]"<<"source_bounds"<<"["<<cells[i].x<<cells[i].y<<cells[i].x+cells[i].width<<cells[i].y+cells[i].height<<"]"<<"coverage"<<coverage[i]<<"background_support"<<bg_support[i]<<"winning_color_support"<<winning_support[i]<<"}";if(coverage[i]>.05&&coverage[i]<threshold)warning=true;}
        fs<<"]"<<"source_to_logical"<<"{"<<"pitch"<<pitch<<"phase"<<"["<<phase_x<<phase_y<<"]"<<"}"<<"}";
    }
    int cols=1,rows=1;auto owner=pp::read_json(a.get("owner"));if(owner.isOpened()){cols=(int)owner["grid"]["cols"];rows=(int)owner["grid"]["rows"];}cols=std::max(1,cols);rows=std::max(1,rows);
    cv::Mat sheet(rows*maxh,cols*maxw,CV_8UC4,cv::Scalar(0,0,0,0));for(auto &s:outputs){int x=s.col*maxw+(maxw-s.image.cols)/2,y=s.row*maxh+(maxh-s.image.rows)/2;s.image.copyTo(sheet(cv::Rect(x,y,s.image.cols,s.image.rows)));}
    fs<<"]"<<"grid"<<"["<<rows<<cols<<"]"<<"tile_dimensions"<<"["<<maxw<<maxh<<"]"<<"state"<<(warning?"completed_with_warnings":"completed")<<"warnings"<<"[";if(warning)fs<<"cells_below_foreground_coverage_threshold";fs<<"]"<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"sprites_dir"<<a.get("sprite-dir")<<"}";
    if(!pp::write_png_atomic(a.get("output"),sheet)||!pp::write_png_atomic(a.get("debug"),debug)||!pp::publish_json(fs,tmp,a.get("meta")))return pp::input_error("could not publish reconstruction stage");
    std::cout<<"completed reconstruction sprites="<<outputs.size()<<" logical="<<sheet.cols<<'x'<<sheet.rows<<'\n';return 0;
}

/* agent: gpt-6 | 2026-10-05 | Record winning foreground color support | v 1.2.0 | c0007c */
