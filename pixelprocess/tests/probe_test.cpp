/*
 * File version: 1.0.0
 * Last updated: 2026-10-09
 * Purpose: Verify exterior probes, transparency, cache signatures, and repeated ownership remapping.
 */
/* agent: gpt-6 | 2026-10-09 | Verify probe masks and ownership remapping | v 1.0.0 | b66afa */

#define main probe_runner_entry
#include "../runner.cpp"
#undef main
#undef NDEBUG
#include <cassert>

/* Write const fs::path settings with const string JSON; return void. */
static void probe_settings(const fs::path &path,const std::string &json){std::ofstream out(path);out<<json;assert(out.good());}

/* Run background from fs::path executable/source/settings into a directory; return mask cv::Mat. */
static cv::Mat probe_background(const fs::path &bin,const fs::path &source,const fs::path &settings,const fs::path &dir){
    fs::create_directories(dir);assert(run_child(bin/"stage_background",{"--input",source.string(),"--settings",settings.string(),"--output",(dir/"clean.png").string(),"--debug",(dir/"debug.png").string(),"--mask",(dir/"background-mask.png").string(),"--meta",(dir/"background.stage.json").string(),"--grid_cols","2","--grid_rows","1"}));
    return cv::imread((dir/"background-mask.png").string(),cv::IMREAD_GRAYSCALE);
}

/* Run ownership for cv::Mat mask and fs::path inputs, optionally prior directory; return metadata path. */
static fs::path probe_ownership(const fs::path &bin,const fs::path &source,const fs::path &settings,const cv::Mat &mask,const fs::path &dir,const fs::path &prior={}){
    fs::create_directories(dir);assert(cv::imwrite((dir/"background-mask.png").string(),mask));fs::path meta=dir/"ownership.json";
    std::vector<std::string> args={"--input",source.string(),"--settings",settings.string(),"--mask",(dir/"background-mask.png").string(),"--output",(dir/"owner.png").string(),"--debug",(dir/"debug.png").string(),"--meta",meta.string(),"--grid_cols","2","--grid_rows","1"};
    if(!prior.empty()){args.push_back("--previous");args.push_back((prior/"ownership.json").string());}assert(run_child(bin/"stage_ownership",args));return meta;
}

/* Find component ID at int x in const fs::path metadata; return string. */
static std::string component_at(const fs::path &path,int x){auto data=pp::read_json(path.string());for(auto sprite:data["sprites"])for(auto component:sprite["components"])if((int)component["bounds"][0]==x)return (std::string)component["id"];assert(false);return {};}

/* Verify background and correction behavior against generated images; return int status. */
int main(int argc,char **argv){
    assert(argc==2);fs::path bin=fs::absolute(argv[1]);fs::path root=fs::temp_directory_path()/("pixelprocess-probes-"+std::to_string(getpid()));fs::create_directories(root);
    fs::path settings=root/"settings.json",source=root/"source.png";probe_settings(settings,"{\"background_probes\":[],\"ownership_overrides\":{}}");
    cv::Mat image(32,64,CV_8UC4,cv::Scalar(255,0,255,255));cv::rectangle(image,{8,8,16,16},cv::Scalar(30,120,190,255),-1);cv::rectangle(image,{12,12,8,8},cv::Scalar(245,245,245,255),-1);assert(cv::imwrite(source.string(),image));
    auto mask=probe_background(bin,source,settings,root/"exterior");assert(mask.at<uchar>(0,0)==255&&mask.at<uchar>(15,15)==0);
    probe_settings(settings,"{\"background_probes\":[{\"id\":\"manual-15-15\",\"x\":15,\"y\":15}],\"disabled_background_probes\":[\"auto-r0-c1\"]}");
    mask=probe_background(bin,source,settings,root/"manual");assert(mask.at<uchar>(15,15)==255);auto metadata=pp::read_json((root/"manual/background.stage.json").string());assert((int)metadata["measurements"]["manual_probe_count"]==1);for(auto probe:metadata["probes"])assert((std::string)probe["id"]!="auto-r0-c1");
    fs::path cache_run=root/"cache-run";
    auto cache_settings=[&](int x,int y){probe_settings(settings,"{\"grid_cols\":2,\"grid_rows\":1,\"pitch\":\"2\",\"min_sprite_cells\":2,\"cleanup_mode\":\"skip\",\"background_probes\":[{\"id\":\"manual-fixed\",\"x\":"+std::to_string(x)+",\"y\":"+std::to_string(y)+"}]}");};
    auto cache_pipeline=[&](){assert(run_child(bin/"runner",{"--input",source.string(),"--run-dir",cache_run.string(),"--settings",settings.string()}));return pp::read_json((cache_run/"extra/pixelprocess.manifest.json").string());};
    cache_settings(15,15);auto graph=cache_pipeline();fs::path cache_dir=fs::path((std::string)graph["settings_path"]).parent_path();assert(cv::imread((cache_dir/"background-mask.png").string(),cv::IMREAD_GRAYSCALE).at<uchar>(15,15)==255);
    cache_settings(4,4);graph=cache_pipeline();cache_dir=fs::path((std::string)graph["settings_path"]).parent_path();assert(cv::imread((cache_dir/"background-mask.png").string(),cv::IMREAD_GRAYSCALE).at<uchar>(15,15)==0);
    graph=cache_pipeline();bool cached=false;std::ifstream events(cache_run/"extra/processing.events.jsonl");std::string event_line;while(std::getline(events,event_line))if(event_line.find("\"kind\":\"stage_reused\"")!=std::string::npos&&event_line.find("\"step_id\":\"background\"")!=std::string::npos)cached=true;assert(cached);
    image.setTo(cv::Scalar(0,0,0,0));cv::rectangle(image,{8,8,16,16},cv::Scalar(10,10,10,255),-1);assert(cv::imwrite(source.string(),image));probe_settings(settings,"{}");mask=probe_background(bin,source,settings,root/"transparent");assert(mask.at<uchar>(12,12)==0&&mask.at<uchar>(0,0)==255);
    probe_settings(settings,"{\"background_probes\":[{\"id\":\"p\",\"x\":1,\"y\":2}]}");auto config=pp::read_json(settings.string());std::string fingerprint=settings_signature(config,{"background_probes"});
    probe_settings(settings,"{\"background_probes\":[{\"id\":\"p\",\"x\":3,\"y\":2}]}");config=pp::read_json(settings.string());assert(fingerprint!=settings_signature(config,{"background_probes"}));
    image.setTo(cv::Scalar(100,100,100,255));assert(cv::imwrite(source.string(),image));probe_settings(settings,"{\"ownership_overrides\":{}}");cv::Mat old_mask(32,64,CV_8UC1,cv::Scalar(255));cv::rectangle(old_mask,{4,4,8,8},cv::Scalar(0),-1);cv::rectangle(old_mask,{36,12,8,8},cv::Scalar(0),-1);
    auto old_meta=probe_ownership(bin,source,settings,old_mask,root/"old");std::string old_id=component_at(old_meta,4);probe_settings(settings,"{\"ownership_overrides\":{\""+old_id+"\":\"sprite-r000-c001\"}}");
    cv::Mat shifted=old_mask.clone();cv::rectangle(shifted,{1,1,2,2},cv::Scalar(0),-1);auto shifted_meta=probe_ownership(bin,source,settings,shifted,root/"shifted",root/"old");std::string new_id=component_at(shifted_meta,4);assert(new_id!=old_id);metadata=pp::read_json(shifted_meta.string());assert((std::string)metadata["ownership_overrides"][new_id]=="sprite-r000-c001");
    assert(rebase_ownership_settings(settings,shifted_meta));auto repeated=probe_ownership(bin,source,settings,shifted,root/"repeated",root/"shifted");metadata=pp::read_json(repeated.string());assert((std::string)metadata["ownership_overrides"][new_id]=="sprite-r000-c001");
    probe_settings(settings,"{\"ownership_overrides\":{\""+old_id+"\":\"sprite-r000-c001\"}}");cv::Mat split=old_mask.clone();cv::line(split,{8,4},{8,11},cv::Scalar(255));auto split_meta=probe_ownership(bin,source,settings,split,root/"split",root/"old");metadata=pp::read_json(split_meta.string());assert(metadata["ownership_overrides"].size()==0);
    cv::Mat merged=old_mask.clone();cv::line(merged,{11,8},{36,16},cv::Scalar(0),2);auto merged_meta=probe_ownership(bin,source,settings,merged,root/"merged",root/"old");metadata=pp::read_json(merged_meta.string());assert(metadata["ownership_overrides"].size()==0);
    assert(rebase_ownership_settings(settings,merged_meta));config=pp::read_json(settings.string());assert(config["ownership_overrides"].size()==0);
    fs::remove_all(root);std::cout<<"probe stage tests passed\n";return 0;
}
/* agent: gpt-6 | 2026-10-09 | Verify probe masks and ownership remapping | v 1.0.0 | b66afa */
