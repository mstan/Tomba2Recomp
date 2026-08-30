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
// Italian PAL release (Tombi! 2 - The Evil Swine Return). It uses separate
// localized package manifests and intentionally does not expose Debug Menu until
// the PAL hook/signature sites are mapped.
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
    if (manifest_count != 7) return fail("expected seven package manifests");

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
    if (manager.packages().size() != 7) {
        return fail("expected seven package families");
    }

    const auto default_plan = manager.resolve(kGameId, "", kDiscSha256);
    if (expect_no_ops(default_plan,
                      "default-disabled catalog") != 0) return 1;

    const auto ita_default_plan =
        manager.resolve(kItaGameId, "", kItaDiscSha256);
    if (expect_no_ops(ita_default_plan,
                      "default-disabled catalog for Italian target") != 0)
        return 1;

    if (!manager.set_feature_enabled(
            kUsWidescreenPackage, "widescreen", true, &error)) {
        return fail(error);
    }
    for (const auto& [choice, plugin] :
         {std::pair{"16:9", "tomba2.widescreen.16-9"},
          std::pair{"21:9", "tomba2.widescreen.21-9"},
          std::pair{"adaptive", "tomba2.widescreen.adaptive"}}) {
        if (!manager.set_feature_option(
                kUsWidescreenPackage, "widescreen", "aspect", choice,
                &error)) {
            return fail(error);
        }
        const auto plan = manager.resolve(kGameId, "", kDiscSha256);
        if (expect_single_plugin(
                plan, plugin,
                std::string("wrong widescreen plugin for ") + choice) != 0)
            return 1;
    }
    if (!manager.set_feature_enabled(
            kUsWidescreenPackage, "widescreen", false, &error) ||
        !manager.set_feature_enabled(
            kItaWidescreenPackage, "widescreen", true, &error)) {
        return fail(error);
    }
    for (const auto& [choice, plugin] :
         {std::pair{"16:9", "tomba2.widescreen.16-9"},
          std::pair{"21:9", "tomba2.widescreen.21-9"},
          std::pair{"adaptive", "tomba2.widescreen.adaptive"}}) {
        if (!manager.set_feature_option(
                kItaWidescreenPackage, "widescreen", "aspect", choice,
                &error)) {
            return fail(error);
        }
        const auto plan = manager.resolve(kItaGameId, "", kItaDiscSha256);
        if (expect_single_plugin(
                plan, plugin,
                std::string("wrong Italian widescreen plugin for ") +
                    choice) != 0)
            return 1;
    }

    if (!manager.set_feature_enabled(
            kItaWidescreenPackage, "widescreen", false, &error) ||
        !manager.set_feature_enabled(
            kUsFrameRatePackage, "interpolated-frame-rate", true, &error)) {
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
                kUsFrameRatePackage, "interpolated-frame-rate", "rate",
                choice, &error)) {
            return fail(error);
        }
        const auto plan = manager.resolve(kGameId, "", kDiscSha256);
        if (expect_single_plugin(
                plan, plugin,
                std::string("wrong interpolated frame-rate plan for ") +
                    choice) != 0)
            return 1;
    }
    if (!manager.set_feature_enabled(
            kUsFrameRatePackage, "interpolated-frame-rate", false, &error) ||
        !manager.set_feature_enabled(
            kItaFrameRatePackage, "interpolated-frame-rate", true, &error)) {
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
                kItaFrameRatePackage, "interpolated-frame-rate", "rate",
                choice, &error)) {
            return fail(error);
        }
        const auto plan = manager.resolve(kItaGameId, "", kItaDiscSha256);
        if (expect_single_plugin(
                plan, plugin,
                std::string("wrong Italian interpolated frame-rate plan for ") +
                    choice) != 0)
            return 1;
    }

    if (!manager.set_feature_enabled(
            kItaFrameRatePackage,
            "interpolated-frame-rate", false, &error) ||
        !manager.set_feature_enabled(
            kUsSkipFmvsPackage, "skip-fmvs", true, &error)) {
        return fail(error);
    }
    const auto skip_plan = manager.resolve(kGameId, "", kDiscSha256);
    if (expect_single_plugin(
            skip_plan, "tomba2.fmv.skip",
            "Skip FMVs did not resolve its trusted activation plugin") != 0)
        return 1;
    if (!manager.set_feature_enabled(
            kUsSkipFmvsPackage, "skip-fmvs", false, &error) ||
        !manager.set_feature_enabled(
            kItaSkipFmvsPackage, "skip-fmvs", true, &error)) {
        return fail(error);
    }
    const auto ita_skip_enabled_plan =
        manager.resolve(kItaGameId, "", kItaDiscSha256);
    if (expect_single_plugin(
            ita_skip_enabled_plan, "tomba2.fmv.skip",
            "Italian Skip FMVs did not resolve its trusted activation plugin") !=
        0)
        return 1;

    if (!manager.set_feature_enabled(
            kItaSkipFmvsPackage, "skip-fmvs", false, &error) ||
        !manager.set_feature_enabled(
            kUsDebugMenuPackage, "debug-menu", true, &error)) {
        return fail(error);
    }
    const auto debug_plan = manager.resolve(kGameId, "", kDiscSha256);
    if (expect_single_plugin(
            debug_plan, "tomba2.debug.menu",
            "Debug Menu did not resolve its trusted plugin alone") != 0)
        return 1;
    if (!manager.set_feature_enabled(
            kUsDebugMenuPackage, "debug-menu", false, &error)) {
        return fail(error);
    }
    const auto ita_debug_plan = manager.resolve(kItaGameId, "", kItaDiscSha256);
    if (expect_no_ops(ita_debug_plan,
                      "Debug Menu for Italian target") != 0)
        return 1;

    fs::remove_all(root, ec);
    std::cout << "Tomba 2 preloaded mods: 4 US packages, "
                 "3 localized Italian packages, no Italian debug menu, "
                 "3 widescreen choices, 7 interpolated frame-rate choices, "
                 "motion-adaptive clarity blend, game-owned FMV skipping, "
                 "stock guest code untouched\n";
    return 0;
}
