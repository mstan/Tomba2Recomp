#include "mod_media.h"
#include "mod_packages.h"
#include "mod_plugins.h"
#include "mario/smash_assets.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <stdexcept>

namespace {
using namespace PSXRecompV4;
using namespace tomba2::mario;
constexpr uint64_t cache_size=678804;
constexpr const char* cache_hash="1cbe9ce74e63464da8582d14dea5f8ba94e30a8c0258f03fee80f9abb3ace588";
std::atomic<unsigned> serial{0};

bool prepare(const ModPrepareContext& context,
             std::map<std::string,std::filesystem::path>& outputs,std::string& error) {
    std::filesystem::path temporary,staging;
    try {
        auto input=context.inputs.find("smash-rom");
        if(input==context.inputs.end())throw std::runtime_error("Select a Smash Bros. 64 US 1.0 ROM for Mario's Hat.");
        std::shared_ptr<const Bytes> rom;
        if(!load_mod_media(input->second,"n64-rom",rom_size,rom_sha256,rom,&error))return false;
        const auto directory=context.cache_root/"tomba2-mario"/"r2";
        const auto final=directory/(std::string(cache_hash)+".cache");
        std::shared_ptr<const Bytes> verified;
        std::string cache_error;
        if(std::filesystem::is_regular_file(final) &&
           load_mod_media(final,"file",cache_size,cache_hash,verified,&cache_error)) {
            outputs["mario-assets"]=final;return true;
        }
        Bytes bytes=build_cache(*rom);
        // Decode before publishing: a cache must be useful, not merely present.
        auto files=read_cache(bytes);(void)decode_model(files);
        std::filesystem::create_directories(directory);
        // create_directory atomically claims a staging location. Cleanup must
        // never remove another process's incomplete import.
        for(unsigned attempt=0;attempt<16 && staging.empty();++attempt) {
            auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
            auto candidate=directory/("prepare-"+std::to_string(stamp)+"-"+std::to_string(++serial));
            if(std::filesystem::create_directory(candidate))staging=std::move(candidate);
        }
        if(staging.empty())throw std::runtime_error("Cannot create Mario import staging directory.");
        temporary=staging/"assets.cache";
        {
            std::ofstream out(temporary,std::ios::binary);
            if(!out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size())))
                throw std::runtime_error("Cannot write Mario's asset cache.");
        }
        if(!load_mod_media(temporary,"file",cache_size,cache_hash,verified,&error))
            throw std::runtime_error(error);
        if(std::filesystem::exists(final)) {
            if(std::filesystem::is_symlink(final) || !std::filesystem::is_regular_file(final))
                throw std::runtime_error("Mario cache destination is not a regular file.");
            std::filesystem::remove(final);
        }
        std::filesystem::rename(temporary,final);temporary.clear();
        std::filesystem::remove(staging);staging.clear();
        outputs["mario-assets"]=final;return true;
    }catch(const std::exception& e) {
        error=e.what();
        if(!temporary.empty()) {std::error_code ec;std::filesystem::remove(temporary,ec);}
        if(!staging.empty()) {std::error_code ec;std::filesystem::remove(staging,ec);}
        return false;
    }
}
}
PSX_MOD_CONSTRUCTOR(tomba2_register_mario_preparer) {
    (void)PSXRecompV4::mod_register_media_preparer("tomba2.mario.import",prepare);
}
