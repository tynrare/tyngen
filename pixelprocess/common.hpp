/*
 * File version: 1.3.0
 * Last updated: 2026-10-05
 * Purpose: Shared pixelprocess CLI, image, and metadata contracts.
 * Flow INFRA-01: 1. UI -> runner CLI -> isolated manifest and run folders.
 * 2. Runner -> stage executables by artifact path -> validated main/debug/meta outputs.
 * 3. Runner -> merge stage records and sprite facts -> revisioned manifest after each stage.
 * 4. Runner -> publish sheet and sprites -> stable export roles and completion state.
 * Invariant: the runner alone owns run state; stages only write their result metadata.
 * Flow PIXEL-01: 1. Background stage -> learn a border Lab model and classify connected background.
 * 2. Ownership stage -> find connected blobs, retain empty grid cells, assign stable sprite IDs.
 * 3. Extraction stage -> write isolated crops with bounded background evidence and source transforms.
 * 4. Lattice stage -> fit pitch/phase per sprite and record candidates/confidence.
 * 5. Reconstruction stage -> sample shared rounded cell boundaries and record coverage/background/winning-color support.
 * 6. Cleanup stage -> apply outside/all/skip from stored evidence; FRINGE-01 steps 1-4 owned by stage_cleanup.cpp.
 * 7. Assembly stage -> preserve slots, anchor clean sprites, and publish sheet plus sprite exports.
 * Shared invariant: every stage emits main/debug/metadata; reruns preserve the immutable source.
 */
/* agent: gpt-6 | 2026-10-05 | Reference canonical fringe cleanup flow | v 1.3.0 | 6dd378 */
#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <map>
#include <iostream>
#include <string>
#include <vector>

namespace pp {
struct Args {
    std::map<std::string, std::string> values;
    std::vector<std::string> positional;
    bool has(const std::string &key) const;
    std::string get(const std::string &key, const std::string &fallback = "") const;
    double number(const std::string &key, double fallback) const;
};

/* Parse --key value or --key=value arguments; return the argument map. */
Args parse_args(int argc, char **argv);
/* Load an image as 8-bit BGRA and validate its dimensions; return cv::Mat. */
cv::Mat read_rgba(const std::string &path);
/* Atomically write a PNG to path; return true on success. */
bool write_png_atomic(const std::string &path, const cv::Mat &image);
/* Write a JSON object with a schema version to path atomically; return bool. */
/* Create a JSON writer at a temporary sibling path; return FileStorage. */
cv::FileStorage json_writer(const std::string &path, std::string &temporary);
/* Close and atomically publish an OpenCV JSON FileStorage; return bool. */
bool publish_json(cv::FileStorage &storage, const std::string &temporary, const std::string &path);
/* Load object keys from a JSON file; return an open FileStorage. */
cv::FileStorage read_json(const std::string &path);
/* Produce a debug overlay for foreground mask and bounds; return BGRA image. */
cv::Mat mask_overlay(const cv::Mat &image, const cv::Mat &mask);
/* Set a readable error and return exit status 2 for invalid input. */
int input_error(const std::string &message);
/* Return string form of an integer; return std::string. */
std::string to_string(int value);
}
/* agent: gpt-6 | 2026-10-05 | Reference canonical fringe cleanup flow | v 1.3.0 | 6dd378 */
