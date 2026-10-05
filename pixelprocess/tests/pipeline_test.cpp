/*
 * File version: 1.3.0
 * Last updated: 2026-10-05
 * Purpose: Verify conversion, reuse, settings reruns, and exported sprites.
 * Flow PIXEL-TEST-01: fixture -> initial run -> cached rerun -> changed-settings rerun.
 */
/* agent: gpt-6.1-sol | 2026-10-05 | Test conversion cache and setting reruns | v 1.3.0 | 02ceea */
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

/* Exercise the complete pipeline and its cache invalidation; return process status. */
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
    fs::remove_all(root);
    std::cout << "pixelprocess pipeline checks passed\n";
    return 0;
}

/* agent: gpt-6.1-sol | 2026-10-05 | Test conversion cache and setting reruns | v 1.3.0 | 02ceea */
