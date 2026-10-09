/*
 * File version: 1.3.0
 * Last updated: 2026-10-09
 * Purpose: Pack clean logical sprites into a centered sheet with placement records.
 * Flow PIXEL-01 step 8: clean sprite set -> stable row/column sheet and source-aligned debug evidence.
 */
/* agent: gpt-6.1-sol | 2026-10-05 | Assemble cleaned spritesheet outputs | v 1.1.0 | 657700 */
/* agent: gpt-6.1-sol | 2026-10-09 | Align assembly flow position | v 1.3.0 | 092824 */
#include "common.hpp"
#include <filesystem>
#include <map>

/* Place sprites in stable cell order without resizing; return process status. */
int main(int argc,char **argv){
    auto a=pp::parse_args(argc,argv);for(const char*k:{"input","state","sprites","output","debug","meta"})if(!a.has(k))return pp::input_error(std::string("missing --")+k);
    cv::Mat source=pp::read_rgba(a.get("input"));auto extraction=pp::read_json(a.get("state")),clean=pp::read_json(a.get("sprites"));
    if(source.empty()||!extraction.isOpened()||!clean.isOpened())return pp::input_error("source, extraction, or cleanup metadata unavailable");
    int rows=(int)extraction["grid"]["rows"],cols=(int)extraction["grid"]["cols"];rows=std::max(1,rows);cols=std::max(1,cols);
    int padding=std::max(0,std::min(256,(int)a.number("padding",1)));std::string anchor=a.get("anchor","center");
    auto nodes=clean["sprites"];int maxw=1,maxh=1;struct Item{std::string id,path;int row,col,w,h;std::vector<int> bounds;};std::vector<Item> items;
    for(auto it=nodes.begin();it!=nodes.end();++it){Item item;item.id=(std::string)(*it)["id"];item.path=(std::string)(*it)["path"];item.row=(int)(*it)["row"];item.col=(int)(*it)["col"];std::vector<int>d;(*it)["dimensions"]>>d;if(d.size()!=2)continue;item.w=d[0];item.h=d[1];
        auto ex=extraction["sprites"];for(auto ei=ex.begin();ei!=ex.end();++ei)if((std::string)(*ei)["id"]==item.id){(*ei)["source_bounds"]>>item.bounds;break;}
        maxw=std::max(maxw,item.w);maxh=std::max(maxh,item.h);items.push_back(item);
    }
    int tilew=maxw+padding*2,tileh=maxh+padding*2;cv::Mat sheet(rows*tileh,cols*tilew,CV_8UC4,cv::Scalar(0,0,0,0));cv::Mat debug=source.clone();
    std::string tmp;auto fs=pp::json_writer(a.get("meta"),tmp);fs<<"schema_version"<<1<<"stage_id"<<"assemble"<<"state"<<"completed"<<"coordinate_space"<<"sheet_pixels"<<"bounds_convention"<<"half_open"<<"anchor"<<anchor<<"padding"<<padding<<"placements"<<"[";
    for(auto &item:items){cv::Mat sprite=pp::read_rgba(item.path);if(sprite.empty())return pp::input_error("clean sprite missing: "+item.id);
        int dx=padding,dy=padding;if(anchor=="center"){dx+=(maxw-sprite.cols)/2;dy+=(maxh-sprite.rows)/2;}else if(anchor=="bottom_center"){dx+=(maxw-sprite.cols)/2;dy+=maxh-sprite.rows;}else if(anchor=="top_right"){dx+=maxw-sprite.cols;}
        int x=item.col*tilew+dx,y=item.row*tileh+dy;sprite.copyTo(sheet(cv::Rect(x,y,sprite.cols,sprite.rows)));
        if(item.bounds.size()==4){cv::Rect src(item.bounds[0],item.bounds[1],item.bounds[2]-item.bounds[0],item.bounds[3]-item.bounds[1]);cv::rectangle(debug,src,cv::Scalar(0,255,120,255),1);}
        fs<<"{"<<"id"<<item.id<<"row"<<item.row<<"col"<<item.col<<"source_bounds"<<"[";for(int v:item.bounds)fs<<v;fs<<"]"<<"destination_bounds"<<"["<<x<<y<<x+sprite.cols<<y+sprite.rows<<"]"<<"logical_dimensions"<<"["<<sprite.cols<<sprite.rows<<"]"<<"}";
    }
    /* agent: gpt-6.1-sol | 2026-10-06 | Retain empty metadata warning array | v 1.2.0 | 705d1d */
    fs<<"]"<<"grid"<<"["<<rows<<cols<<"]"<<"tile_dimensions"<<"["<<tilew<<tileh<<"]"<<"empty_slots_preserved"<<true<<"warnings"<<"["<<"]"<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"sprites_dir"<<std::filesystem::path(items.empty()?"":items[0].path).parent_path().string()<<"}";
    if(!pp::write_png_atomic(a.get("output"),sheet)||!pp::write_png_atomic(a.get("debug"),debug)||!pp::publish_json(fs,tmp,a.get("meta")))return pp::input_error("could not publish assembled spritesheet");
    std::cout<<"completed assemble sheet="<<sheet.cols<<'x'<<sheet.rows<<" sprites="<<items.size()<<'\n';return 0;
}

/* agent: gpt-6.1-sol | 2026-10-05 | Assemble cleaned spritesheet outputs | v 1.1.0 | 657700 */
/* agent: gpt-6.1-sol | 2026-10-06 | Retain empty metadata warning array | v 1.2.0 | 705d1d */
/* agent: gpt-6.1-sol | 2026-10-09 | Align assembly flow position | v 1.3.0 | 092824 */
