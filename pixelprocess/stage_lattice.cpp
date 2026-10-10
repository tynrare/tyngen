/*
 * File version: 2.0.0
 * Last updated: 2026-10-10
 * Purpose: Fit shared or per-sprite fractional square lattices.
 * Flow PIXEL-01 step 4: owned crops -> exact integer fit, then supported fractional candidates, confidence, and debug overlay.
 * Exact fit rule: every observed color/alpha edge on each axis shares one integer phase; require evidence on both axes.
 * Pitch 1 is the identity fallback and is exempt from the configured minimum pitch and logical-cell floor.
 * agent: gpt-6.1-sol | 2026-10-08 | Score lattice edge evidence | v 1.9.0 | 13bed6
 */
/* agent: GPT-6 | 2026-10-10 | Fit exact integer pixel lattices | v 2.0.0 | 0efbb2 */
/* agent: gpt-6.1-sol | 2026-10-08 | Score lattice edge evidence | v 1.9.0 | 13bed6 */
#include "common.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <cmath>

/* agent: gpt-6.1-sol | 2026-10-08 | Score lattice edge evidence | v 1.9.0 | 13bed6 */
struct Fit { double pitch=4, phase_x=0, phase_y=0, confidence=0, pitch_strength=0, phase_confidence_x=0, phase_confidence_y=0, ambiguity=0; bool quality_rejected=false, exact_integer=false, has_edges_x=false, has_edges_y=false; std::vector<double> candidates, phases_x, phases_y, phase_conf_x, phase_conf_y, alignment_x, alignment_y; std::vector<int> support_x,support_y; };

/* Find the largest uniform integer lattice in const cv::Mat& image from source origin and maximum pitch; return Fit. */
/* agent: GPT-6 | 2026-10-10 | Fit exact integer pixel lattices | v 2.0.0 | 0efbb2 */
static Fit exact_integer_fit(const cv::Mat&image,int source_x,int source_y,int maximum){
    Fit fit;fit.pitch=1;fit.confidence=fit.pitch_strength=1;fit.ambiguity=0;
    std::vector<int> edges_x,edges_y;
    for(int y=0;y<image.rows;y++)for(int x=1;x<image.cols;x++)if(image.at<cv::Vec4b>(y,x)!=image.at<cv::Vec4b>(y,x-1))edges_x.push_back(source_x+x);
    for(int y=1;y<image.rows;y++)for(int x=0;x<image.cols;x++)if(image.at<cv::Vec4b>(y,x)!=image.at<cv::Vec4b>(y-1,x))edges_y.push_back(source_y+y);
    auto phase_for=[](const std::vector<int>&edges,int pitch,int&phase){if(edges.size()<2)return false;std::array<int,128>counts{};int lo=*std::min_element(edges.begin(),edges.end()),hi=*std::max_element(edges.begin(),edges.end());for(int edge:edges)counts[(size_t)((edge%pitch+pitch)%pitch)]++;phase=(int)(std::max_element(counts.begin(),counts.begin()+pitch)-counts.begin());return counts[(size_t)phase]==(int)edges.size()&&hi-lo>=pitch;};
    for(int pitch=std::min(maximum,128);pitch>=2;pitch--){int px=0,py=0;if(!phase_for(edges_x,pitch,px)||!phase_for(edges_y,pitch,py))continue;fit.pitch=pitch;fit.phase_x=(px-source_x%pitch+pitch)%pitch;fit.phase_y=(py-source_y%pitch+pitch)%pitch;fit.has_edges_x=fit.has_edges_y=true;fit.phase_confidence_x=fit.phase_confidence_y=1;fit.support_x=edges_x;fit.support_y=edges_y;fit.exact_integer=true;return fit;}
    fit.has_edges_x=!edges_x.empty();fit.has_edges_y=!edges_y.empty();return fit;
}

/* Place a backed label near cv::Rect bounds on cv::Mat canvas; return bool. */
static bool draw_label(cv::Mat &canvas,std::vector<cv::Rect>&occupied,const cv::Rect&bounds,const std::string&text,const cv::Scalar&color){
    int baseline=0;cv::Size size=cv::getTextSize(text,cv::FONT_HERSHEY_SIMPLEX,.35,1,&baseline);int pad=2;
    std::vector<cv::Point> origins={{bounds.x,bounds.y-size.height-6},{bounds.x,bounds.y+bounds.height+size.height+6},{bounds.x-size.width-6,bounds.y+bounds.height/2},{bounds.x+bounds.width+6,bounds.y+bounds.height/2}};
    for(const auto&origin:origins){cv::Rect box(origin.x,origin.y-size.height-pad,size.width+pad*2,size.height+baseline+pad*2);if(box.x<0||box.y<0||box.br().x>canvas.cols||box.br().y>canvas.rows)continue;bool collision=false;for(const auto&prior:occupied)if((prior&box).area()>0){collision=true;break;}if(collision)continue;
        occupied.push_back(box);cv::rectangle(canvas,box,cv::Scalar(0,0,0,220),cv::FILLED);cv::putText(canvas,text,{box.x+pad,box.y+pad+size.height},cv::FONT_HERSHEY_SIMPLEX,.35,color,1,cv::LINE_AA);return true;}
    return false;
}

/* Count clipped logical cells for int length and double pitch/phase; return int. */
static int logical_cell_count(int length,double pitch,double phase){int count=1;double last=0;long first=(long)std::floor(-phase/pitch);for(long k=first+1;;++k){double edge=phase+k*pitch;if(edge>=length)break;if(edge>last+1e-8){count++;last=edge;}}return count;}

/* Fit cv::Mat inputs, int origin, pitch bounds, double shared tolerance, and int cell floor; return Fit. */
static Fit fit_lattice(const cv::Mat &image, const cv::Mat &background, int source_x, int source_y, double forced, double minimum, double maximum, double precision, bool shared_mode, double tolerance, int min_cells) {
    Fit best; cv::Mat gray, edge_x, edge_y;
    cv::cvtColor(image,gray,cv::COLOR_BGRA2GRAY); cv::Sobel(gray,edge_x,CV_32F,1,0,3);cv::Sobel(gray,edge_y,CV_32F,0,1,3);
    std::vector<double> ex((size_t)image.cols,0), ey((size_t)image.rows,0); double total_x=0,total_y=0;
    for(int y=0;y<image.rows;y++)for(int x=1;x<image.cols;x++){
        const auto left=image.at<cv::Vec4b>(y,x-1),right=image.at<cv::Vec4b>(y,x);int sx=source_x+x,sy=source_y+y;
        if(!((left[3]>0&&!background.at<uchar>(sy,sx-1))||(right[3]>0&&!background.at<uchar>(sy,sx))))continue;
        double color=0;for(int c=0;c<3;c++)color+=std::abs((int)left[c]-(int)right[c]);
        double mask=background.at<uchar>(sy,sx-1)!=background.at<uchar>(sy,sx)?64.0:0.0;
        double alpha_edge=std::abs((int)left[3]-(int)right[3])*.25;
        double energy=std::max({color/3.0,mask,alpha_edge})+std::abs(edge_x.at<float>(y,x))*.18;
        ex[(size_t)x]+=energy;total_x+=energy;
    }
    for(int y=1;y<image.rows;y++)for(int x=0;x<image.cols;x++){
        const auto top=image.at<cv::Vec4b>(y-1,x),bottom=image.at<cv::Vec4b>(y,x);int sx=source_x+x,sy=source_y+y;
        if(!((top[3]>0&&!background.at<uchar>(sy-1,sx))||(bottom[3]>0&&!background.at<uchar>(sy,sx))))continue;
        double color=0;for(int c=0;c<3;c++)color+=std::abs((int)top[c]-(int)bottom[c]);
        double mask=background.at<uchar>(sy-1,sx)!=background.at<uchar>(sy,sx)?64.0:0.0;
        double alpha_edge=std::abs((int)top[3]-(int)bottom[3])*.25;
        double energy=std::max({color/3.0,mask,alpha_edge})+std::abs(edge_y.at<float>(y,x))*.18;
        ey[(size_t)y]+=energy;total_y+=energy;
    }
    auto meaningful_edges=[](const std::vector<double>&energy){std::vector<std::pair<double,int>>peaks;double maximum=energy.empty()?0:*std::max_element(energy.begin(),energy.end());if(maximum<=0)return std::vector<std::pair<double,int>>{};
        for(size_t i=1;i+1<energy.size();i++)if(energy[i]>=energy[i-1]&&energy[i]>=energy[i+1]&&energy[i]>=maximum*.18)peaks.emplace_back(energy[i],(int)i);
        std::sort(peaks.begin(),peaks.end(),[](const auto&a,const auto&b){return a.first>b.first;});std::vector<std::pair<double,int>>edges;for(const auto&p:peaks){bool near=false;for(const auto&edge:edges)if(std::abs(edge.second-p.second)<2)near=true;if(!near)edges.push_back(p);}return edges;};
    const auto edges_x=meaningful_edges(ex),edges_y=meaningful_edges(ey);
    best.has_edges_x=!edges_x.empty();best.has_edges_y=!edges_y.empty();
    for(size_t i=0;i<std::min<size_t>(12,edges_x.size());++i)best.support_x.push_back(edges_x[i].second);
    for(size_t i=0;i<std::min<size_t>(12,edges_y.size());++i)best.support_y.push_back(edges_y[i].second);
    double step=std::max(.01,precision), start=forced>0?forced:minimum, end=forced>0?forced:maximum;
    if(forced>0&&shared_mode&&tolerance>0){start=std::max(minimum,forced*(1.0-tolerance));end=std::min(maximum,forced*(1.0+tolerance));}
    if(min_cells>0&&(forced<=0||shared_mode))end=std::min(end,(double)std::max(image.cols,image.rows)/min_cells);
    if(start>end){best.quality_rejected=min_cells>0;return best;}
    double best_score=-1;
    for(double pitch=start;pitch<=end+step*.5;pitch+=step){
        double phase_step=std::max(.1,pitch/32.0), phase_x=0,phase_y=0,confidence_x=0,confidence_y=0;
        auto axis=[&](const std::vector<double>&energy,double total,double &phase,double &phase_confidence){
            if(total<=0){phase=0;phase_confidence=0;return .0;} double local_best=-1;
            auto evaluate=[&](double ph){double sum=0;for(size_t i=0;i<energy.size();i++){double delta=std::abs(std::remainder((double)i-ph,pitch));if(delta<=.75)sum+=energy[i]*(1.0-delta/1.5);}return sum/total;};
            for(double ph=0;ph<pitch;ph+=phase_step){double value=evaluate(ph);if(value>local_best){local_best=value;phase=ph;}}
            double coarse=phase;for(double ph=coarse-phase_step;ph<=coarse+phase_step;ph+=1.0/16.0){double wrapped=std::fmod(ph+pitch,pitch),value=evaluate(wrapped);if(value>local_best){local_best=value;phase=wrapped;}}
            double second=0;for(double ph=0;ph<pitch;ph+=phase_step)if(std::abs(std::remainder(ph-phase,pitch))>phase_step*1.5)second=std::max(second,evaluate(ph));
            phase_confidence=std::clamp((local_best-second)/std::max(.05,local_best),0.0,1.0);return local_best;
        };
        double sx=axis(ex,total_x,phase_x,confidence_x),sy=axis(ey,total_y,phase_y,confidence_y);
        auto coverage=[](const std::vector<std::pair<double,int>>&edges,double phase,double pitch){double total=0,aligned=0;for(const auto&e:edges){total+=e.first;double delta=std::abs(std::remainder((double)e.second-phase,pitch));if(delta<=.75)aligned+=e.first*(1.0-delta/1.5);}return total>0?aligned/total:0.0;};
        double candidate=(sx+sy)*.5 / std::max(.08,1.1/pitch);
        if(candidate>best_score){best_score=candidate;best.pitch=pitch;best.phase_x=phase_x;best.phase_y=phase_y;best.confidence=std::min(1.0,std::max(0.0,(candidate-0.35)/1.8));best.pitch_strength=best.confidence;best.phase_confidence_x=confidence_x;best.phase_confidence_y=confidence_y;}
        if(forced<=0){best.candidates.push_back(candidate);best.phases_x.push_back(phase_x);best.phases_y.push_back(phase_y);best.phase_conf_x.push_back(confidence_x);best.phase_conf_y.push_back(confidence_y);best.alignment_x.push_back(coverage(edges_x,phase_x,pitch));best.alignment_y.push_back(coverage(edges_y,phase_y,pitch));}
    }
    double selected_score=best_score;
    if(forced<=0){
        // Prefer the finest supported local peak; broader harmonic pitches often pool the same edges.
        const double support_floor=best_score*.68;
        for(size_t i=0;i<best.candidates.size();i++){
            const double score=best.candidates[i];
            const bool peak=(i==0||score>=best.candidates[i-1])&&(i+1==best.candidates.size()||score>=best.candidates[i+1]);
            if(peak&&score>=support_floor){
                best.pitch=start+i*step;best.phase_x=best.phases_x[i];best.phase_y=best.phases_y[i];selected_score=score;
                best.confidence=std::min(1.0,std::max(0.0,(score-.35)/1.8));best.pitch_strength=best.confidence;best.phase_confidence_x=best.phase_conf_x[i];best.phase_confidence_y=best.phase_conf_y[i];break;
            }
        }
        double runner_up=0;for(size_t i=0;i<best.candidates.size();i++){double p=start+i*step;bool peak=(i==0||best.candidates[i]>=best.candidates[i-1])&&(i+1==best.candidates.size()||best.candidates[i]>=best.candidates[i+1]);if(peak&&std::abs(p-best.pitch)>std::max(.25,best.pitch*.08))runner_up=std::max(runner_up,best.candidates[i]);}
        best.ambiguity=std::clamp((selected_score-runner_up)/std::max(.05,selected_score),0.0,1.0);
        best.confidence*=.5+.5*best.ambiguity;
    }
    if(forced>0&&!shared_mode)best.ambiguity=1.0;
    if(min_cells>0&&(forced<=0||shared_mode)&&std::max(logical_cell_count(image.cols,best.pitch,best.phase_x),logical_cell_count(image.rows,best.pitch,best.phase_y))<min_cells)best.quality_rejected=true;
    (void)shared_mode;
    return best;
}

/* agent: gpt-6.1-sol | 2026-10-07 | Keep failed sprites inspectable | v 1.8.0 | 4ec6a3 */
/* Process CLI argc and argv, fit sprites, and return int status. */
int main(int argc,char **argv){
    auto a=pp::parse_args(argc,argv);for(const char*k:{"input","mask","state","output","debug","meta","sprite-dir"})if(!a.has(k))return pp::input_error(std::string("missing --")+k);
    cv::Mat source=pp::read_rgba(a.get("input")),background=cv::imread(a.get("mask"),cv::IMREAD_GRAYSCALE);auto extracted=pp::read_json(a.get("state"));if(source.empty()||background.empty()||source.size()!=background.size()||!extracted.isOpened())return pp::input_error("source, mask, or extraction metadata unavailable");
    auto settings=pp::read_json(a.get("settings"));double minp=1,maxp=16,precision=.1;std::string mode=a.get("pitch","auto"),phase_x="auto",phase_y="auto";
    if(settings.isOpened()){if(!settings["min_pitch"].empty())minp=(double)settings["min_pitch"];if(!settings["max_pitch"].empty())maxp=(double)settings["max_pitch"];if(!settings["pitch_precision"].empty())precision=(double)settings["pitch_precision"];if(!settings["pitch"].empty())mode=(std::string)settings["pitch"];if(!settings["phase_x"].empty())phase_x=(std::string)settings["phase_x"];if(!settings["phase_y"].empty())phase_y=(std::string)settings["phase_y"];}
    const bool shared_pitch=a.has("sheet-pitch");if(shared_pitch)mode=a.get("sheet-pitch");
    double forced=0;try{if(mode!="auto")forced=std::stod(mode);}catch(...){return pp::input_error("pitch must be auto or a positive fractional value");}
    if(!std::isfinite(minp)||!std::isfinite(maxp)||!std::isfinite(precision)||minp<1||maxp>128||minp>maxp||precision<.01||precision>4||!std::isfinite(forced)||forced<0||(forced>0&&forced<1))return pp::input_error("pitch search settings are out of range");
    int min_cells=settings["min_sprite_cells"].empty()?16:(int)settings["min_sprite_cells"],tolerance_percent=settings["sprite_pitch_tolerance"].empty()?5:(int)settings["sprite_pitch_tolerance"];
    if(min_cells<0||min_cells>256||tolerance_percent<0||tolerance_percent>25)return pp::input_error("pitch quality settings are out of range");
    double tolerance=tolerance_percent/100.0;cv::Mat debug=source.clone();auto sprites=extracted["sprites"],overrides=settings["sprite_overrides"];int auto_sprite_count=0;
    for(auto it=sprites.begin();it!=sprites.end();++it){bool manual=false;std::string id=(std::string)(*it)["id"];for(auto oi=overrides.begin();oi!=overrides.end();++oi)if((std::string)(*oi)["id"]==id&&!(*oi)["pitch"].empty())manual=true;if(!manual)auto_sprite_count++;}
    std::string tmp;auto fs=pp::json_writer(a.get("meta"),tmp);
    fs<<"schema_version"<<1<<"stage_id"<<"lattice"<<"state"<<"completed"<<"coordinate_space"<<"source_pixels"<<"pitch_selection"<<(shared_pitch?"edge_periodicity_v2":forced>0?"manual":"per_sprite_auto")<<"selected_sheet_pitch"<<forced<<"automatic_sprite_count"<<auto_sprite_count<<"sprites"<<"[";
    bool uncertain=false;std::vector<cv::Rect> debug_labels,source_labels;
    for(auto it=sprites.begin();it!=sprites.end();++it){std::string id=(std::string)(*it)["id"],path=(std::string)(*it)["path"];int row=(int)(*it)["row"],col=(int)(*it)["col"];cv::Mat crop=pp::read_rgba(path);if(crop.empty())continue;
        int x0=0,y0=0;std::vector<int> rect;(*it)["crop_bounds"]>>rect;if(rect.size()==4){x0=rect[0];y0=rect[1];}
        std::string sprite_pitch=mode,sprite_phase_x=phase_x,sprite_phase_y=phase_y;bool sprite_pitch_override=false;
        for(auto oi=overrides.begin();oi!=overrides.end();++oi)if((std::string)(*oi)["id"]==id){if(!(*oi)["pitch"].empty()){sprite_pitch=(std::string)(*oi)["pitch"];sprite_pitch_override=true;}if(!(*oi)["phase_x"].empty())sprite_phase_x=(std::string)(*oi)["phase_x"];if(!(*oi)["phase_y"].empty())sprite_phase_y=(std::string)(*oi)["phase_y"];}
        double sprite_forced=0;try{if(sprite_pitch!="auto")sprite_forced=std::stod(sprite_pitch);}catch(...){return pp::input_error("sprite pitch override is invalid");}
        if(!std::isfinite(sprite_forced)||sprite_forced<0||(sprite_forced>0&&sprite_forced<1))return pp::input_error("sprite pitch override is invalid");
        bool local_shared=shared_pitch&&!sprite_pitch_override&&sprite_phase_x=="auto"&&sprite_phase_y=="auto";
        /* agent: GPT-6 | 2026-10-10 | Fit exact integer pixel lattices | v 2.0.0 | 0efbb2 */
        Fit f;bool auto_phases=sprite_phase_x=="auto"&&sprite_phase_y=="auto";
        if(!sprite_pitch_override&&auto_phases){Fit exact=exact_integer_fit(crop,x0,y0,(int)std::floor(sprite_forced>0?sprite_forced:maxp));if((sprite_forced<=0&&exact.exact_integer)||(sprite_forced>=2&&exact.exact_integer&&std::abs(exact.pitch-sprite_forced)<1e-8))f=std::move(exact);else f=fit_lattice(crop,background,x0,y0,sprite_forced,minp,maxp,precision,local_shared,tolerance,min_cells);}
        else f=fit_lattice(crop,background,x0,y0,sprite_forced,minp,maxp,precision,local_shared,tolerance,min_cells);
        if(!f.exact_integer&&sprite_forced==0&&f.confidence<.25){f.pitch=1;f.phase_x=f.phase_y=0;f.confidence=f.pitch_strength=1;f.quality_rejected=false;}
        if(std::abs(f.pitch-1.0)<1e-8){f.phase_x=f.phase_y=0;f.exact_integer=true;}
        bool failed=f.quality_rejected;
        const bool manual_phase_x=sprite_phase_x!="auto",manual_phase_y=sprite_phase_y!="auto";
        try{if(manual_phase_x)f.phase_x=std::stod(sprite_phase_x)-x0;if(manual_phase_y)f.phase_y=std::stod(sprite_phase_y)-y0;}catch(...){return pp::input_error("sprite phase override is invalid");}
        if(!std::isfinite(f.phase_x)||!std::isfinite(f.phase_y))return pp::input_error("sprite phase override is invalid");
        bool pitch_manual=sprite_pitch_override||(!shared_pitch&&sprite_pitch!="auto");
        if(!pitch_manual&&!f.exact_integer&&f.pitch>1&&min_cells>0&&std::max(logical_cell_count(crop.cols,f.pitch,f.phase_x),logical_cell_count(crop.rows,f.pitch,f.phase_y))<min_cells)failed=true;
        int x1=x0+crop.cols,y1=y0+crop.rows;cv::Scalar confidence_color(0,255*f.confidence,255*(1-f.confidence),210);
        if(!failed){for(double x=f.phase_x+x0;x<x1;x+=f.pitch)if(x>=x0){cv::line(debug,cv::Point((int)std::lround(x),y0),cv::Point((int)std::lround(x),y1-1),cv::Scalar(255,160,0,200),1);cv::line(source,cv::Point((int)std::lround(x),y0),cv::Point((int)std::lround(x),y1-1),confidence_color,1);}
        for(double y=f.phase_y+y0;y<y1;y+=f.pitch)if(y>=y0){cv::line(debug,cv::Point(x0,(int)std::lround(y)),cv::Point(x1-1,(int)std::lround(y)),cv::Scalar(255,160,0,200),1);cv::line(source,cv::Point(x0,(int)std::lround(y)),cv::Point(x1-1,(int)std::lround(y)),confidence_color,1);}}
        for(int x:f.support_x)cv::line(debug,cv::Point(x0+x,y0),cv::Point(x0+x,std::min(y1-1,y0+3)),cv::Scalar(255,255,0,255),1);
        for(int y:f.support_y)cv::line(debug,cv::Point(x0,y0+y),cv::Point(std::min(x1-1,x0+3),y0+y),cv::Scalar(255,255,0,255),1);
        const cv::Rect sprite_bounds(x0,y0,crop.cols,crop.rows);
        draw_label(debug,debug_labels,sprite_bounds,failed?"r"+std::to_string(row)+"c"+std::to_string(col)+" FAILED":"r"+std::to_string(row)+"c"+std::to_string(col)+" p"+cv::format("%.2f",f.pitch)+" c"+cv::format("%.2f",f.confidence),failed?cv::Scalar(0,0,255,255):cv::Scalar(255,255,255,255));
        if(failed){cv::rectangle(debug,sprite_bounds,cv::Scalar(0,0,255,255),2);cv::rectangle(source,sprite_bounds,cv::Scalar(0,0,255,255),2);}else draw_label(source,source_labels,sprite_bounds,"r"+std::to_string(row)+"c"+std::to_string(col)+" c"+cv::format("%.2f",f.confidence),confidence_color);
        /* agent: gpt-6.1-sol | 2026-10-07 | Avoid overlapping lattice labels | v 1.7.0 | 983d66 */
        bool shared_forced=shared_pitch&&!sprite_pitch_override;
        int logical_w=logical_cell_count(crop.cols,f.pitch,f.phase_x),logical_h=logical_cell_count(crop.rows,f.pitch,f.phase_y);
        double max_pitch_for_size=min_cells>0?(double)std::max(crop.cols,crop.rows)/min_cells:maxp;
        uncertain|=!failed&&f.confidence<.25;fs<<"{"<<"id"<<id<<"status"<<(failed?"failed":"completed")<<"failure_reason"<<(failed?"no lattice pitch meets minimum logical sprite size":"")<<"pitch"<<f.pitch<<"exact_integer_fit"<<f.exact_integer<<"shared_pitch"<<(shared_pitch?forced:f.pitch)<<"pitch_delta_percent"<<(shared_pitch?100.0*(f.pitch-forced)/forced:0.0)<<"maximum_pitch_for_minimum_cells"<<max_pitch_for_size<<"logical_dimensions"<<"["<<logical_w<<logical_h<<"]"<<"phase_x"<<(f.pitch<=1?0:f.phase_x+x0)<<"phase_y"<<(f.pitch<=1?0:f.phase_y+y0)<<"confidence"<<f.confidence<<"pitch_confidence"<<f.pitch_strength<<"phase_confidence"<<"["<<f.phase_confidence_x<<f.phase_confidence_y<<"]"<<"phase_manual"<<"["<<manual_phase_x<<manual_phase_y<<"]"<<"pitch_ambiguity"<<(pitch_manual||shared_forced?0.0:1.0-f.ambiguity)<<"phase_refinement_step"<<1.0/16.0<<"pitch_manual"<<pitch_manual<<"shared_pitch_override"<<shared_forced<<"supporting_edges_x"<<"[";for(int x:f.support_x)fs<<x+x0;fs<<"]"<<"supporting_edges_y"<<"[";for(int y:f.support_y)fs<<y+y0;fs<<"]"<<"candidates"<<"[";
        for(size_t i=0;i<f.candidates.size();++i){double score=f.candidates[i],pitch=minp+i*precision;bool peak=(i==0||score>=f.candidates[i-1])&&(i+1==f.candidates.size()||score>=f.candidates[i+1]);double ex=f.alignment_x[i],ey=f.alignment_y[i],axes=(f.has_edges_x?1.0:0.0)+(f.has_edges_y?1.0:0.0),coverage=axes?((f.has_edges_x?ex:0)+(f.has_edges_y?ey:0))/axes:0,expected=axes?std::min(1.0,1.125/pitch):0,periodicity=axes?std::clamp((coverage-expected)/std::max(.001,1.0-expected),0.0,1.0):0;fs<<"{"<<"pitch"<<pitch<<"score"<<score<<"legacy_score"<<score<<"alignment_x"<<ex<<"alignment_y"<<ey<<"edge_coverage"<<coverage<<"periodicity"<<periodicity<<"local_peak"<<peak<<"}";}fs<<"]"<<"manual_input_required"<<(f.confidence<.25)<<"}";
    }
    fs<<"]"<<"warnings"<<"[";if(uncertain)fs<<"weak_or_ambiguous_pitch_evidence";fs<<"]"<<"debug_confidence_encoding"<<"red_low_green_high"<<"shared_pitch_override"<<shared_pitch<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"}";
    if(!pp::write_png_atomic(a.get("output"),debug)||!pp::write_png_atomic(a.get("debug"),source)||!pp::publish_json(fs,tmp,a.get("meta")))return pp::input_error("could not publish lattice result");
    std::cout<<"completed lattice sprites="<<sprites.size()<<(uncertain?" with warnings":"")<<'\n';return 0;
}

/* agent: gpt-6.1-sol | 2026-10-08 | Score lattice edge evidence | v 1.9.0 | 13bed6 */
/* agent: GPT-6 | 2026-10-10 | Fit exact integer pixel lattices | v 2.0.0 | 0efbb2 */
