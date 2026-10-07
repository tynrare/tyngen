/*
 * File version: 1.0.0
 * Last updated: 2026-10-06
 * Purpose: Show resolved grid cells and component ownership on the source sheet.
 * Flow PIXEL-01 step 2a: ownership -> reviewable diagnosis -> extraction.
 */
/* agent: gpt-6 | 2026-10-06 | Show grid ownership diagnosis | v 1.0.0 | 244c9e */
#include "common.hpp"
#include <algorithm>
#include <cstdio>

/* Render ownership cells on the source and publish diagnosis metadata; return int status. */
int main(int argc,char**argv){
    auto a=pp::parse_args(argc,argv);
    for(const char*key:{"input","state","output","debug","meta"})if(!a.has(key))return pp::input_error(std::string("missing --")+key);
    cv::Mat source=pp::read_rgba(a.get("input"));auto ownership=pp::read_json(a.get("state"));
    if(source.empty()||!ownership.isOpened())return pp::input_error("source or ownership metadata is unavailable");
    cv::Mat preview=source.clone(),debug=source.clone();int rows=(int)ownership["grid"]["rows"],cols=(int)ownership["grid"]["cols"];
    std::vector<int>y_edges;ownership["grid"]["y_edges"]>>y_edges;auto local=ownership["grid"]["row_x_edges"];auto column_local=ownership["grid"]["column_y_edges"];
    if(rows<=0||cols<=0||y_edges.size()!=(size_t)rows+1)return pp::input_error("ownership grid dimensions are invalid");
    std::string temporary;auto meta=pp::json_writer(a.get("meta"),temporary);if(!meta.isOpened())return pp::input_error("cannot write diagnosis metadata");
    meta<<"schema_version"<<1<<"stage_id"<<"diagnosis"<<"state"<<(std::string)ownership["state"]<<"coordinate_space"<<"source_pixels"<<"bounds_convention"<<"half_open";
    meta<<"source_dimensions"<<"["<<source.cols<<source.rows<<"]"<<"ownership_metadata"<<a.get("state");
    std::vector<int>x_edges,y_values;ownership["grid"]["x_edges"]>>x_edges;ownership["grid"]["y_edges"]>>y_values;
    meta<<"grid"<<"{"<<"rows"<<rows<<"cols"<<cols<<"inference"<<(std::string)ownership["grid"]["inference"]<<"x_edges"<<"[";for(int v:x_edges)meta<<v;meta<<"]"<<"y_edges"<<"[";for(int v:y_values)meta<<v;meta<<"]"<<"row_x_edges"<<"[";
    for(auto row=local.begin();row!=local.end();++row){std::vector<int>edges;(*row)>>edges;meta<<"[";for(int edge:edges)meta<<edge;meta<<"]";}meta<<"]"<<"column_y_edges"<<"[";for(auto col=column_local.begin();col!=column_local.end();++col){std::vector<int>edges;(*col)>>edges;meta<<"[";for(int edge:edges)meta<<edge;meta<<"]";}meta<<"]"<<"}";
    meta<<"sprites"<<"[";
    for(auto sprite=ownership["sprites"].begin();sprite!=ownership["sprites"].end();++sprite){meta<<"{"<<"id"<<(std::string)(*sprite)["id"]<<"row"<<(int)(*sprite)["row"]<<"col"<<(int)(*sprite)["col"]<<"components"<<"[";for(auto component=(*sprite)["components"].begin();component!=(*sprite)["components"].end();++component){std::vector<int>b;(*component)["bounds"]>>b;meta<<"{"<<"id"<<(std::string)(*component)["id"]<<"bounds"<<"[";for(int v:b)meta<<v;meta<<"]"<<"}";}meta<<"]"<<"}";}meta<<"]";
    meta<<"warnings"<<"[";for(auto warning=ownership["warnings"].begin();warning!=ownership["warnings"].end();++warning)meta<<(std::string)*warning;meta<<"]";
    meta<<"artifacts"<<"{"<<"main"<<a.get("output")<<"debug"<<a.get("debug")<<"}";
    for(int row=0;row<rows;row++){
        std::vector<int>edges;if(!local.empty()&&(int)local.size()>row)local[row]>>edges;
        if(edges.size()!=(size_t)cols+1){edges.clear();ownership["grid"]["x_edges"]>>edges;}
        if(edges.size()!=(size_t)cols+1)return pp::input_error("ownership row boundaries are invalid");
        int y0=y_edges[(size_t)row],y1=y_edges[(size_t)row+1];
        for(int col=0;col<cols;col++){
            std::vector<int>ys;if(!column_local.empty()&&(int)column_local.size()>col)column_local[col]>>ys;if(ys.size()!=(size_t)rows+1)ys=y_edges;y0=ys[(size_t)row];y1=ys[(size_t)row+1];
            int x0=edges[(size_t)col],x1=edges[(size_t)col+1];
            cv::rectangle(preview,{x0,y0,std::max(1,x1-x0),std::max(1,y1-y0)},cv::Scalar(30,220,255,255),1);
            char id[48];std::snprintf(id,sizeof(id),"r%03d-c%03d",row,col);
            cv::putText(preview,id,{x0+3,std::min(y1-2,y0+16)},cv::FONT_HERSHEY_SIMPLEX,.42,cv::Scalar(0,0,0,255),2,cv::LINE_AA);
            cv::putText(preview,id,{x0+3,std::min(y1-2,y0+16)},cv::FONT_HERSHEY_SIMPLEX,.42,cv::Scalar(255,255,255,255),1,cv::LINE_AA);
            const std::string sprite_id="sprite-r"+std::string(3-std::to_string(row).size(),'0')+std::to_string(row)+"-c"+std::string(3-std::to_string(col).size(),'0')+std::to_string(col);
            auto sprites=ownership["sprites"];
            for(auto sprite=sprites.begin();sprite!=sprites.end();++sprite)if((std::string)(*sprite)["id"]==sprite_id){
                for(auto component=(*sprite)["components"].begin();component!=(*sprite)["components"].end();++component){std::vector<int>b;(*component)["bounds"]>>b;if(b.size()!=4)continue;std::string blob=(std::string)(*component)["id"];cv::putText(preview,blob,{b[0],std::max(y0+12,b[1]-2)},cv::FONT_HERSHEY_SIMPLEX,.32,cv::Scalar(0,0,0,255),2,cv::LINE_AA);cv::putText(preview,blob,{b[0],std::max(y0+12,b[1]-2)},cv::FONT_HERSHEY_SIMPLEX,.32,cv::Scalar(40,255,170,255),1,cv::LINE_AA);}
                break;
            }
        }
    }
    debug=preview.clone();
    if(!pp::write_png_atomic(a.get("output"),preview)||!pp::write_png_atomic(a.get("debug"),debug)||!pp::publish_json(meta,temporary,a.get("meta")))return pp::input_error("could not publish diagnosis result");
    return 0;
}
/* agent: gpt-6 | 2026-10-06 | Show grid ownership diagnosis | v 1.0.0 | 244c9e */
