/*
 * File version: 1.3.0
 * Last updated: 2026-10-09
 * Purpose: Verify shaded magenta cleanup and safe outline reconstruction.
 */
/* agent: gpt-6.1-sol | 2026-10-09 | Test weak outline restoration | v 1.2.0 | 0b083c */
/* agent: gpt-6 | 2026-10-05 | Verify evidence guided fringe boundaries | v 1.0.0 | 14a592 */
#define main cleanup_stage_main
#include "../stage_cleanup.cpp"
#undef main
#define main reconstruction_stage_main
#include "../stage_reconstruct.cpp"
#undef main
#define main outline_reconstruction_stage_main
#include "../stage_outline_reconstruct.cpp"
#undef main
#undef NDEBUG
#include <cassert>
#include <unistd.h>

/* Create evidence from int width, bool winner, double neighbor support/color; return FileStorage. */
static cv::FileStorage evidence(int width, bool winner=true, double neighbor_bg=.15, double neighbor_win=.3) {
    cv::FileStorage fs("",cv::FileStorage::WRITE|cv::FileStorage::MEMORY|cv::FileStorage::FORMAT_JSON);
    fs<<"cells"<<"[";
    for(int x=0;x<width;x++){
        double bg=x==0?.3:(x==1?neighbor_bg:(x==4?0:.15)),win=x==0?.2:(x==1?neighbor_win:(x==5?.6:.3));
        fs<<"{"<<"logical_cell"<<"["<<x<<0<<"]"<<"background_support"<<bg<<"foreground_occupancy"<<1-bg;
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
    assert(pp::shaded_magenta(cv::Vec4b(197,51,209,255),55)); // shaded spill within line-distance tolerance
    assert(!pp::shaded_magenta(cv::Vec4b(130,90,132,255),55)); // muted purple outside tolerance
    cv::Mat image(1,6,CV_8UC4,cv::Scalar(220,40,240,255));
    auto counts=clean_outside(image,state["cells"],.2,.5,.1,80,scratch);
    assert(counts.seeds==1&&counts.fringe==1&&counts.retained==4);
    assert(image.at<cv::Vec4b>(0,0)==cv::Vec4b(0,0,0,0));
    assert(image.at<cv::Vec4b>(0,1)==cv::Vec4b(0,0,0,0));
    assert(image.at<cv::Vec4b>(0,2)[3]==255); // A removed neighbor cannot seed another step.
    assert(image.at<cv::Vec4b>(0,4)[3]==255); // No background support: enclosed/detail pink survives.
    assert(image.at<cv::Vec4b>(0,5)[3]==255); // Strong winning-color evidence survives.
    image.setTo(cv::Scalar(220,40,240,255));image.at<cv::Vec4b>(0,1)=cv::Vec4b(20,20,20,255);
    counts=clean_outside(image,state["cells"],.2,.5,.1,80,scratch);
    assert(counts.seeds==1&&counts.fringe==0&&image.at<cv::Vec4b>(0,2)[3]==255);
    for(auto protected_state:{evidence(2,true,0,.3),evidence(2,true,.15,.6)}){
        image=cv::Mat(1,2,CV_8UC4,cv::Scalar(220,40,240,255));
        counts=clean_outside(image,protected_state["cells"],.2,.5,.1,80,scratch);
        assert(counts.seeds==1&&counts.fringe==0&&image.at<cv::Vec4b>(0,1)[3]==255);
    }
    image=cv::Mat(1,6,CV_8UC4,cv::Scalar(220,40,240,255));auto missing_winner=evidence(6,false);
    counts=clean_outside(image,missing_winner["cells"],.2,.5,.1,80,scratch);
    assert(counts.seeds==0&&counts.fringe==0); // Missing winning-color evidence cannot authorize fringe edits.
    image.setTo(cv::Scalar(0,0,0,0));image.at<cv::Vec4b>(0,1)=cv::Vec4b(220,40,240,255);
    counts=clean_outside(image,state["cells"],.2,.5,.1,80,scratch);
    assert(counts.seeds==0&&counts.fringe==0); // Transparency is never a seed.
    auto isolated=evidence(1);image=cv::Mat(1,1,CV_8UC4,cv::Scalar(220,40,240,255));
    counts=clean_outside(image,isolated["cells"],.2,.5,.1,80,scratch);
    assert(counts.seeds==1&&counts.fringe==0); // Scratch shrinks and cannot reach another sprite.

    cv::Mat enclosed(3,3,CV_8UC4,cv::Scalar(20,20,20,255));enclosed.at<cv::Vec4b>(1,1)=cv::Vec4b(197,33,193,255);
    cv::FileStorage enclosed_state("",cv::FileStorage::WRITE|cv::FileStorage::MEMORY|cv::FileStorage::FORMAT_JSON);enclosed_state<<"cells"<<"[";
    for(int y=0;y<3;y++)for(int x=0;x<3;x++)enclosed_state<<"{"<<"logical_cell"<<"["<<x<<y<<"]"<<"background_support"<<.3<<"foreground_occupancy"<<.7<<"winning_color_support"<<.2<<"}";
    enclosed_state<<"]";auto enclosed_json=cv::FileStorage(enclosed_state.releaseAndGetString(),cv::FileStorage::READ|cv::FileStorage::MEMORY);
    auto enclosed_counts=clean_outside(enclosed,enclosed_json["cells"],.2,.5,.1,55,scratch);
    assert(enclosed_counts.seeds==0&&enclosed.at<cv::Vec4b>(1,1)[3]==255); // enclosed magenta detail survives

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
        std::vector<std::string> args={"cleanup","--state",meta.string(),"--output",(dir/"main.png").string(),"--debug",(dir/"debug.png").string(),"--meta",(dir/"meta.json").string(),"--sprite-dir",(dir/"sprites").string(),"--mode",mode,"--tolerance","80"};
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
    auto reconstructed=root/"reconstructed.png",cleaned=root/"cleaned.png",magenta_mask=root/"deleted.png",reason_mask=root/"reasons.png";auto outdir=root/"outline";std::filesystem::create_directories(outdir);
    cv::Mat source=cv::Mat::zeros(1,2,CV_8UC4),clean=cv::Mat::zeros(1,2,CV_8UC4),deleted=cv::Mat::zeros(1,2,CV_8U);source.at<cv::Vec4b>(0,0)=cv::Vec4b(255,0,255,255);source.at<cv::Vec4b>(0,1)=cv::Vec4b(0,0,255,255);clean.at<cv::Vec4b>(0,1)=source.at<cv::Vec4b>(0,1);deleted.at<uchar>(0,0)=255;cv::Mat reasons=cv::Mat::zeros(1,2,CV_8U);reasons.at<uchar>(0,0)=pp::CleanupAll;
    assert(pp::write_png_atomic(reconstructed.string(),source)&&pp::write_png_atomic(cleaned.string(),clean)&&pp::write_png_atomic(magenta_mask.string(),deleted)&&pp::write_png_atomic(reason_mask.string(),reasons));
    auto original_meta=root/"original.json",clean_meta=root/"clean.json";std::string original_tmp,clean_tmp;auto original_writer=pp::json_writer(original_meta.string(),original_tmp);original_writer<<"grid"<<"["<<1<<1<<"]"<<"sprites"<<"["<<"{"<<"id"<<"sprite"<<"row"<<0<<"col"<<0<<"path"<<reconstructed.string()<<"cells"<<"[";
    original_writer<<"{"<<"logical_cell"<<"["<<0<<0<<"]"<<"foreground_occupancy"<<.8<<"background_support"<<.1<<"winning_color_support"<<.5<<"}"<<"{"<<"logical_cell"<<"["<<1<<0<<"]"<<"foreground_occupancy"<<.8<<"background_support"<<.1<<"winning_color_support"<<.8<<"}"<<"]"<<"}"<<"]";assert(pp::publish_json(original_writer,original_tmp,original_meta.string()));
    auto clean_writer=pp::json_writer(clean_meta.string(),clean_tmp);clean_writer<<"sprites"<<"["<<"{"<<"id"<<"sprite"<<"row"<<0<<"col"<<0<<"path"<<cleaned.string()<<"magenta_mask"<<magenta_mask.string()<<"reason_mask"<<reason_mask.string()<<"}"<<"]";assert(pp::publish_json(clean_writer,clean_tmp,clean_meta.string()));
    auto invoke_outline=[&](const std::string &mode){std::vector<std::string> args={"outline","--state",original_meta.string(),"--clean",clean_meta.string(),"--output",(outdir/(mode+".png")).string(),"--debug",(outdir/(mode+".debug.png")).string(),"--meta",(outdir/(mode+".json")).string(),"--sprite-dir",(outdir/(mode+"-sprites")).string(),"--mode",mode};std::vector<char*> argv;for(auto &arg:args)argv.push_back(arg.data());return outline_reconstruction_stage_main((int)argv.size(),argv.data());};
    assert(invoke_outline("weak")==0);auto repaired=pp::read_rgba((outdir/"weak-sprites/sprite.png").string());assert(repaired.at<cv::Vec4b>(0,0)==source.at<cv::Vec4b>(0,1));auto repair_mask=cv::imread((outdir/"weak-sprites/sprite.mask.png").string(),cv::IMREAD_GRAYSCALE);assert(repair_mask.at<uchar>(0,0)==255&&repair_mask.at<uchar>(0,1)==0);
    assert(invoke_outline("off")==0);auto unchanged=pp::read_rgba((outdir/"off-sprites/sprite.png").string());assert(unchanged.at<cv::Vec4b>(0,0)[3]==0);
    std::filesystem::remove_all(root);return 0;
}
/* agent: gpt-6 | 2026-10-05 | Verify evidence guided fringe boundaries | v 1.0.0 | 14a592 */
/* agent: gpt-6.1-sol | 2026-10-09 | Test weak outline restoration | v 1.2.0 | 0b083c */

/* agent: gpt-6.1-sol | 2026-10-09 | Verify shaded magenta decisions | v 1.3.0 | 0bb558 */
