/*
 * File version: 1.4.0
 * Last updated: 2026-10-05
 * Purpose: Verify conversion, cache reuse, and textured-background sprite detection.
 * Flow PIXEL-TEST-01: 1 fixture -> run and inspect artifacts; 2 rerun -> verify cache;
 * 3 settings change -> verify invalidation; 4 textured sheet -> verify grid, pink cleanup;
 * 5 transparent sprite -> preserve opaque dark pixels without border color evidence.
 */
/* agent: gpt-6.1-sol | 2026-10-05 | Cover textured backgrounds and stage previews | v 1.4.0 | 5fbea5 */
#include "../common.hpp"
#undef NDEBUG
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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
static fs::path write_settings(const fs::path &root, const std::string &cleanup) {
    fs::path path = root / "settings.json";
    std::ofstream out(path);
    out << "{\"schema_version\":1,\"background_tolerance\":24,\"grid_cols\":2,\"grid_rows\":1,"
        << "\"extraction_padding\":1,\"pitch\":\"2\",\"phase_x\":\"auto\",\"phase_y\":\"auto\","
        << "\"min_pitch\":2,\"max_pitch\":8,\"pitch_precision\":0.1,\"foreground_coverage\":0.2,"
        << "\"cleanup_mode\":\"" << cleanup << "\",\"cleanup_tolerance\":55,\"sheet_anchor\":\"center\","
        << "\"sheet_padding\":1,\"sprite_overrides\":[],\"ownership_overrides\":{}}";
    out.close();
    assert(out.good());
    return path;
}

/* Run a runner command with safely quoted paths and return success. */
static bool invoke(const fs::path &runner, const fs::path &input, const fs::path &run, const fs::path &settings) {
    std::string command = "\"" + runner.string() + "\" --input \"" + input.string() + "\" --run-dir \"" +
                          run.string() + "\" --settings \"" + settings.string() + "\"";
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
    cv::Mat final = pp::read_rgba((run / "final.pixel.png").string());
    assert(!final.empty() && final.cols >= 2 && final.rows >= 1);
    auto manifest = pp::read_json((run / "extra" / "pixelprocess.manifest.json").string());
    assert(manifest.isOpened() && (std::string)manifest["run_state"] == "completed");
    assert((int)manifest["revision"] == 1 && manifest["stages"].size() == 8);
    auto ownership = pp::read_json((run / "extra" / "pixelprocess" / "revision-01" / "ownership.json").string());
    assert((int)ownership["grid"]["cols"] == 2 && (int)ownership["grid"]["rows"] == 1);
    cv::Mat bg_preview=pp::read_rgba((run/"extra"/"stage-97-background.png").string());
    cv::Mat owner_preview=pp::read_rgba((run/"extra"/"stage-94-ownership.png").string());
    cv::Mat lattice_preview=pp::read_rgba((run/"extra"/"stage-80-lattice.png").string());
    assert(bg_preview.at<cv::Vec4b>(0,0)[3]==0 && cv::countNonZero(owner_preview.reshape(1)!=lattice_preview.reshape(1))>0);
    assert(invoke(runner, source, run, settings));
    manifest = pp::read_json((run / "extra" / "pixelprocess.manifest.json").string());
    assert((int)manifest["revision"] == 2);
    std::ifstream journal(run / "extra" / "processing.events.jsonl");
    std::string line, contents; while (std::getline(journal, line)) contents += line;
    assert(contents.find("stage_reused") != std::string::npos);
    settings = write_settings(root, "all");
    assert(invoke(runner, source, run, settings));
    manifest = pp::read_json((run / "extra" / "pixelprocess.manifest.json").string());
    assert((int)manifest["revision"] == 3);
    fs::path sheet_source=write_textured_sheet(root),sheet_run=root/"sheet-run",auto_settings=root/"auto-settings.json";
    {std::ofstream out(auto_settings);out<<"{\"schema_version\":1,\"background_tolerance\":24,\"grid_cols\":0,\"grid_rows\":0,\"extraction_padding\":1,\"pitch\":\"auto\",\"min_pitch\":2,\"max_pitch\":16,\"pitch_precision\":0.1,\"foreground_coverage\":0.35,\"cleanup_mode\":\"outside\",\"cleanup_tolerance\":55,\"sheet_anchor\":\"center\",\"sheet_padding\":1}";}
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
    fs::path transparent_source=write_transparent_sprite(root),transparent_run=root/"transparent-run";
    assert(invoke(runner,transparent_source,transparent_run,auto_settings));
    auto transparent_bg=pp::read_json((transparent_run/"extra"/"pixelprocess"/"revision-01"/"background.stage.json").string());
    cv::Mat transparent_mask=cv::imread((transparent_run/"extra"/"pixelprocess"/"revision-01"/"background-mask.png").string(),cv::IMREAD_GRAYSCALE);
    cv::Mat transparent_final=pp::read_rgba((transparent_run/"final.pixel.png").string());
    assert((int)transparent_bg["model"]["border_samples"]==0 && transparent_mask.at<uchar>(12,12)==0);
    assert(!transparent_final.empty() && cv::countNonZero(transparent_final.reshape(1))>0);
    fs::remove_all(root);
    std::cout << "pixelprocess pipeline checks passed\n";
    return 0;
}

/* agent: gpt-6.1-sol | 2026-10-05 | Cover textured backgrounds and stage previews | v 1.4.0 | 5fbea5 */
