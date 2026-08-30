#include "mod_packages.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

namespace fs = std::filesystem;

namespace {

constexpr const char* kGameId = "SCUS-94454";
constexpr const char* kDiscSha256 =
    "8e9568388155787384c3166a5934a495fbff3fa57154ab0489b0f214fe05a8ab";
constexpr const char* kItaGameId = "SCES-02686";
constexpr const char* kItaDiscSha256 =
    "b9c8ff05f265f2ec359bc559b0731109de15c0a0d0d036b0da23ddbf98a19188";

constexpr const char* kUsWidescreenPackage = "tomba2.enhancement.widescreen";
constexpr const char* kUsFrameRatePackage =
    "tomba2.experimental.interpolated-frame-rate";
constexpr const char* kUsSkipFmvsPackage = "tomba2.enhancement.skip-fmvs";
constexpr const char* kUsDebugMenuPackage = "tomba2.debug.debug-menu";

constexpr const char* kItaWidescreenPackage = "tombi2.enhancement.widescreen";
constexpr const char* kItaFrameRatePackage =
    "tombi2.experimental.interpolated-frame-rate";
constexpr const char* kItaSkipFmvsPackage = "tombi2.enhancement.skip-fmvs";

int fail(const std::string& message) {
    std::cerr << "FAIL: " << message << "\n";
    return 1;
}

void no_op_plugin() {}

int expect_no_ops(const PSXRecompV4::ModResolution& plan,
                  const std::string& context) {
    if (!plan.ok || !plan.writes.empty() || !plan.plugins.empty()) {
        return fail(context + " produced runtime operations");
    }
    return 0;
}

int expect_single_plugin(const PSXRecompV4::ModResolution& plan,
                         const std::string& plugin,
                         const std::string& context) {
    if (!plan.ok || !plan.writes.empty() || plan.plugins.size() != 1 ||
        plan.plugins.front().id != plugin) {
        return fail(context);
    }
    return 0;
}

int count_manifests(const fs::path& root) {
    int count = 0;
    for (const fs::directory_entry& entry :
         fs::recursive_directory_iterator(root / "packages")) {
        if (!entry.is_regular_file() ||
            entry.path().filename() != "manifest.toml") {
            continue;
        }
        ++count;
        PSXRecompV4::ModPackage package;
        std::string error;
        if (!PSXRecompV4::ModPackageManager::read_manifest(
                entry.path(), package, &error)) {
            return -1;
        }
    }
    return count;
}

int register_plugins() {
    PSXRecompV4::mod_clear_plugins_for_tests();
    for (const char* id : {
             "tomba2.widescreen.16-9",
             "tomba2.widescreen.21-9",
             "tomba2.widescreen.adaptive",
             "tomba2.framerate.display",
             "tomba2.framerate.60",
             "tomba2.framerate.90",
             "tomba2.framerate.120",
             "tomba2.framerate.144",
             "tomba2.framerate.165",
             "tomba2.framerate.240",
             "tomba2.fmv.skip",
             "tomba2.debug.menu"}) {
        if (!PSXRecompV4::mod_register_activation_plugin(id, no_op_plugin)) {
            return fail(std::string("could not register test plugin ") + id);
        }
    }
    return 0;
}

int load_catalog(const fs::path& root, PSXRecompV4::ModPackageManager& manager,
                 size_t expected_packages, int expected_manifests) {
    std::string error;
    const int manifest_count = count_manifests(root);
    if (manifest_count != expected_manifests) {
        return fail("unexpected manifest count in " + root.string());
    }
    manager.set_root(root);
    if (!manager.scan(&error)) return fail("catalog scan failed: " + error);
    if (!manager.load_state(&error)) return fail("default state failed: " + error);
    if (manager.packages().size() != expected_packages) {
        return fail("unexpected package family count in " + root.string());
    }
    return 0;
}

int check_widescreen(PSXRecompV4::ModPackageManager& manager,
                     const char* package_id, const char* game_id,
                     const char* disc_sha256, const char* label) {
    std::string error;
    if (!manager.set_feature_enabled(package_id, "widescreen", true, &error)) {
        return fail(error);
    }
    for (const auto& [choice, plugin] :
         {std::pair{"16:9", "tomba2.widescreen.16-9"},
          std::pair{"21:9", "tomba2.widescreen.21-9"},
          std::pair{"adaptive", "tomba2.widescreen.adaptive"}}) {
        if (!manager.set_feature_option(
                package_id, "widescreen", "aspect", choice, &error)) {
            return fail(error);
        }
        const auto plan = manager.resolve(game_id, "", disc_sha256);
        if (expect_single_plugin(
                plan, plugin,
                std::string("wrong ") + label + " widescreen plugin for " +
                    choice) != 0)
            return 1;
    }
    if (!manager.set_feature_enabled(package_id, "widescreen", false, &error)) {
        return fail(error);
    }
    return 0;
}

int check_frame_rate(PSXRecompV4::ModPackageManager& manager,
                     const char* package_id, const char* game_id,
                     const char* disc_sha256, const char* label) {
    std::string error;
    if (!manager.set_feature_enabled(
            package_id, "interpolated-frame-rate", true, &error)) {
        return fail(error);
    }
    for (const auto& [choice, plugin] :
         {std::pair{"display", "tomba2.framerate.display"},
          std::pair{"60", "tomba2.framerate.60"},
          std::pair{"90", "tomba2.framerate.90"},
          std::pair{"120", "tomba2.framerate.120"},
          std::pair{"144", "tomba2.framerate.144"},
          std::pair{"165", "tomba2.framerate.165"},
          std::pair{"240", "tomba2.framerate.240"}}) {
        if (!manager.set_feature_option(
                package_id, "interpolated-frame-rate", "rate", choice,
                &error)) {
            return fail(error);
        }
        const auto plan = manager.resolve(game_id, "", disc_sha256);
        if (expect_single_plugin(
                plan, plugin,
                std::string("wrong ") + label +
                    " interpolated frame-rate plan for " + choice) != 0)
            return 1;
    }
    if (!manager.set_feature_enabled(
            package_id, "interpolated-frame-rate", false, &error)) {
        return fail(error);
    }
    return 0;
}

int check_skip_fmvs(PSXRecompV4::ModPackageManager& manager,
                    const char* package_id, const char* game_id,
                    const char* disc_sha256, const char* label) {
    std::string error;
    if (!manager.set_feature_enabled(package_id, "skip-fmvs", true, &error)) {
        return fail(error);
    }
    const auto plan = manager.resolve(game_id, "", disc_sha256);
    if (expect_single_plugin(
            plan, "tomba2.fmv.skip",
            std::string(label) +
                " Skip FMVs did not resolve its trusted activation plugin") != 0)
        return 1;
    if (!manager.set_feature_enabled(package_id, "skip-fmvs", false, &error)) {
        return fail(error);
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        return fail("expected US and Italian preloaded mods roots");
    }
    if (register_plugins() != 0) return 1;

    const fs::path us_root =
        fs::temp_directory_path() / "tomba2-preloaded-mods-test-us";
    const fs::path ita_root =
        fs::temp_directory_path() / "tomba2-preloaded-mods-test-ita";
    std::error_code ec;
    fs::remove_all(us_root, ec);
    fs::remove_all(ita_root, ec);
    fs::copy(fs::path(argv[1]), us_root, fs::copy_options::recursive);
    fs::copy(fs::path(argv[2]), ita_root, fs::copy_options::recursive);

    PSXRecompV4::ModPackageManager us_manager;
    if (load_catalog(us_root, us_manager, 4, 4) != 0) return 1;
    if (expect_no_ops(us_manager.resolve(kGameId, "", kDiscSha256),
                      "default-disabled US catalog") != 0)
        return 1;
    if (check_widescreen(us_manager, kUsWidescreenPackage, kGameId,
                         kDiscSha256, "US") != 0)
        return 1;
    if (check_frame_rate(us_manager, kUsFrameRatePackage, kGameId,
                         kDiscSha256, "US") != 0)
        return 1;
    if (check_skip_fmvs(us_manager, kUsSkipFmvsPackage, kGameId,
                        kDiscSha256, "US") != 0)
        return 1;

    std::string error;
    if (!us_manager.set_feature_enabled(
            kUsDebugMenuPackage, "debug-menu", true, &error)) {
        return fail(error);
    }
    if (expect_single_plugin(
            us_manager.resolve(kGameId, "", kDiscSha256),
            "tomba2.debug.menu",
            "Debug Menu did not resolve its trusted plugin alone") != 0)
        return 1;

    PSXRecompV4::ModPackageManager ita_manager;
    if (load_catalog(ita_root, ita_manager, 3, 3) != 0) return 1;
    if (ita_manager.packages().find(kUsDebugMenuPackage) !=
        ita_manager.packages().end()) {
        return fail("Italian catalog must not include Debug Menu");
    }
    if (expect_no_ops(ita_manager.resolve(kItaGameId, "", kItaDiscSha256),
                      "default-disabled Italian catalog") != 0)
        return 1;
    if (check_widescreen(ita_manager, kItaWidescreenPackage, kItaGameId,
                         kItaDiscSha256, "Italian") != 0)
        return 1;
    if (check_frame_rate(ita_manager, kItaFrameRatePackage, kItaGameId,
                         kItaDiscSha256, "Italian") != 0)
        return 1;
    if (check_skip_fmvs(ita_manager, kItaSkipFmvsPackage, kItaGameId,
                        kItaDiscSha256, "Italian") != 0)
        return 1;

    fs::remove_all(us_root, ec);
    fs::remove_all(ita_root, ec);
    std::cout << "Tomba 2 preloaded mods: 4 US packages, "
                 "3 localized Italian packages, no Italian debug menu, "
                 "3 widescreen choices, 7 interpolated frame-rate choices, "
                 "motion-adaptive clarity blend, game-owned FMV skipping, "
                 "stock guest code untouched\n";
    return 0;
}
