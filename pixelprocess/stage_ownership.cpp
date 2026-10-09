/*
 * File version: 1.4.0
 * Last updated: 2026-10-09
 * Purpose: Detect foreground blobs and assign stable spritesheet cell owners.
 * Flow PIXEL-01 step 2: mask -> grid slots/components; empty cells and small blobs remain recorded.
 */
/* agent: gpt-6.1-sol | 2026-10-09 | Preserve clear ownership matches | v 1.4.0 | d70f2b */
#include "common.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>

struct LocalGrid { std::vector<int> x_edges; std::vector<std::vector<int>> row_edges; bool refined=false,transposed=false,ambiguous=false; };

/* Infer lanes from cv::Mat, bool axis, and int count; return std::vector<int>. */
static std::vector<int> lane_edges(const cv::Mat &mask, bool vertical, int requested) {
    int length = vertical ? mask.cols : mask.rows;
    std::vector<int> edges{0};
    std::vector<int> projection((size_t)length);
    for (int i = 0; i < length; ++i) {
        cv::Mat slice = vertical ? mask.col(i) : mask.row(i);
        projection[(size_t)i] = cv::countNonZero(slice);
    }
    std::vector<std::pair<int,int>> gaps;
    int begin = -1;
    for (int i = 0; i <= length; ++i) {
        bool empty = i < length && projection[(size_t)i] == 0;
        if (empty && begin < 0) begin = i;
        if (!empty && begin >= 0) { if (i - begin >= (requested>0?1:2) && begin > 0 && i < length) gaps.emplace_back(begin, i); begin = -1; }
    }
    if(requested>0){
        int needed=requested-1;
        if((int)gaps.size()>=needed){std::stable_sort(gaps.begin(),gaps.end(),[](const auto&a,const auto&b){return a.second-a.first>b.second-b.first;});gaps.resize((size_t)needed);for(auto [lo,hi]:gaps)edges.push_back((lo+hi)/2);std::sort(edges.begin(),edges.end());edges.push_back(length);return edges;}
        for(int i=1;i<requested;i++)edges.push_back((int)std::lround((double)length*i/requested));edges.push_back(length);return edges;
    }
    for (auto [lo, hi] : gaps) edges.push_back((lo + hi) / 2);
    edges.push_back(length); std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    return edges;
}

/* Find empty x intervals in a foreground band; return ordered separator midpoints. */
static std::vector<int> band_gaps(const cv::Mat &mask,int y0,int y1){
    std::vector<int> projection((size_t)mask.cols);
    for(int x=0;x<mask.cols;x++){int n=0;for(int y=y0;y<y1;y++)n+=mask.at<uchar>(y,x)!=0;projection[(size_t)x]=n;}
    std::vector<int> edges{0};int begin=-1;
    for(int x=0;x<=mask.cols;x++){bool empty=x<mask.cols&&projection[(size_t)x]==0;if(empty&&begin<0)begin=x;if(!empty&&begin>=0){if(begin>0&&x<mask.cols)edges.push_back((begin+x)/2);begin=-1;}}
    edges.push_back(mask.cols);return edges;
}

/* Resolve shifted row-local separators into one logical grid; return LocalGrid. */
static LocalGrid resolve_row_local_grid(const cv::Mat &mask,const std::vector<int>&y_edges,std::vector<int>global,int requested_cols){
    LocalGrid result;result.x_edges=std::move(global);int rows=(int)y_edges.size()-1;if(rows<2)return result;
    int expected=-1,reliable=0,tolerance=std::max(3,mask.cols/32);result.row_edges.reserve((size_t)rows);
    for(int row=0;row<rows;row++){
        auto edges=band_gaps(mask,y_edges[(size_t)row],y_edges[(size_t)row+1]);int gaps=(int)edges.size()-2;
        if(gaps<=0){result.ambiguous=reliable>0;result.row_edges.clear();return result;}
        if(expected<0)expected=gaps;
        if(gaps!=expected){result.ambiguous=true;result.row_edges.clear();return result;}
        if(reliable)for(int i=1;i<=gaps;i++)if(std::abs(edges[(size_t)i]-result.row_edges.front()[(size_t)i])>tolerance){result.ambiguous=true;result.row_edges.clear();return result;}
        result.row_edges.push_back(std::move(edges));reliable++;
    }
    if(reliable<2||(requested_cols>0&&expected!=requested_cols-1)){result.ambiguous=reliable>0;return result;}
    std::vector<int> nominal((size_t)expected+2);nominal.front()=0;nominal.back()=mask.cols;
    for(int i=1;i<=expected;i++){std::vector<int>values;for(const auto&edges:result.row_edges)if(!edges.empty())values.push_back(edges[(size_t)i]);std::sort(values.begin(),values.end());nominal[(size_t)i]=values[values.size()/2];}
    result.x_edges=std::move(nominal);result.refined=true;return result;
}

/* Find one unambiguous new component matching an old component box; return label or 0. */
static int matching_component(const cv::Rect &old_box,int old_pixels,const cv::Mat &stats,int count){
    int best=0;double best_score=0,second_score=0;
    for(int label=1;label<count;label++){
        cv::Rect box(stats.at<int>(label,cv::CC_STAT_LEFT),stats.at<int>(label,cv::CC_STAT_TOP),stats.at<int>(label,cv::CC_STAT_WIDTH),stats.at<int>(label,cv::CC_STAT_HEIGHT));
        int pixels=stats.at<int>(label,cv::CC_STAT_AREA);double ratio=(double)std::min(old_pixels,pixels)/std::max(old_pixels,pixels);
        cv::Rect overlap=old_box&box;double union_area=(double)old_box.area()+box.area()-overlap.area();double iou=union_area>0?overlap.area()/union_area:0;
        double score=iou*ratio;if(iou<.72||ratio<.72)continue;
        if(score>best_score){second_score=best_score;best_score=score;best=label;}else if(score>second_score)second_score=score;
    }
    return best&&best_score-second_score>=.12?best:0;
}

/* Process CLI argc and argv, emit ownership, and return int status. */
int main(int argc, char **argv) {
    auto a = pp::parse_args(argc, argv);
    for (const char *key : {"input", "mask", "output", "debug", "meta"}) if (!a.has(key)) return pp::input_error(std::string("missing --") + key);
    cv::Mat image = pp::read_rgba(a.get("input")), mask = cv::imread(a.get("mask"), cv::IMREAD_GRAYSCALE);
    if (image.empty() || mask.empty() || image.size() != mask.size()) return pp::input_error("input and background mask must be readable and equal-sized");
    cv::Mat foreground; cv::compare(mask, 0, foreground, cv::CMP_EQ);
    cv::Mat labels, stats, centers; int count = cv::connectedComponentsWithStats(foreground, labels, stats, centers, 8, CV_32S);
    auto ys = lane_edges(foreground, false, (int)a.number("grid_rows", 0));
    auto xs = lane_edges(foreground, true, (int)a.number("grid_cols", 0));
    LocalGrid local_grid;
    if((int)a.number("grid_rows",0)==0||(int)a.number("grid_cols",0)==0 || ((int)a.number("grid_cols",0)>0&&(int)a.number("grid_rows",0)>0)){
        local_grid=resolve_row_local_grid(foreground,ys,std::move(xs),(int)a.number("grid_cols",0));xs=local_grid.x_edges;
    }
    if(!local_grid.refined&&xs.size()>2){cv::Mat transposed;cv::transpose(foreground,transposed);auto evidence=resolve_row_local_grid(transposed,xs,ys,(int)a.number("grid_rows",0));if(evidence.refined){local_grid.row_edges=std::move(evidence.row_edges);local_grid.refined=true;local_grid.transposed=true;ys=std::move(evidence.x_edges);}else local_grid.ambiguous=local_grid.ambiguous||evidence.ambiguous;}
    int cols = (int)xs.size() - 1, rows = (int)ys.size() - 1;
    std::vector<int> owner((size_t)count, 0);
    for (int label = 1; label < count; ++label) {
        int cx = (int)std::floor(centers.at<double>(label, 0)), cy = (int)std::floor(centers.at<double>(label, 1));
        int row=0,col=0;
        if(local_grid.transposed){col=std::clamp((int)(std::upper_bound(xs.begin(),xs.end(),cx)-xs.begin())-1,0,cols-1);const auto&column_edges=local_grid.row_edges[(size_t)col];row=std::clamp((int)(std::upper_bound(column_edges.begin(),column_edges.end(),cy)-column_edges.begin())-1,0,rows-1);}
        else{row=std::clamp((int)(std::upper_bound(ys.begin(),ys.end(),cy)-ys.begin())-1,0,rows-1);const auto&row_edges=local_grid.refined?local_grid.row_edges[(size_t)row]:xs;col=std::clamp((int)(std::upper_bound(row_edges.begin(),row_edges.end(),cx)-row_edges.begin())-1,0,cols-1);}
        owner[(size_t)label] = row * cols + col;
    }
    auto settings=pp::read_json(a.get("settings"));auto overrides=settings["ownership_overrides"];cv::FileStorage prior;if(a.has("previous"))prior=pp::read_json(a.get("previous"));
    for(auto oi=overrides.begin();oi!=overrides.end();++oi){
        cv::FileNode entry=*oi;std::string blob=entry.name(),target=(std::string)entry;int old_label=0,new_label=0,row=-1,col=-1;
        if(sscanf(blob.c_str(),"blob-%d",&old_label)!=1||old_label<1||sscanf(target.c_str(),"sprite-r%d-c%d",&row,&col)!=2||row<0||row>=rows||col<0||col>=cols)continue;
        if(prior.isOpened()){
            for(auto si=prior["sprites"].begin();si!=prior["sprites"].end();++si)for(auto bi=(*si)["components"].begin();bi!=(*si)["components"].end();++bi){
                if((std::string)(*bi)["id"]!=blob)continue;std::vector<int>b;(*bi)["bounds"]>>b;if(b.size()!=4)continue;
                new_label=matching_component(cv::Rect(b[0],b[1],b[2]-b[0],b[3]-b[1]),(int)(*bi)["pixels"],stats,count);break;
            }
        }else if(!a.has("previous")&&old_label<count)new_label=old_label;
        if(new_label)owner[(size_t)new_label]=row*cols+col;
    }
    cv::Mat debug = image.clone(), ownership_view=image.clone(); std::string temp; auto fs = pp::json_writer(a.get("meta"), temp);
    if (!fs.isOpened()) return pp::input_error("cannot write stage metadata");
    bool grouped_in_one_inferred_cell=(int)a.number("grid_cols",0)==0&&(int)a.number("grid_rows",0)==0&&cols==1&&rows==1&&count>2;bool needs_warning=grouped_in_one_inferred_cell||local_grid.ambiguous;
    fs << "schema_version" << 1 << "stage_id" << "ownership" << "state" << (needs_warning?"completed_with_warnings":"completed");
    fs << "coordinate_space" << "source_pixels" << "bounds_convention" << "half_open";
    fs << "grid" << "{" << "rows" << rows << "cols" << cols << "x_edges" << "["; for (int x : xs) fs << x; fs << "]" << "y_edges" << "["; for (int y : ys) fs << y; fs << "]" << "inference" << (local_grid.transposed?"column_local":local_grid.refined?"row_local":"global_gaps") << "supporting_bands" << (local_grid.refined?std::max(rows,cols):0) << "row_x_edges" << "[";
    for(int row=0;row<rows;row++){const auto&edges=local_grid.refined&&!local_grid.transposed?local_grid.row_edges[(size_t)row]:xs;fs<<"[";for(int x:edges)fs<<x;fs<<"]";}fs<<"]"<<"column_y_edges"<<"[";
    for(int col=0;col<cols;col++){const auto&edges=local_grid.transposed?local_grid.row_edges[(size_t)col]:ys;fs<<"[";for(int y:edges)fs<<y;fs<<"]";}fs<<"]"<<"}";
    fs << "sprites" << "[";
    for (int slot = 0; slot < rows * cols; ++slot) {
        int row = slot / cols, col = slot % cols, y0 = ys[(size_t)row], y1 = ys[(size_t)row + 1];
        const auto&row_edges=local_grid.refined&&!local_grid.transposed?local_grid.row_edges[(size_t)row]:xs;const auto&column_y=local_grid.transposed?local_grid.row_edges[(size_t)col]:ys;int x0=row_edges[(size_t)col],x1=row_edges[(size_t)col+1];y0=column_y[(size_t)row];y1=column_y[(size_t)row+1];
        int bx0 = image.cols, by0 = image.rows, bx1 = 0, by1 = 0, pixels = 0;
        for (int label = 1; label < count; ++label) if (owner[(size_t)label] == slot) {
            int x = stats.at<int>(label, cv::CC_STAT_LEFT), y = stats.at<int>(label, cv::CC_STAT_TOP);
            int w = stats.at<int>(label, cv::CC_STAT_WIDTH), h = stats.at<int>(label, cv::CC_STAT_HEIGHT);
            bx0 = std::min(bx0, x); by0 = std::min(by0, y); bx1 = std::max(bx1, x + w); by1 = std::max(by1, y + h); pixels += stats.at<int>(label, cv::CC_STAT_AREA);
        }
        bool occupied = pixels > 0; if (!occupied) { bx0 = x0; by0 = y0; bx1 = x0; by1 = y0; }
        std::ostringstream id; id << "sprite-r" << std::setw(3) << std::setfill('0') << row << "-c" << std::setw(3) << col;
        fs << "{" << "id" << id.str() << "row" << row << "col" << col << "occupied" << occupied << "owned_pixels" << pixels;
        fs << "cell_bounds" << "[" << x0 << y0 << x1 << y1 << "]" << "source_bounds" << "[" << bx0 << by0 << bx1 << by1 << "]";
        fs << "components" << "[";
        int local_component = 0;
        for (int label = 1; label < count; ++label) if (owner[(size_t)label] == slot) {
            cv::Rect b(stats.at<int>(label, cv::CC_STAT_LEFT), stats.at<int>(label, cv::CC_STAT_TOP), stats.at<int>(label, cv::CC_STAT_WIDTH), stats.at<int>(label, cv::CC_STAT_HEIGHT));
            fs << "{" << "id" << (std::string("blob-") + std::to_string(label)) << "bounds" << "[" << b.x << b.y << b.x + b.width << b.y + b.height << "]" << "pixels" << stats.at<int>(label, cv::CC_STAT_AREA) << "}";
            ++local_component;
            cv::Scalar color((label * 53) % 220 + 25, (label * 97) % 220 + 25, (label * 131) % 220 + 25, 255);
            cv::rectangle(debug, b, color, 1);
            for(int py=b.y;py<b.y+b.height;py++)for(int px=b.x;px<b.x+b.width;px++)if(labels.at<int>(py,px)==label){
                auto &p=ownership_view.at<cv::Vec4b>(py,px);p[0]=(uchar)((p[0]+color[0])/2);p[1]=(uchar)((p[1]+color[1])/2);p[2]=(uchar)((p[2]+color[2])/2);
            }
        }
        fs << "]" << "}";
        cv::rectangle(debug, cv::Rect(x0, y0, std::max(1, x1-x0), std::max(1, y1-y0)), cv::Scalar(0, 210, 255, 255), 1);
        cv::putText(ownership_view,id.str(),cv::Point(x0+3,y0+16),cv::FONT_HERSHEY_SIMPLEX,.4,cv::Scalar(0,0,0,255),2,cv::LINE_AA);
        cv::putText(ownership_view,id.str(),cv::Point(x0+3,y0+16),cv::FONT_HERSHEY_SIMPLEX,.4,cv::Scalar(255,255,255,255),1,cv::LINE_AA);
    }
    fs << "]" << "measurements" << "{" << "component_count" << count - 1 << "slot_count" << rows * cols << "}";
    fs << "warnings" << "["; if (grouped_in_one_inferred_cell) fs << "multiple_components_grouped_in_single_inferred_cell"; if(local_grid.ambiguous)fs<<"local_grid_evidence_ambiguous"; fs << "]";
    fs << "artifacts" << "{" << "main" << a.get("output") << "debug" << a.get("debug") << "}";
    if (!pp::write_png_atomic(a.get("output"), ownership_view) || !pp::write_png_atomic(a.get("debug"), debug) || !pp::publish_json(fs, temp, a.get("meta"))) return pp::input_error("could not publish ownership result");
    std::cout << "completed ownership components=" << count - 1 << " cells=" << rows << 'x' << cols << '\n'; return 0;
}

/* agent: gpt-6.1-sol | 2026-10-09 | Preserve clear ownership matches | v 1.4.0 | d70f2b */
