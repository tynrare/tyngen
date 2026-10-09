/*
 * File version: 3.5.0
 * Last updated: 2026-10-09
 * Purpose: Verify graph manifests, cache reuse, and sprite evidence.
 * Flow PIXEL-TEST-01: 1 fixture -> run and inspect artifacts; 2 rerun -> verify cache;
 * 3 settings change -> verify invalidation; 4 sprite override -> verify independent lattice;
 * 5 textured sheet -> verify grid, pink cleanup;
 * 5 transparent sprite -> preserve opaque dark pixels without border color evidence;
 * 6 pipeline -> publish progress nodes and artifact-specific sprite evidence;
 * 7 reconstruction -> expose fractional bounds and per-cell confidence;
 * 8 revisions -> archive graphs and preserve branch identity; 9 transaction -> stale writes and atomic pin updates; 10 setup -> stable replacement and inherited overrides.
 * 11 setup branch -> publish stages only at/after fork and retain fork shape on replacement; automatic pitch run -> record selection reason and bounded alternate; 12 quantization -> publish final shared-color sheet and sprite crops; 13 separators -> share refined masks and preserve logical slot counts; 14 background setup -> save probe settings on a pinned background fork and preserve parent settings; 15 fresh rerun -> prune unreferenced final sheets and sprite exports.
 * 16 cleanup -> verify clear/keep masks and outline reconstruction stage.
 * agent: gpt-6 | 2026-10-09 | Verify fresh rerun removes stale exports | v 3.3.0 | 1e965b
 * agent: gpt-6 | 2026-10-09 | Verify background probe setup branches | v 3.2.0 | 154bb4
 * agent: gpt-6.1-sol | 2026-10-08 | Verify stages begin at branch fork | v 2.8.0 | c792cf
 * agent: gpt-6.1-sol | 2026-10-09 | Record pitch selection metadata | v 2.9.0 | 4e1573
 * agent: gpt-6.1-sol | 2026-10-09 | Verify separator pipeline artifacts | v 3.1.0 | 69a940
 */
/* agent: gpt-6.1-sol | 2026-10-09 | Test cleanup masks and outline stage | v 3.4.0 | a14474 */
/* agent: gpt-6 | 2026-10-07 | Verify atomic revisions and stale writes | v 2.7.0 | 5d5643 */
/* agent: gpt-6.1-sol | 2026-10-09 | Assert final quantization artifacts | v 3.0.0 | e31043 */
/* agent: gpt-6.1-sol | 2026-10-09 | Verify separator pipeline artifacts | v 3.1.0 | 69a940 */
#include "../common.hpp"
#undef NDEBUG
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <unistd.h>

namespace fs = std::filesystem;

/* Write a two-cell magenta-backed fixture and return its image path. */
static fs::path write_fixture(const fs::path &root) {
    cv::Mat image(16, 32, CV_8UC4, cv::Scalar(255, 0, 255, 255));
    for (int cell = 0; cell < 2; ++cell) {
        int x = cell ? 19 : 3, y = 4;
        for (int yy = 0; yy < 8; ++yy) for (int xx = 0; xx < 8; ++xx)
            if (xx == 0 || yy == 0 || xx == 7 || yy == 7)
                image.at<cv::Vec4b>(y + yy, x + xx) = cv::Vec4b(40 + cell * 50, 100, 210, 255);
    }
    fs::path path = root / "fixture.png";
    assert(cv::imwrite(path.string(), image));
    return path;
}

/* Write fixture from fs::path root; return fs::path. */
static fs::path write_textured_sheet(const fs::path &root) {
    cv::Mat image(128,128,CV_8UC4);
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++){
        int variation=((x*13+y*7)%7)-3;
        image.at<cv::Vec4b>(y,x)=cv::Vec4b((uchar)(194+variation),(uchar)(82+variation),(uchar)(252+variation),255);
    }
    for(int row=0;row<4;row++)for(int col=0;col<4;col++){
        cv::Rect sprite(col*32+8,row*32+8,16,16);
        cv::Scalar color(35+col*38,220-row*25,70+row*35,255);
        if(row==0&&col==0){cv::rectangle(image,sprite,color,cv::FILLED);image.at<cv::Vec4b>(sprite.y+7,sprite.x+7)=cv::Vec4b(194,82,252,255);}
        else {cv::rectangle(image,sprite,color,2);cv::line(image,{sprite.x+sprite.width-1,sprite.y+5},{sprite.x+sprite.width-1,sprite.y+10},cv::Scalar(194,82,252,255),2);}
    }
    for(int edge=32;edge<128;edge+=32){cv::line(image,{edge,0},{edge,127},cv::Scalar(0,0,0,255),2);cv::line(image,{0,edge},{127,edge},cv::Scalar(0,0,0,255),2);}
    fs::path path=root/"textured-sheet.png";assert(cv::imwrite(path.string(),image));return path;
}

/* Write a transparent-backed dark sprite from fs::path root; return fs::path. */
static fs::path write_transparent_sprite(const fs::path &root) {
    cv::Mat image(32,32,CV_8UC4,cv::Scalar(0,0,0,0));
    cv::rectangle(image,{9,7,14,19},cv::Scalar(0,0,0,255),cv::FILLED);
    cv::rectangle(image,{11,9,10,15},cv::Scalar(90,45,25,255),cv::FILLED);
    fs::path path=root/"transparent-sprite.png";assert(cv::imwrite(path.string(),image));return path;
}

/* Write deterministic processor settings for the fixture; return path. */
static fs::path write_settings(const fs::path &root, const std::string &cleanup, const std::string &overrides="[]") {
    fs::path path = root / "settings.json";
    std::ofstream out(path);
    out << "{\"schema_version\":1,\"background_tolerance\":24,\"grid_cols\":2,\"grid_rows\":1,"
        << "\"extraction_padding\":1,\"pitch\":\"2\",\"phase_x\":\"auto\",\"phase_y\":\"auto\","
        << "\"min_pitch\":2,\"max_pitch\":8,\"pitch_precision\":0.1,\"foreground_coverage\":0.2,"
        << "\"cleanup_mode\":\"" << cleanup << "\",\"cleanup_tolerance\":55,\"sheet_anchor\":\"center\","
        << "\"sheet_padding\":1,\"sprite_overrides\":" << overrides << ",\"ownership_overrides\":{}}";
    out.close();
    assert(out.good());
    return path;
}

/* Write a two-color JSON palette from (const fs::path&, bool) and return fs::path. */
static fs::path write_palette(const fs::path&root,bool valid=true){fs::path path=root/(valid?"test.palette":"invalid.palette");std::ofstream out(path);out<<(valid?"{\"schema_version\":1,\"colors\":[[0,0,0],[255,255,255]]}":"{\"schema_version\":1,\"colors\":[]}");out.close();assert(out.good());return path;}

/* Copy settings from (const fs::path&, const fs::path&, const fs::path&) and return fs::path. */
static fs::path write_palette_settings(const fs::path&root,const fs::path&base,const fs::path&palette){std::ifstream in(base);std::string json((std::istreambuf_iterator<char>(in)),{});assert(!json.empty()&&json.back()=='}');json.pop_back();json+=",\"palette_path\":\""+palette.string()+"\"}";fs::path path=root/"palette-settings.json";std::ofstream out(path);out<<json;out.close();assert(out.good());return path;}

/* Run runner with safely quoted paths, settings, and target; return bool. */
/* Run runner with optional target and grid save mode; return bool. */
/* agent: gpt-6 | 2026-10-06 | Verify grid diagnosis artifacts | v 2.0.0 | 07b9cc */
static bool invoke(const fs::path &runner, const fs::path &input, const fs::path &run, const fs::path &settings, const std::string &target = "all", const std::string &save_mode = "", const std::string &branch = "primary", bool fresh = false, bool branch_revision = false, int expected_revision = -1) {
    std::string command = "\"" + runner.string() + "\" --input \"" + input.string() + "\" --run-dir \"" +
                          run.string() + "\" --settings \"" + settings.string() + "\" --target \"" + target + "\"";
    if (!save_mode.empty()) command += " --save-mode \"" + save_mode + "\"";
    if (!save_mode.empty() || branch != "primary") command += " --branch-id \"" + branch + "\"";
    if (fresh) command += " --fresh 1";
    if (branch_revision) command += " --branch-revision 1";
    if (expected_revision >= 0) command += " --expected-revision " + std::to_string(expected_revision);
    return std::system(command.c_str()) == 0;
}

/* Commit a branch pin state against an expected graph revision; return bool. */
static bool invoke_pin(const fs::path&runner,const fs::path&input,const fs::path&run,const std::string&branch,bool pinned,int expected_revision){
    std::string command="\""+runner.string()+"\" --input \""+input.string()+"\" --run-dir \""+run.string()+"\" --pin-branch \""+branch+"\" --pinned "+(pinned?"1":"0")+" --expected-revision "+std::to_string(expected_revision);
    return std::system(command.c_str())==0;
}

/* Run integration checks with int argc and char **argv; return int status. */
int main(int argc, char **argv) {
    assert(argc == 2);
    fs::path root = fs::temp_directory_path() / ("pixelprocess-test-" + std::to_string((long)getpid()));
    fs::remove_all(root);
    fs::create_directories(root);
    fs::path source = write_fixture(root), run = root / "run", settings = write_settings(root, "skip");
    fs::path runner = fs::absolute(argv[1]);
    assert(invoke(runner, source, run, settings));
    assert(fs::is_regular_file(run / "final.pixel.png"));
    assert(fs::is_regular_file(run / "extra" / "sprite-sprite-r000-c000.png"));
    {std::ifstream evidence(run/"extra"/"processing.sprites.jsonl");std::string record((std::istreambuf_iterator<char>(evidence)),{});assert(record.find("\"source_bounds\":[")!=std::string::npos&&record.find("\"source_dimensions\":[")!=std::string::npos&&record.find("\"artifact\":\""+(run/"source.png").string()+"\"")!=std::string::npos);}
    cv::Mat final = pp::read_rgba((run / "final.pixel.png").string());
    assert(!final.empty() && final.cols >= 2 && final.rows >= 1);
    auto manifest = pp::read_json((run / "extra" / "pixelprocess.manifest.json").string());
    assert(manifest.isOpened() && (std::string)manifest["run_state"] == "completed");
    assert((int)manifest["schema_version"] == 4 && (int)manifest["revision"] == 1);
    /* agent: gpt-6.1-sol | 2026-10-09 | Record pitch selection metadata | v 2.9.0 | 4e1573 */
    assert((std::string)manifest["pitch_selection_summary"]=="manual pitch");
    assert(fs::is_regular_file(run/"extra"/"pixelprocess"/"revision-01"/"pitch-selection.json"));
    /* agent: gpt-6.1-sol | 2026-10-09 | Record pitch selection metadata | v 2.9.0 | 4e1573 */
    assert(!invoke(runner,source,run,settings,"all","","primary",false,false,0));
    assert((int)pp::read_json((run/"extra"/"pixelprocess.manifest.json").string())["revision"]==1);
    assert(manifest["stages"].size() == 13 && manifest["nodes"].size() == 13);
    auto quant_meta=pp::read_json((run/"extra"/"pixelprocess"/"revision-01"/"quantize.json").string());
    assert((std::string)manifest["nodes"][12]["step_id"]=="quantize"&&quant_meta["placements"].size()==2);
    assert((std::string)manifest["nodes"][10]["step_id"]=="outline_reconstruct");
    auto outline_meta=pp::read_json((run/"extra"/"pixelprocess"/"revision-01"/"outline-reconstruction.json").string());
    assert(outline_meta.isOpened()&&(int)outline_meta["measurements"]["restored_pixels"]>=0);
    auto preview_artifact_meta=pp::read_json((run/"extra"/"pixelprocess"/"revision-01"/"cleanup-preview.stage.json").string());
    assert((std::string)preview_artifact_meta["mask_order"][0]=="clear"&&(std::string)preview_artifact_meta["mask_order"][1]=="keep");
    assert(fs::is_regular_file((std::string)preview_artifact_meta["sprites"][0]["clear_mask"]));
    assert(fs::is_regular_file((std::string)preview_artifact_meta["sprites"][0]["keep_mask"]));
    auto separator_meta=pp::read_json((run/"extra"/"pixelprocess"/"revision-01"/"separators.json").string());
    assert((std::string)manifest["nodes"][2]["step_id"]=="separators"&&separator_meta.isOpened());
    assert(separator_meta.isOpened());
    assert(fs::is_regular_file(run/"extra"/"pixelprocess"/"revision-01"/"separator-mask.png"));
    assert(fs::is_regular_file(run/"extra"/"pixelprocess"/"revision-01"/"refined-background-mask.png"));
    assert((int)quant_meta["placements"][0]["color_count"]>0);
    /* agent: gpt-6 | 2026-10-09 | Verify conditional palette mapping outputs | v 3.5.0 | 63244c */
    fs::path palette_root=root/"palette-fixture";fs::create_directories(palette_root);fs::path palette=write_palette(palette_root),palette_settings=write_palette_settings(palette_root,settings,palette),palette_run=palette_root/"run";
    assert(invoke(runner,source,palette_run,palette_settings));auto palette_graph=pp::read_json((palette_run/"extra"/"pixelprocess.manifest.json").string());
    assert(palette_graph["nodes"].size()==14&&(std::string)palette_graph["nodes"][12]["step_id"]=="quantize"&&(std::string)palette_graph["nodes"][13]["step_id"]=="palette_map");
    auto palette_stage=pp::read_json((std::string)palette_graph["nodes"][13]["metadata"]);auto mapped=pp::read_rgba((palette_run/"final.pixel.png").string());assert(palette_stage.isOpened()&&!mapped.empty());
    for(int y=0;y<mapped.rows;y++)for(int x=0;x<mapped.cols;x++){const auto p=mapped.at<cv::Vec4b>(y,x);if(p[3])assert((p[0]==0&&p[1]==0&&p[2]==0)||(p[0]==255&&p[1]==255&&p[2]==255));}
    std::string mapped_crop=(std::string)palette_stage["placements"][0]["path"];cv::Mat crop=pp::read_rgba(mapped_crop),exported=pp::read_rgba((palette_run/"extra"/("sprite-"+(std::string)palette_stage["placements"][0]["id"]+".png")).string());assert(!crop.empty()&&crop.size()==exported.size()&&cv::norm(crop,exported,cv::NORM_INF)==0);
    fs::path bad_palette=write_palette(palette_root,false),bad_settings=write_palette_settings(palette_root,settings,bad_palette);assert(!invoke(runner,source,palette_run,bad_settings));assert((int)pp::read_json((palette_run/"extra"/"pixelprocess.manifest.json").string())["revision"]==1);
    bool has_palette_map=false;for(auto node=manifest["nodes"].begin();node!=manifest["nodes"].end();++node)has_palette_map|=(std::string)(*node)["step_id"]=="palette_map";assert(!has_palette_map);
    /* agent: gpt-6 | 2026-10-09 | Verify conditional palette mapping outputs | v 3.5.0 | 63244c */
    std::map<std::string,int> last_progress;
    for (auto node = manifest["nodes"].begin(); node != manifest["nodes"].end(); ++node) {
        int progress = (int)(*node)["progress"];
        std::string branch = (std::string)(*node)["branch_id"];
        if ((std::string)(*node)["step_id"] != "source") assert(progress > last_progress[branch]);
        if ((std::string)(*node)["step_id"] != "source") last_progress[branch] = progress;
        assert(!((std::string)(*node)["id"]).empty());
    }
    { std::ifstream evidence(run / "extra" / "processing.sprites.jsonl"); std::string all((std::istreambuf_iterator<char>(evidence)), {});
      assert(all.find("\"stage_number\":91") != std::string::npos && all.find("\"stage_number\":60") != std::string::npos && all.find("\"stage_number\":2") != std::string::npos); }
    auto ownership = pp::read_json((run / "extra" / "pixelprocess" / "revision-01" / "ownership.json").string());
    assert((int)ownership["grid"]["cols"] == 2 && (int)ownership["grid"]["rows"] == 1);
    fs::path revision = run / "extra" / "pixelprocess" / "revision-01";
    for (const char *folder : {"crops", "logical-sprites", "clean-sprites"}) {
        cv::Mat preview = pp::read_rgba((revision / folder / "sprite-r000-c000.png").string());
        assert(!preview.empty() && preview.cols > 0 && preview.rows > 0);
    }
    cv::Mat bg_preview=pp::read_rgba((run/"extra"/"stage-97-background.png").string());
    cv::Mat diagnosis=pp::read_rgba((run/"extra"/"stage-93-diagnosis.png").string());
    cv::Mat owner_preview=pp::read_rgba((run/"extra"/"stage-94-ownership.png").string());
    cv::Mat lattice_preview=pp::read_rgba((run/"extra"/"stage-80-lattice.png").string());
    /* agent: gpt-6 | 2026-10-07 | Assert lattice reconstruction evidence | v 2.1.0 | 7d7ef8 */
    cv::Mat lattice_debug=pp::read_rgba((run/"extra"/"stage-debug-80-lattice.png").string());
    assert(bg_preview.at<cv::Vec4b>(0,0)[3]==0 && !diagnosis.empty() && diagnosis.size()==pp::read_rgba(source.string()).size());
    assert(cv::countNonZero(owner_preview.reshape(1)!=lattice_preview.reshape(1))>0);
    auto lattice_data=pp::read_json((revision/"lattice.json").string());auto reconstruction_data=pp::read_json((revision/"reconstruction.json").string());
    assert(!lattice_debug.empty()&&lattice_data["sprites"][0]["phase_confidence"].size()==2&&lattice_data["sprites"][0]["supporting_edges_x"].size()>0);
    assert(!reconstruction_data["sprites"][0]["cells"].empty());
    assert(!reconstruction_data["sprites"][0]["cells"][0]["color_confidence"].empty());
    /* agent: gpt-6 | 2026-10-07 | Assert lattice reconstruction evidence | v 2.1.0 | 7d7ef8 */
    fs::path preview = root / "grid-preview.png", preview_meta = root / "grid-preview.json", preview_work = root / "preview-work";
    std::string preview_command = "\"" + runner.string() + "\" --preview-grid 1 --input \"" + source.string() + "\" --mask \"" + (revision / "background-mask.png").string() +
        "\" --settings \"" + settings.string() + "\" --output \"" + preview.string() + "\" --meta \"" + preview_meta.string() + "\" --work-dir \"" + preview_work.string() + "\" --grid_rows 2 --grid_cols 2";
    assert(std::system(preview_command.c_str()) == 0 && fs::is_regular_file(preview) && fs::is_regular_file(preview_meta));
    assert(!fs::exists(preview_work / "extraction"));
    cv::Mat preview_image = pp::read_rgba(preview.string());
    auto preview_data = pp::read_json(preview_meta.string());
    assert(preview_image.size() == pp::read_rgba(source.string()).size() && (int)preview_data["grid"]["rows"] == 2 && (int)preview_data["grid"]["cols"] == 2);
    assert(invoke(runner, source, run, settings));
    manifest = pp::read_json((run / "extra" / "pixelprocess.manifest.json").string());
    assert((int)manifest["revision"] == 2);
    assert(manifest["branches"].size() == 0);
    std::ifstream journal(run / "extra" / "processing.events.jsonl");
    std::string line, contents; while (std::getline(journal, line)) contents += line;
    assert(contents.find("stage_reused") != std::string::npos);
    settings = write_settings(root, "all");
    assert(invoke(runner, source, run, settings));
    manifest = pp::read_json((run / "extra" / "pixelprocess.manifest.json").string());
    assert((int)manifest["revision"] == 3);
    assert(manifest["branches"].size() == 0);
    assert(contents.find("\"schema_version\":2") != std::string::npos);
    assert(contents.find("\"branch_id\":\"primary\"") != std::string::npos);
    settings=write_settings(root,"all","[{\"id\":\"sprite-r000-c000\",\"pitch\":\"4.5\",\"phase_x\":\"auto\",\"phase_y\":\"auto\"}]");
    assert(invoke(runner,source,run,settings));
    auto lattice=pp::read_json((run/"extra"/"pixelprocess"/"revision-04"/"lattice.json").string());
    bool corrected=false,untouched=false;
    for(auto it=lattice["sprites"].begin();it!=lattice["sprites"].end();++it){
        std::string id=(std::string)(*it)["id"];double pitch=(double)(*it)["pitch"];
        if(id=="sprite-r000-c000")corrected=std::abs(pitch-4.5)<0.01;
        if(id=="sprite-r000-c001")untouched=std::abs(pitch-2.0)<0.01;
    }
    assert(corrected&&untouched);
    /* agent: gpt-6 | 2026-10-07 | Assert lattice reconstruction evidence | v 2.1.0 | 7d7ef8 */
    auto fractional_reconstruction=pp::read_json((run/"extra"/"pixelprocess"/"revision-04"/"reconstruction.json").string());bool fractional_bounds=false;
    for(auto sprite=fractional_reconstruction["sprites"].begin();sprite!=fractional_reconstruction["sprites"].end();++sprite)for(auto cell=(*sprite)["cells"].begin();cell!=(*sprite)["cells"].end();++cell){std::vector<double>bounds;(*cell)["source_bounds"]>>bounds;for(double edge:bounds)if(std::abs(edge-std::round(edge))>.01)fractional_bounds=true;}
    assert(fractional_bounds);
    /* agent: gpt-6 | 2026-10-07 | Assert lattice reconstruction evidence | v 2.1.0 | 7d7ef8 */
    fs::path sheet_source=write_textured_sheet(root),sheet_run=root/"sheet-run",auto_settings=root/"auto-settings.json";
    {std::ofstream out(auto_settings);out<<"{\"schema_version\":1,\"background_tolerance\":24,\"grid_cols\":0,\"grid_rows\":0,\"extraction_padding\":1,\"pitch\":\"auto\",\"min_pitch\":2,\"max_pitch\":16,\"pitch_precision\":0.1,\"foreground_coverage\":0.35,\"min_sprite_cells\":4,\"sprite_pitch_tolerance\":5,\"cleanup_mode\":\"outside\",\"cleanup_tolerance\":55,\"sheet_anchor\":\"center\",\"sheet_padding\":1}";}
    assert(invoke(runner,sheet_source,sheet_run,auto_settings));
    auto bg_meta=pp::read_json((sheet_run/"extra"/"pixelprocess"/"revision-01"/"background.stage.json").string());
    auto sheet_owner=pp::read_json((sheet_run/"extra"/"pixelprocess"/"revision-01"/"ownership.json").string());
    auto sheet_separators=pp::read_json((sheet_run/"extra"/"pixelprocess"/"revision-01"/"separators.json").string());
    assert(sheet_separators["horizontal_bands"].size()==3&&sheet_separators["vertical_bands"].size()==3);
    auto sheet_cleanup=pp::read_json((sheet_run/"extra"/"pixelprocess"/"revision-01"/"cleanup.json").string());
    cv::Mat bg_mask=cv::imread((sheet_run/"extra"/"pixelprocess"/"revision-01"/"background-mask.png").string(),cv::IMREAD_GRAYSCALE);
    cv::Mat rebuilt=pp::read_rgba((sheet_run/"final.pixel.png").string());
    assert((int)bg_meta["measurements"]["background_pixels"]>128*128/3);
    assert((int)sheet_owner["grid"]["cols"]==4&&(int)sheet_owner["grid"]["rows"]==4&&sheet_owner["sprites"].size()==16);
    assert((int)sheet_cleanup["measurements"]["removed_pixels"]>0);
    assert(bg_mask.at<uchar>(15,15)==0 && !rebuilt.empty() && rebuilt.at<cv::Vec4b>(0,0)[3]==0);
    auto shared_lattice=pp::read_json((sheet_run/"extra"/"pixelprocess"/"revision-01"/"lattice.json").string());
    auto shared_reconstruction=pp::read_json((sheet_run/"extra"/"pixelprocess"/"revision-01"/"reconstruction.json").string());
    for(auto fit=shared_lattice["sprites"].begin();fit!=shared_lattice["sprites"].end();++fit){
        assert(std::abs((double)(*fit)["pitch_delta_percent"])<=5.01);
        for(auto sprite=shared_reconstruction["sprites"].begin();sprite!=shared_reconstruction["sprites"].end();++sprite)if((std::string)(*fit)["id"]==(std::string)(*sprite)["id"]){
            std::vector<int> fit_dimensions,recon_dimensions;(*fit)["logical_dimensions"]>>fit_dimensions;(*sprite)["dimensions"]>>recon_dimensions;assert(fit_dimensions==recon_dimensions);
        }
    }
    auto graph=pp::read_json((sheet_run/"extra"/"pixelprocess.manifest.json").string());
    auto pitch_selection=pp::read_json((sheet_run/"extra"/"pixelprocess"/"revision-01"/"pitch-selection.json").string());
    assert(pitch_selection.isOpened()&&(std::string)pitch_selection["policy"]=="shared-pitch-harmonic-v1");
    assert((std::string)graph["pitch_selection_summary"]!="unavailable");
    assert((int)graph["schema_version"]==4 && graph["branches"].size()>=1);
    assert((std::string)graph["branches"][0]["fork_step"]=="lattice");
    std::string alternate=(std::string)graph["branches"][0]["final"];
    assert(fs::is_regular_file(alternate));
    bool primary_final_typed=false,alternate_final_typed=false;
    for(auto artifact=graph["artifacts"].begin();artifact!=graph["artifacts"].end();++artifact){
        std::string role=(std::string)(*artifact)["role"],path=(std::string)(*artifact)["path"],branch=(std::string)(*artifact)["branch_id"];
        if(role=="sheet_final"&&branch=="primary")primary_final_typed=true;
        if(role=="sheet_variant"&&path==alternate&&!branch.empty())alternate_final_typed=true;
    }
    assert(primary_final_typed&&alternate_final_typed);
    cv::Mat saved_primary_sheet=rebuilt.clone();std::string selected_scale=(std::string)graph["branches"][0]["id"];
    assert(invoke(runner,sheet_source,sheet_run,auto_settings,"lattice","",selected_scale));
    auto branched_graph=pp::read_json((sheet_run/"extra"/"pixelprocess.manifest.json").string());
    assert(branched_graph["branches"].size()==1);
    bool selected_branch_retained=false,implicit_child=false;std::string previous_final;
    for(auto branch=branched_graph["branches"].begin();branch!=branched_graph["branches"].end();++branch){
        std::string id=(std::string)(*branch)["id"],parent=(std::string)(*branch)["parent_branch"],final=(std::string)(*branch)["final"];
        if(id==selected_scale)selected_branch_retained=true;
        if(parent==selected_scale&&id.find("edit-")==0)implicit_child=true;
    }
    assert(selected_branch_retained&&!implicit_child);
    assert(cv::countNonZero(saved_primary_sheet.reshape(1)!=pp::read_rgba((sheet_run/"final.pixel.png").string()).reshape(1))==0);
    bool alternate_assembly=false,alternate_preview=false;
    for(auto node=graph["nodes"].begin();node!=graph["nodes"].end();++node){
        if((std::string)(*node)["branch_id"]!=(std::string)graph["branches"][0]["id"])continue;
        if((std::string)(*node)["step_id"]=="assemble"&&!((std::string)(*node)["sprite_dir"]).empty())alternate_assembly=true;
        if((std::string)(*node)["step_id"]=="cleanup_preview")alternate_preview=true;
    }
    assert(alternate_assembly&&alternate_preview);
    {std::ifstream evidence(sheet_run/"extra"/"processing.sprites.jsonl");std::string all((std::istreambuf_iterator<char>(evidence)),{});assert(all.find(alternate)!=std::string::npos);}
    {std::ifstream events(sheet_run/"extra"/"processing.events.jsonl");std::string all((std::istreambuf_iterator<char>(events)),{});std::string branch=(std::string)graph["branches"][0]["id"];assert(all.find("\"branch_id\":\""+branch+"\"")!=std::string::npos&&all.find("\"node_id\":\""+branch+"/lattice\"")!=std::string::npos);}
    assert(invoke(runner,sheet_source,sheet_run,auto_settings,"lattice"));
    graph=pp::read_json((sheet_run/"extra"/"pixelprocess.manifest.json").string());
    assert(graph["branches"].size()==1);
    for(auto branch=graph["branches"].begin();branch!=graph["branches"].end();++branch)
        assert((std::string)(*branch)["fork_reason"]!="previous_primary_result"&&(std::string)(*branch)["fork_reason"]!="previous_settings_result");
    {std::ifstream events(sheet_run/"extra"/"processing.events.jsonl");std::string all((std::istreambuf_iterator<char>(events)),{});assert(all.find("\"node_id\":\"primary-before-r02/lattice\"")==std::string::npos);}
    fs::path strict_settings=root/"strict-settings.json",strict_run=root/"strict-run";
    {std::ofstream out(strict_settings);out<<"{\"schema_version\":1,\"background_tolerance\":24,\"grid_cols\":0,\"grid_rows\":0,\"extraction_padding\":1,\"pitch\":\"auto\",\"min_pitch\":2,\"max_pitch\":16,\"pitch_precision\":0.1,\"foreground_coverage\":0.35,\"min_sprite_cells\":16,\"sprite_pitch_tolerance\":5,\"cleanup_mode\":\"outside\",\"cleanup_tolerance\":55,\"sheet_anchor\":\"center\",\"sheet_padding\":1}";}
    assert(invoke(runner,sheet_source,strict_run,strict_settings));
    assert(fs::exists(strict_run/"final.pixel.png"));
    fs::path transparent_source=write_transparent_sprite(root),transparent_run=root/"transparent-run";
    assert(invoke(runner,transparent_source,transparent_run,auto_settings));
    auto transparent_bg=pp::read_json((transparent_run/"extra"/"pixelprocess"/"revision-01"/"background.stage.json").string());
    cv::Mat transparent_mask=cv::imread((transparent_run/"extra"/"pixelprocess"/"revision-01"/"background-mask.png").string(),cv::IMREAD_GRAYSCALE);
    cv::Mat transparent_final=pp::read_rgba((transparent_run/"final.pixel.png").string());
    assert((int)transparent_bg["model"]["border_samples"]==0 && transparent_mask.at<uchar>(12,12)==0);
    assert(!transparent_final.empty() && cv::countNonZero(transparent_final.reshape(1))>0);
    fs::path manual_run=root/"manual-grid-run", manual_settings=write_settings(root,"skip");
    assert(invoke(runner,source,manual_run,manual_settings));
    fs::path primary_final=manual_run/"final.pixel.png";
    cv::Mat saved_primary=pp::read_rgba(primary_final.string());
    manual_settings=write_settings(root,"skip");
    {std::ofstream out(manual_settings);out<<"{\"schema_version\":1,\"background_tolerance\":24,\"grid_cols\":1,\"grid_rows\":2,\"extraction_padding\":1,\"pitch\":\"2\",\"phase_x\":\"auto\",\"phase_y\":\"auto\",\"min_pitch\":2,\"max_pitch\":8,\"pitch_precision\":0.1,\"foreground_coverage\":0.2,\"cleanup_mode\":\"skip\",\"cleanup_tolerance\":55,\"sheet_anchor\":\"center\",\"sheet_padding\":1,\"sprite_overrides\":[],\"ownership_overrides\":{}}";}
    assert(invoke(runner,source,manual_run,manual_settings,"ownership","copy"));
    auto manual_graph=pp::read_json((manual_run/"extra"/"pixelprocess.manifest.json").string());
    assert(manual_graph["branches"].size()==1&&(std::string)manual_graph["branches"][0]["fork_reason"]=="manual_grid_copy");
    assert(fs::is_regular_file((std::string)manual_graph["branches"][0]["final"]));
    /* agent: gpt-6.1-sol | 2026-10-08 | Verify stages begin at branch fork | v 2.8.0 | c792cf */
    std::string first_grid_copy=(std::string)manual_graph["branches"][0]["id"];bool grid_copy_has_ownership=false,grid_copy_has_inherited=false;
    bool grid_copy_has_separators=false;for(auto node=manual_graph["nodes"].begin();node!=manual_graph["nodes"].end();++node)if((std::string)(*node)["branch_id"]==first_grid_copy){std::string step=(std::string)(*node)["step_id"];grid_copy_has_ownership|=step=="ownership";grid_copy_has_separators|=step=="separators";grid_copy_has_inherited|=step=="background";}
    assert(grid_copy_has_ownership&&grid_copy_has_separators&&!grid_copy_has_inherited);
    /* agent: gpt-6.1-sol | 2026-10-08 | Verify stages begin at branch fork | v 2.8.0 | c792cf */
    assert(cv::countNonZero(saved_primary.reshape(1)!=pp::read_rgba(primary_final.string()).reshape(1))==0);
    std::string parent_grid_branch=(std::string)manual_graph["branches"][0]["id"],parent_grid_final=(std::string)manual_graph["branches"][0]["final"];
    assert(invoke(runner,source,manual_run,manual_settings,"ownership","copy",parent_grid_branch));
    manual_graph=pp::read_json((manual_run/"extra"/"pixelprocess.manifest.json").string());
    bool child_grid_branch=false;std::string child_grid_final,child_grid_branch_id;
    for(auto branch=manual_graph["branches"].begin();branch!=manual_graph["branches"].end();++branch)if((std::string)(*branch)["parent_branch"]==parent_grid_branch){child_grid_branch=true;child_grid_branch_id=(std::string)(*branch)["id"];child_grid_final=(std::string)(*branch)["final"];}
    assert(child_grid_branch&&child_grid_final!=parent_grid_final&&fs::is_regular_file(parent_grid_final)&&fs::is_regular_file(child_grid_final));
    std::string child_assemble_before;for(auto node=manual_graph["nodes"].begin();node!=manual_graph["nodes"].end();++node)if((std::string)(*node)["branch_id"]==child_grid_branch_id&&(std::string)(*node)["step_id"]=="assemble")child_assemble_before=(std::string)(*node)["main"];
    assert(invoke(runner,source,manual_run,manual_settings,"ownership","replace",parent_grid_branch));
    auto replaced_graph=pp::read_json((manual_run/"extra"/"pixelprocess.manifest.json").string());
    bool replaced_same_id=false;for(auto branch=replaced_graph["branches"].begin();branch!=replaced_graph["branches"].end();++branch)if((std::string)(*branch)["id"]==parent_grid_branch){replaced_same_id=true;assert((std::string)(*branch)["parent_branch"]=="primary");}
    std::ifstream pin_settings(manual_run/"extra"/"pixelprocess"/"pins"/(parent_grid_branch+".json"));std::string pin_text((std::istreambuf_iterator<char>(pin_settings)),{});
    assert(replaced_same_id&&pin_text.find("\"grid_rows\":2")!=std::string::npos);
    assert(invoke(runner,source,manual_run,manual_settings,"ownership","",parent_grid_branch));
    auto descendant_graph=pp::read_json((manual_run/"extra"/"pixelprocess.manifest.json").string());
    std::string child_assemble_after;for(auto node=descendant_graph["nodes"].begin();node!=descendant_graph["nodes"].end();++node)if((std::string)(*node)["branch_id"]==child_grid_branch_id&&(std::string)(*node)["step_id"]=="assemble")child_assemble_after=(std::string)(*node)["main"];
    assert(!child_assemble_before.empty()&&child_assemble_before==child_assemble_after);
    assert(invoke(runner,source,manual_run,manual_settings,"ownership","replace"));
    manual_graph=pp::read_json((manual_run/"extra"/"pixelprocess.manifest.json").string());
    assert((int)pp::read_json((manual_run/"extra"/"pixelprocess"/"revision-06"/"ownership.json").string())["grid"]["rows"]==2);
    bool manual_copy_retained=false,previous_revision_retained=false;
    for(auto branch=manual_graph["branches"].begin();branch!=manual_graph["branches"].end();++branch){
        manual_copy_retained|=(std::string)(*branch)["fork_reason"]=="manual_grid_copy";
        previous_revision_retained|=(std::string)(*branch)["fork_reason"]=="previous_primary_result";
    }
    assert(manual_copy_retained&&!previous_revision_retained&&manual_graph["branches"].size()>=2);
    assert(invoke(runner,source,manual_run,manual_settings,"all","","primary",true,false));
    manual_graph=pp::read_json((manual_run/"extra"/"pixelprocess.manifest.json").string());
    bool parent_pin_retained=false,child_pin_retained=false;
    for(auto branch=manual_graph["branches"].begin();branch!=manual_graph["branches"].end();++branch){
        std::string id=(std::string)(*branch)["id"];
        parent_pin_retained|=id==parent_grid_branch&&((int)(*branch)["pinned"]!=0);
        child_pin_retained|=id.find("grid-copy-r")==0&&id!=parent_grid_branch&&((int)(*branch)["pinned"]!=0);
    }
    assert(parent_pin_retained&&child_pin_retained&&(int)manual_graph["revision"]>=7);
    fs::path revision_run=root/"fresh-rerun";assert(invoke(runner,source,revision_run,manual_settings));
    cv::Mat original_final=pp::read_rgba((revision_run/"final.pixel.png").string());
    /* agent: gpt-6 | 2026-10-09 | Verify fresh rerun removes stale exports | v 3.3.0 | 1e965b */
    {std::ofstream stale_sheet(revision_run/"extra"/"final.pixel.retired.png");stale_sheet<<"stale";std::ofstream stale_sprite(revision_run/"extra"/"sprite-retired.png");stale_sprite<<"stale";}
    assert(invoke(runner,source,revision_run,manual_settings,"all","","primary",true,false));
    assert(!fs::exists(revision_run/"extra"/"final.pixel.retired.png")&&!fs::exists(revision_run/"extra"/"sprite-retired.png"));
    auto fresh_graph=pp::read_json((revision_run/"extra"/"pixelprocess.manifest.json").string());
    assert((int)fresh_graph["revision"]==2 && fs::is_regular_file(revision_run/"extra"/"pixelprocess"/"revision-02"/"stage-97-background.png"));
    assert(fresh_graph["branches"].size()==0 && fs::is_regular_file(revision_run/"final.pixel.png"));
    assert(fresh_graph["history"].size()==1 && fs::is_regular_file((std::string)fresh_graph["history"][0]));
    auto historical=pp::read_json((std::string)fresh_graph["history"][0]);
    assert((int)historical["revision"]==1 && fs::is_regular_file((std::string)historical["artifacts"][0]["path"]));
    assert(invoke(runner,source,revision_run,manual_settings,"ownership","","primary",false,true));
    fresh_graph=pp::read_json((revision_run/"extra"/"pixelprocess.manifest.json").string());bool primary_child=false;
    for(auto branch=fresh_graph["branches"].begin();branch!=fresh_graph["branches"].end();++branch)if((std::string)(*branch)["parent_branch"]=="primary"&&(std::string)(*branch)["fork_reason"]=="settings_revision")primary_child=true;
    assert(!primary_child&&fs::is_regular_file(revision_run/"final.pixel.png"));
    assert(cv::countNonZero(original_final.reshape(1)!=pp::read_rgba((revision_run/"final.pixel.png").string()).reshape(1))==0);
    fs::path inherited_run=root/"inherited-settings-run", inherited_base=root/"inherited-base.json", inherited_copy=root/"inherited-copy.json", inherited_update=root/"inherited-update.json";
    {std::ofstream out(inherited_base);out<<"{\"schema_version\":1,\"background_tolerance\":24,\"grid_cols\":2,\"grid_rows\":1,\"pitch\":\"2\",\"cleanup_mode\":\"skip\"}";}
    assert(invoke(runner,source,inherited_run,inherited_base));
    {std::ofstream out(inherited_copy);out<<"{\"schema_version\":1,\"background_tolerance\":24,\"grid_cols\":1,\"grid_rows\":2,\"pitch\":\"2\",\"cleanup_mode\":\"skip\"}";}
    assert(invoke(runner,source,inherited_run,inherited_copy,"ownership","copy"));
    auto copy_graph=pp::read_json((inherited_run/"extra"/"pixelprocess.manifest.json").string());std::string copy_id;for(auto branch=copy_graph["branches"].begin();branch!=copy_graph["branches"].end();++branch)if((std::string)(*branch)["fork_reason"]=="manual_grid_copy")copy_id=(std::string)(*branch)["id"];
    assert(!copy_id.empty());int copy_revision=(int)copy_graph["revision"];assert(invoke_pin(runner,source,inherited_run,copy_id,false,copy_revision));
    copy_graph=pp::read_json((inherited_run/"extra"/"pixelprocess.manifest.json").string());assert((int)copy_graph["revision"]==copy_revision+1);bool unpinned=false;for(auto branch=copy_graph["branches"].begin();branch!=copy_graph["branches"].end();++branch)if((std::string)(*branch)["id"]==copy_id)unpinned=(int)(*branch)["pinned"]==0;assert(unpinned);
    assert(!invoke_pin(runner,source,inherited_run,copy_id,true,copy_revision));assert(invoke_pin(runner,source,inherited_run,copy_id,true,copy_revision+1));
    {std::ofstream out(inherited_update);out<<"{\"schema_version\":1,\"background_tolerance\":31,\"grid_cols\":2,\"grid_rows\":1,\"pitch\":\"2\",\"cleanup_mode\":\"skip\"}";}
    assert(invoke(runner,source,inherited_run,inherited_update,"background"));
    auto inherited_graph=pp::read_json((inherited_run/"extra"/"pixelprocess.manifest.json").string());bool inherited_values=false;
    for(auto branch=inherited_graph["branches"].begin();branch!=inherited_graph["branches"].end();++branch)if((std::string)(*branch)["fork_reason"]=="manual_grid_copy"){
        auto effective=pp::read_json((std::string)(*branch)["settings_path"]);inherited_values=(int)effective["background_tolerance"]==31&&(int)effective["grid_cols"]==1&&(int)effective["grid_rows"]==2;
    }
    assert(inherited_values);
    /* agent: gpt-6 | 2026-10-09 | Verify background probe setup branches | v 3.2.0 | 154bb4 */
    fs::path background_run=root/"background-setup-run",background_settings=root/"background-settings.json",background_update=root/"background-update.json";
    {std::ofstream out(background_settings);out<<"{\"schema_version\":1,\"background_tolerance\":24,\"grid_cols\":2,\"grid_rows\":1,\"pitch\":\"2\",\"cleanup_mode\":\"skip\",\"background_probes\":[{\"id\":\"manual-5-6\",\"x\":5,\"y\":6}],\"disabled_background_probes\":[\"auto-r0-c1\"]}";}
    {std::ofstream out(background_update);out<<"{\"schema_version\":1,\"background_tolerance\":31,\"grid_cols\":2,\"grid_rows\":1,\"pitch\":\"2\",\"cleanup_mode\":\"skip\",\"background_probes\":[{\"id\":\"manual-7-8\",\"x\":7,\"y\":8}],\"disabled_background_probes\":[]}";}
    assert(invoke(runner,source,background_run,inherited_base));assert(invoke(runner,source,background_run,background_settings,"background","copy"));
    auto background_graph=pp::read_json((background_run/"extra"/"pixelprocess.manifest.json").string());std::string background_id;
    for(auto branch=background_graph["branches"].begin();branch!=background_graph["branches"].end();++branch)if((std::string)(*branch)["fork_reason"]=="manual_background_copy")background_id=(std::string)(*branch)["id"];
    assert(!background_id.empty()&&(std::string)background_graph["selected_branch"]==background_id&&fs::is_regular_file(background_run/"extra"/"pixelprocess"/"pins"/(background_id+".json")));
    bool owns_background=false,owns_separator=false;for(auto node=background_graph["nodes"].begin();node!=background_graph["nodes"].end();++node)if((std::string)(*node)["branch_id"]==background_id){std::string step=(std::string)(*node)["step_id"];owns_background|=step=="background";owns_separator|=step=="separators";}
    assert(owns_background&&owns_separator);auto background_branch=pp::read_json((background_run/"extra"/"pixelprocess"/"pins"/(background_id+".json")).string());assert(background_branch["background_probes"].size()==1&&(int)background_branch["background_probes"][0]["x"]==5);
    assert(invoke(runner,source,background_run,background_update,"background","replace",background_id));background_graph=pp::read_json((background_run/"extra"/"pixelprocess.manifest.json").string());
    bool replaced_probe=false;for(auto branch=background_graph["branches"].begin();branch!=background_graph["branches"].end();++branch)if((std::string)(*branch)["id"]==background_id){background_branch=pp::read_json((std::string)(*branch)["settings_path"]);replaced_probe=background_branch["background_probes"].size()==1&&(int)background_branch["background_probes"][0]["x"]==7&&(int)background_branch["background_tolerance"]==31;}
    auto background_primary=pp::read_json((background_run/"extra"/"pixelprocess"/"revision-01"/"effective-settings.json").string());
    assert(replaced_probe&&background_primary["background_probes"].empty());
    /* agent: gpt-6 | 2026-10-09 | Verify background probe setup branches | v 3.2.0 | 154bb4 */
    fs::path fork80_run=root/"fork-at-80-run";assert(invoke(runner,source,fork80_run,inherited_base));assert(invoke(runner,source,fork80_run,inherited_copy,"lattice","copy"));
    auto fork80_graph=pp::read_json((fork80_run/"extra"/"pixelprocess.manifest.json").string());std::string fork80_id;for(auto branch=fork80_graph["branches"].begin();branch!=fork80_graph["branches"].end();++branch)if((std::string)(*branch)["fork_reason"]=="manual_lattice_copy")fork80_id=(std::string)(*branch)["id"];
    /* agent: gpt-6.1-sol | 2026-10-08 | Verify stages begin at branch fork | v 2.8.0 | c792cf */
    bool copy_has_lattice=false,copy_has_reconstruction=false,copy_has_inherited=false;
    for(auto node=fork80_graph["nodes"].begin();node!=fork80_graph["nodes"].end();++node)if((std::string)(*node)["branch_id"]==fork80_id){std::string step=(std::string)(*node)["step_id"];copy_has_lattice|=step=="lattice";copy_has_reconstruction|=step=="reconstruct";copy_has_inherited|=step=="background"||step=="ownership"||step=="diagnosis"||step=="extract";}
    assert(!fork80_id.empty()&&copy_has_lattice&&copy_has_reconstruction&&!copy_has_inherited);
    assert(invoke(runner,source,fork80_run,inherited_update,"lattice","replace",fork80_id));
    fork80_graph=pp::read_json((fork80_run/"extra"/"pixelprocess.manifest.json").string());copy_has_lattice=false;copy_has_reconstruction=false;copy_has_inherited=false;
    for(auto node=fork80_graph["nodes"].begin();node!=fork80_graph["nodes"].end();++node)if((std::string)(*node)["branch_id"]==fork80_id){std::string step=(std::string)(*node)["step_id"];copy_has_lattice|=step=="lattice";copy_has_reconstruction|=step=="reconstruct";copy_has_inherited|=step=="background"||step=="ownership"||step=="diagnosis"||step=="extract";}
    assert(copy_has_lattice&&copy_has_reconstruction&&!copy_has_inherited);
    /* agent: gpt-6.1-sol | 2026-10-08 | Verify stages begin at branch fork | v 2.8.0 | c792cf */
    assert(!fork80_id.empty()&&(std::string)fork80_graph["selected_branch"]==fork80_id);assert(invoke(runner,source,fork80_run,inherited_update,"background"));
    fork80_graph=pp::read_json((fork80_run/"extra"/"pixelprocess.manifest.json").string());bool fork_preserved=false;for(auto branch=fork80_graph["branches"].begin();branch!=fork80_graph["branches"].end();++branch)if((std::string)(*branch)["id"]==fork80_id)fork_preserved=(std::string)(*branch)["fork_step"]=="lattice"&&(std::string)(*branch)["parent_branch"]=="primary";
    assert(fork_preserved);
    fs::remove_all(root);
    std::cout << "pixelprocess pipeline checks passed\n";
    return 0;
}

/* agent: gpt-6 | 2026-10-05 | Check per-sprite preview artifacts | v 1.7.0 | e26847 */
/* agent: gpt-6.1-sol | 2026-10-05 | Cover textured backgrounds and stage previews | v 1.4.0 | 5fbea5 */
/* agent: gpt-6 | 2026-10-06 | Verify grid diagnosis artifacts | v 2.0.0 | 07b9cc */
/* agent: gpt-6 | 2026-10-07 | Verify stable setup branch replacement | v 2.4.0 | e97f95 */
/* agent: gpt-6 | 2026-10-07 | Verify atomic revisions and stale writes | v 2.7.0 | 5d5643 */
/* agent: gpt-6.1-sol | 2026-10-09 | Assert final quantization artifacts | v 3.0.0 | e31043 */
/* agent: gpt-6.1-sol | 2026-10-09 | Test cleanup masks and outline stage | v 3.4.0 | a14474 */
