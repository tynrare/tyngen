/*
 * File version: 3.6.0
 * Last updated: 2026-10-07
 * Purpose: Execute, cache, record, and atomically publish a pixelprocess revision.
 * Flow PIXEL-02 canonical: 1. App -> snapshot source/settings and selected branch -> runner receives edit scope.
 * 2. Runner -> resolve selected branch nodes and its settings pin -> reuse matching completed stages.
 * 3. Runner -> stage new outputs under revision-specific paths -> existing branch artifacts remain intact until completion.
 * 4. Runner -> append lifecycle events and publish manifest snapshots -> UI keeps earlier finals visible during work.
 * 5. User -> request rerun -> runner replaces the selected branch ID and applies its resolved settings.
 * 6. User -> Save setup -> replace the selected branch; Save as copy -> create a pinned branch with its own settings.
 * 7. User -> unpin a branch -> keep its current artifacts until the next successful scoped rerun retires it.
 * 8. Runner -> setup save/pinned replay fails -> retain that branch's published final and manifest record.
 * 9. User -> select an artifact and rerun from its stage -> reprocess pinned descendants and retire affected unpinned branches.
 * 10. User -> export artifact -> validate ownership and atomically copy PNG.
 * 11. User -> pin/unpin a branch -> runner changes only the preservation flag; Save updates settings under the same ID.
 * Invariants: runner owns run state; input is immutable; pinned means preserve across rerun; generated outputs have revision-specific paths.
 * agent: gpt-6 | 2026-10-07 | Rerun scoped branch descendants | v 3.6.0 | 0c8b39
 */
/* agent: gpt-6.1-sol | 2026-10-07 | Bound alternate pitch and rerun branches | v 3.2.0 | edf89e */
/* agent: gpt-6 | 2026-10-07 | Align scale lanes after diagnosis | v 2.8.0 | 9adbe1 */
#include "common.hpp"
#include <algorithm>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <tuple>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs=std::filesystem;
static volatile sig_atomic_t interrupted=0;
static volatile sig_atomic_t active_child=0;
static void on_signal(int signal){interrupted=signal;if(active_child>0)kill((pid_t)active_child,SIGINT);}
struct Stage {std::string id,name,cache_key,main,debug,meta,state;int number;double started=0,elapsed=0;std::string branch="primary";int progress=0;std::string sprite_dir;};
struct Branch {std::string id,label,parent,reason,fork_step,settings_path;double pitch=0;bool pinned=false;std::vector<Stage> stages;std::string final;};
struct Spec {std::string id,name;int number;std::vector<std::string> inputs,argv;std::string setting_keys;};
static std::string now_utc(){auto now=std::chrono::system_clock::now();auto t=std::chrono::system_clock::to_time_t(now);std::tm tm{};gmtime_r(&t,&tm);char b[40];strftime(b,sizeof(b),"%Y-%m-%dT%H:%M:%SZ",&tm);return b;}
static void json_quote(std::ostream&o,const std::string&s){o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<(char)c;else if(c=='\n')o<<"\\n";else if(c=='\r')o<<"\\r";else if(c=='\t')o<<"\\t";else if(c<32)o<<' ';else o<<(char)c;}o<<'"';}
/* Append a schema-2 lifecycle record tagged by step, node, branch, and progress; return void. */
/* agent: gpt-6.1-sol | 2026-10-06 | Publish typed branch lifecycle events | v 2.3.0 | 59b7f0 */
static void event(const fs::path&path,int&seq,const std::string&kind,const std::string&stage,int number,const std::string&message,const std::string&branch="primary",int progress=-1){std::ofstream f(path,std::ios::app);if(!f)return;f<<"{\"schema_version\":2,\"sequence\":"<<++seq<<",\"timestamp_utc\":";json_quote(f,now_utc());f<<",\"kind\":";json_quote(f,kind);f<<",\"step_id\":";json_quote(f,stage);f<<",\"node_id\":";json_quote(f,branch+"/"+stage);f<<",\"branch_id\":";json_quote(f,branch);f<<",\"progress\":"<<progress<<",\"stage_number\":"<<number<<",\"message\":";json_quote(f,message);f<<"}\n";f.flush();}
static uint64_t hash_bytes(uint64_t h,const char*data,size_t n){for(size_t i=0;i<n;i++){h^=(unsigned char)data[i];h*=1099511628211ULL;}return h;}
static uint64_t hash_file(uint64_t h,const fs::path&p){std::ifstream f(p,std::ios::binary);if(!f)return h;char b[65536];while(f){f.read(b,sizeof(b));h=hash_bytes(h,b,(size_t)f.gcount());}return h;}
static uint64_t hash_path(uint64_t h,const fs::path&p){std::error_code ec;if(fs::is_regular_file(p,ec))return hash_file(h,p);if(fs::is_directory(p,ec)){std::vector<fs::path>paths;for(fs::recursive_directory_iterator i(p,ec),end;i!=end&&!ec;i.increment(ec))if(i->is_regular_file())paths.push_back(i->path());std::sort(paths.begin(),paths.end());for(auto&q:paths){auto rel=q.lexically_relative(p).generic_string();h=hash_bytes(h,rel.data(),rel.size());h=hash_file(h,q);}}return h;}
static std::string hash_hex(uint64_t h){std::ostringstream o;o<<std::hex<<std::setw(16)<<std::setfill('0')<<h;return o.str();}
static std::string signature(const Spec&s,const fs::path&exe){uint64_t h=1469598103934665603ULL;h=hash_bytes(h,s.id.data(),s.id.size());h=hash_bytes(h,s.setting_keys.data(),s.setting_keys.size());for(auto&p:s.inputs)h=hash_path(h,p);h=hash_path(h,exe);return hash_hex(h);}
static bool copy_atomic(const fs::path&from,const fs::path&to){std::error_code ec;fs::create_directories(to.parent_path(),ec);auto tmp=to;tmp += ".tmp";fs::copy_file(from,tmp,fs::copy_options::overwrite_existing,ec);if(ec)return false;fs::rename(tmp,to,ec);return !ec;}
static std::string stage_number(int number){std::ostringstream o;o<<std::setw(2)<<std::setfill('0')<<number;return o.str();}
static bool run_child(const fs::path&exe,const std::vector<std::string>&args){pid_t pid=fork();if(pid<0)return false;if(pid==0){setpgid(0,0);std::vector<char*>argv;std::string name=exe.string();argv.push_back(name.data());for(auto &a:args)argv.push_back(const_cast<char*>(a.c_str()));argv.push_back(nullptr);execv(name.c_str(),argv.data());_exit(127);}setpgid(pid,pid);active_child=pid;int status=0;for(;;){pid_t result=waitpid(pid,&status,WNOHANG);if(result==pid)break;if(result<0&&errno!=EINTR){active_child=0;return false;}if(interrupted){kill(-pid,SIGINT);for(int i=0;i<50;i++){result=waitpid(pid,&status,WNOHANG);if(result==pid)break;usleep(20000);}if(result!=pid){kill(-pid,SIGKILL);waitpid(pid,&status,0);}break;}usleep(20000);}active_child=0;return WIFEXITED(status)&&WEXITSTATUS(status)==0;}
static std::string node_string(const cv::FileNode&node){if(node.empty())return "";if(node.isString())return (std::string)node;if(node.isInt())return std::to_string((int)node);if(node.isReal()){std::ostringstream out;out.precision(17);out<<(double)node;return out.str();}if(node.isSeq()){std::string out="[";for(auto it=node.begin();it!=node.end();++it)out+=node_string(*it)+",";return out+"]";}return "";}
static std::string setting(const cv::FileStorage&s,const std::string&key,const std::string&fallback){auto n=s[key];std::string value=node_string(n);return value.empty()?fallback:value;}
static std::string settings_signature(cv::FileStorage&s,const std::vector<std::string>&keys){std::string result;bool sprites=false,owners=false;for(auto&key:keys){if(key=="sprite_overrides"){sprites=true;continue;}if(key=="ownership_overrides"){owners=true;continue;}result+=key+"="+setting(s,key,"")+";";}if(sprites){auto overrides=s["sprite_overrides"];for(auto it=overrides.begin();it!=overrides.end();++it){std::string id=(std::string)(*it)["id"];result+="sprite:"+id+":";for(auto&k:std::vector<std::string>{"pitch","phase_x","phase_y"})result+=k+"="+node_string((*it)[k])+";";}}if(owners){auto ownership=s["ownership_overrides"];for(auto it=ownership.begin();it!=ownership.end();++it){cv::FileNode entry=*it;result+="owner:"+entry.name()+"="+node_string(entry)+";";}}return result;}
/* Rank size-eligible pitch peaks from metadata/settings and report auto sprites; return pitch candidates. */
/* agent: gpt-6 | 2026-10-07 | Weight shared pitch evidence | v 2.9.0 | 042650 */
static std::vector<double> shared_pitch_candidates(const fs::path&path,const cv::FileStorage&settings,bool&has_auto_sprite,double max_ratio){
    auto data=pp::read_json(path.string());if(!data.isOpened())return{};
    std::map<std::string,bool>overridden;cv::FileNode sprite_overrides=settings["sprite_overrides"];for(auto item=sprite_overrides.begin();item!=sprite_overrides.end();++item)if(!(*item)["pitch"].empty())overridden[(std::string)(*item)["id"]]=true;
    std::vector<double>sums,weights,pitches;std::vector<int>eligible;has_auto_sprite=false;int auto_count=0;
    for(auto sprite=data["sprites"].begin();sprite!=data["sprites"].end();++sprite){
        cv::FileNode sprite_node=*sprite;if(overridden[(std::string)sprite_node["id"]])continue;
        has_auto_sprite=true;auto_count++;
        double confidence=(double)sprite_node["confidence"],weight=std::max(.05,confidence);
        double max_pitch=sprite_node["maximum_pitch_for_minimum_cells"].empty()?std::numeric_limits<double>::infinity():(double)sprite_node["maximum_pitch_for_minimum_cells"];
        auto candidates=sprite_node["candidates"];double peak=0;for(auto c=candidates.begin();c!=candidates.end();++c)peak=std::max(peak,(double)(*c)["score"]);
        if(peak<=0)continue;if(pitches.empty())for(auto c=candidates.begin();c!=candidates.end();++c)pitches.push_back((double)(*c)["pitch"]);
        size_t i=0;for(auto c=candidates.begin();c!=candidates.end()&&i<pitches.size();++c,++i){if(i>=sums.size()){sums.resize(i+1);weights.resize(i+1);eligible.resize(i+1);}if(pitches[i]<=max_pitch){sums[i]+=(double)(*c)["score"]/peak*weight;weights[i]+=weight;eligible[i]++;}}
    }
    if(pitches.empty()||auto_count==0)return{};std::vector<std::pair<double,double>>peaks;std::vector<double>score;for(size_t i=0;i<sums.size();i++)score.push_back(weights[i]>0&&eligible[i]==auto_count?sums[i]/weights[i]:0);
    for(size_t i=0;i<score.size()&&i<pitches.size();i++){bool local=(i==0||score[i]>=score[i-1])&&(i+1==score.size()||score[i]>=score[i+1]);if(local&&score[i]>0)peaks.emplace_back(pitches[i],score[i]);}
    std::sort(peaks.begin(),peaks.end(),[](const auto&a,const auto&b){return a.second>b.second;});if(peaks.empty())return{};
    std::vector<double>chosen{peaks[0].first};for(const auto&p:peaks){double ratio=p.first/chosen[0];double relative=std::max(ratio,1.0/ratio);if(relative>1.001&&relative<=max_ratio){chosen.push_back(p.first);break;}}
    return chosen;
}
/* agent: gpt-6 | 2026-10-07 | Weight shared pitch evidence | v 2.9.0 | 042650 */
/* agent: gpt-6.1-sol | 2026-10-05 | Tag sprite export artifact metadata | v 1.6.0 | 841122 */
/* Write manifest from fs::path inputs, strings, int revision, Stage records, and artifact paths; return void. */
/* agent: gpt-6.1-sol | 2026-10-06 | Add shared scale variants | v 1.8.0 | f639df */
/* agent: gpt-6.1-sol | 2026-10-06 | Preserve typed artifacts across failed runs | v 1.9.0 | 0ba0b3 */
/* Write manifest from fs::path inputs, strings, int revision, Stage/Branch records, and artifact paths; return void. */
static void write_manifest(const fs::path&path,const fs::path&source,const std::string&state,int revision,const std::string&settings_path,const std::vector<Stage>&stages,const std::vector<fs::path>&sprite_exports={},const std::vector<fs::path>&sheet_variants={},const std::vector<Branch>&branches={}) {
    std::vector<fs::path> retained_sprites=sprite_exports;
    if(state!="completed") {
        std::error_code ec;
        for(fs::directory_iterator it(path.parent_path(),ec),end;it!=end&&!ec;it.increment(ec)) {
            if(!it->is_regular_file(ec))continue;
            std::string name=it->path().filename().string();
            if(name.rfind("sprite-",0)==0&&it->path().extension()==".png"&&std::find(retained_sprites.begin(),retained_sprites.end(),it->path())==retained_sprites.end())retained_sprites.push_back(it->path());
        }
    }
    std::string tmp;std::uint64_t digest=hash_file(1469598103934665603ULL,source);auto fsout=pp::json_writer(path.string(),tmp);
    fsout<<"schema_version"<<2<<"metadata_format"<<"pixelprocess-graph"<<"processor_version"<<"2.2.0"<<"run_state"<<state<<"revision"<<revision<<"source"<<source.string()<<"source_fingerprint"<<hash_hex(digest)<<"fingerprint_algorithm"<<"fnv1a64"<<"event_journal"<<(path.parent_path()/"processing.events.jsonl").string()<<"settings_path"<<settings_path<<"stages"<<"[";
    for(auto&s:stages)fsout<<"{"<<"id"<<s.id<<"number"<<s.number<<"progress"<<s.progress<<"name"<<s.name<<"state"<<s.state<<"cache_key"<<s.cache_key<<"main"<<s.main<<"debug"<<s.debug<<"metadata"<<s.meta<<"started_at"<<s.started<<"elapsed_seconds"<<s.elapsed<<"}";
    fsout<<"]"<<"branches"<<"[";
    for(const auto&branch:branches)fsout<<"{"<<"id"<<branch.id<<"label"<<branch.label<<"parent_branch"<<branch.parent<<"fork_step"<<branch.fork_step<<"fork_reason"<<branch.reason<<"pitch"<<branch.pitch<<"final"<<branch.final<<"pinned"<<branch.pinned<<"settings_path"<<branch.settings_path<<"}";
    fsout<<"]"<<"nodes"<<"[";
    auto write_node=[&](const Stage&s,const std::string&branch){fsout<<"{"<<"id"<<branch+"/"+s.id<<"step_id"<<s.id<<"branch_id"<<branch<<"number"<<s.number<<"progress"<<s.progress<<"name"<<s.name<<"state"<<s.state<<"cache_key"<<s.cache_key<<"main"<<s.main<<"debug"<<s.debug<<"metadata"<<s.meta<<"sprite_dir"<<s.sprite_dir<<"started_at"<<s.started<<"elapsed_seconds"<<s.elapsed<<"}";};
    for(const auto&s:stages)write_node(s,"primary");
    for(const auto&branch:branches)for(const auto&s:branch.stages)write_node(s,branch.id);
    fsout<<"]"<<"artifacts"<<"[";
    /* agent: gpt-6.1-sol | 2026-10-06 | Tag final sheet artifacts by branch | v 2.3.0 | 78f6df */
    fs::path primary_final=path.parent_path().parent_path()/"final.pixel.png";
    if(fs::is_regular_file(primary_final))fsout<<"{"<<"path"<<primary_final.string()<<"role"<<"sheet_final"<<"branch_id"<<"primary"<<"progress"<<(stages.empty()?0:stages.back().progress)<<"}";
    for(const auto&artifact:retained_sprites)fsout<<"{"<<"path"<<artifact.string()<<"role"<<"sprite_export"<<"}";
    for(const auto&branch:branches)if(!branch.final.empty())fsout<<"{"<<"path"<<branch.final<<"role"<<"sheet_variant"<<"branch_id"<<branch.id<<"progress"<<(branch.stages.empty()?0:branch.stages.back().progress)<<"}";
    fsout<<"]";pp::publish_json(fsout,tmp,path.string());
}
/* agent: gpt-6.1-sol | 2026-10-06 | Preserve typed artifacts across failed runs | v 1.9.0 | 0ba0b3 */
/* Load fs::path manifest into Stage records; return std::vector<Stage>. */
/* Load reusable primary nodes only from the supported graph manifest; return stages. */
/* agent: gpt-6.1-sol | 2026-10-06 | Require graph manifests for cache reuse | v 2.3.0 | b7bc65 */
static std::vector<Stage> load_previous(const fs::path&path){std::vector<Stage>out;if(!fs::is_regular_file(path))return out;auto f=pp::read_json(path.string());if(!f.isOpened()||(int)f["schema_version"]!=2||(std::string)f["metadata_format"]!="pixelprocess-graph")return out;for(auto it=f["stages"].begin();it!=f["stages"].end();++it){Stage s;s.id=(std::string)(*it)["id"];s.name=(std::string)(*it)["name"];s.number=(int)(*it)["number"];s.progress=(int)(*it)["progress"];s.state=(std::string)(*it)["state"];s.cache_key=(std::string)(*it)["cache_key"];s.main=(std::string)(*it)["main"];s.debug=(std::string)(*it)["debug"];s.meta=(std::string)(*it)["metadata"];out.push_back(s);}return out;}
/* Load retained non-primary graph lanes from the schema-2 manifest; return branches. */
static std::vector<Branch> load_graph_branches(const fs::path&path){
    std::vector<Branch>out;if(!fs::is_regular_file(path))return out;auto f=pp::read_json(path.string());if(!f.isOpened())return out;
    for(auto item=f["branches"].begin();item!=f["branches"].end();++item){Branch branch;branch.id=(std::string)(*item)["id"];branch.label=(std::string)(*item)["label"];branch.parent=(std::string)(*item)["parent_branch"];branch.fork_step=(std::string)(*item)["fork_step"];branch.reason=(std::string)(*item)["fork_reason"];branch.pitch=(double)(*item)["pitch"];branch.final=(std::string)(*item)["final"];branch.settings_path=(std::string)(*item)["settings_path"];fs::path pin=path.parent_path()/"pixelprocess"/"pins"/(branch.id+".json");branch.pinned=fs::is_regular_file(pin);
        for(auto node=f["nodes"].begin();node!=f["nodes"].end();++node)if((std::string)(*node)["branch_id"]==branch.id){Stage stage;stage.id=(std::string)(*node)["step_id"];stage.name=(std::string)(*node)["name"];stage.number=(int)(*node)["number"];stage.progress=(int)(*node)["progress"];stage.state=(std::string)(*node)["state"];stage.cache_key=(std::string)(*node)["cache_key"];stage.main=(std::string)(*node)["main"];stage.debug=(std::string)(*node)["debug"];stage.meta=(std::string)(*node)["metadata"];stage.sprite_dir=(std::string)(*node)["sprite_dir"];stage.branch=branch.id;branch.stages.push_back(std::move(stage));}
        if(!branch.id.empty()&&!branch.stages.empty())out.push_back(std::move(branch));
    }
    return out;
}
/* Load retained branches from const fs::path& manifest; return std::vector<Branch>. */
static bool validate_artifacts(const Stage&s){return fs::is_regular_file(s.main)&&fs::is_regular_file(s.debug)&&fs::is_regular_file(s.meta);}
/* Read const std::string& stage metadata's sprite output directory; return std::string. */
static std::string sprite_directory(const std::string&metadata){auto f=pp::read_json(metadata);if(!f.isOpened())return{};return (std::string)f["artifacts"]["sprites_dir"];}
/* agent: gpt-6 | 2026-10-05 | Add source mapping to sprite records | v 1.5.0 | 9dbe3e */
/* Write source and final sprite hit regions plus source blob bounds; return bool. */
/* agent: gpt-6.1-sol | 2026-10-06 | Tag source sprite evidence by artifact | v 2.3.0 | 4c0f6c */
static bool write_sprite_records(const fs::path&path,const fs::path&source,const fs::path&owner_path,const fs::path&assemble_path,const fs::path&final_path,bool append=false,bool include_source=true){
    auto owner=pp::read_json(owner_path.string()),assembly=pp::read_json(assemble_path.string());cv::Mat src=pp::read_rgba(source.string()),final=pp::read_rgba(final_path.string());
    if(!owner.isOpened()||!assembly.isOpened()||src.empty()||final.empty())return false;
    std::ofstream out(path,append?std::ios::app:std::ios::trunc);if(!out)return false;std::string stamp=now_utc();
    if(include_source)for(auto it=owner["sprites"].begin();it!=owner["sprites"].end();++it){
        std::string id=(std::string)(*it)["id"];int row=(int)(*it)["row"],col=(int)(*it)["col"];std::vector<int>b;(*it)["source_bounds"]>>b;if(b.size()!=4||b[2]<=b[0]||b[3]<=b[1])continue;
        out<<"{\"stage_number\":100,\"artifact\":";json_quote(out,source.string());out<<",\"sprite_id\":";json_quote(out,id);out<<",\"row\":"<<row<<",\"col\":"<<col<<",\"artifact_bounds\":["<<b[0]<<","<<b[1]<<","<<b[2]-b[0]<<","<<b[3]-b[1]<<"],\"artifact_dimensions\":["<<src.cols<<","<<src.rows<<"],\"source_bounds\":["<<b[0]<<","<<b[1]<<","<<b[2]-b[0]<<","<<b[3]-b[1]<<"],\"source_dimensions\":["<<src.cols<<","<<src.rows<<"],\"timestamp_utc\":";json_quote(out,stamp);out<<",\"summary\":\"Source ownership bounds\",\"blobs\":[";
        bool first=true;for(auto ci=(*it)["components"].begin();ci!=(*it)["components"].end();++ci){if(!first)out<<',';first=false;std::string blob=(std::string)(*ci)["id"];std::vector<int>bb;(*ci)["bounds"]>>bb;if(bb.size()!=4)continue;out<<"{\"blob_id\":";json_quote(out,blob);out<<",\"artifact_bounds\":["<<bb[0]<<","<<bb[1]<<","<<bb[2]-bb[0]<<","<<bb[3]-bb[1]<<"],\"pixels\":"<<(int)(*ci)["pixels"]<<"}";}
        out<<"]}\n";
    }
    for(auto it=assembly["placements"].begin();it!=assembly["placements"].end();++it){std::string id=(std::string)(*it)["id"];int row=(int)(*it)["row"],col=(int)(*it)["col"];std::vector<int>b,sb;(*it)["destination_bounds"]>>b;for(auto si=owner["sprites"].begin();si!=owner["sprites"].end();++si)if((std::string)(*si)["id"]==id){(*si)["source_bounds"]>>sb;break;}if(b.size()!=4||sb.size()!=4||b[2]<=b[0]||b[3]<=b[1])continue;out<<"{\"stage_number\":0,\"artifact\":";json_quote(out,final_path.string());out<<",\"sprite_id\":";json_quote(out,id);out<<",\"row\":"<<row<<",\"col\":"<<col<<",\"artifact_bounds\":["<<b[0]<<","<<b[1]<<","<<b[2]-b[0]<<","<<b[3]-b[1]<<"],\"artifact_dimensions\":["<<final.cols<<","<<final.rows<<"],\"source_bounds\":["<<sb[0]<<","<<sb[1]<<","<<sb[2]-sb[0]<<","<<sb[3]-sb[1]<<"],\"source_dimensions\":["<<src.cols<<","<<src.rows<<"],\"timestamp_utc\":";json_quote(out,stamp);out<<",\"summary\":\"Final spritesheet placement\"}\n";}
    return out.good();
}
/* Append sprite bounds from const fs::path& journal/owner/source, const Stage&, and bool debug flag; return bool. */
static bool write_stage_sprite_records(const fs::path&path,const Stage&stage,const fs::path&owner_path,const fs::path&source_path,bool debug_image=false){
    auto meta=pp::read_json(stage.meta),owner=pp::read_json(owner_path.string());cv::Mat image=pp::read_rgba(debug_image?stage.debug:stage.main),source=pp::read_rgba(source_path.string());
    if(!meta.isOpened()||!owner.isOpened()||image.empty()||source.empty())return false;
    bool packed=(stage.id=="extract"&&!debug_image)||stage.id=="reconstruct"||stage.id=="cleanup_preview"||stage.id=="cleanup"||stage.id=="assemble";
    if(debug_image&&stage.id!="cleanup_preview"&&stage.id!="cleanup")packed=false;
    int tw=0,th=0;cv::FileNode dims=stage.id=="extract"?meta["preview_tile_dimensions"]:meta["tile_dimensions"];
    if(!dims.empty()&&dims.size()>=2){tw=(int)dims[0];th=(int)dims[1];}
    auto sprites=owner["sprites"];std::ofstream out(path,std::ios::app);if(!out)return false;std::string stamp=now_utc();
    for(auto it=sprites.begin();it!=sprites.end();++it){std::string id=(std::string)(*it)["id"];int row=(int)(*it)["row"],col=(int)(*it)["col"];std::vector<int>srcb;(*it)["source_bounds"]>>srcb;if(srcb.size()!=4)continue;
        int x=srcb[0],y=srcb[1],w=srcb[2]-srcb[0],h=srcb[3]-srcb[1];
        if(stage.id=="assemble"&&!debug_image){bool exists=false;for(auto pi=meta["placements"].begin();pi!=meta["placements"].end();++pi)if((std::string)(*pi)["id"]==id){std::vector<int>b;(*pi)["destination_bounds"]>>b;if(b.size()==4){exists=true;x=b[0];y=b[1];w=b[2]-b[0];h=b[3]-b[1];}break;}if(!exists)continue;}
        else if(packed){auto found=meta["sprites"];bool exists=false;for(auto si=found.begin();si!=found.end();++si)if((std::string)(*si)["id"]==id){exists=true;std::vector<int>d;(*si)["dimensions"]>>d;if(d.size()==2){w=d[0];h=d[1];}else{std::vector<int>b;(*si)["crop_bounds"]>>b;if(b.size()==4){w=b[2]-b[0];h=b[3]-b[1];}}if(tw<=0||th<=0){tw=w;th=h;}break;}if(!exists||tw<=0||th<=0||w<=0||h<=0)continue;x=col*tw+(tw-w)/2;y=row*th+(th-h)/2;}
        if(w<=0||h<=0||x<0||y<0||x+w>image.cols||y+h>image.rows)continue;
        out<<"{\"stage_number\":"<<stage.number<<",\"artifact\":";json_quote(out,debug_image?stage.debug:stage.main);out<<",\"sprite_id\":";json_quote(out,id);out<<",\"row\":"<<row<<",\"col\":"<<col<<",\"artifact_bounds\":["<<x<<","<<y<<","<<w<<","<<h<<"],\"artifact_dimensions\":["<<image.cols<<","<<image.rows<<"],\"source_bounds\":["<<srcb[0]<<","<<srcb[1]<<","<<srcb[2]-srcb[0]<<","<<srcb[3]-srcb[1]<<"],\"source_dimensions\":["<<source.cols<<","<<source.rows<<"],\"timestamp_utc\":";json_quote(out,stamp);out<<",\"summary\":";json_quote(out,debug_image?stage.name+" debug":stage.name);out<<"}\n";
    }
    return out.good();
}
/* agent: gpt-6 | 2026-10-05 | Add source mapping to sprite records | v 1.5.0 | 9dbe3e */
static bool mirror_artifacts(const Stage&s,const fs::path&extra){std::string prefix="stage-"+stage_number(s.number)+"-"+s.id;return copy_atomic(s.main,extra/(prefix+".png"))&&copy_atomic(s.debug,extra/("stage-debug-"+stage_number(s.number)+"-"+s.id+".png"));}
static bool copy_tree(const fs::path&from,const fs::path&to){std::error_code ec;if(!fs::exists(from))return false;fs::create_directories(to,ec);if(ec)return false;for(fs::recursive_directory_iterator i(from,ec),end;i!=end&&!ec;i.increment(ec)){if(!i->is_regular_file())continue;auto relative=i->path().lexically_relative(from);if(!copy_atomic(i->path(),to/relative))return false;}return !ec;}

/* Recompute ownership and diagnosis for draft counts without downstream stages; return int status. */
static int preview_grid(const pp::Args&a,const fs::path&bin){
    for(const char*key:{"input","mask","settings","output","meta","work-dir","grid_rows","grid_cols"})if(!a.has(key))return pp::input_error(std::string("missing --")+key);
    fs::path work=fs::absolute(a.get("work-dir"));std::error_code ec;fs::create_directories(work,ec);if(ec)return pp::input_error("cannot create grid preview directory");
    fs::path owner=work/"ownership.json",owner_png=work/"ownership.png",owner_debug=work/"ownership.debug.png",diagnosis_debug=work/"diagnosis.debug.png";
    std::vector<std::string>own={"--input",fs::absolute(a.get("input")).string(),"--mask",fs::absolute(a.get("mask")).string(),"--output",owner_png.string(),"--debug",owner_debug.string(),"--meta",owner.string(),"--settings",fs::absolute(a.get("settings")).string(),"--grid_rows",a.get("grid_rows"),"--grid_cols",a.get("grid_cols")};
    if(!run_child(bin/"stage_ownership",own))return pp::input_error("grid ownership preview failed");
    std::vector<std::string>diag={"--input",fs::absolute(a.get("input")).string(),"--state",owner.string(),"--output",fs::absolute(a.get("output")).string(),"--debug",diagnosis_debug.string(),"--meta",fs::absolute(a.get("meta")).string()};
    if(!run_child(bin/"stage_diagnosis",diag))return pp::input_error("grid diagnosis preview failed");
    return 0;
}

/* Run the lattice fitter against the selected branch's extraction artifacts; return int status. */
static int preview_lattice(const pp::Args&a,const fs::path&bin){
    for(const char*key:{"input","mask","state","sprite-dir","settings","output","debug","meta"})if(!a.has(key))return pp::input_error(std::string("missing --")+key);
    std::vector<std::string>args={"--input",fs::absolute(a.get("input")).string(),"--mask",fs::absolute(a.get("mask")).string(),"--state",fs::absolute(a.get("state")).string(),"--sprite-dir",fs::absolute(a.get("sprite-dir")).string(),"--settings",fs::absolute(a.get("settings")).string(),"--output",fs::absolute(a.get("output")).string(),"--debug",fs::absolute(a.get("debug")).string(),"--meta",fs::absolute(a.get("meta")).string()};
    auto settings=pp::read_json(a.get("settings"));if(setting(settings,"pitch","auto")=="auto"){
        bool has_auto=false;double max_ratio=1.25;if(!settings["alternate_pitch_max_percent"].empty())max_ratio=std::clamp((double)settings["alternate_pitch_max_percent"]/100.0,1.0,4.0);
        auto candidates=shared_pitch_candidates(a.get("state"),settings,has_auto,max_ratio);if(has_auto&&candidates.empty())return pp::input_error("no shared lattice pitch meets minimum logical sprite size");
        if(!candidates.empty()){args.push_back("--sheet-pitch");args.push_back(std::to_string(candidates[0]));}
    }
    if(!run_child(bin/"stage_lattice",args))return pp::input_error("lattice preview failed");
    return 0;
}

/* Process CLI argc and argv, run stages, and return int status. */
/* agent: gpt-6.1-sol | 2026-10-05 | Launch source-named stage executables | v 1.4.0 | 05e28c */
int main(int argc,char**argv){
    auto a=pp::parse_args(argc,argv);fs::path bin=fs::canonical("/proc/self/exe").parent_path();if(a.get("preview-grid")=="1")return preview_grid(a,bin);if(a.get("preview-lattice")=="1")return preview_lattice(a,bin);if(!a.has("input")||!a.has("run-dir"))return pp::input_error("usage: runner --input IMAGE --run-dir DIR [--settings JSON] [--target STAGE]");
    const std::string save_mode=a.get("save-mode"),selected_branch=a.get("branch-id","primary");
    const bool fresh=a.get("fresh")=="1",pinned_replay=a.get("pinned-replay")=="1",scope_replay=a.get("branch-scope-replay")=="1";
    const bool setup_save=save_mode=="replace"||save_mode=="copy";
    if(!save_mode.empty()&&!setup_save)return pp::input_error("unknown setup save mode");
    if(setup_save&&a.get("target")!="ownership"&&a.get("target")!="lattice")return pp::input_error("setup save requires ownership or lattice target");
    fs::path source=fs::absolute(a.get("input")),run=fs::absolute(a.get("run-dir"));if(pp::read_rgba(source.string()).empty())return pp::input_error("input image is unreadable or unsupported");
    fs::path extra=run/"extra",root=extra/"pixelprocess",manifest=extra/"pixelprocess.manifest.json",journal=extra/"processing.events.jsonl";std::error_code ec;fs::create_directories(root,ec);if(ec)return pp::input_error("cannot create run directory");
    int revision=1;auto old=load_previous(manifest);auto primary_old=old;std::vector<Branch>branches=load_graph_branches(manifest);std::vector<std::string>existing_branch_ids;for(const auto&branch:branches)existing_branch_ids.push_back(branch.id);if(fs::exists(manifest)){auto f=pp::read_json(manifest.string());revision=(int)f["revision"]+1;if(revision<1)revision=1;}
    fs::path selected_pin_path=extra/"pixelprocess"/"pins"/(selected_branch+".json");const bool selected_is_pinned=fs::is_regular_file(selected_pin_path);
    const bool branch_mode=(selected_branch!="primary"&&(!setup_save||save_mode=="replace"))||pinned_replay;
    if(selected_branch!="primary"){
        /* Overlay a branch lineage iteratively so deep copy chains remain loadable. */
        std::vector<const Branch*> lineage;std::string cursor=selected_branch;
        while(cursor!="primary"){
            auto found=std::find_if(branches.begin(),branches.end(),[&](const Branch&branch){return branch.id==cursor;});
            if(found==branches.end()||std::find_if(lineage.begin(),lineage.end(),[&](const Branch*branch){return branch->id==cursor;})!=lineage.end())return pp::input_error("selected branch ancestry is missing or cyclic");
            lineage.push_back(&*found);cursor=found->parent;
            if(cursor.empty())return pp::input_error("selected branch has no primary ancestry");
        }
        old=primary_old;
        for(auto item=lineage.rbegin();item!=lineage.rend();++item)for(const auto&stage:(*item)->stages){
            auto prior=std::find_if(old.begin(),old.end(),[&](const Stage&candidate){return candidate.id==stage.id;});
            if(prior==old.end())old.push_back(stage);else *prior=stage;
        }
        if(old.empty())return pp::input_error("selected branch has no reusable pipeline stages");
    }
    const std::string output_branch=branch_mode?selected_branch:"primary";
    fs::path rev=root/("revision-"+stage_number(revision));fs::create_directories(rev,ec);fs::create_directories(extra/"debug",ec);
    fs::path settings_path=rev/"effective-settings.json";
    fs::path pin_dir=extra/"pixelprocess"/"pins";fs::path selected_pin=pin_dir/(selected_branch+".json");
    fs::path requested_settings=a.has("settings")?fs::absolute(a.get("settings")):fs::path{};
    if(save_mode.empty()&&fs::is_regular_file(selected_pin))requested_settings=selected_pin;
    if(!requested_settings.empty()){if(!copy_atomic(requested_settings,settings_path))return pp::input_error("cannot snapshot settings file");}
    else {std::ofstream f(settings_path);f<<"{\"schema_version\":1,\"background_tolerance\":24,\"grid_cols\":0,\"grid_rows\":0,\"extraction_padding\":1,\"pitch\":\"auto\",\"min_pitch\":2,\"max_pitch\":16,\"pitch_precision\":0.1,\"foreground_coverage\":0.35,\"min_sprite_cells\":16,\"sprite_pitch_tolerance\":5,\"alternate_pitch_max_percent\":125,\"cleanup_mode\":\"outside\",\"cleanup_tolerance\":55,\"sheet_anchor\":\"center\",\"sheet_padding\":1}";}
    auto config=pp::read_json(settings_path.string());if(!config.isOpened())return pp::input_error("settings must be valid JSON");
    /* Preserve the prior graph only when effective processing settings changed. */
    /* agent: gpt-6.1-sol | 2026-10-06 | Avoid duplicate rerun branches | v 2.5.0 | 963a18 */
    if(!old.empty()){
        std::string previous_settings_path;
        if(fs::exists(manifest)){auto previous_manifest=pp::read_json(manifest.string());previous_settings_path=(std::string)previous_manifest["settings_path"];}
        auto previous_config=pp::read_json(previous_settings_path);
        const std::vector<std::string> all_settings={"background_tolerance","grid_cols","grid_rows","extraction_padding","cleanup_mode","pitch","phase_x","phase_y","min_pitch","max_pitch","pitch_precision","foreground_coverage","min_sprite_cells","sprite_pitch_tolerance","alternate_pitch_max_percent","cleanup_tolerance","sheet_anchor","sheet_padding","sprite_overrides","ownership_overrides"};
        bool changed=!previous_config.isOpened()||settings_signature(previous_config,all_settings)!=settings_signature(config,all_settings);
        if(changed&&!fresh&&!branch_mode&&save_mode.empty()&&selected_branch=="primary"){
            Branch prior;prior.id="settings-r"+stage_number(revision-1);prior.label="Previous settings";prior.parent="primary";prior.reason="previous_settings_result";prior.fork_step="background";
            for(const auto&stage:old)if(stage.id!="source"){Stage copy=stage;copy.branch=prior.id;copy.sprite_dir=sprite_directory(copy.meta);prior.stages.push_back(copy);if(stage.id=="assemble")prior.final=stage.main;}
            if(!prior.stages.empty()){
                fs::path preserved=root/("revision-"+stage_number(revision-1))/"retained-final.pixel.png";
                if(!prior.final.empty()&&!copy_atomic(prior.final,preserved))return pp::input_error("could not preserve previous final spritesheet");
                if(!prior.final.empty())prior.final=preserved.string();
                branches.push_back(std::move(prior));
            }
        }
    }
    int seq=0;{std::ifstream f(journal);std::string line;while(std::getline(f,line))if(line.find("\"sequence\"")!=std::string::npos)++seq;}
    signal(SIGINT,on_signal);signal(SIGTERM,on_signal);
    fs::path source_copy=run/"source.png";if(source_copy!=source&&!fs::exists(source_copy))copy_atomic(source,source_copy);fs::path active_source=fs::exists(source_copy)?source_copy:source;
    Stage source_stage{"source","Source", "",active_source.string(),active_source.string(),"", "completed",100};
    std::vector<Stage> records{source_stage};event(journal,seq,"artifact","source",100,"immutable source ready",output_branch,0);
    Branch working_branch;working_branch.id=output_branch;working_branch.label="Revision from "+selected_branch;working_branch.parent=selected_branch;working_branch.fork_step=a.get("target","all");working_branch.reason="settings_revision";
    if(branch_mode){auto existing=std::find_if(branches.begin(),branches.end(),[&](const Branch&branch){return branch.id==output_branch;});if(existing!=branches.end())working_branch=*existing;}
    fs::path branch_final=extra/("final.pixel."+output_branch+".png");
    /* Publish primary or branch snapshots while keeping the selected base immutable. */
    auto manifest_snapshot=[&](const std::string&state,const std::vector<fs::path>&sprites,const std::vector<fs::path>&variants){
        auto graph_branches=branches;
        if(branch_mode){working_branch.stages.clear();for(const auto&stage:records)if(stage.id!="source")working_branch.stages.push_back(stage);if(state=="completed"&&fs::is_regular_file(branch_final))working_branch.final=branch_final.string();working_branch.settings_path=settings_path.string();auto existing=std::find_if(graph_branches.begin(),graph_branches.end(),[&](const Branch&branch){return branch.id==working_branch.id;});if(existing!=graph_branches.end()){working_branch.pinned=existing->pinned;*existing=working_branch;}else graph_branches.push_back(working_branch);}
        write_manifest(manifest,active_source,state,revision,settings_path.string(),branch_mode?primary_old:records,sprites,variants,graph_branches);
    };
    const std::string cleanup=setting(config,"cleanup_mode","outside");const std::string tolerance=setting(config,"background_tolerance","24");const std::string gridc=setting(config,"grid_cols","0"),gridr=setting(config,"grid_rows","0"),pad=setting(config,"extraction_padding","1");
    const std::string pitch=setting(config,"pitch","auto"),minp=setting(config,"min_pitch","2"),maxp=setting(config,"max_pitch","16"),precision=setting(config,"pitch_precision","0.1"),coverage=setting(config,"foreground_coverage","0.35"),min_cells=setting(config,"min_sprite_cells","16"),pitch_tolerance=setting(config,"sprite_pitch_tolerance","5"),cleantol=setting(config,"cleanup_tolerance","55"),anchor=setting(config,"sheet_anchor","center"),sheetpad=setting(config,"sheet_padding","1");
    fs::path mask=rev/"background-mask.png",owner_meta=rev/"ownership.json",extract_meta=rev/"extraction.json",lattice_meta=rev/"lattice.json",recon_meta=rev/"reconstruction.json",clean_meta=rev/"cleanup.json";
    fs::path crops=rev/"crops",logical=rev/"logical-sprites",clean_sprites=rev/"clean-sprites";
    struct Plan{const char*id;const char*name;int n;std::vector<fs::path>inputs;std::string key;std::vector<std::string>args;};std::vector<Plan>plans;
    auto mk=[&](const char*id,const char*name,int n,std::vector<fs::path>inputs,std::string keys,std::vector<std::string>args){plans.push_back({id,name,n,std::move(inputs),std::move(keys),std::move(args)});};
    fs::path bgmain=rev/"background.png",bgdebug=rev/"background.debug.png",bgmeta=rev/"background.stage.json";
    mk("background","Background mask",97,{active_source},"background_tolerance",{"--input",active_source.string(),"--output",bgmain.string(),"--debug",bgdebug.string(),"--mask",mask.string(),"--meta",bgmeta.string(),"--tolerance",tolerance});
    fs::path ownmain=rev/"ownership.png",owndebug=rev/"ownership.debug.png";
    mk("ownership","Blob ownership",94,{active_source,mask},"background_tolerance,grid_cols,grid_rows,ownership_overrides",{"--input",active_source.string(),"--mask",mask.string(),"--output",ownmain.string(),"--debug",owndebug.string(),"--meta",owner_meta.string(),"--grid_cols",gridc,"--grid_rows",gridr,"--settings",settings_path.string()});
    fs::path diagmain=rev/"diagnosis.png",diagdebug=rev/"diagnosis.debug.png",diagmeta=rev/"diagnosis.json";
    mk("diagnosis","Grid diagnosis",93,{active_source,owner_meta},"grid_cols,grid_rows,ownership_overrides",{"--input",active_source.string(),"--state",owner_meta.string(),"--output",diagmain.string(),"--debug",diagdebug.string(),"--meta",diagmeta.string()});
    fs::path exmain=rev/"extraction.png",exdebug=rev/"extraction.debug.png";
    mk("extract","Sprite extraction",91,{active_source,mask,owner_meta},"background_tolerance,grid_cols,grid_rows,extraction_padding,cleanup_mode,ownership_overrides",{"--input",active_source.string(),"--mask",mask.string(),"--state",owner_meta.string(),"--output",exmain.string(),"--debug",exdebug.string(),"--meta",extract_meta.string(),"--sprite-dir",crops.string(),"--padding",pad,"--settings",settings_path.string()});
    fs::path latmain=rev/"lattice.png",latdebug=rev/"lattice.debug.png";
    mk("lattice","Lattice fitting",80,{active_source,mask,extract_meta,crops},"background_tolerance,grid_cols,grid_rows,extraction_padding,cleanup_mode,pitch,phase_x,phase_y,min_pitch,max_pitch,pitch_precision,min_sprite_cells,sprite_pitch_tolerance,alternate_pitch_max_percent,sprite_overrides,ownership_overrides",{"--input",active_source.string(),"--mask",mask.string(),"--state",extract_meta.string(),"--output",latmain.string(),"--debug",latdebug.string(),"--meta",lattice_meta.string(),"--sprite-dir",crops.string(),"--settings",settings_path.string()});
    fs::path recmain=rev/"reconstruction.png",recdebug=rev/"reconstruction.debug.png";
    mk("reconstruct","Pixel reconstruction",60,{active_source,mask,extract_meta,lattice_meta,crops},"background_tolerance,grid_cols,grid_rows,extraction_padding,cleanup_mode,pitch,phase_x,phase_y,min_pitch,max_pitch,pitch_precision,min_sprite_cells,sprite_pitch_tolerance,foreground_coverage,sprite_overrides,ownership_overrides",{"--input",active_source.string(),"--mask",mask.string(),"--state",extract_meta.string(),"--owner",owner_meta.string(),"--lattice",lattice_meta.string(),"--output",recmain.string(),"--debug",recdebug.string(),"--meta",recon_meta.string(),"--sprite-dir",logical.string(),"--coverage",coverage,"--cleanup-mode",cleanup});
    /* agent: gpt-6.1-sol | 2026-10-06 | Add cleanup preview stage | v 1.7.0 | 590ae6 */
    fs::path previewmain=rev/"cleanup-preview.png",previewdebug=rev/"cleanup-preview.debug.png",previewmeta=rev/"cleanup-preview.stage.json",preview_sprites=rev/"cleanup-preview-sprites";
    /* agent: gpt-6.1-sol | 2026-10-06 | Match preview cleanup settings | v 2.4.0 | 387abe */
    mk("cleanup_preview","Pre-cleanup magenta preview",24,{recon_meta,logical},"cleanup_mode,cleanup_tolerance,foreground_coverage,pitch,phase_x,phase_y,sprite_overrides",{"--state",recon_meta.string(),"--output",previewmain.string(),"--debug",previewdebug.string(),"--meta",previewmeta.string(),"--sprite-dir",preview_sprites.string(),"--mode",cleanup,"--tolerance",cleantol});
    /* agent: gpt-6.1-sol | 2026-10-06 | Match preview cleanup settings | v 2.4.0 | 387abe */
    /* agent: gpt-6.1-sol | 2026-10-06 | Add cleanup preview stage | v 1.7.0 | 590ae6 */
    fs::path clmain=rev/"cleanup.png",cldebug=rev/"cleanup.debug.png";
    mk("cleanup","Magenta cleanup",23,{recon_meta,logical},"background_tolerance,grid_cols,grid_rows,extraction_padding,cleanup_mode,pitch,phase_x,phase_y,min_pitch,max_pitch,pitch_precision,min_sprite_cells,sprite_pitch_tolerance,foreground_coverage,cleanup_tolerance,sprite_overrides,ownership_overrides",{"--state",recon_meta.string(),"--output",clmain.string(),"--debug",cldebug.string(),"--meta",clean_meta.string(),"--sprite-dir",clean_sprites.string(),"--mode",cleanup,"--tolerance",cleantol});
    fs::path final=rev/"final.pixel.png",finaldebug=rev/"assemble.debug.png",finalmeta=rev/"assemble.json";
    mk("assemble","Spritesheet assembly",4,{active_source,extract_meta,clean_meta,clean_sprites},"background_tolerance,grid_cols,grid_rows,extraction_padding,cleanup_mode,pitch,phase_x,phase_y,min_pitch,max_pitch,pitch_precision,foreground_coverage,cleanup_tolerance,sprite_overrides,ownership_overrides,sheet_anchor,sheet_padding",{"--input",active_source.string(),"--state",extract_meta.string(),"--sprites",clean_meta.string(),"--output",final.string(),"--debug",finaldebug.string(),"--meta",finalmeta.string(),"--padding",sheetpad,"--anchor",anchor});
    auto previous=[&](const std::string&id)->Stage*{for(auto&s:old)if(s.id==id)return &s;return nullptr;};
    int force_index=-1;std::string target=a.get("target","all");for(size_t i=0;i<plans.size();i++)if(target==plans[i].id||target==std::to_string(plans[i].n))force_index=(int)i;if(fresh)force_index=0;
    if(target!="all"&&force_index<0)return pp::input_error("unknown --target stage");
    if(pinned_replay)force_index=0;
    if((!setup_save||save_mode=="replace")&&force_index>=0&&selected_branch=="primary"&&!branch_mode){
        Branch prior;prior.id="primary-before-r"+stage_number(revision-1);prior.label="Primary before rerun";prior.parent="primary";prior.reason="previous_primary_result";prior.fork_step=plans[(size_t)force_index].id;
            for(const auto&stage:old)if(stage.progress>=force_index+1){Stage node=stage;node.branch=prior.id;node.sprite_dir=sprite_directory(node.meta);prior.stages.push_back(node);if(stage.id=="assemble")prior.final=stage.main;}
        if(!prior.final.empty()){fs::path preserved=root/("revision-"+stage_number(revision-1))/"retained-primary-final.pixel.png";if(!copy_atomic(prior.final,preserved))return pp::input_error("could not preserve targeted rerun final");prior.final=preserved.string();}
        if(!prior.stages.empty())branches.push_back(std::move(prior));
    }
    for(const auto&branch:branches)for(const auto&node:branch.stages)event(journal,seq,"node_retained",node.id,node.number,"previous graph output retained",branch.id,node.progress);
    std::string upstream_signature;double alternate_pitch=0;
    for(size_t i=0;i<plans.size();i++){
        auto &p=plans[i];fs::path exe=bin/(std::string("stage_")+p.id);Stage result;result.id=p.id;result.name=p.name;result.number=p.n;result.progress=(int)i+1;result.branch=output_branch;result.started=(double)std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::vector<std::string> input_paths;for(auto&input:p.inputs)input_paths.push_back(input.string());input_paths.push_back(active_source.string());
        std::vector<std::string> relevant;std::stringstream key_stream(p.key);std::string key;while(std::getline(key_stream,key,','))if(!key.empty())relevant.push_back(key);
        result.cache_key=signature(Spec{p.id,p.name,p.n,input_paths,{},settings_signature(config,relevant)+"|"+upstream_signature},exe);
        upstream_signature+=std::string(p.id)+":"+result.cache_key+";";
        Stage*old_stage=previous(p.id);bool force=force_index>=0&&(int)i>=force_index;bool can_reuse=!force&&old_stage&&old_stage->cache_key==result.cache_key&&validate_artifacts(*old_stage);if(p.id==std::string("lattice")&&pitch=="auto")can_reuse=false;
        if(interrupted){result.state="skipped";records.push_back(result);if(!setup_save&&!pinned_replay&&!branch_mode)manifest_snapshot("canceled",{},{});event(journal,seq,"canceled",p.id,p.n,"run canceled before stage",output_branch,result.progress);return 130;}
        if(can_reuse){
            result=*old_stage;result.started=(double)std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();result.elapsed=0;
            fs::path expected_main,expected_debug,expected_meta;
            for(size_t k=0;k+1<p.args.size();k++){if(p.args[k]=="--output")expected_main=p.args[k+1];else if(p.args[k]=="--debug")expected_debug=p.args[k+1];else if(p.args[k]=="--meta")expected_meta=p.args[k+1];}
            if(!expected_main.empty()&&!copy_atomic(result.main,expected_main))return pp::input_error("could not reuse main artifact");
            if(!expected_debug.empty()&&!copy_atomic(result.debug,expected_debug))return pp::input_error("could not reuse debug artifact");
            if(!expected_meta.empty()&&!copy_atomic(result.meta,expected_meta))return pp::input_error("could not reuse stage metadata");
            if(p.id=="background"){fs::path old_mask=fs::path(old_stage->meta).parent_path()/"background-mask.png";if(!copy_atomic(old_mask,mask))return pp::input_error("could not reuse background mask");}
            if(p.id=="extract"){fs::path old_crops=fs::path(old_stage->meta).parent_path()/"crops";if(!copy_tree(old_crops,crops))return pp::input_error("could not reuse extracted sprites");}
            if(p.id=="reconstruct"){fs::path old_sprites=fs::path(old_stage->meta).parent_path()/"logical-sprites";if(!copy_tree(old_sprites,logical))return pp::input_error("could not reuse logical sprites");}
            if(p.id=="cleanup"){fs::path old_sprites=fs::path(old_stage->meta).parent_path()/"clean-sprites";if(!copy_tree(old_sprites,clean_sprites))return pp::input_error("could not reuse cleaned sprites");}
            if(!expected_main.empty())result.main=expected_main.string();if(!expected_debug.empty())result.debug=expected_debug.string();if(!expected_meta.empty())result.meta=expected_meta.string();
            std::cout<<"stage "<<p.id<<" reused cache="<<result.cache_key<<'\n';event(journal,seq,"stage_reused",p.id,p.n,"matching inputs, settings, and executable",output_branch,result.progress);
        }
        else{
            result.main=(rev/("stage-"+stage_number(p.n)+"-"+p.id+".png")).string();result.debug=(rev/("stage-"+stage_number(p.n)+"-"+p.id+".debug.png")).string();
            // Every stage writes through its explicit --output/--debug arguments above.
            if(p.id==std::string("background")){p.args[3]=result.main;p.args[5]=result.debug;}
            if(p.id==std::string("ownership")){p.args[5]=result.main;p.args[7]=result.debug;}
            if(p.id==std::string("diagnosis")){p.args[5]=result.main;p.args[7]=result.debug;}
            if(p.id==std::string("extract")){p.args[7]=result.main;p.args[9]=result.debug;}
            if(p.id==std::string("lattice")){p.args[7]=result.main;p.args[9]=result.debug;}
            if(p.id==std::string("reconstruct")){p.args[11]=result.main;p.args[13]=result.debug;}
            /* agent: gpt-6.1-sol | 2026-10-06 | Redirect cleanup preview outputs | v 1.9.0 | a8f552 */
            if(p.id==std::string("cleanup")||p.id==std::string("cleanup_preview")){p.args[3]=result.main;p.args[5]=result.debug;}
            if(p.id==std::string("assemble")){p.args[7]=result.main;p.args[9]=result.debug;}
            for(size_t j=0;j+1<p.args.size();j++)if(p.args[j]=="--meta")result.meta=p.args[j+1];
            result.state="running";records.push_back(result);event(journal,seq,"stage_started",p.id,p.n,"stage process started",output_branch,result.progress);if(!setup_save&&!pinned_replay&&!branch_mode)manifest_snapshot("running",{},{});
            auto t0=std::chrono::steady_clock::now();if(!fs::exists(exe)||!run_child(exe,p.args)){result.state=interrupted?"canceled":"failed";result.elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();records.back()=result;if(!setup_save&&!pinned_replay&&!branch_mode)manifest_snapshot(result.state,{},{});event(journal,seq,interrupted?"canceled":"stage_failed",p.id,p.n,"stage exited without a complete result",output_branch,result.progress);return interrupted?130:1;}
            result.elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();result.state="completed";
            {auto metadata=pp::read_json(result.meta);if(metadata.isOpened()&&!metadata["state"].empty())result.state=(std::string)metadata["state"];}
            if(!validate_artifacts(result)){result.state="failed";records.back()=result;if(!setup_save&&!pinned_replay&&!branch_mode)manifest_snapshot("failed",{},{});event(journal,seq,"stage_failed",p.id,p.n,"required stage artifacts missing",output_branch,result.progress);return 1;}
            /* agent: gpt-6.1-sol | 2026-10-06 | Add shared scale variants | v 1.8.0 | f639df */
            if(p.id==std::string("lattice")&&pitch=="auto"){
                bool has_auto_sprite=false;double alternate_max=1.25;cv::FileNode alternate_setting=config["alternate_pitch_max_percent"];if(!alternate_setting.empty())alternate_max=std::clamp((double)alternate_setting/100.0,1.0,4.0);auto candidates=shared_pitch_candidates(result.meta,config,has_auto_sprite,alternate_max);
                if(has_auto_sprite&&candidates.empty()){
                    result.state="failed";records.back()=result;if(!pinned_replay&&!branch_mode&&!setup_save)manifest_snapshot("failed",{},{});
                    event(journal,seq,"stage_failed",p.id,p.n,"no shared pitch meets minimum logical sprite size",output_branch,result.progress);
                    return pp::input_error("no shared lattice pitch meets the minimum logical sprite size; lower min_sprite_cells or set pitch manually");
                }
                if(!candidates.empty()){
                    if(candidates.size()>1)alternate_pitch=candidates[1];
                    fs::path diagnostic=rev/"lattice-diagnostics.json";if(!copy_atomic(result.meta,diagnostic))return pp::input_error("could not preserve lattice diagnostics");
                    auto shared_args=p.args;shared_args.push_back("--sheet-pitch");shared_args.push_back(std::to_string(candidates[0]));
                    if(!run_child(exe,shared_args)){result.state="failed";records.back()=result;if(!pinned_replay&&!branch_mode&&!setup_save)manifest_snapshot("failed",{},{});event(journal,seq,"stage_failed",p.id,p.n,"shared lattice pitch could not be fitted",output_branch,result.progress);return pp::input_error("could not fit the primary shared lattice");}
                    result.cache_key+="|shared_pitch="+std::to_string(candidates[0]);upstream_signature+="sheet_pitch="+std::to_string(candidates[0])+";";
                    event(journal,seq,"lattice_candidates","lattice",80,"ranked shared scales; primary lattice fitted for every sprite",output_branch,result.progress);
                }
            }
            event(journal,seq,"artifact",p.id,p.n,"main, debug, and metadata published",output_branch,result.progress);records.pop_back();
        }
        result.branch=output_branch;result.sprite_dir=sprite_directory(result.meta);records.push_back(result);if(!setup_save&&!pinned_replay&&!branch_mode)manifest_snapshot("running",{},{});
        if(!branch_mode&&!mirror_artifacts(result,extra))return pp::input_error("could not publish stage artifact aliases");
    }
    Stage*sheet=previous("assemble");(void)sheet;std::string final_path=records.back().main;std::vector<fs::path>sheet_variants;
    fs::path published_final=branch_mode?branch_final:run/"final.pixel.png";
    if(save_mode=="copy"){Branch copy;copy.id=(a.get("target")=="ownership"?"grid-copy-r":"lattice-copy-r")+stage_number(revision);copy.label="Pinned setup copy";copy.parent=selected_branch;copy.fork_step=a.get("target");copy.reason=a.get("target")=="ownership"?"manual_grid_copy":"manual_lattice_copy";copy.pinned=true;fs::create_directories(pin_dir,ec);copy.settings_path=settings_path.string();for(const auto&stage:records)if(stage.id!="source"){Stage node=stage;node.branch=copy.id;copy.stages.push_back(node);}published_final=extra/("final.pixel."+copy.id+".png");copy.final=published_final.string();branches.push_back(std::move(copy));}
    if(!copy_atomic(final_path,published_final))return pp::input_error("could not publish final spritesheet");
    if(!copy_atomic(records.front().main,extra/"stage-100-source.png"))return pp::input_error("could not publish source stage");
    /* agent: gpt-6.1-sol | 2026-10-06 | Add shared scale variants | v 1.8.0 | f639df */
    if(alternate_pitch>0&&pitch=="auto"){
/* agent: gpt-6 | 2026-10-06 | Add grid diagnosis stage | v 2.7.0 | 802275 */
        Branch alt;alt.id="pitch-"+std::to_string((int)std::lround(alternate_pitch*100))+"-r"+stage_number(revision);alt.label="Pitch "+cv::format("%.2f px/cell",alternate_pitch);alt.parent=branch_mode?selected_branch:"primary";alt.fork_step="lattice";alt.reason="ranked_shared_pitch";alt.pitch=alternate_pitch;
        /* agent: gpt-6.1-sol | 2026-10-06 | Keep scale branch IDs stable | v 2.6.0 | 2835bc */
        fs::path branch=rev/"scale-alternative";fs::create_directories(branch,ec);fs::path latmeta=branch/"lattice.json",reconmeta=branch/"reconstruction.json",previewmeta=branch/"cleanup-preview.stage.json",cleanupmeta=branch/"cleanup.json",assemblem=branch/"assemble.json";
        fs::path latticeimg=branch/"lattice.png",latticedbg=branch/"lattice.debug.png",reconimg=branch/"reconstruction.png",recondbg=branch/"reconstruction.debug.png",recondir=branch/"logical-sprites";
        fs::path previewimg=branch/"cleanup-preview.png",previewdbg=branch/"cleanup-preview.debug.png",previewdir=branch/"cleanup-preview-sprites";
        fs::path cleanimg=branch/"cleanup.png",cleandbg=branch/"cleanup.debug.png",cleandir=branch/"clean-sprites",altfinal=branch/"final.pixel.png",altdebug=branch/"assemble.debug.png";
        std::string altpitch=std::to_string(alternate_pitch);
        std::vector<std::string>latargs={"--input",active_source.string(),"--mask",mask.string(),"--state",extract_meta.string(),"--output",latticeimg.string(),"--debug",latticedbg.string(),"--meta",latmeta.string(),"--sprite-dir",crops.string(),"--settings",settings_path.string(),"--sheet-pitch",altpitch};
        std::vector<std::string>recargs={"--input",active_source.string(),"--mask",mask.string(),"--state",extract_meta.string(),"--owner",owner_meta.string(),"--lattice",latmeta.string(),"--output",reconimg.string(),"--debug",recondbg.string(),"--meta",reconmeta.string(),"--sprite-dir",recondir.string(),"--coverage",coverage,"--cleanup-mode",cleanup};
        /* agent: gpt-6.1-sol | 2026-10-06 | Match preview cleanup settings | v 2.4.0 | 387abe */
        std::vector<std::string>previewargs={"--state",reconmeta.string(),"--output",previewimg.string(),"--debug",previewdbg.string(),"--meta",previewmeta.string(),"--sprite-dir",previewdir.string(),"--mode",cleanup,"--tolerance",cleantol};
        /* agent: gpt-6.1-sol | 2026-10-06 | Match preview cleanup settings | v 2.4.0 | 387abe */
        std::vector<std::string>clargs={"--state",reconmeta.string(),"--output",cleanimg.string(),"--debug",cleandbg.string(),"--meta",cleanupmeta.string(),"--sprite-dir",cleandir.string(),"--mode",cleanup,"--tolerance",cleantol};
        std::vector<std::string>asargs={"--input",active_source.string(),"--state",extract_meta.string(),"--sprites",cleanupmeta.string(),"--output",altfinal.string(),"--debug",altdebug.string(),"--meta",assemblem.string(),"--padding",sheetpad,"--anchor",anchor};
        std::vector<std::tuple<std::string,std::string,int,fs::path,fs::path,fs::path,std::vector<std::string>>> branch_steps={
            {"lattice","Lattice fitting",80,latmeta,latticeimg,latticedbg,latargs},
            {"reconstruct","Pixel reconstruction",60,reconmeta,reconimg,recondbg,recargs},
            {"cleanup_preview","Pre-cleanup magenta preview",24,previewmeta,previewimg,previewdbg,previewargs},
            {"cleanup","Magenta cleanup",23,cleanupmeta,cleanimg,cleandbg,clargs},
            {"assemble","Spritesheet assembly",4,assemblem,altfinal,altdebug,asargs}};
        bool ok=true;
        /* Start the alternative at its primary fork step's progress row. */
        /* agent: gpt-6 | 2026-10-07 | Align scale lanes after diagnosis | v 2.8.0 | 9adbe1 */
        int branch_order=0;for(size_t i=0;i<plans.size();i++)if(plans[i].id==std::string("lattice")){branch_order=(int)i+1;break;}
        for(auto&step:branch_steps){auto&id=std::get<0>(step);auto&name=std::get<1>(step);auto number=std::get<2>(step);auto&meta=std::get<3>(step);auto&main=std::get<4>(step);auto&debug=std::get<5>(step);auto&args=std::get<6>(step);
            Stage node;node.id=id;node.name=name;node.number=number;node.progress=branch_order++;node.branch=alt.id;node.main=main.string();node.debug=debug.string();node.meta=meta.string();node.state="running";node.started=(double)std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();alt.stages.push_back(node);
            event(journal,seq,"node_started",id,number,"branch node started",alt.id,node.progress);
            auto t0=std::chrono::steady_clock::now();ok=run_child(bin/("stage_"+id),args);node.elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();node.state=ok&&validate_artifacts(node)?"completed":"failed";node.sprite_dir=sprite_directory(node.meta);alt.stages.back()=node;
            event(journal,seq,ok?"node_completed":"node_failed",id,number,"branch node completed",alt.id,node.progress);if(!ok)break;
        }
        if(!ok){event(journal,seq,"branch_failed","branch",62,"alternate shared-scale branch failed; primary result retained",alt.id,alt.stages.empty()?-1:alt.stages.back().progress);}
        else {
        /* agent: gpt-6.1-sol | 2026-10-06 | Publish scale alternative as final | v 2.0.0 | 4bd882 */
        fs::path published=extra/("final.pixel."+alt.id+".png");if(!copy_atomic(altfinal,published))return pp::input_error("could not publish alternate spritesheet");alt.final=published.string();sheet_variants.push_back(published);
        fs::path variant_root=extra/"pixelprocess"/alt.id;if(!copy_tree(cleandir,variant_root/"clean-sprites"))return pp::input_error("could not publish alternate sprite exports");
        branches.push_back(std::move(alt));event(journal,seq,"branch_completed","branch",62,"alternate shared-pitch spritesheet ready for comparison",branches.back().id,branches.back().stages.back().progress);
        }
    }
    std::vector<fs::path> sprite_exports;
    {auto metadata=pp::read_json(clean_meta.string());for(auto it=metadata["sprites"].begin();it!=metadata["sprites"].end();++it){std::string id=(std::string)(*it)["id"],path=(std::string)(*it)["path"];fs::path published=extra/("sprite-"+id+(branch_mode?"."+output_branch:"")+".png");if(save_mode!="copy"&&!copy_atomic(path,published))return pp::input_error("could not publish sprite export "+id);if(save_mode!="copy")sprite_exports.push_back(published);}}
    if(save_mode=="copy"||branch_mode)write_sprite_records(extra/"processing.sprites.jsonl",active_source,owner_meta,finalmeta,published_final,true,false);
    else if(!write_sprite_records(extra/"processing.sprites.jsonl",active_source,owner_meta,finalmeta,published_final))return pp::input_error("could not publish sprite inspection records");
    for(const auto&stage:records)if(stage.id!="source"&&(!write_stage_sprite_records(extra/"processing.sprites.jsonl",stage,owner_meta,active_source)||!write_stage_sprite_records(extra/"processing.sprites.jsonl",stage,owner_meta,active_source,true)))return pp::input_error("could not publish stage sprite inspection records");
    for(const auto&branch:branches){for(const auto&stage:branch.stages)if(!write_stage_sprite_records(extra/"processing.sprites.jsonl",stage,owner_meta,active_source)||!write_stage_sprite_records(extra/"processing.sprites.jsonl",stage,owner_meta,active_source,true))return pp::input_error("could not publish alternate stage sprite records");if(!branch.final.empty()&&!write_sprite_records(extra/"processing.sprites.jsonl",active_source,owner_meta,branch.stages.back().meta,branch.final,true,false))return pp::input_error("could not publish alternate final sprite records");}
    if(save_mode=="copy"){
        const std::string copy_id=(a.get("target")=="ownership"?"grid-copy-r":"lattice-copy-r")+stage_number(revision);
        if(!copy_atomic(settings_path,pin_dir/(copy_id+".json")))return pp::input_error("could not pin setup settings");
    }else if(save_mode=="replace"&&selected_is_pinned){
        if(!copy_atomic(settings_path,selected_pin))return pp::input_error("could not update pinned setup settings");
    }
    if(branch_mode)manifest_snapshot("completed",sprite_exports,sheet_variants);
    else if(save_mode=="copy")write_manifest(manifest,active_source,"completed",revision,settings_path.string(),primary_old,sprite_exports,sheet_variants,branches);
    else {
        const bool full_primary=selected_branch=="primary"&&target=="all"&&!branch_mode;
        write_manifest(manifest,active_source,"completed",revision,settings_path.string(),records,sprite_exports,sheet_variants,branches);
        if(full_primary&&!pinned_replay){
            fs::path self=fs::canonical("/proc/self/exe");
            for(const auto&branch:branches){if(!branch.pinned||branch.id=="primary")continue;fs::path pin=pin_dir/(branch.id+".json");if(!fs::is_regular_file(pin))continue;
                std::vector<std::string>args={"--input",active_source.string(),"--run-dir",run.string(),"--settings",pin.string(),"--branch-id",branch.id,"--target","all","--pinned-replay","1"};
                if(!run_child(self,args))return pp::input_error("pinned branch rerun failed: "+branch.id);
            }
            auto completed_branches=load_graph_branches(manifest);completed_branches.erase(std::remove_if(completed_branches.begin(),completed_branches.end(),[&](const Branch&branch){return !branch.pinned&&std::find(existing_branch_ids.begin(),existing_branch_ids.end(),branch.id)!=existing_branch_ids.end();}),completed_branches.end());
            for(auto&branch:completed_branches)if(!branch.parent.empty()&&branch.parent!="primary"&&std::find(existing_branch_ids.begin(),existing_branch_ids.end(),branch.parent)!=existing_branch_ids.end()&&std::none_of(completed_branches.begin(),completed_branches.end(),[&](const Branch&candidate){return candidate.id==branch.parent;}))branch.parent="primary";
            auto completed_primary=load_previous(manifest);auto latest_manifest=pp::read_json(manifest.string());int published_revision=std::max(revision,(int)latest_manifest["revision"]);write_manifest(manifest,active_source,"completed",published_revision,settings_path.string(),completed_primary,sprite_exports,sheet_variants,completed_branches);
        }
    }
    if(save_mode.empty()&&!scope_replay&&!pinned_replay&&(branch_mode||(selected_branch=="primary"&&target!="all"))){
        auto stage_index=[&](const std::string&step){if(step.empty()||step=="all")return 0;for(size_t i=0;i<plans.size();++i)if(step==plans[i].id||step==std::to_string(plans[i].n))return (int)i;return -1;};
        const int boundary=target=="all"?0:force_index;std::vector<std::string>affected;
        for(const auto&candidate:branches){
            if(candidate.id==selected_branch)continue;
            std::string cursor=candidate.id;std::vector<std::string>seen;const Branch*first_child=nullptr;bool descendant=false;
            while(cursor!="primary"){
                if(std::find(seen.begin(),seen.end(),cursor)!=seen.end())break;seen.push_back(cursor);
                auto ancestor=std::find_if(branches.begin(),branches.end(),[&](const Branch&branch){return branch.id==cursor;});if(ancestor==branches.end())break;
                if(ancestor->parent==selected_branch){first_child=&*ancestor;descendant=true;break;}cursor=ancestor->parent;
            }
            if(descendant&&first_child&&stage_index(first_child->fork_step)>=boundary)affected.push_back(candidate.id);
        }
        fs::path self=fs::canonical("/proc/self/exe");
        for(const auto&id:affected){
            auto branch=std::find_if(branches.begin(),branches.end(),[&](const Branch&item){return item.id==id;});if(branch==branches.end()||!branch->pinned)continue;
            fs::path pin=pin_dir/(id+".json");if(!fs::is_regular_file(pin))continue;
            std::vector<std::string>args={"--input",active_source.string(),"--run-dir",run.string(),"--settings",pin.string(),"--branch-id",id,"--target",target,"--branch-scope-replay","1"};
            if(!run_child(self,args))return pp::input_error("downstream pinned branch rerun failed: "+id);
        }
        auto current_branches=load_graph_branches(manifest);current_branches.erase(std::remove_if(current_branches.begin(),current_branches.end(),[&](const Branch&branch){return std::find(affected.begin(),affected.end(),branch.id)!=affected.end()&&!branch.pinned;}),current_branches.end());
        if(branch_mode&&!selected_is_pinned&&std::find(affected.begin(),affected.end(),selected_branch)==affected.end()){
            current_branches.erase(std::remove_if(current_branches.begin(),current_branches.end(),[&](const Branch&branch){return branch.id==selected_branch;}),current_branches.end());
        }
        for(auto&branch:current_branches)if(!branch.parent.empty()&&branch.parent!="primary"&&std::none_of(current_branches.begin(),current_branches.end(),[&](const Branch&candidate){return candidate.id==branch.parent;}))branch.parent="primary";
        auto current_manifest=pp::read_json(manifest.string());int current_revision=(int)current_manifest["revision"];
        auto current_primary=load_previous(manifest);write_manifest(manifest,active_source,"completed",current_revision,settings_path.string(),current_primary,sprite_exports,sheet_variants,current_branches);
    }
    event(journal,seq,"run_completed","assemble",4,"spritesheet variants and individual sprites ready",branch_mode?output_branch:save_mode=="copy"?branches.back().id:"primary",8);
    std::cout<<"done output="<<published_final.string()<<" manifest="<<manifest.string()<<" revision="<<revision<<'\n';return 0;
}
/* agent: gpt-6 | 2026-10-07 | Align scale lanes after diagnosis | v 2.8.0 | 9adbe1 */

/* agent: gpt-6.1-sol | 2026-10-06 | Add shared scale variants | v 1.8.0 | f639df */
/* agent: gpt-6.1-sol | 2026-10-06 | Add shared scale variants | v 1.8.0 | f639df */
/* agent: gpt-6.1-sol | 2026-10-06 | Add shared scale variants | v 1.8.0 | f639df */
/* agent: gpt-6.1-sol | 2026-10-06 | Redirect cleanup preview outputs | v 1.9.0 | a8f552 */
/* agent: gpt-6.1-sol | 2026-10-06 | Keep only current settings sibling | v 2.3.0 | c44b6b */
/* agent: gpt-6.1-sol | 2026-10-06 | Publish typed branch lifecycle events | v 2.3.0 | 59b7f0 */
/* agent: gpt-6.1-sol | 2026-10-06 | Require graph manifests for cache reuse | v 2.3.0 | b7bc65 */
/* agent: gpt-6.1-sol | 2026-10-06 | Tag source sprite evidence by artifact | v 2.3.0 | 4c0f6c */
/* agent: gpt-6.1-sol | 2026-10-06 | Tag final sheet artifacts by branch | v 2.3.0 | 78f6df */

/* agent: gpt-6.1-sol | 2026-10-07 | Bound alternate pitch and rerun branches | v 3.2.0 | edf89e */
/* agent: gpt-6 | 2026-10-07 | Rerun scoped branch descendants | v 3.6.0 | 0c8b39 */
