/*
 * File version: 2.3.0
 * Last updated: 2026-10-07
 * Purpose: Verify graph manifests, cache reuse, and sprite evidence.
 * Flow PIXEL-TEST-01: 1 fixture -> run and inspect artifacts; 2 rerun -> verify cache;
 * 3 settings change -> verify invalidation; 4 sprite override -> verify independent lattice;
 * 5 textured sheet -> verify grid, pink cleanup;
 * 5 transparent sprite -> preserve opaque dark pixels without border color evidence;
 * 6 pipeline -> publish progress nodes and artifact-specific sprite evidence;
 * 7 reconstruction -> expose fractional bounds and per-cell confidence;
 * 8 revisions -> preserve branch finals, enforce pitch quality, and test fresh/child reruns.
 */
/* agent: gpt-6.1-sol | 2026-10-07 | Test fresh and child branch reruns | v 2.3.0 | d3b5d0 */
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

/* Run runner with safely quoted paths, settings, and target; return bool. */
/* Run runner with optional target and grid save mode; return bool. */
/* agent: gpt-6 | 2026-10-06 | Verify grid diagnosis artifacts | v 2.0.0 | 07b9cc */
static bool invoke(const fs::path &runner, const fs::path &input, const fs::path &run, const fs::path &settings, const std::string &target = "all", const std::string &save_mode = "", const std::string &branch = "primary", bool fresh = false, bool branch_revision = false) {
    std::string command = "\"" + runner.string() + "\" --input \"" + input.string() + "\" --run-dir \"" +
                          run.string() + "\" --settings \"" + settings.string() + "\" --target \"" + target + "\"";
    if (!save_mode.empty()) command += " --save-mode \"" + save_mode + "\"";
    if (!save_mode.empty() || branch != "primary") command += " --branch-id \"" + branch + "\"";
    if (fresh) command += " --fresh 1";
    if (branch_revision) command += " --branch-revision 1";
    return std::system(command.c_str()) == 0;
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
    assert((int)manifest["schema_version"] == 2 && (int)manifest["revision"] == 1);
    assert(manifest["stages"].size() == 10 && manifest["nodes"].size() == 10);
    std::map<std::string,int> last_progress;
    for (auto node = manifest["nodes"].begin(); node != manifest["nodes"].end(); ++node) {
        int progress = (int)(*node)["progress"];
        std::string branch = (std::string)(*node)["branch_id"];
        if ((std::string)(*node)["step_id"] != "source") assert(progress > last_progress[branch]);
        if ((std::string)(*node)["step_id"] != "source") last_progress[branch] = progress;
        assert(!((std::string)(*node)["id"]).empty());
    }
    { std::ifstream evidence(run / "extra" / "processing.sprites.jsonl"); std::string all((std::istreambuf_iterator<char>(evidence)), {});
      assert(all.find("\"stage_number\":91") != std::string::npos && all.find("\"stage_number\":60") != std::string::npos); }
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
    assert(manifest["branches"].size() == 1);
    assert((std::string)manifest["branches"][0]["fork_step"] == "background");
    assert((std::string)manifest["branches"][0]["id"] == "settings-r02");
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
    assert((int)graph["schema_version"]==2 && graph["branches"].size()>=1);
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
    assert(branched_graph["branches"].size()>=2);
    bool selected_child=false;std::string previous_final,child_final;
    for(auto branch=branched_graph["branches"].begin();branch!=branched_graph["branches"].end();++branch){
        std::string id=(std::string)(*branch)["id"],parent=(std::string)(*branch)["parent_branch"],final=(std::string)(*branch)["final"];
        if(id==selected_scale)previous_final=final;
        if(parent==selected_scale&&id.find("edit-")==0){selected_child=true;child_final=final;}
    }
    assert(selected_child&&!previous_final.empty()&&!child_final.empty()&&previous_final!=child_final);
    assert(fs::is_regular_file(previous_final)&&fs::is_regular_file(child_final));
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
    bool settings_fork=false;
    for(auto branch=graph["branches"].begin();branch!=graph["branches"].end();++branch)if((std::string)(*branch)["fork_reason"]=="previous_primary_result"){
        assert((std::string)(*branch)["fork_step"]=="lattice");std::string id=(std::string)(*branch)["id"];
        bool prior_sprite_dir=false;for(auto node=graph["nodes"].begin();node!=graph["nodes"].end();++node)if((std::string)(*node)["branch_id"]==id){assert((int)(*node)["progress"]>=4);if((std::string)(*node)["step_id"]=="cleanup")prior_sprite_dir=!((std::string)(*node)["sprite_dir"]).empty();}
        assert(prior_sprite_dir);
        settings_fork=true;
    }
    assert(settings_fork&&graph["branches"].size()>=2);
    {std::ifstream events(sheet_run/"extra"/"processing.events.jsonl");std::string all((std::istreambuf_iterator<char>(events)),{});assert(all.find("\"kind\":\"node_retained\"")!=std::string::npos&&all.find("\"node_id\":\"primary-before-r02/lattice\"")!=std::string::npos);}
    fs::path strict_settings=root/"strict-settings.json",strict_run=root/"strict-run";
    {std::ofstream out(strict_settings);out<<"{\"schema_version\":1,\"background_tolerance\":24,\"grid_cols\":0,\"grid_rows\":0,\"extraction_padding\":1,\"pitch\":\"auto\",\"min_pitch\":2,\"max_pitch\":16,\"pitch_precision\":0.1,\"foreground_coverage\":0.35,\"min_sprite_cells\":16,\"sprite_pitch_tolerance\":5,\"cleanup_mode\":\"outside\",\"cleanup_tolerance\":55,\"sheet_anchor\":\"center\",\"sheet_padding\":1}";}
    assert(!invoke(runner,sheet_source,strict_run,strict_settings));
    assert(!fs::exists(strict_run/"final.pixel.png"));
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
    assert(cv::countNonZero(saved_primary.reshape(1)!=pp::read_rgba(primary_final.string()).reshape(1))==0);
    std::string parent_grid_branch=(std::string)manual_graph["branches"][0]["id"],parent_grid_final=(std::string)manual_graph["branches"][0]["final"];
    assert(invoke(runner,source,manual_run,manual_settings,"ownership","copy",parent_grid_branch));
    manual_graph=pp::read_json((manual_run/"extra"/"pixelprocess.manifest.json").string());
    bool child_grid_branch=false;std::string child_grid_final;
    for(auto branch=manual_graph["branches"].begin();branch!=manual_graph["branches"].end();++branch)if((std::string)(*branch)["parent_branch"]==parent_grid_branch){child_grid_branch=true;child_grid_final=(std::string)(*branch)["final"];}
    assert(child_grid_branch&&child_grid_final!=parent_grid_final&&fs::is_regular_file(parent_grid_final)&&fs::is_regular_file(child_grid_final));
    assert(invoke(runner,source,manual_run,manual_settings,"ownership","replace"));
    manual_graph=pp::read_json((manual_run/"extra"/"pixelprocess.manifest.json").string());
    assert((int)pp::read_json((manual_run/"extra"/"pixelprocess"/"revision-04"/"ownership.json").string())["grid"]["rows"]==2);
    bool manual_copy_retained=false,previous_revision_retained=false;
    for(auto branch=manual_graph["branches"].begin();branch!=manual_graph["branches"].end();++branch){
        manual_copy_retained|=(std::string)(*branch)["fork_reason"]=="manual_grid_copy";
        previous_revision_retained|=(std::string)(*branch)["fork_reason"]=="previous_primary_result";
    }
    assert(manual_copy_retained&&previous_revision_retained&&manual_graph["branches"].size()>=3);
    fs::path revision_run=root/"fresh-rerun";assert(invoke(runner,source,revision_run,manual_settings));
    cv::Mat original_final=pp::read_rgba((revision_run/"final.pixel.png").string());
    assert(invoke(runner,source,revision_run,manual_settings,"all","","primary",true,false));
    auto fresh_graph=pp::read_json((revision_run/"extra"/"pixelprocess.manifest.json").string());
    assert((int)fresh_graph["revision"]==2 && fs::is_regular_file(revision_run/"extra"/"pixelprocess"/"revision-02"/"stage-97-background.png"));
    bool retained_final=false;for(auto branch=fresh_graph["branches"].begin();branch!=fresh_graph["branches"].end();++branch)if((std::string)(*branch)["fork_reason"]=="previous_primary_result")retained_final=fs::is_regular_file((std::string)(*branch)["final"]);
    assert(retained_final && fs::is_regular_file(revision_run/"final.pixel.png"));
    assert(invoke(runner,source,revision_run,manual_settings,"ownership","","primary",false,true));
    fresh_graph=pp::read_json((revision_run/"extra"/"pixelprocess.manifest.json").string());bool primary_child=false;std::string child_path;
    for(auto branch=fresh_graph["branches"].begin();branch!=fresh_graph["branches"].end();++branch)if((std::string)(*branch)["parent_branch"]=="primary"&&(std::string)(*branch)["fork_reason"]=="settings_revision"){primary_child=true;child_path=(std::string)(*branch)["final"];}
    assert(primary_child&&!child_path.empty()&&fs::is_regular_file(child_path));
    assert(cv::countNonZero(original_final.reshape(1)!=pp::read_rgba((revision_run/"final.pixel.png").string()).reshape(1))==0);
    fs::remove_all(root);
    std::cout << "pixelprocess pipeline checks passed\n";
    return 0;
}

/* agent: gpt-6 | 2026-10-05 | Check per-sprite preview artifacts | v 1.7.0 | e26847 */
/* agent: gpt-6.1-sol | 2026-10-05 | Cover textured backgrounds and stage previews | v 1.4.0 | 5fbea5 */
/* agent: gpt-6 | 2026-10-06 | Verify grid diagnosis artifacts | v 2.0.0 | 07b9cc */
