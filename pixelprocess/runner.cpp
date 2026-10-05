/*
 * File version: 1.2.0
 * Last updated: 2026-10-05
 * Purpose: Execute, cache, record, and atomically publish a pixelprocess revision.
 * Flow INFRA-01: 1 UI -> isolated run/revision; 2 runner -> ordered stages; 3 runner -> validated artifacts; 4 runner -> manifest/status.
 * Invariants: runner exclusively owns run state; input source is immutable; failed revisions retain valid earlier stage outputs.
 */
/* agent: gpt-6.1-sol | 2026-10-05 | Run and cache pixelprocess stages | v 1.2.0 | d938bb */
#include "common.hpp"
#include <algorithm>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs=std::filesystem;
static volatile sig_atomic_t interrupted=0;
static volatile sig_atomic_t active_child=0;
static void on_signal(int signal){interrupted=signal;if(active_child>0)kill((pid_t)active_child,SIGINT);}
struct Stage {std::string id,name,cache_key,main,debug,meta,state;int number;double started=0,elapsed=0;};
struct Spec {std::string id,name;int number;std::vector<std::string> inputs,argv;std::string setting_keys;};
static std::string now_utc(){auto now=std::chrono::system_clock::now();auto t=std::chrono::system_clock::to_time_t(now);std::tm tm{};gmtime_r(&t,&tm);char b[40];strftime(b,sizeof(b),"%Y-%m-%dT%H:%M:%SZ",&tm);return b;}
static void json_quote(std::ostream&o,const std::string&s){o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<(char)c;else if(c=='\n')o<<"\\n";else if(c=='\r')o<<"\\r";else if(c=='\t')o<<"\\t";else if(c<32)o<<' ';else o<<(char)c;}o<<'"';}
static void event(const fs::path&path,int&seq,const std::string&kind,const std::string&stage,int number,const std::string&message){std::ofstream f(path,std::ios::app);if(!f)return;f<<"{\"schema_version\":1,\"sequence\":"<<++seq<<",\"timestamp_utc\":";json_quote(f,now_utc());f<<",\"kind\":";json_quote(f,kind);f<<",\"stage_id\":";json_quote(f,stage);f<<",\"stage_number\":"<<number<<",\"message\":";json_quote(f,message);f<<"}\n";f.flush();}
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
static void write_manifest(const fs::path&path,const fs::path&source,const std::string&state,int revision,const std::string&settings_path,const std::vector<Stage>&stages){std::string tmp;std::uint64_t digest=hash_file(1469598103934665603ULL,source);auto fsout=pp::json_writer(path.string(),tmp);fsout<<"schema_version"<<1<<"metadata_format"<<"pixelprocess-run"<<"processor_version"<<"1.1.0"<<"run_state"<<state<<"revision"<<revision<<"source"<<source.string()<<"source_fingerprint"<<hash_hex(digest)<<"fingerprint_algorithm"<<"fnv1a64"<<"event_journal"<<(path.parent_path()/"processing.events.jsonl").string()<<"settings_path"<<settings_path<<"stages"<<"[";for(auto&s:stages){fsout<<"{"<<"id"<<s.id<<"number"<<s.number<<"name"<<s.name<<"state"<<s.state<<"cache_key"<<s.cache_key<<"main"<<s.main<<"debug"<<s.debug<<"metadata"<<s.meta<<"started_at"<<s.started<<"elapsed_seconds"<<s.elapsed<<"}";}fsout<<"]";pp::publish_json(fsout,tmp,path.string());}
static std::vector<Stage> load_previous(const fs::path&path){std::vector<Stage>out;auto f=pp::read_json(path.string());if(!f.isOpened())return out;for(auto it=f["stages"].begin();it!=f["stages"].end();++it){Stage s;s.id=(std::string)(*it)["id"];s.name=(std::string)(*it)["name"];s.number=(int)(*it)["number"];s.state=(std::string)(*it)["state"];s.cache_key=(std::string)(*it)["cache_key"];s.main=(std::string)(*it)["main"];s.debug=(std::string)(*it)["debug"];s.meta=(std::string)(*it)["metadata"];out.push_back(s);}return out;}
static bool validate_artifacts(const Stage&s){return fs::is_regular_file(s.main)&&fs::is_regular_file(s.debug)&&fs::is_regular_file(s.meta);}
/* Write source and final sprite hit regions plus source blob bounds; return bool. */
static bool write_sprite_records(const fs::path&path,const fs::path&source,const fs::path&owner_path,const fs::path&assemble_path,const fs::path&final_path){
    auto owner=pp::read_json(owner_path.string()),assembly=pp::read_json(assemble_path.string());cv::Mat src=pp::read_rgba(source.string()),final=pp::read_rgba(final_path.string());
    if(!owner.isOpened()||!assembly.isOpened()||src.empty()||final.empty())return false;
    std::ofstream out(path,std::ios::trunc);if(!out)return false;std::string stamp=now_utc();
    for(auto it=owner["sprites"].begin();it!=owner["sprites"].end();++it){
        std::string id=(std::string)(*it)["id"];int row=(int)(*it)["row"],col=(int)(*it)["col"];std::vector<int>b;(*it)["source_bounds"]>>b;if(b.size()!=4||b[2]<=b[0]||b[3]<=b[1])continue;
        out<<"{\"stage_number\":100,\"sprite_id\":";json_quote(out,id);out<<",\"row\":"<<row<<",\"col\":"<<col<<",\"artifact_bounds\":["<<b[0]<<","<<b[1]<<","<<b[2]-b[0]<<","<<b[3]-b[1]<<"],\"artifact_dimensions\":["<<src.cols<<","<<src.rows<<"],\"timestamp_utc\":";json_quote(out,stamp);out<<",\"summary\":\"Source ownership bounds\",\"blobs\":[";
        bool first=true;for(auto ci=(*it)["components"].begin();ci!=(*it)["components"].end();++ci){if(!first)out<<',';first=false;std::string blob=(std::string)(*ci)["id"];std::vector<int>bb;(*ci)["bounds"]>>bb;if(bb.size()!=4)continue;out<<"{\"blob_id\":";json_quote(out,blob);out<<",\"artifact_bounds\":["<<bb[0]<<","<<bb[1]<<","<<bb[2]-bb[0]<<","<<bb[3]-bb[1]<<"],\"pixels\":"<<(int)(*ci)["pixels"]<<"}";}
        out<<"]}\n";
    }
    for(auto it=assembly["placements"].begin();it!=assembly["placements"].end();++it){std::string id=(std::string)(*it)["id"];int row=(int)(*it)["row"],col=(int)(*it)["col"];std::vector<int>b;(*it)["destination_bounds"]>>b;if(b.size()!=4||b[2]<=b[0]||b[3]<=b[1])continue;out<<"{\"stage_number\":0,\"artifact\":";json_quote(out,final_path.string());out<<",\"sprite_id\":";json_quote(out,id);out<<",\"row\":"<<row<<",\"col\":"<<col<<",\"artifact_bounds\":["<<b[0]<<","<<b[1]<<","<<b[2]-b[0]<<","<<b[3]-b[1]<<"],\"artifact_dimensions\":["<<final.cols<<","<<final.rows<<"],\"timestamp_utc\":";json_quote(out,stamp);out<<",\"summary\":\"Final spritesheet placement\"}\n";}
    return out.good();
}
static bool mirror_artifacts(const Stage&s,const fs::path&extra){std::string prefix="stage-"+stage_number(s.number)+"-"+s.id;return copy_atomic(s.main,extra/(prefix+".png"))&&copy_atomic(s.debug,extra/("stage-debug-"+stage_number(s.number)+"-"+s.id+".png"));}
static bool copy_tree(const fs::path&from,const fs::path&to){std::error_code ec;if(!fs::exists(from))return false;fs::create_directories(to,ec);if(ec)return false;for(fs::recursive_directory_iterator i(from,ec),end;i!=end&&!ec;i.increment(ec)){if(!i->is_regular_file())continue;auto relative=i->path().lexically_relative(from);if(!copy_atomic(i->path(),to/relative))return false;}return !ec;}

/* Validate CLI, execute the ordered pipeline, and publish partial/final state; return exit status. */
int main(int argc,char**argv){
    auto a=pp::parse_args(argc,argv);if(!a.has("input")||!a.has("run-dir"))return pp::input_error("usage: pixelprocess-run --input IMAGE --run-dir DIR [--settings JSON] [--target STAGE]");
    fs::path source=fs::absolute(a.get("input")),run=fs::absolute(a.get("run-dir"));if(pp::read_rgba(source.string()).empty())return pp::input_error("input image is unreadable or unsupported");
    fs::path extra=run/"extra",root=extra/"pixelprocess",manifest=extra/"pixelprocess.manifest.json",journal=extra/"processing.events.jsonl";std::error_code ec;fs::create_directories(root,ec);if(ec)return pp::input_error("cannot create run directory");
    int revision=1;auto old=load_previous(manifest);if(fs::exists(manifest)){auto f=pp::read_json(manifest.string());revision=(int)f["revision"]+1;if(revision<1)revision=1;}
    fs::path rev=root/("revision-"+stage_number(revision));fs::create_directories(rev,ec);fs::create_directories(extra/"debug",ec);
    fs::path settings_path=rev/"effective-settings.json";
    if(a.has("settings")){fs::path requested=fs::absolute(a.get("settings"));if(!copy_atomic(requested,settings_path))return pp::input_error("cannot snapshot settings file");}
    else {std::ofstream f(settings_path);f<<"{\"schema_version\":1,\"background_tolerance\":24,\"grid_cols\":0,\"grid_rows\":0,\"extraction_padding\":1,\"pitch\":\"auto\",\"min_pitch\":2,\"max_pitch\":16,\"pitch_precision\":0.1,\"foreground_coverage\":0.35,\"cleanup_mode\":\"outside\",\"cleanup_tolerance\":55,\"sheet_anchor\":\"center\",\"sheet_padding\":1}";}
    auto config=pp::read_json(settings_path.string());if(!config.isOpened())return pp::input_error("settings must be valid JSON");
    fs::path bin=fs::canonical("/proc/self/exe").parent_path();int seq=0;{std::ifstream f(journal);std::string line;while(std::getline(f,line))if(line.find("\"sequence\"")!=std::string::npos)++seq;}
    signal(SIGINT,on_signal);signal(SIGTERM,on_signal);
    fs::path source_copy=run/"source.png";if(source_copy!=source&&!fs::exists(source_copy))copy_atomic(source,source_copy);fs::path active_source=fs::exists(source_copy)?source_copy:source;
    Stage source_stage{"source","Source", "",active_source.string(),active_source.string(),"", "completed",100};
    std::vector<Stage> records{source_stage};event(journal,seq,"artifact","source",100,"immutable source ready");
    const std::string cleanup=setting(config,"cleanup_mode","outside");const std::string tolerance=setting(config,"background_tolerance","24");const std::string gridc=setting(config,"grid_cols","0"),gridr=setting(config,"grid_rows","0"),pad=setting(config,"extraction_padding","1");
    const std::string pitch=setting(config,"pitch","auto"),minp=setting(config,"min_pitch","2"),maxp=setting(config,"max_pitch","16"),precision=setting(config,"pitch_precision","0.1"),coverage=setting(config,"foreground_coverage","0.35"),cleantol=setting(config,"cleanup_tolerance","55"),anchor=setting(config,"sheet_anchor","center"),sheetpad=setting(config,"sheet_padding","1");
    fs::path mask=rev/"background-mask.png",owner_meta=rev/"ownership.json",extract_meta=rev/"extraction.json",lattice_meta=rev/"lattice.json",recon_meta=rev/"reconstruction.json",clean_meta=rev/"cleanup.json";
    fs::path crops=rev/"crops",logical=rev/"logical-sprites",clean_sprites=rev/"clean-sprites";
    struct Plan{const char*id;const char*name;int n;std::vector<fs::path>inputs;std::string key;std::vector<std::string>args;};std::vector<Plan>plans;
    auto mk=[&](const char*id,const char*name,int n,std::vector<fs::path>inputs,std::string keys,std::vector<std::string>args){plans.push_back({id,name,n,std::move(inputs),std::move(keys),std::move(args)});};
    fs::path bgmain=rev/"background.png",bgdebug=rev/"background.debug.png",bgmeta=rev/"background.stage.json";
    mk("background","Background mask",97,{active_source},"background_tolerance",{"--input",active_source.string(),"--output",bgmain.string(),"--debug",bgdebug.string(),"--mask",mask.string(),"--meta",bgmeta.string(),"--tolerance",tolerance});
    fs::path ownmain=rev/"ownership.png",owndebug=rev/"ownership.debug.png";
    mk("ownership","Blob ownership",94,{active_source,mask},"background_tolerance,grid_cols,grid_rows,ownership_overrides",{"--input",active_source.string(),"--mask",mask.string(),"--output",ownmain.string(),"--debug",owndebug.string(),"--meta",owner_meta.string(),"--grid_cols",gridc,"--grid_rows",gridr,"--settings",settings_path.string()});
    fs::path exmain=rev/"extraction.png",exdebug=rev/"extraction.debug.png";
    mk("extract","Sprite extraction",91,{active_source,mask,owner_meta},"background_tolerance,grid_cols,grid_rows,extraction_padding,cleanup_mode,ownership_overrides",{"--input",active_source.string(),"--mask",mask.string(),"--state",owner_meta.string(),"--output",exmain.string(),"--debug",exdebug.string(),"--meta",extract_meta.string(),"--sprite-dir",crops.string(),"--padding",pad,"--settings",settings_path.string()});
    fs::path latmain=rev/"lattice.png",latdebug=rev/"lattice.debug.png";
    mk("lattice","Lattice fitting",80,{active_source,mask,extract_meta,crops},"background_tolerance,grid_cols,grid_rows,extraction_padding,cleanup_mode,pitch,phase_x,phase_y,min_pitch,max_pitch,pitch_precision,sprite_overrides,ownership_overrides",{"--input",active_source.string(),"--mask",mask.string(),"--state",extract_meta.string(),"--output",latmain.string(),"--debug",latdebug.string(),"--meta",lattice_meta.string(),"--sprite-dir",crops.string(),"--settings",settings_path.string()});
    fs::path recmain=rev/"reconstruction.png",recdebug=rev/"reconstruction.debug.png";
    mk("reconstruct","Pixel reconstruction",60,{active_source,mask,extract_meta,lattice_meta,crops},"background_tolerance,grid_cols,grid_rows,extraction_padding,cleanup_mode,pitch,phase_x,phase_y,min_pitch,max_pitch,pitch_precision,foreground_coverage,sprite_overrides,ownership_overrides",{"--input",active_source.string(),"--mask",mask.string(),"--state",extract_meta.string(),"--owner",owner_meta.string(),"--lattice",lattice_meta.string(),"--output",recmain.string(),"--debug",recdebug.string(),"--meta",recon_meta.string(),"--sprite-dir",logical.string(),"--coverage",coverage,"--cleanup-mode",cleanup});
    fs::path clmain=rev/"cleanup.png",cldebug=rev/"cleanup.debug.png";
    mk("cleanup","Magenta cleanup",23,{recon_meta,logical},"background_tolerance,grid_cols,grid_rows,extraction_padding,cleanup_mode,pitch,phase_x,phase_y,min_pitch,max_pitch,pitch_precision,foreground_coverage,cleanup_tolerance,sprite_overrides,ownership_overrides",{"--state",recon_meta.string(),"--output",clmain.string(),"--debug",cldebug.string(),"--meta",clean_meta.string(),"--sprite-dir",clean_sprites.string(),"--mode",cleanup,"--tolerance",cleantol});
    fs::path final=rev/"final.pixel.png",finaldebug=rev/"assemble.debug.png",finalmeta=rev/"assemble.json";
    mk("assemble","Spritesheet assembly",4,{active_source,extract_meta,clean_meta,clean_sprites},"background_tolerance,grid_cols,grid_rows,extraction_padding,cleanup_mode,pitch,phase_x,phase_y,min_pitch,max_pitch,pitch_precision,foreground_coverage,cleanup_tolerance,sprite_overrides,ownership_overrides,sheet_anchor,sheet_padding",{"--input",active_source.string(),"--state",extract_meta.string(),"--sprites",clean_meta.string(),"--output",final.string(),"--debug",finaldebug.string(),"--meta",finalmeta.string(),"--padding",sheetpad,"--anchor",anchor});
    auto previous=[&](const std::string&id)->Stage*{for(auto&s:old)if(s.id==id)return &s;return nullptr;};
    int force_index=-1;std::string target=a.get("target","all");for(size_t i=0;i<plans.size();i++)if(target==plans[i].id||target==std::to_string(plans[i].n))force_index=(int)i;
    if(target!="all"&&force_index<0)return pp::input_error("unknown --target stage");
    std::string upstream_signature;
    for(size_t i=0;i<plans.size();i++){
        auto &p=plans[i];fs::path exe=bin/(std::string("pixelprocess-")+p.id);Stage result;result.id=p.id;result.name=p.name;result.number=p.n;result.started=(double)std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::vector<std::string> input_paths;for(auto&input:p.inputs)input_paths.push_back(input.string());input_paths.push_back(active_source.string());
        std::vector<std::string> relevant;std::stringstream key_stream(p.key);std::string key;while(std::getline(key_stream,key,','))if(!key.empty())relevant.push_back(key);
        result.cache_key=signature(Spec{p.id,p.name,p.n,input_paths,{},settings_signature(config,relevant)+"|"+upstream_signature},exe);
        upstream_signature+=std::string(p.id)+":"+result.cache_key+";";
        Stage*old_stage=previous(p.id);bool force=force_index>=0&&(int)i>=force_index;bool can_reuse=!force&&old_stage&&old_stage->cache_key==result.cache_key&&validate_artifacts(*old_stage);
        if(interrupted){result.state="skipped";records.push_back(result);write_manifest(manifest,active_source,"canceled",revision,settings_path.string(),records);event(journal,seq,"canceled",p.id,p.n,"run canceled before stage");return 130;}
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
            std::cout<<"stage "<<p.id<<" reused cache="<<result.cache_key<<'\n';event(journal,seq,"stage_reused",p.id,p.n,"matching inputs, settings, and executable");
        }
        else{
            result.main=(rev/("stage-"+stage_number(p.n)+"-"+p.id+".png")).string();result.debug=(rev/("stage-"+stage_number(p.n)+"-"+p.id+".debug.png")).string();
            // Every stage writes through its explicit --output/--debug arguments above.
            if(p.id==std::string("background")){p.args[3]=result.main;p.args[5]=result.debug;}
            if(p.id==std::string("ownership")){p.args[5]=result.main;p.args[7]=result.debug;}
            if(p.id==std::string("extract")){p.args[7]=result.main;p.args[9]=result.debug;}
            if(p.id==std::string("lattice")){p.args[7]=result.main;p.args[9]=result.debug;}
            if(p.id==std::string("reconstruct")){p.args[11]=result.main;p.args[13]=result.debug;}
            if(p.id==std::string("cleanup")){p.args[3]=result.main;p.args[5]=result.debug;}
            if(p.id==std::string("assemble")){p.args[7]=result.main;p.args[9]=result.debug;}
            for(size_t j=0;j+1<p.args.size();j++)if(p.args[j]=="--meta")result.meta=p.args[j+1];
            result.state="running";records.push_back(result);event(journal,seq,"stage_started",p.id,p.n,"stage process started");write_manifest(manifest,active_source,"running",revision,settings_path.string(),records);
            auto t0=std::chrono::steady_clock::now();if(!fs::exists(exe)||!run_child(exe,p.args)){result.state=interrupted?"canceled":"failed";result.elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();records.back()=result;write_manifest(manifest,active_source,result.state,revision,settings_path.string(),records);event(journal,seq,interrupted?"canceled":"stage_failed",p.id,p.n,"stage exited without a complete result");return interrupted?130:1;}
            result.elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();result.state="completed";
            {auto metadata=pp::read_json(result.meta);if(metadata.isOpened()&&!metadata["state"].empty())result.state=(std::string)metadata["state"];}
            if(!validate_artifacts(result)){result.state="failed";records.back()=result;write_manifest(manifest,active_source,"failed",revision,settings_path.string(),records);event(journal,seq,"stage_failed",p.id,p.n,"required stage artifacts missing");return 1;}
            event(journal,seq,"artifact",p.id,p.n,"main, debug, and metadata published");records.pop_back();
        }
        records.push_back(result);write_manifest(manifest,active_source,"running",revision,settings_path.string(),records);
        if(!mirror_artifacts(result,extra))return pp::input_error("could not publish stage artifact aliases");
    }
    Stage*sheet=previous("assemble");(void)sheet;std::string final_path=records.back().main;
    if(!copy_atomic(final_path,run/"final.pixel.png"))return pp::input_error("could not publish final spritesheet");
    if(!copy_atomic(records.front().main,extra/"stage-100-source.png"))return pp::input_error("could not publish source stage");
    {auto metadata=pp::read_json(clean_meta.string());for(auto it=metadata["sprites"].begin();it!=metadata["sprites"].end();++it){std::string id=(std::string)(*it)["id"],path=(std::string)(*it)["path"];if(!copy_atomic(path,extra/("sprite-"+id+".png")))return pp::input_error("could not publish sprite export "+id);}}
    if(!write_sprite_records(extra/"processing.sprites.jsonl",active_source,owner_meta,finalmeta,run/"final.pixel.png"))return pp::input_error("could not publish sprite inspection records");
    write_manifest(manifest,active_source,"completed",revision,settings_path.string(),records);event(journal,seq,"run_completed","assemble",4,"spritesheet and individual sprites ready");
    std::cout<<"done output="<<(run/"final.pixel.png").string()<<" manifest="<<manifest.string()<<" revision="<<revision<<'\n';return 0;
}

/* agent: gpt-6.1-sol | 2026-10-05 | Run and cache pixelprocess stages | v 1.2.0 | d938bb */
