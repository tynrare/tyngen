/*
 * File version: 2.2.0
 * Last updated: 2026-10-09
 * Purpose: Shared pixelprocess graph, CLI, image, and metadata contracts.
 * Flow PIXEL-02 scope index: execution and publication owner runner.cpp; UI loading and selection owner lib.c; shared schema and step registry owner common.hpp. Stage 22 restoration owner stage_outline_reconstruct.cpp; stage 24/23 cleanup policy owner stage_cleanup_preview.cpp and stage_cleanup.cpp.
 * PIXEL-02 steps 1-7: runner.cpp. App interaction steps 5-7: lib.c. Shared schema validation: common.cpp.
 * PALETTE-MAP-01 canonical steps: runner.cpp; Stage 02 picker and preview: lib.c; processor mapping: stage_palette_map.cpp.
 * agent: gpt-6 | 2026-10-09 | Index optional palette mapping stage | v 2.2.0 | c3878f
 * Flow BPROBE-01 scope index: canonical steps runner.cpp; bootstrap classification stage_background.cpp; enclosed-region probes stage_separators.cpp; probe state and inspector lib.c; shared probe type lib.h.
 * Invariant: source is immutable; progress rows and alternative lanes use stable graph IDs; manifest snapshots carry explicit node and artifact identities.
 * Invariant: the runner alone owns run state; stages only write their result metadata.
 * Flow SEP-01 scope: stage_separators.cpp detects divider bands and fills color-matched background regions; runner.cpp orders outputs; ownership/extraction share masks.
 * Flow PIXEL-01: 1. Background stage -> detect mass and border colors, fill probe-connected regions, and publish probe locations and region evidence.
 * 2. Separators stage -> remove supported divider pixels, complete enclosed background regions, and publish shared masks.
 * 3. Ownership stage -> assign connected blobs to accepted separator cells, retain empty slots, and assign stable sprite IDs.
 * 4. Diagnosis stage -> render resolved bounds, cell IDs, and component ownership for review.
 * 5. Extraction stage -> write isolated crops with bounded background evidence and source transforms.
 * 6. Lattice stage -> fit pitch from color/mask edges; refine sprite phase and record pitch/phase confidence.
 * 7. Reconstruction stage -> area-sample fractional cells and record occupancy/color evidence.
 * 8. Cleanup stage -> apply outside/all/skip from stored evidence; FRINGE-01 steps 1-4 owned by stage_cleanup.cpp.
 * 9. Cleanup preview -> publish matching clear/keep masks and adjustable color/background thresholds.
 * 10. Outline reconstruction -> restore supported deleted magenta cells once; publish the exact restoration mask.
 * 11. Assembly stage -> preserve slots, anchor reconstructed sprites, and publish sheet plus sprite exports.
 * 12. Quantization stage -> reduce colors and publish quantized sheet and crops.
 * 13. Optional palette mapping stage -> map quantized output and publish final sheet and crops.
 * Shared invariant: every stage emits main/debug/metadata; reruns preserve the immutable source.
 * agent: gpt-6 | 2026-10-09 | Index optional palette mapping stage | v 2.2.0 | c3878f
 */
/* agent: gpt-6.1-sol | 2026-10-09 | Index cleanup and outline flow | v 2.1.0 | 7ee854 */
/* agent: gpt-6.1-sol | 2026-10-09 | Index probe ownership and processing flow | v 1.8.0 | 721686 */
/* agent: gpt-6.1-sol | 2026-10-09 | Index separator processing flow | v 2.0.0 | 44b2b5 */
#pragma once
/* agent: gpt-6.1-sol | 2026-10-09 | Index probe ownership and processing flow | v 1.8.0 | 721686 */

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
/* agent: gpt-6.1-sol | 2026-10-06 | Define graph metadata contract | v 1.4.0 | 86b0cc */
/* agent: gpt-6.1-sol | 2026-10-09 | Index separator processing flow | v 2.0.0 | 44b2b5 */
/* agent: gpt-6.1-sol | 2026-10-09 | Index cleanup and outline flow | v 2.1.0 | 7ee854 */
