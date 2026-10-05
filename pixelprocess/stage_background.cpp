/*
 * File version: 1.1.0
 * Last updated: 2026-10-05
 * Purpose: Classify transparent and border-connected magenta background.
 * Flow PIXEL-01 step 1: immutable source -> mask/debug/meta; enclosed magenta stays foreground.
 */
/* agent: gpt-6.1-sol | 2026-10-05 | Classify border-connected image background | v 1.1.0 | 0929b8 */
#include "common.hpp"
#include <deque>

/* Validate required stage paths and return input error status or zero. */
static int validate(const pp::Args &a) {
    for (const char *key : {"input", "output", "debug", "mask", "meta"})
        if (!a.has(key)) return pp::input_error(std::string("missing --") + key);
    return 0;
}

/* Run four-neighbor border magenta classification; return process exit status. */
int main(int argc, char **argv) {
    auto a = pp::parse_args(argc, argv); if (int code = validate(a)) return code;
    cv::Mat image = pp::read_rgba(a.get("input")); if (image.empty()) return pp::input_error("cannot decode input image");
    int tolerance = std::max(0, std::min(255, (int)a.number("tolerance", 24)));
    cv::Mat mask(image.rows, image.cols, CV_8UC1, cv::Scalar(0));
    std::deque<cv::Point> queue;
    auto magenta = [&](int x, int y) {
        const auto p = image.at<cv::Vec4b>(y, x);
        if (p[3] == 0) return true;
        int db = (int)p[0] - 255, dg = (int)p[1], dr = (int)p[2] - 255;
        return db * db + dg * dg + dr * dr <= tolerance * tolerance;
    };
    for (int y = 0; y < image.rows; ++y) for (int x : {0, image.cols - 1})
        if (!mask.at<uchar>(y, x) && magenta(x, y)) { mask.at<uchar>(y, x) = 255; queue.emplace_back(x, y); }
    for (int x = 0; x < image.cols; ++x) for (int y : {0, image.rows - 1})
        if (!mask.at<uchar>(y, x) && magenta(x, y)) { mask.at<uchar>(y, x) = 255; queue.emplace_back(x, y); }
    while (!queue.empty()) {
        cv::Point p = queue.front(); queue.pop_front();
        const cv::Point neighbors[] = {{p.x - 1, p.y}, {p.x + 1, p.y}, {p.x, p.y - 1}, {p.x, p.y + 1}};
        for (auto n : neighbors) if (n.x >= 0 && n.y >= 0 && n.x < image.cols && n.y < image.rows &&
            !mask.at<uchar>(n.y, n.x) && magenta(n.x, n.y)) { mask.at<uchar>(n.y, n.x) = 255; queue.push_back(n); }
    }
    for (int y = 0; y < image.rows; ++y) for (int x = 0; x < image.cols; ++x)
        if (image.at<cv::Vec4b>(y, x)[3] == 0) mask.at<uchar>(y, x) = 255;
    if (!pp::write_png_atomic(a.get("output"), image) || !pp::write_png_atomic(a.get("mask"), mask) ||
        !pp::write_png_atomic(a.get("debug"), pp::mask_overlay(image, mask))) return pp::input_error("could not publish background artifacts");
    std::string temp; auto fs = pp::json_writer(a.get("meta"), temp);
    if (!fs.isOpened()) return pp::input_error("cannot write stage metadata");
    int mask_pixels = cv::countNonZero(mask), magenta_count = 0;
    for (int y = 0; y < image.rows; ++y) for (int x = 0; x < image.cols; ++x) if (magenta(x, y)) ++magenta_count;
    bool weak = magenta_count > 0 && mask_pixels < magenta_count;
    fs << "schema_version" << 1 << "stage_id" << "background" << "state" << (weak ? "completed_with_warnings" : "completed");
    fs << "settings" << "{" << "tolerance_rgb_euclidean" << tolerance << "connectivity" << 4 << "}";
    fs << "measurements" << "{" << "background_pixels" << mask_pixels << "magenta_like_pixels" << magenta_count << "}";
    fs << "warnings" << "["; if (weak) fs << "magenta_like_pixels_not_border_connected_preserved"; fs << "]";
    fs << "artifacts" << "{" << "main" << a.get("output") << "debug" << a.get("debug") << "background_mask" << a.get("mask") << "}";
    if (!pp::publish_json(fs, temp, a.get("meta"))) return pp::input_error("could not publish stage metadata");
    std::cout << "completed background mask pixels=" << mask_pixels << '\n'; return 0;
}

/* agent: gpt-6.1-sol | 2026-10-05 | Classify border-connected image background | v 1.1.0 | 0929b8 */
