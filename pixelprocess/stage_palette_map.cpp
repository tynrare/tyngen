/*
 * File version: 1.0.0
 * Last updated: 2026-10-09
 * Purpose: Map quantized sprite sheets to a selected palette.
 * Flow PALETTE-MAP-01: 1. Runner -> pass quantized sheet, placements, and palette -> nearest-color map visible pixels while preserving alpha.
 * 2. Stage -> map sheet -> publish matching sprite crops, debug image, and metadata.
 * 3. Runner -> consume mapped stage when enabled -> publish its sheet and crops as final exports.
 */
/* agent: gpt-6 | 2026-10-09 | Add conditional palette mapping stage | v 1.0.0 | 708f23 */
#include "common.hpp"
#include "palette_mapping.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace {
struct Color { uint8_t r=0,g=0,b=0; };
/* Crop image to placement bounds and atomically write PNG; return bool. */
static bool save_crop(const cv::Mat&image,const std::vector<int>&bounds,const std::filesystem::path&path){if(bounds.size()!=4)return false;cv::Rect r(bounds[0],bounds[1],bounds[2],bounds[3]);r&=cv::Rect(0,0,image.cols,image.rows);return r.width>0&&r.height>0&&pp::write_png_atomic(path.string(),image(r));}
}

/* Map an assembled quantized RGBA sheet to a selected palette; return process status. */
int main(int argc,char**argv){
    auto a=pp::parse_args(argc,argv);for(const char*k:{"input","state","palette","output","debug","meta","sprite-dir"})if(!a.has(k))return pp::input_error(std::string("missing --")+k);
    cv::Mat source=pp::read_rgba(a.get("input"));auto placements=pp::read_json(a.get("state")),palette_file=pp::read_json(a.get("palette"));
    if(source.empty()||!placements.isOpened()||!palette_file.isOpened())return pp::input_error("quantized sheet, placements, or palette unavailable");
    std::vector<Color>colors;for(auto item=palette_file["colors"].begin();item!=palette_file["colors"].end();++item){std::vector<int>rgb;(*item)>>rgb;if(rgb.size()==3)colors.push_back({(uint8_t)std::clamp(rgb[0],0,255),(uint8_t)std::clamp(rgb[1],0,255),(uint8_t)std::clamp(rgb[2],0,255)});}
    if(colors.empty()||colors.size()>4096)return pp::input_error("selected palette must contain 1 through 4096 colors");
    std::vector<uint32_t>packed;packed.reserve(colors.size());for(const auto&c:colors)packed.push_back((uint32_t(c.r)<<16)|(uint32_t(c.g)<<8)|c.b);PpPaletteTree tree{};if(!pp_palette_build(&tree,packed.data(),packed.size()))return pp::input_error("selected palette could not be indexed");
    cv::Mat output=source.clone();for(int y=0;y<source.rows;y++){const auto*in=source.ptr<cv::Vec4b>(y);auto*out=output.ptr<cv::Vec4b>(y);for(int x=0;x<source.cols;x++)if(in[x][3]){int best=pp_palette_nearest(&tree,(uint32_t(in[x][2])<<16)|(uint32_t(in[x][1])<<8)|in[x][0]);out[x][0]=colors[best].b;out[x][1]=colors[best].g;out[x][2]=colors[best].r;}}
    std::filesystem::path sprite_dir=a.get("sprite-dir");std::error_code ec;std::filesystem::create_directories(sprite_dir,ec);if(ec)return pp::input_error("could not create mapped sprite directory");
    cv::Mat debug=source.clone();for(int y=0;y<source.rows;y++)for(int x=0;x<source.cols;x++)if(source.at<cv::Vec4b>(y,x)[3]){const auto p=source.at<cv::Vec4b>(y,x),q=output.at<cv::Vec4b>(y,x);if(p!=q)debug.at<cv::Vec4b>(y,x)=cv::Vec4b(0,0,255,255);}
    std::string temp;auto meta=pp::json_writer(a.get("meta"),temp);meta<<"schema_version"<<1<<"stage_id"<<"palette_map"<<"state"<<"completed"<<"palette"<<a.get("palette")<<"placements"<<"[";
    for(auto item=placements["placements"].begin();item!=placements["placements"].end();++item){std::string id=(std::string)(*item)["id"];std::vector<int>b;(*item)["destination_bounds"]>>b;if(b.size()!=4)continue;b[2]-=b[0];b[3]-=b[1];auto path=(sprite_dir/(id+".png")).string();if(!save_crop(output,b,path))return pp::input_error("could not write mapped sprite: "+id);meta<<"{"<<"id"<<id<<"row"<<(int)(*item)["row"]<<"col"<<(int)(*item)["col"]<<"path"<<path<<"destination_bounds"<<"["<<b[0]<<b[1]<<b[0]+b[2]<<b[1]+b[3]<<"]"<<"}";}
    meta<<"]"<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"sprites_dir"<<sprite_dir.string()<<"}";
    if(!pp::write_png_atomic(a.get("output"),output)||!pp::write_png_atomic(a.get("debug"),debug)||!pp::publish_json(meta,temp,a.get("meta")))return pp::input_error("could not publish palette mapping artifacts");return 0;
}
/* agent: gpt-6 | 2026-10-09 | Add conditional palette mapping stage | v 1.0.0 | 708f23 */
