/*
 * File version: 1.0.0
 * Last updated: 2026-10-09
 * Purpose: Verify local divider cleanup preserves artwork and grid edges.
 */
/* agent: gpt-6 | 2026-10-09 | Test local divider cleanup and ownership | v 1.0.0 | ec4328 */
#define main separator_runner_entry
#include "../runner.cpp"
#undef main
#undef NDEBUG
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unistd.h>

/* Process int argc and char** argv, verify separator stages, and return int status. */
int main(int argc,char**argv){
    assert(argc==2);fs::path bin=fs::absolute(argv[1]),root=fs::temp_directory_path()/("pixelprocess-separators-"+std::to_string(getpid()));fs::create_directories(root);
    cv::Mat image(400,500,CV_8UC4,cv::Scalar(160,67,248,255));
    for(int x=100;x<500;x+=100)cv::line(image,{x,0},{x,399},cv::Scalar(109,32,153,255),5);
    for(int y=100;y<400;y+=100)cv::line(image,{0,y},{499,y},cv::Scalar(109,32,153,255),5);
    for(int row=0;row<4;row++)for(int col=0;col<5;col++)cv::rectangle(image,{col*100+28,row*100+28,42,42},cv::Scalar(50+col*25,170-row*20,110+row*20,255),cv::FILLED);
    cv::rectangle(image,{92,132,17,17},cv::Scalar(20,15,60,255),cv::FILLED);
    for(int x:{120,220,320})cv::line(image,{x,302},{x,399},cv::Scalar(109,32,153,255),5);
    cv::line(image,{140,315},{140,350},cv::Scalar(24,18,62,255),2);cv::line(image,{28,380},{70,380},cv::Scalar(20,17,52,255),4);
    fs::path source=root/"source.png",settings=root/"settings.json",background=root/"background-mask.png",bgmeta=root/"background.stage.json",separator_mask=root/"separator-mask.png",refined=root/"refined-mask.png",sepmeta=root/"separators.json";
    assert(cv::imwrite(source.string(),image));{std::ofstream out(settings);out<<"{\"grid_cols\":5,\"grid_rows\":4,\"background_probes\":[],\"disabled_background_probes\":[],\"ownership_overrides\":{}}";assert(out.good());}
    assert(run_child(bin/"stage_background",{"--input",source.string(),"--settings",settings.string(),"--output",(root/"background.png").string(),"--debug",(root/"background.debug.png").string(),"--mask",background.string(),"--meta",bgmeta.string(),"--tolerance","24","--grid_cols","5","--grid_rows","4"}));
    assert(run_child(bin/"stage_separators",{"--input",source.string(),"--mask",background.string(),"--background-state",bgmeta.string(),"--settings",settings.string(),"--output",(root/"separators.png").string(),"--debug",(root/"separators.debug.png").string(),"--separator-mask",separator_mask.string(),"--refined-mask",refined.string(),"--meta",sepmeta.string()}));
    auto meta=pp::read_json(sepmeta.string());assert(meta["local_vertical_dividers"].size()==3&&meta["vertical_bands"].size()==4&&meta["horizontal_bands"].size()==3);
    cv::Mat removed=cv::imread(separator_mask.string(),cv::IMREAD_GRAYSCALE),clean=cv::imread(refined.string(),cv::IMREAD_GRAYSCALE);for(int x:{100,120,200,220,300,320,400})assert(removed.at<uchar>(350,x)==255);
    assert(removed.at<uchar>(140,100)==0&&clean.at<uchar>(140,100)==0);
    assert(removed.at<uchar>(330,140)==0&&clean.at<uchar>(330,140)==0&&removed.at<uchar>(380,45)==0&&clean.at<uchar>(380,45)==0);
    fs::path ownmeta=root/"ownership.json";assert(run_child(bin/"stage_ownership",{"--input",source.string(),"--mask",refined.string(),"--separator-mask",separator_mask.string(),"--separator-state",sepmeta.string(),"--settings",settings.string(),"--grid_rows","4","--grid_cols","5","--output",(root/"ownership.png").string(),"--debug",(root/"ownership.debug.png").string(),"--meta",ownmeta.string()}));
    auto ownership=pp::read_json(ownmeta.string());assert((int)ownership["grid"]["rows"]==4&&(int)ownership["grid"]["cols"]==5&&ownership["sprites"].size()==20);
    std::vector<int>xs,ys;ownership["grid"]["x_edges"]>>xs;ownership["grid"]["y_edges"]>>ys;assert((xs==std::vector<int>{0,100,200,300,400,500}));assert((ys==std::vector<int>{0,100,200,300,400}));
    fs::remove_all(root);return 0;
}
/* agent: gpt-6 | 2026-10-09 | Test local divider cleanup and ownership | v 1.0.0 | ec4328 */
