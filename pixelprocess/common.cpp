/*
 * File version: 1.1.0
 * Last updated: 2026-10-05
 * Purpose: Shared pixelprocess CLI, image, and metadata helpers.
 */
/* agent: gpt-6.1-sol | 2026-10-05 | Add shared pixelprocess file helpers | v 1.1.0 | 129e87 */
#include "common.hpp"
#include <filesystem>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;
namespace pp {
bool Args::has(const std::string &key) const { return values.count(key) != 0; }
std::string Args::get(const std::string &key, const std::string &fallback) const {
    auto it = values.find(key); return it == values.end() ? fallback : it->second;
}
double Args::number(const std::string &key, double fallback) const {
    auto it = values.find(key); if (it == values.end()) return fallback;
    try { size_t used = 0; double value = std::stod(it->second, &used); return used == it->second.size() ? value : fallback; }
    catch (...) { return fallback; }
}
/* Parse --key value or --key=value arguments; return Args. */
Args parse_args(int argc, char **argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string token(argv[i]);
        if (token.rfind("--", 0) != 0) { args.positional.push_back(token); continue; }
        token.erase(0, 2); auto split = token.find('=');
        if (split != std::string::npos) args.values[token.substr(0, split)] = token.substr(split + 1);
        else if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) args.values[token] = argv[++i];
        else args.values[token] = "true";
    }
    return args;
}
/* Load an image as 8-bit BGRA and validate its dimensions; return cv::Mat. */
cv::Mat read_rgba(const std::string &path) {
    cv::Mat image = cv::imread(path, cv::IMREAD_UNCHANGED);
    if (image.empty()) return {};
    if (image.depth() != CV_8U) image.convertTo(image, CV_8U);
    if (image.channels() == 1) cv::cvtColor(image, image, cv::COLOR_GRAY2BGRA);
    else if (image.channels() == 3) cv::cvtColor(image, image, cv::COLOR_BGR2BGRA);
    else if (image.channels() == 4) image = image.clone();
    else return {};
    return image;
}
/* Atomically write a PNG to path; return true on success. */
bool write_png_atomic(const std::string &path, const cv::Mat &image) {
    if (image.empty()) return false;
    fs::path target(path); std::error_code ec;
    if (!target.parent_path().empty()) fs::create_directories(target.parent_path(), ec);
    fs::path temporary = target.parent_path() / (target.stem().string() + ".tmp" + target.extension().string());
    if (!cv::imwrite(temporary.string(), image)) return false;
    fs::rename(temporary, target, ec);
    if (ec) { fs::remove(target, ec); ec.clear(); fs::rename(temporary, target, ec); }
    return !ec;
}
/* Create a JSON writer at a temporary sibling path; return FileStorage. */
cv::FileStorage json_writer(const std::string &path, std::string &temporary) {
    fs::path target(path); if (!target.parent_path().empty()) fs::create_directories(target.parent_path());
    temporary = (target.parent_path() / (target.stem().string() + ".tmp.json")).string();
    return cv::FileStorage(temporary, cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
}
/* Close and atomically publish an OpenCV JSON FileStorage; return bool. */
bool publish_json(cv::FileStorage &storage, const std::string &temporary, const std::string &path) {
    storage.release(); std::error_code ec; fs::rename(temporary, path, ec);
    if (ec) { fs::remove(path, ec); ec.clear(); fs::rename(temporary, path, ec); }
    return !ec;
}
/* Load object keys from a JSON file; return an open FileStorage. */
cv::FileStorage read_json(const std::string &path) { return cv::FileStorage(path, cv::FileStorage::READ); }
/* Produce a debug overlay for foreground mask; return BGRA image. */
cv::Mat mask_overlay(const cv::Mat &image, const cv::Mat &mask) {
    cv::Mat out = image.clone();
    for (int y = 0; y < out.rows; ++y) for (int x = 0; x < out.cols; ++x) {
        if (mask.at<uchar>(y, x)) {
            auto &p = out.at<cv::Vec4b>(y, x); p[0] = (uchar)((p[0] + 255) / 2); p[1] = (uchar)((p[1] + 190) / 2); p[2] = (uchar)(p[2] / 2); p[3] = 255;
        }
    }
    return out;
}
/* Set a readable error and return exit status 2 for invalid input. */
int input_error(const std::string &message) { std::cerr << "pixelprocess: " << message << '\n'; return 2; }
/* Return string form of an integer; return std::string. */
std::string to_string(int value) { return std::to_string(value); }
}

/* agent: gpt-6.1-sol | 2026-10-05 | Add shared pixelprocess file helpers | v 1.1.0 | 129e87 */
