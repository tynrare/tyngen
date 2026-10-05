/*
 * File version: 1.1.0
 * Last updated: 2026-10-05
 * Purpose: Detect foreground blobs and assign stable spritesheet cell owners.
 * Flow PIXEL-01 step 2: mask -> grid slots/components; empty cells and small blobs remain recorded.
 */
/* agent: gpt-6.1-sol | 2026-10-05 | Assign foreground blobs to sprite cells | v 1.1.0 | 8d79a8 */
#include "common.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>

/* Find clear projection lanes and return half-open cell edges. */
static std::vector<int> lane_edges(const cv::Mat &mask, bool vertical, int requested) {
    int length = vertical ? mask.cols : mask.rows;
    std::vector<int> edges{0};
    if (requested > 0) {
        for (int i = 1; i < requested; ++i) edges.push_back((int)std::lround((double)length * i / requested));
        edges.push_back(length); return edges;
    }
    std::vector<int> projection((size_t)length);
    for (int i = 0; i < length; ++i) {
        cv::Mat slice = vertical ? mask.col(i) : mask.row(i);
        projection[(size_t)i] = cv::countNonZero(slice);
    }
    std::vector<std::pair<int,int>> gaps;
    int begin = -1;
    for (int i = 0; i <= length; ++i) {
        bool empty = i < length && projection[(size_t)i] == 0;
        if (empty && begin < 0) begin = i;
        if (!empty && begin >= 0) { if (i - begin >= 2) gaps.emplace_back(begin, i); begin = -1; }
    }
    for (auto [lo, hi] : gaps) edges.push_back((lo + hi) / 2);
    edges.push_back(length); std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    return edges;
}

/* Run connected-component ownership and emit sprite/cell records; return exit status. */
int main(int argc, char **argv) {
    auto a = pp::parse_args(argc, argv);
    for (const char *key : {"input", "mask", "output", "debug", "meta"}) if (!a.has(key)) return pp::input_error(std::string("missing --") + key);
    cv::Mat image = pp::read_rgba(a.get("input")), mask = cv::imread(a.get("mask"), cv::IMREAD_GRAYSCALE);
    if (image.empty() || mask.empty() || image.size() != mask.size()) return pp::input_error("input and background mask must be readable and equal-sized");
    cv::Mat foreground; cv::compare(mask, 0, foreground, cv::CMP_EQ);
    cv::Mat labels, stats, centers; int count = cv::connectedComponentsWithStats(foreground, labels, stats, centers, 8, CV_32S);
    auto xs = lane_edges(foreground, true, (int)a.number("grid_cols", 0));
    auto ys = lane_edges(foreground, false, (int)a.number("grid_rows", 0));
    int cols = (int)xs.size() - 1, rows = (int)ys.size() - 1;
    std::vector<int> owner((size_t)count, 0);
    for (int label = 1; label < count; ++label) {
        int cx = (int)std::floor(centers.at<double>(label, 0)), cy = (int)std::floor(centers.at<double>(label, 1));
        int col = std::clamp((int)(std::upper_bound(xs.begin(), xs.end(), cx) - xs.begin()) - 1, 0, cols - 1);
        int row = std::clamp((int)(std::upper_bound(ys.begin(), ys.end(), cy) - ys.begin()) - 1, 0, rows - 1);
        owner[(size_t)label] = row * cols + col;
    }
    auto settings=pp::read_json(a.get("settings"));auto overrides=settings["ownership_overrides"];
    for(int label=1;label<count;label++)for(auto oi=overrides.begin();oi!=overrides.end();++oi){
        cv::FileNode entry=*oi;std::string blob=entry.name();if(blob!="blob-"+std::to_string(label))continue;
        std::string target=(std::string)entry;int row=-1,col=-1;
        if(sscanf(target.c_str(),"sprite-r%d-c%d",&row,&col)==2&&row>=0&&row<rows&&col>=0&&col<cols)owner[(size_t)label]=row*cols+col;
    }
    cv::Mat debug = image.clone(); std::string temp; auto fs = pp::json_writer(a.get("meta"), temp);
    if (!fs.isOpened()) return pp::input_error("cannot write stage metadata");
    fs << "schema_version" << 1 << "stage_id" << "ownership" << "state" << "completed";
    fs << "coordinate_space" << "source_pixels" << "bounds_convention" << "half_open";
    fs << "grid" << "{" << "rows" << rows << "cols" << cols << "x_edges" << "["; for (int x : xs) fs << x; fs << "]" << "y_edges" << "["; for (int y : ys) fs << y; fs << "]" << "}";
    fs << "sprites" << "[";
    for (int slot = 0; slot < rows * cols; ++slot) {
        int row = slot / cols, col = slot % cols, x0 = xs[(size_t)col], x1 = xs[(size_t)col + 1], y0 = ys[(size_t)row], y1 = ys[(size_t)row + 1];
        int bx0 = image.cols, by0 = image.rows, bx1 = 0, by1 = 0, pixels = 0;
        for (int label = 1; label < count; ++label) if (owner[(size_t)label] == slot) {
            int x = stats.at<int>(label, cv::CC_STAT_LEFT), y = stats.at<int>(label, cv::CC_STAT_TOP);
            int w = stats.at<int>(label, cv::CC_STAT_WIDTH), h = stats.at<int>(label, cv::CC_STAT_HEIGHT);
            bx0 = std::min(bx0, x); by0 = std::min(by0, y); bx1 = std::max(bx1, x + w); by1 = std::max(by1, y + h); pixels += stats.at<int>(label, cv::CC_STAT_AREA);
        }
        bool occupied = pixels > 0; if (!occupied) { bx0 = x0; by0 = y0; bx1 = x0; by1 = y0; }
        std::ostringstream id; id << "sprite-r" << std::setw(3) << std::setfill('0') << row << "-c" << std::setw(3) << col;
        fs << "{" << "id" << id.str() << "row" << row << "col" << col << "occupied" << occupied << "owned_pixels" << pixels;
        fs << "cell_bounds" << "[" << x0 << y0 << x1 << y1 << "]" << "source_bounds" << "[" << bx0 << by0 << bx1 << by1 << "]";
        fs << "components" << "[";
        int local_component = 0;
        for (int label = 1; label < count; ++label) if (owner[(size_t)label] == slot) {
            cv::Rect b(stats.at<int>(label, cv::CC_STAT_LEFT), stats.at<int>(label, cv::CC_STAT_TOP), stats.at<int>(label, cv::CC_STAT_WIDTH), stats.at<int>(label, cv::CC_STAT_HEIGHT));
            fs << "{" << "id" << (std::string("blob-") + std::to_string(label)) << "bounds" << "[" << b.x << b.y << b.x + b.width << b.y + b.height << "]" << "pixels" << stats.at<int>(label, cv::CC_STAT_AREA) << "}";
            ++local_component;
            cv::Scalar color((label * 53) % 220 + 25, (label * 97) % 220 + 25, (label * 131) % 220 + 25, 255);
            cv::rectangle(debug, b, color, 1);
        }
        fs << "]" << "}";
        cv::rectangle(debug, cv::Rect(x0, y0, std::max(1, x1-x0), std::max(1, y1-y0)), cv::Scalar(0, 210, 255, 255), 1);
    }
    fs << "]" << "measurements" << "{" << "component_count" << count - 1 << "slot_count" << rows * cols << "}";
    fs << "warnings" << "["; if ((int)a.number("grid_cols", 0) == 0 && cols == 1 && rows == 1 && count > 2) fs << "multiple_components_grouped_in_single_inferred_cell"; fs << "]";
    fs << "artifacts" << "{" << "main" << a.get("output") << "debug" << a.get("debug") << "}";
    if (!pp::write_png_atomic(a.get("output"), image) || !pp::write_png_atomic(a.get("debug"), debug) || !pp::publish_json(fs, temp, a.get("meta"))) return pp::input_error("could not publish ownership result");
    std::cout << "completed ownership components=" << count - 1 << " cells=" << rows << 'x' << cols << '\n'; return 0;
}

/* agent: gpt-6.1-sol | 2026-10-05 | Assign foreground blobs to sprite cells | v 1.1.0 | 8d79a8 */
