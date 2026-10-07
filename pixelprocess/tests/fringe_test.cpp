/*
 * File version: 1.1.0
 * Last updated: 2026-10-07
 * Purpose: Verify FRINGE-01 removal boundaries, evidence fallback, and cleanup modes.
 */
/* agent: gpt-6 | 2026-10-05 | Verify evidence guided fringe boundaries | v 1.0.0 | 14a592 */
#define main cleanup_stage_main
#include "../stage_cleanup.cpp"
#undef main
#define main reconstruction_stage_main
#include "../stage_reconstruct.cpp"
#undef main
#undef NDEBUG
#include <cassert>
#include <unistd.h>

/* Create evidence from int width, bool winner, double neighbor support/color; return FileStorage. */
static cv::FileStorage evidence(int width, bool winner=true, double neighbor_bg=.15, double neighbor_win=.3) {
    cv::FileStorage fs("",cv::FileStorage::WRITE|cv::FileStorage::MEMORY|cv::FileStorage::FORMAT_JSON);
    fs<<"uncertain_cells"<<"[";
    for(int x=0;x<width;x++){
        double bg=x==0?.3:(x==1?neighbor_bg:(x==4?0:.15)),win=x==0?.2:(x==1?neighbor_win:(x==5?.6:.3));
        fs<<"{"<<"logical_cell"<<"["<<x<<0<<"]"<<"background_support"<<bg<<"coverage"<<1-bg;
        if(winner)fs<<"winning_color_support"<<win;
        fs<<"}";
    }
    fs<<"]";
    return cv::FileStorage(fs.releaseAndGetString(),cv::FileStorage::READ|cv::FileStorage::MEMORY);
}

/* Verify cleanup with no CLI arguments; return int status. */
int main() {
    cv::Mat crop(3,3,CV_8UC4,cv::Scalar(220,40,240,255)),mask(3,3,CV_8U,cv::Scalar(0));
    mask.at<uchar>(0,0)=255;mask.at<uchar>(0,1)=255;
    crop.at<cv::Vec4b>(1,1)=cv::Vec4b(230,45,250,255);
    std::vector<CellEvidence> cells;
    auto logical=reconstruct_one(crop,mask,0,0,1.5,0,0,.35,false,true,cells);
    assert(logical.rows==2&&logical.cols==2&&cells[0].bounds==cv::Rect(0,0,2,2));
    assert(std::abs(cells[0].coverage-1.0/3.0)<.01&&std::abs(cells[0].background-2.0/3.0)<.01);
    assert(cells.back().bounds==cv::Rect(1,1,2,2)&&cells.back().coverage==1&&cells.back().color_support>0);
    auto state=evidence(6);FringeScratch scratch;
    cv::Mat image(1,6,CV_8UC4,cv::Scalar(220,40,240,255));
    auto counts=clean_outside(image,state["uncertain_cells"],.2,.5,.1,scratch);
    assert(counts.seeds==1&&counts.fringe==1&&counts.retained==4);
    assert(image.at<cv::Vec4b>(0,0)==cv::Vec4b(0,0,0,0));
    assert(image.at<cv::Vec4b>(0,1)==cv::Vec4b(0,0,0,0));
    assert(image.at<cv::Vec4b>(0,2)[3]==255); // A removed neighbor cannot seed another step.
    assert(image.at<cv::Vec4b>(0,4)[3]==255); // No background support: enclosed/detail pink survives.
    assert(image.at<cv::Vec4b>(0,5)[3]==255); // Strong winning-color evidence survives.
    image.setTo(cv::Scalar(220,40,240,255));image.at<cv::Vec4b>(0,1)=cv::Vec4b(20,20,20,255);
    counts=clean_outside(image,state["uncertain_cells"],.2,.5,.1,scratch);
    assert(counts.seeds==1&&counts.fringe==0&&image.at<cv::Vec4b>(0,2)[3]==255);
    for(auto protected_state:{evidence(2,true,0,.3),evidence(2,true,.15,.6)}){
        image=cv::Mat(1,2,CV_8UC4,cv::Scalar(220,40,240,255));
        counts=clean_outside(image,protected_state["uncertain_cells"],.2,.5,.1,scratch);
        assert(counts.seeds==1&&counts.fringe==0&&image.at<cv::Vec4b>(0,1)[3]==255);
    }
    image=cv::Mat(1,6,CV_8UC4,cv::Scalar(220,40,240,255));auto legacy=evidence(6,false);
    counts=clean_outside(image,legacy["uncertain_cells"],.2,.5,.1,scratch);
    assert(counts.seeds==0&&counts.fringe==0); // Older metadata cannot authorize fringe edits.
    image.setTo(cv::Scalar(0,0,0,0));image.at<cv::Vec4b>(0,1)=cv::Vec4b(220,40,240,255);
    counts=clean_outside(image,state["uncertain_cells"],.2,.5,.1,scratch);
    assert(counts.seeds==0&&counts.fringe==0); // Transparency is never a seed.
    auto isolated=evidence(1);image=cv::Mat(1,1,CV_8UC4,cv::Scalar(220,40,240,255));
    counts=clean_outside(image,isolated["uncertain_cells"],.2,.5,.1,scratch);
    assert(counts.seeds==1&&counts.fringe==0); // Scratch shrinks and cannot reach another sprite.

    auto root=std::filesystem::temp_directory_path()/("fringe-test-"+std::to_string((long)getpid()));
    std::filesystem::create_directories(root);auto input=root/"input.png",meta=root/"state.json";
    image=cv::Mat(1,2,CV_8UC4,cv::Scalar(255,0,255,255));assert(pp::write_png_atomic(input.string(),image));
    std::string temp;auto fs=pp::json_writer(meta.string(),temp);
    fs<<"grid"<<"["<<1<<1<<"]"<<"sprites"<<"["<<"{"<<"id"<<"sprite"<<"path"<<input.string()<<"row"<<0<<"col"<<0<<"cells"<<"[";
    fs<<"{"<<"logical_cell"<<"["<<0<<0<<"]"<<"background_support"<<.3<<"foreground_occupancy"<<.7<<"winning_color_support"<<.2<<"}";
    fs<<"{"<<"logical_cell"<<"["<<1<<0<<"]"<<"background_support"<<.15<<"foreground_occupancy"<<.85<<"winning_color_support"<<.3<<"}";
    fs<<"]"<<"}"<<"]";assert(pp::publish_json(fs,temp,meta.string()));
    for(const std::string mode:{"outside","skip","all"}){
        auto dir=root/mode;std::filesystem::create_directories(dir);
        std::vector<std::string> args={"cleanup","--state",meta.string(),"--output",(dir/"main.png").string(),"--debug",(dir/"debug.png").string(),"--meta",(dir/"meta.json").string(),"--sprite-dir",(dir/"sprites").string(),"--mode",mode};
        std::vector<char*> argv;for(auto &arg:args)argv.push_back(arg.data());
        assert(cleanup_stage_main((int)argv.size(),argv.data())==0);
        auto output=pp::read_rgba((dir/"main.png").string());
        for(int x=0;x<2;x++)assert(output.at<cv::Vec4b>(0,x)[3]==(mode=="skip"?255:0));
        if(mode=="outside"){
            auto debug=pp::read_rgba((dir/"debug.png").string());
            assert(debug.at<cv::Vec4b>(0,0)==cv::Vec4b(0,128,255,255));
            assert(debug.at<cv::Vec4b>(0,1)==cv::Vec4b(0,255,255,255));
            auto result=pp::read_json((dir/"meta.json").string());
            assert((int)result["measurements"]["fringe_seed_removed"]==1);
            assert((int)result["measurements"]["fringe_neighbor_removed"]==1);
        }
    }
    std::filesystem::remove_all(root);return 0;
}
/* agent: gpt-6 | 2026-10-05 | Verify evidence guided fringe boundaries | v 1.0.0 | 14a592 */
