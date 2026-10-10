/*
 * File version: 1.5.0
 * Last updated: 2026-10-10
 * Purpose: Reconstruct successful sprites from fitted lattices.
 * Flow PIXEL-01 step 6: copy pitch-1 cells and area-sample other fits; record occupancy and color evidence.
 * agent: gpt-6.1-sol | 2026-10-07 | Skip sprites with failed lattice fits | v 1.4.0 | 2f19f6
 */
/* agent: GPT-6 | 2026-10-10 | Copy identity lattice pixels | v 1.5.0 | 0058e8 */
/* agent: gpt-6 | 2026-10-07 | Area sample reconstructed pixel colors | v 1.3.0 | 26b825 */
#include "common.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <tuple>

struct SpriteImage { std::string id, path; int row=0,col=0; cv::Mat image; double confidence=0; };
struct CellEvidence { cv::Rect bounds; double left=0,top=0,right=0,bottom=0,coverage=0,background=0,color_support=0,color_margin=0,color_spread=0,occupancy_confidence=0; };
struct ColorBucket { double weight=0,interior=0; cv::Vec3d sum{0,0,0};cv::Vec4b representative{0,0,0,0}; };
struct ColorChoice { cv::Vec4b color{0,0,0,0}; std::tuple<int,int,int> bucket{0,0,0}; double weight=0,runner=0,spread=0; };

/* Build clipped fractional grid edges from int bounds, double pitch and phase; return sorted edges. */
static std::vector<double> grid_edges(int lo,int hi,double pitch,double phase){
    std::vector<double> edges{(double)lo};if(pitch<.25){edges.push_back((double)hi);return edges;}
    long first=(long)std::floor((lo-phase)/pitch);
    for(long k=first+1;;++k){double edge=phase+k*pitch;if(edge>=hi)break;if(edge>edges.back()+1e-8)edges.push_back(edge);}
    if(edges.back()<(double)hi)edges.push_back((double)hi);return edges;
}

/* Select nearby RGB buckets from const std::map<tuple,ColorBucket>& colors; return ColorChoice. */
static ColorChoice choose_color(const std::map<std::tuple<int,int,int>,ColorBucket>&colors){
    ColorChoice result;if(colors.empty())return result;
    double best_interior=-1;
    for(auto seed=colors.begin();seed!=colors.end();++seed){double weight=0,interior=0;cv::Vec3d mean(0,0,0);
        const int br=std::get<0>(seed->first),bg=std::get<1>(seed->first),bb=std::get<2>(seed->first);
        for(int dr=-2;dr<=2;dr++)for(int dg=-2;dg<=2;dg++)for(int db=-2;db<=2;db++){
            if(dr*dr+dg*dg+db*db>6)continue;auto found=colors.find({br+dr,bg+dg,bb+db});if(found==colors.end())continue;
            double w=found->second.weight;weight+=w;interior+=found->second.interior;mean+=found->second.sum;
        }
        bool wins=weight>result.weight+1e-8||(std::abs(weight-result.weight)<=1e-8&&interior>best_interior);
        if(!wins)continue;
        result.weight=weight;best_interior=interior;result.bucket=seed->first;result.color=seed->second.representative;
        if(weight>0){mean*=1.0/weight;double nearest=1e9;
            for(int dr=-2;dr<=2;dr++)for(int dg=-2;dg<=2;dg++)for(int db=-2;db<=2;db++){
                if(dr*dr+dg*dg+db*db>6)continue;auto found=colors.find({br+dr,bg+dg,bb+db});if(found==colors.end())continue;auto c=found->second.representative;
                cv::Vec3d delta(c[0]-mean[0],c[1]-mean[1],c[2]-mean[2]);double d=delta.dot(delta);if(d<nearest){nearest=d;result.color=c;}
            }
            result.spread=std::sqrt(nearest);
        }
    }
    for(auto seed=colors.begin();seed!=colors.end();++seed){
        int br=std::get<0>(seed->first),bg=std::get<1>(seed->first),bb=std::get<2>(seed->first);double weight=0;
        for(int dr=-2;dr<=2;dr++)for(int dg=-2;dg<=2;dg++)for(int db=-2;db<=2;db++){
            if(dr*dr+dg*dg+db*db>6)continue;auto found=colors.find({br+dr,bg+dg,bb+db});if(found!=colors.end())weight+=found->second.weight;
        }
        int dr=br-std::get<0>(result.bucket),dg=bg-std::get<1>(result.bucket),db=bb-std::get<2>(result.bucket);
        if(dr*dr+dg*dg+db*db>6)result.runner=std::max(result.runner,weight);
    }
    return result;
}

/* Sample cv::Mat crop/mask, int origin, double pitch/phases/threshold, bool modes, and vector<CellEvidence>&; return BGRA cv::Mat. */
static cv::Mat reconstruct_one(const cv::Mat&crop,const cv::Mat&background,int x0,int y0,double pitch,double phase_x,double phase_y,double threshold,bool keep_background,bool preserve_background,std::vector<CellEvidence>&evidence){
    auto xs=grid_edges(x0,x0+crop.cols,pitch,phase_x),ys=grid_edges(y0,y0+crop.rows,pitch,phase_y);
    int cols=(int)xs.size()-1,rows=(int)ys.size()-1;cv::Mat out(rows,cols,CV_8UC4,cv::Scalar(0,0,0,0));evidence.clear();evidence.reserve((size_t)(rows*cols));
    for(int r=0;r<rows;r++)for(int c=0;c<cols;c++){
        CellEvidence cell;cell.left=xs[(size_t)c];cell.right=xs[(size_t)c+1];cell.top=ys[(size_t)r];cell.bottom=ys[(size_t)r+1];
        cell.bounds=cv::Rect((int)std::floor(cell.left),(int)std::floor(cell.top),(int)std::ceil(cell.right)-(int)std::floor(cell.left),(int)std::ceil(cell.bottom)-(int)std::floor(cell.top));
        double area=(cell.right-cell.left)*(cell.bottom-cell.top),opaque=0,bg=0;std::map<std::tuple<int,int,int>,ColorBucket> colors,bg_colors;
        int xa=std::max(x0,(int)std::floor(cell.left)),xb=std::min(x0+crop.cols,(int)std::ceil(cell.right));int ya=std::max(y0,(int)std::floor(cell.top)),yb=std::min(y0+crop.rows,(int)std::ceil(cell.bottom));
        for(int sy=ya;sy<yb;sy++)for(int sx=xa;sx<xb;sx++){
            const auto p=crop.at<cv::Vec4b>(sy-y0,sx-x0);if(p[3]==0)continue;
            double overlap_x=std::max(0.0,std::min(cell.right,sx+1.0)-std::max(cell.left,(double)sx));double overlap_y=std::max(0.0,std::min(cell.bottom,sy+1.0)-std::max(cell.top,(double)sy));double weight=overlap_x*overlap_y*p[3]/255.0;if(weight<=0)continue;
            bool is_background=background.at<uchar>(sy,sx)!=0;if(is_background)bg+=weight;
            if(is_background&&!preserve_background)continue;
            auto key=std::make_tuple((int)p[0]>>3,(int)p[1]>>3,(int)p[2]>>3);auto &bucket=is_background&&!keep_background?bg_colors[key]:colors[key];
            double next_weight=bucket.weight+weight;cv::Vec3d sample(p[0],p[1],p[2]);bucket.sum+=sample*weight;
            cv::Vec3d mean=bucket.sum*(1.0/next_weight),old(bucket.representative[0],bucket.representative[1],bucket.representative[2]);
            if(bucket.weight==0||(sample-mean).dot(sample-mean)<(old-mean).dot(old-mean))bucket.representative=p;bucket.weight=next_weight;
            bool interior=sx+.5>=cell.left+(cell.right-cell.left)*.2&&sx+.5<=cell.right-(cell.right-cell.left)*.2&&sy+.5>=cell.top+(cell.bottom-cell.top)*.2&&sy+.5<=cell.bottom-(cell.bottom-cell.top)*.2;
            if(interior)bucket.interior+=weight;
            if(!is_background||keep_background)opaque+=weight;
        }
        cell.coverage=area>0?opaque/area:0;cell.background=area>0?bg/area:0;cell.occupancy_confidence=std::clamp(std::abs(cell.coverage-threshold)/std::max(.05,threshold),0.0,1.0);
        ColorChoice chosen=choose_color(colors);
        if(cell.coverage>=threshold&&chosen.weight>0){out.at<cv::Vec4b>(r,c)=chosen.color;cell.color_support=area>0?chosen.weight/area:0;cell.color_margin=chosen.weight>0?std::clamp((chosen.weight-chosen.runner)/chosen.weight,0.0,1.0):0;cell.color_spread=chosen.spread;}
        else if(preserve_background&&!keep_background&&cell.background>=.5){ColorChoice bg_choice=choose_color(bg_colors);if(bg_choice.weight>0){out.at<cv::Vec4b>(r,c)=bg_choice.color;cell.color_support=area>0?bg_choice.weight/area:0;cell.color_margin=std::clamp((bg_choice.weight-bg_choice.runner)/bg_choice.weight,0.0,1.0);cell.color_spread=bg_choice.spread;}}
        evidence.push_back(cell);
    }
    return out;
}

/* Process CLI arguments, reconstruct sprites, and return int status. */
int main(int argc,char**argv){
    auto a=pp::parse_args(argc,argv);for(const char*k:{"input","mask","state","lattice","output","debug","meta","sprite-dir"})if(!a.has(k))return pp::input_error(std::string("missing --")+k);
    cv::Mat source=pp::read_rgba(a.get("input")),background=cv::imread(a.get("mask"),cv::IMREAD_GRAYSCALE);auto extracted=pp::read_json(a.get("state")),fits=pp::read_json(a.get("lattice"));
    if(source.empty()||background.empty()||source.size()!=background.size()||!extracted.isOpened()||!fits.isOpened())return pp::input_error("source, mask, extraction, or lattice metadata unavailable");
    double threshold=std::clamp(a.number("coverage",.35),0.0,1.0);std::string cleanup_mode=a.get("cleanup-mode","outside");bool keep_background=cleanup_mode=="skip",preserve_background=cleanup_mode!="all";auto fit_nodes=fits["sprites"];std::map<std::string,cv::FileNode>fit_by_id;
    for(auto it=fit_nodes.begin();it!=fit_nodes.end();++it)fit_by_id[(std::string)(*it)["id"]]=*it;
    std::filesystem::create_directories(a.get("sprite-dir"));std::vector<SpriteImage>outputs;cv::Mat debug=source.clone();auto nodes=extracted["sprites"];
    std::string tmp;auto fs=pp::json_writer(a.get("meta"),tmp);fs<<"schema_version"<<1<<"stage_id"<<"reconstruct"<<"coordinate_space"<<"logical_pixels"<<"sprites"<<"[";
    int maxw=1,maxh=1;bool warning=false;std::vector<CellEvidence>evidence;
    for(auto it=nodes.begin();it!=nodes.end();++it){std::string id=(std::string)(*it)["id"],crop_path=(std::string)(*it)["path"];cv::Mat crop=pp::read_rgba(crop_path);if(crop.empty())continue;
        std::vector<int>cb;(*it)["crop_bounds"]>>cb;if(cb.size()!=4)continue;int x0=cb[0],y0=cb[1];auto fn=fit_by_id.find(id);if(fn!=fit_by_id.end()&&!fn->second["status"].empty()&&(std::string)fn->second["status"]=="failed")continue;double pitch=4,phase_x=x0,phase_y=y0,lattice_confidence=0,phase_conf_x=0,phase_conf_y=0;
        if(fn!=fit_by_id.end()){pitch=(double)fn->second["pitch"];phase_x=(double)fn->second["phase_x"];phase_y=(double)fn->second["phase_y"];lattice_confidence=fn->second["pitch_confidence"].empty()?(double)fn->second["confidence"]:(double)fn->second["pitch_confidence"];std::vector<double>pc;fn->second["phase_confidence"]>>pc;if(pc.size()==2){phase_conf_x=pc[0];phase_conf_y=pc[1];}}
        /* agent: GPT-6 | 2026-10-10 | Copy identity lattice pixels | v 1.5.0 | 0058e8 */
        cv::Mat logical=reconstruct_one(crop,background,x0,y0,pitch,phase_x,phase_y,threshold,keep_background,preserve_background,evidence);
        if(std::abs(pitch-1.0)<1e-8){logical=crop.clone();if(!preserve_background)for(int py=0;py<logical.rows;py++)for(int px=0;px<logical.cols;px++)if(background.at<uchar>(y0+py,x0+px))logical.at<cv::Vec4b>(py,px)=cv::Vec4b(0,0,0,0);}
        std::string outpath=(std::filesystem::path(a.get("sprite-dir"))/(id+".png")).string();if(!pp::write_png_atomic(outpath,logical))return pp::input_error("could not publish reconstructed sprite "+id);
        int row=(int)(*it)["row"],col=(int)(*it)["col"];maxw=std::max(maxw,logical.cols);maxh=std::max(maxh,logical.rows);outputs.push_back({id,outpath,row,col,logical,lattice_confidence});
        fs<<"{"<<"id"<<id<<"row"<<row<<"col"<<col<<"path"<<outpath<<"dimensions"<<"["<<logical.cols<<logical.rows<<"]"<<"pitch"<<pitch<<"phase_source_pixels"<<"["<<phase_x<<phase_y<<"]"<<"lattice_confidence"<<lattice_confidence<<"phase_confidence"<<"["<<phase_conf_x<<phase_conf_y<<"]"<<"occupancy_threshold"<<threshold<<"cells"<<"[";
        for(size_t i=0;i<evidence.size();i++){
            const auto&e=evidence[i];double color_confidence=e.color_margin*(1.0-std::clamp(e.color_spread/64.0,0.0,1.0));
            cv::Vec4b selected=logical.at<cv::Vec4b>((int)(i/(size_t)logical.cols),(int)(i%(size_t)logical.cols));
            cv::Rect bounds= cv::Rect(std::max(0,e.bounds.x),std::max(0,e.bounds.y),std::max(0,std::min(source.cols,e.bounds.x+e.bounds.width)-std::max(0,e.bounds.x)),std::max(0,std::min(source.rows,e.bounds.y+e.bounds.height)-std::max(0,e.bounds.y)));
            if(bounds.area()>0){cv::Mat roi=debug(bounds);cv::Mat tint(roi.size(),CV_8UC4,cv::Scalar(selected[0],selected[1],selected[2],255));cv::addWeighted(roi,.68,tint,.32,0,roi);cv::Scalar border=color_confidence<.35?cv::Scalar(255,0,255,255):e.occupancy_confidence<.35?cv::Scalar(0,180,255,255):cv::Scalar(0,255,0,255);cv::rectangle(debug,bounds,border,1);}
            fs<<"{"<<"logical_cell"<<"["<<(int)(i%(size_t)logical.cols)<<(int)(i/(size_t)logical.cols)<<"]"<<"source_bounds"<<"["<<e.left<<e.top<<e.right<<e.bottom<<"]"<<"foreground_occupancy"<<e.coverage<<"background_support"<<e.background<<"selected_color"<<"["<<(int)selected[2]<<(int)selected[1]<<(int)selected[0]<<(int)selected[3]<<"]"<<"winning_color_support"<<e.color_support<<"color_margin"<<e.color_margin<<"color_spread"<<e.color_spread<<"color_confidence"<<color_confidence<<"occupancy_confidence"<<e.occupancy_confidence<<"lattice_confidence"<<lattice_confidence<<"}";
            if(e.coverage>.05&&e.coverage<threshold)warning=true;
        }
        fs<<"]"<<"source_to_logical"<<"{"<<"pitch"<<pitch<<"phase"<<"["<<phase_x<<phase_y<<"]"<<"}"<<"}";
    }
    int cols=1,rows=1;auto owner=pp::read_json(a.get("owner"));if(owner.isOpened()){cols=(int)owner["grid"]["cols"];rows=(int)owner["grid"]["rows"];}cols=std::max(1,cols);rows=std::max(1,rows);
    cv::Mat sheet(rows*maxh,cols*maxw,CV_8UC4,cv::Scalar(0,0,0,0));for(auto&s:outputs){int x=s.col*maxw+(maxw-s.image.cols)/2,y=s.row*maxh+(maxh-s.image.rows)/2;s.image.copyTo(sheet(cv::Rect(x,y,s.image.cols,s.image.rows)));}
    fs<<"]"<<"grid"<<"["<<rows<<cols<<"]"<<"tile_dimensions"<<"["<<maxw<<maxh<<"]"<<"debug_encoding"<<"fill=reconstructed_color; green=confident; orange=occupancy_uncertain; magenta=color_uncertain"<<"state"<<(warning?"completed_with_warnings":"completed")<<"warnings"<<"[";if(warning)fs<<"cells_below_foreground_coverage_threshold";fs<<"]"<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"sprites_dir"<<a.get("sprite-dir")<<"}";
    if(!pp::write_png_atomic(a.get("output"),sheet)||!pp::write_png_atomic(a.get("debug"),debug)||!pp::publish_json(fs,tmp,a.get("meta")))return pp::input_error("could not publish reconstruction stage");
    std::cout<<"completed reconstruction sprites="<<outputs.size()<<" logical="<<sheet.cols<<'x'<<sheet.rows<<'\n';return 0;
}

/* agent: gpt-6.1-sol | 2026-10-07 | Skip sprites with failed lattice fits | v 1.4.0 | 2f19f6 */
/* agent: gpt-6 | 2026-10-07 | Area sample reconstructed pixel colors | v 1.3.0 | 26b825 */
/* agent: GPT-6 | 2026-10-10 | Copy identity lattice pixels | v 1.5.0 | 0058e8 */
