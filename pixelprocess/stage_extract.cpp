/*
 * File version: 1.2.0
 * Last updated: 2026-10-05
 * Purpose: Extract isolated per-cell sprite crops and preserve source mappings.
 * Flow PIXEL-01 step 3: ownership -> bounded crops retaining mask evidence for cleanup.
 */
/* agent: gpt-6.1-sol | 2026-10-05 | Preview isolated sprite crops | v 1.2.0 | 7bc996 */
#include "common.hpp"
#include <algorithm>
#include <filesystem>

/* Process CLI argc and argv, write crop previews, and return int status. */
int main(int argc, char **argv) {
    auto a = pp::parse_args(argc, argv);
    for (const char *key : {"input", "mask", "state", "output", "debug", "meta", "sprite-dir"}) if (!a.has(key)) return pp::input_error(std::string("missing --") + key);
    cv::Mat image = pp::read_rgba(a.get("input")), background = cv::imread(a.get("mask"), cv::IMREAD_GRAYSCALE);
    auto owner = pp::read_json(a.get("state"));
    if (image.empty() || background.empty() || !owner.isOpened()) return pp::input_error("source, mask, or ownership metadata is unavailable");
    auto settings=pp::read_json(a.get("settings"));std::string cleanup="outside";if(settings.isOpened()&&!settings["cleanup_mode"].empty())cleanup=(std::string)settings["cleanup_mode"];
    cv::Mat foreground; cv::compare(background, 0, foreground, cv::CMP_EQ);
    cv::Mat labels, stats, centers; int label_count = cv::connectedComponentsWithStats(foreground, labels, stats, centers, 8, CV_32S);
    std::filesystem::create_directories(a.get("sprite-dir"));
    cv::Mat debug = image.clone(); int padding = std::max(0, std::min(64, (int)a.number("padding", 1)));
    int grid_rows=std::max(1,(int)owner["grid"]["rows"]),grid_cols=std::max(1,(int)owner["grid"]["cols"]);
    struct PreviewTile{int row,col;cv::Mat image;};std::vector<PreviewTile> preview_tiles;int tile_w=1,tile_h=1;
    std::string temp; auto fs = pp::json_writer(a.get("meta"), temp);
    fs << "schema_version" << 1 << "stage_id" << "extract" << "state" << "completed";
    fs << "coordinate_space" << "source_pixels" << "bounds_convention" << "half_open";
    fs << "grid" << "{" << "rows" << (int)owner["grid"]["rows"] << "cols" << (int)owner["grid"]["cols"] << "}";
    fs << "sprites" << "[";
    cv::FileNode sprites = owner["sprites"];
    for (auto it = sprites.begin(); it != sprites.end(); ++it) {
        std::string id = (std::string)(*it)["id"];
        int sx0=0, sy0=0, sx1=0, sy1=0;
        std::vector<int> bounds; (*it)["source_bounds"] >> bounds;
        if (bounds.size() != 4) continue;
        sx0=bounds[0]; sy0=bounds[1]; sx1=bounds[2]; sy1=bounds[3];
        std::vector<int> component_labels;
        cv::FileNode components = (*it)["components"];
        for (auto ci = components.begin(); ci != components.end(); ++ci) {
            std::string blob = (std::string)(*ci)["id"];
            try { component_labels.push_back(std::stoi(blob.substr(5))); } catch (...) {}
        }
        if (sx1 <= sx0 || sy1 <= sy0) { sx0=sx1=bounds[0]; sy0=sy1=bounds[1]; }
        int x0=std::max(0,sx0-padding), y0=std::max(0,sy0-padding), x1=std::min(image.cols,sx1+padding), y1=std::min(image.rows,sy1+padding);
        cv::Mat crop(std::max(1,y1-y0), std::max(1,x1-x0), CV_8UC4, cv::Scalar(0,0,0,0));
        for (int y=y0; y<y1; ++y) for (int x=x0; x<x1; ++x) {
            int label=labels.at<int>(y,x); const auto px=image.at<cv::Vec4b>(y,x);
            bool owned=std::find(component_labels.begin(),component_labels.end(),label)!=component_labels.end();
            bool magenta=(int)px[0]>=220&&(int)px[2]>=220&&(int)px[1]<=55&&px[3]>0;
            bool protected_magenta=cleanup!="all"&&magenta&&x>=sx0&&x<sx1&&y>=sy0&&y<sy1;
            if(owned||background.at<uchar>(y,x)||protected_magenta)crop.at<cv::Vec4b>(y-y0,x-x0)=px;
        }
        std::string path=(std::filesystem::path(a.get("sprite-dir"))/(id+".png")).string();
        if (!pp::write_png_atomic(path,crop)) return pp::input_error("could not write sprite crop " + id);
        preview_tiles.push_back({(int)(*it)["row"],(int)(*it)["col"],crop});tile_w=std::max(tile_w,crop.cols);tile_h=std::max(tile_h,crop.rows);
        cv::rectangle(debug,cv::Rect(x0,y0,std::max(1,x1-x0),std::max(1,y1-y0)),cv::Scalar(0,220,255,255),1);
        fs << "{" << "id" << id << "row" << (int)(*it)["row"] << "col" << (int)(*it)["col"] << "occupied" << ((int)(*it)["occupied"] != 0);
        fs << "path" << path << "source_bounds" << "[" << sx0 << sy0 << sx1 << sy1 << "]" << "crop_bounds" << "[" << x0 << y0 << x1 << y1 << "]";
        fs << "source_to_crop" << "[" << 1 << 0 << -x0 << 0 << 1 << -y0 << "]" << "padding_requested" << padding << "}";
    }
    cv::Mat preview(grid_rows*tile_h,grid_cols*tile_w,CV_8UC4,cv::Scalar(0,0,0,0));
    for(const auto &tile:preview_tiles){int x=tile.col*tile_w+(tile_w-tile.image.cols)/2,y=tile.row*tile_h+(tile_h-tile.image.rows)/2;tile.image.copyTo(preview(cv::Rect(x,y,tile.image.cols,tile.image.rows)));}
    for(int c=1;c<grid_cols;c++)cv::line(preview,{c*tile_w,0},{c*tile_w,preview.rows-1},cv::Scalar(110,110,110,255),1);
    for(int r=1;r<grid_rows;r++)cv::line(preview,{0,r*tile_h},{preview.cols-1,r*tile_h},cv::Scalar(110,110,110,255),1);
    fs << "]" << "preview_grid" << "[" << grid_rows << grid_cols << "]" << "preview_tile_dimensions" << "[" << tile_w << tile_h << "]" << "artifacts" << "{" << "main" << a.get("output") << "debug" << a.get("debug") << "sprites_dir" << a.get("sprite-dir") << "}";
    if (!pp::write_png_atomic(a.get("output"),preview)||!pp::write_png_atomic(a.get("debug"),debug)||!pp::publish_json(fs,temp,a.get("meta"))) return pp::input_error("could not publish extraction result");
    (void)label_count; std::cout<<"completed extraction sprites="<<sprites.size()<<'\n'; return 0;
}

/* agent: gpt-6.1-sol | 2026-10-05 | Preview isolated sprite crops | v 1.2.0 | 7bc996 */
