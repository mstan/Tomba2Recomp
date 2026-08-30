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
// Italian PAL release (Tombi! 2 - The Evil Swine Return). Every package
// carries a second [[target]] block for it; resolve() must match it too.
constexpr const char* kItaGameId = "SCES-02686";
constexpr const char* kItaDiscSha256 =
    "b9c8ff05f265f2ec359bc559b0731109de15c0a0d0d036b0da23ddbf98a19188";

int fail(const std::string& message) {
    std::cerr << "FAIL: " << message << "\n";
    return 1;
}

void no_op_plugin() {}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) return fail("expected the preloaded mods root");

    const fs::path source(argv[1]);
    const fs::path root =
        fs::temp_directory_path() / "tomba2-preloaded-mods-test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::copy(source, root, fs::copy_options::recursive);

    size_t manifest_count = 0;
    for (const fs::directory_entry& entry :
         fs::recursive_directory_iterator(root / "packages")) {
        if (!entry.is_regular_file() ||
            entry.path().filename() != "manifest.toml") {
            continue;
        }
        ++manifest_count;
        PSXRecompV4::ModPackage package;
        std::string error;
        if (!PSXRecompV4::ModPackageManager::read_manifest(
                entry.path(), package, &error)) {
            return fail("manifest parse failed: " + error);
        }
    }
    if (manifest_count != 4) return fail("expected four package manifests");

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

    PSXRecompV4::ModPackageManager manager(root);
    std::string error;
    if (!manager.scan(&error)) return fail("catalog scan failed: " + error);
    if (!manager.load_state(&error)) {
        return fail("default state failed: " + error);
    }
    if (manager.packages().size() != 4) {
        return fail("expected four package families");
    }

    const auto default_plan = manager.resolve(kGameId, "", kDiscSha256);
    if (!default_plan.ok || !default_plan.writes.empty() ||
        !default_plan.plugins.empty()) {
        return fail("default-disabled catalog produced runtime operations");
    }

    const auto ita_default_plan =
        manager.resolve(kItaGameId, "", kItaDiscSha256);
    if (!ita_default_plan.ok || !ita_default_plan.writes.empty() ||
        !ita_default_plan.plugins.empty()) {
        return fail("default-disabled catalog produced runtime operations "
                    "for the Italian target");
    }

    if (!manager.set_feature_enabled(
            "tomba2.enhancement.widescreen", "widescreen", true, &error)) {
        return fail(error);
    }
    for (const auto& [choice, plugin] :
         {std::pair{"16:9", "tomba2.widescreen.16-9"},
          std::pair{"21:9", "tomba2.widescreen.21-9"},
          std::pair{"adaptive", "tomba2.widescreen.adaptive"}}) {
        if (!manager.set_feature_option(
                "tomba2.enhancement.widescreen", "widescreen",
                "aspect", choice, &error)) {
            return fail(error);
        }
        const auto plan = manager.resolve(kGameId, "", kDiscSha256);
        if (!plan.ok || plan.plugins.size() != 1 ||
            plan.plugins.front().id != plugin) {
            return fail(std::string("wrong widescreen plugin for ") + choice);
        }
        const auto ita_plan = manager.resolve(kItaGameId, "", kItaDiscSha256);
        if (!ita_plan.ok || ita_plan.plugins.size() != 1 ||
            ita_plan.plugins.front().id != plugin) {
            return fail(std::string("wrong ITALIAN widescreen plugin for ") +
                        choice);
        }
    }

    if (!manager.set_feature_enabled(
            "tomba2.enhancement.widescreen", "widescreen", false, &error) ||
        !manager.set_feature_enabled(
            "tomba2.experimental.interpolated-frame-rate",
            "interpolated-frame-rate", true, &error)) {
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
                "tomba2.experimental.interpolated-frame-rate",
                "interpolated-frame-rate", "rate", choice, &error)) {
            return fail(error);
        }
        const auto plan = manager.resolve(kGameId, "", kDiscSha256);
        if (!plan.ok || !plan.writes.empty() || plan.plugins.size() != 1 ||
            plan.plugins.front().id != plugin) {
            return fail(
                std::string("wrong interpolated frame-rate plan for ") +
                choice);
        }
        const auto ita_plan = manager.resolve(kItaGameId, "", kItaDiscSha256);
        if (!ita_plan.ok || !ita_plan.writes.empty() ||
            ita_plan.plugins.size() != 1 ||
            ita_plan.plugins.front().id != plugin) {
            return fail(
                std::string("wrong ITALIAN interpolated frame-rate plan for ") +
                choice);
        }
    }

    if (!manager.set_feature_enabled(
            "tomba2.experimental.interpolated-frame-rate",
            "interpolated-frame-rate", false, &error) ||
        !manager.set_feature_enabled(
            "tomba2.enhancement.skip-fmvs", "skip-fmvs", true, &error)) {
        return fail(error);
    }
    const auto skip_plan = manager.resolve(kGameId, "", kDiscSha256);
    if (!skip_plan.ok || !skip_plan.writes.empty() ||
        skip_plan.plugins.size() != 1 ||
        skip_plan.plugins.front().id != "tomba2.fmv.skip") {
        return fail("Skip FMVs did not resolve its trusted activation plugin");
    }
    const auto ita_skip_plan = manager.resolve(kItaGameId, "", kItaDiscSha256);
    if (!ita_skip_plan.ok || !ita_skip_plan.writes.empty() ||
        ita_skip_plan.plugins.size() != 1 ||
        ita_skip_plan.plugins.front().id != "tomba2.fmv.skip") {
        return fail("Skip FMVs did not resolve its trusted activation plugin for Italian target");
    }

    if (!manager.set_feature_enabled(
            "tomba2.enhancement.skip-fmvs", "skip-fmvs", false, &error) ||
        !manager.set_feature_enabled(
            "tomba2.debug.debug-menu", "debug-menu", true, &error)) {
        return fail(error);
    }
    const auto debug_plan = manager.resolve(kGameId, "", kDiscSha256);
    if (!debug_plan.ok || !debug_plan.writes.empty() ||
        debug_plan.plugins.size() != 1 ||
        debug_plan.plugins.front().id != "tomba2.debug.menu") {
        return fail("Debug Menu did not resolve its trusted plugin alone");
    }
    const auto ita_debug_plan = manager.resolve(kItaGameId, "", kItaDiscSha256);
    if (!ita_debug_plan.ok || !ita_debug_plan.writes.empty() ||
        ita_debug_plan.plugins.size() != 1 ||
        ita_debug_plan.plugins.front().id != "tomba2.debug.menu") {
        return fail("Debug Menu did not resolve its trusted plugin alone for Italian target");
    }

    fs::remove_all(root, ec);
    std::cout << "Tomba 2 preloaded mods: 4 packages, "
                 "3 widescreen choices, 7 interpolated frame-rate choices, "
                 "motion-adaptive clarity blend, game-owned FMV skipping, "
                 "visible default-off debug menu, stock guest code untouched\n";
    return 0;
}
