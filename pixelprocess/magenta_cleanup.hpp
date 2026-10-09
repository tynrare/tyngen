/*
 * File version: 1.0.0
 * Last updated: 2026-10-09
 * Purpose: Share evidence-guided magenta cleanup decisions.
 * Flow MAGENTA-01:
 * 1. Cleanup/preview -> shaded-color classifier and cell evidence -> majority-background decisions.
 * 2. Cleanup/preview -> support-eligible magenta cells and sprite edge -> frozen exterior-connected seeds.
 * 3. Cleanup/preview -> frozen seeds and one neighbor ring -> removals plus per-pixel reasons.
 * Branch: all mode -> bright-magenta RGB distance only; skip mode -> no magenta removals.
 * Invariants: opaque pixels only; no cross-sprite traversal or recursive fringe growth;
 * missing evidence preserves pixels; all mode matches bright magenta by RGB distance.
 */
/* agent: gpt-6.1-sol | 2026-10-09 | Share shaded magenta cleanup decisions | v 1.0.0 | 4a3d94 */
#ifndef PIXELPROCESS_MAGENTA_CLEANUP_HPP
#define PIXELPROCESS_MAGENTA_CLEANUP_HPP
#include "common.hpp"
#include <cmath>
#include <limits>

namespace pp {
enum CleanupReason : uchar { CleanupKeep=0, CleanupBackground=1, CleanupSeed=2, CleanupFringe=3, CleanupAll=4 };
struct CleanupScratch {
    std::vector<double> background, winner;
    std::vector<cv::Point> queue, seeds;
    cv::Mat reason, eligible, exterior;
};
struct CleanupCounts { int background=0, seeds=0, fringe=0, all=0, retained=0; };

/* Match shaded magenta cv::Vec4b p within integer line distance tolerance; return bool. */
inline bool shaded_magenta(const cv::Vec4b &p,int tolerance) {
    if(p[3]<=127 || std::min(p[2],p[0])-p[1]<20 || p[2]+p[0]-2*p[1]<90)return false;
    int rb=(int)p[2]-(int)p[0],g=p[1];
    return rb*rb+2*g*g<=2*tolerance*tolerance;
}

/* Classify const cv::Mat image and cv::FileNode cells with mode/tolerance/support; reuse CleanupScratch and return CleanupCounts. */
inline CleanupCounts decide_magenta_cleanup(const cv::Mat &image,const cv::FileNode &cells,const std::string &mode,
                                             int tolerance,double support_min,CleanupScratch &scratch) {
    const size_t size=image.total();const double missing=std::numeric_limits<double>::quiet_NaN();
    scratch.background.assign(size,missing);scratch.winner.assign(size,missing);
    scratch.reason.create(image.size(),CV_8U);scratch.reason.setTo(0);
    scratch.eligible.create(image.size(),CV_8U);scratch.eligible.setTo(0);
    scratch.exterior.create(image.size(),CV_8U);scratch.exterior.setTo(0);
    scratch.queue.clear();scratch.seeds.clear();CleanupCounts counts;
    for(auto cell:cells){
        auto xy=cell["logical_cell"];if(xy.size()!=2)continue;
        int x=(int)xy[0],y=(int)xy[1];if(x<0||y<0||x>=image.cols||y>=image.rows)continue;
        size_t i=(size_t)y*image.cols+x;double bg=cell["background_support"],win=cell["winning_color_support"];
        double fg=(double)cell["foreground_occupancy"];
        bool has_fg=!cell["foreground_occupancy"].empty();
        if(!cell["background_support"].empty())scratch.background[i]=bg;
        if(!cell["winning_color_support"].empty())scratch.winner[i]=win;
        if(mode=="outside"&&image.at<cv::Vec4b>(y,x)[3]&&std::isfinite(bg)&&has_fg&&std::isfinite(fg)&&bg>=.5&&fg<.5)
            scratch.reason.at<uchar>(y,x)=CleanupBackground;
    }
    if(mode=="all"){
        for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++){
            const auto &p=image.at<cv::Vec4b>(y,x);if(p[3]<=127)continue;
            int db=(int)p[0]-255,dg=p[1],dr=(int)p[2]-255;
            if(db*db+dg*dg+dr*dr<=tolerance*tolerance)scratch.reason.at<uchar>(y,x)=CleanupAll;
        }
    } else if(mode=="outside") {
        for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++){
            if(scratch.reason.at<uchar>(y,x)||!shaded_magenta(image.at<cv::Vec4b>(y,x),tolerance))continue;
            size_t i=(size_t)y*image.cols+x;double bg=scratch.background[i],win=scratch.winner[i];
            if(!std::isfinite(bg)||!std::isfinite(win)||bg<support_min||win>=.5||win>bg)continue;
            scratch.eligible.at<uchar>(y,x)=255;
        }
        static const cv::Point offsets[]={{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}};
        for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++)if(x==0||y==0||x==image.cols-1||y==image.rows-1){
            if(scratch.eligible.at<uchar>(y,x)) {scratch.exterior.at<uchar>(y,x)=255;scratch.queue.emplace_back(x,y);}
            else if(!image.at<cv::Vec4b>(y,x)[3])scratch.queue.emplace_back(x,y);
        }
        for(size_t head=0;head<scratch.queue.size();head++){
            auto p=scratch.queue[head];for(auto d:offsets){cv::Point q=p+d;if(q.x<0||q.y<0||q.x>=image.cols||q.y>=image.rows||scratch.exterior.at<uchar>(q))continue;
                if(scratch.eligible.at<uchar>(q)){scratch.exterior.at<uchar>(q)=255;scratch.queue.push_back(q);}
                else if(!image.at<cv::Vec4b>(q)[3]){scratch.exterior.at<uchar>(q)=255;scratch.queue.push_back(q);}
            }
        }
        for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++)if(scratch.eligible.at<uchar>(y,x)&&scratch.exterior.at<uchar>(y,x)){
            scratch.reason.at<uchar>(y,x)=CleanupSeed;scratch.seeds.emplace_back(x,y);
        }
        for(auto seed:scratch.seeds)for(auto d:offsets){cv::Point q=seed+d;if(q.x<0||q.y<0||q.x>=image.cols||q.y>=image.rows||scratch.reason.at<uchar>(q)||!shaded_magenta(image.at<cv::Vec4b>(q),tolerance))continue;
            size_t i=(size_t)q.y*image.cols+q.x;double bg=scratch.background[i],win=scratch.winner[i];
            if(!std::isfinite(bg)||!std::isfinite(win)||bg<support_min*.5||win>=.5)continue;
            scratch.reason.at<uchar>(q)=CleanupFringe;
        }
    }
    for(int y=0;y<image.rows;y++)for(int x=0;x<image.cols;x++){
        switch(scratch.reason.at<uchar>(y,x)){
        case CleanupBackground:counts.background++;break;
        case CleanupSeed:counts.seeds++;break;
        case CleanupFringe:counts.fringe++;break;
        case CleanupAll:counts.all++;break;
        default:if(shaded_magenta(image.at<cv::Vec4b>(y,x),tolerance))counts.retained++;break;
        }
    }
    return counts;
}
}
#endif
/* agent: gpt-6.1-sol | 2026-10-09 | Share shaded magenta cleanup decisions | v 1.0.0 | 4a3d94 */
