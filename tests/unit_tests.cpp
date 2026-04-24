// Verifies the pure helper modules that back settings, pixel conversion, and
// USB label formatting.

#include "i18n.h"
#include "pixel_conversion.h"
#include "settings_codec.h"
#include "usb_topology_format.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace {

class ScopedLangReset {
public:
    ScopedLangReset() = default;

    ~ScopedLangReset() {
        i18n::setLang(previous_);
    }

private:
    i18n::Lang previous_ = i18n::getLang();
};

} // namespace

TEST_CASE("i18n codes and labels") {
    const ScopedLangReset reset;

    REQUIRE(i18n::langFromCode(nullptr) == i18n::Lang::Japanese);
    REQUIRE(i18n::langFromCode("en") == i18n::Lang::English);
    REQUIRE(i18n::langFromCode("ko") == i18n::Lang::Korean);
    REQUIRE(i18n::langFromCode("unknown") == i18n::Lang::Japanese);

    REQUIRE(std::string(i18n::langCode(i18n::Lang::Japanese)) == "ja");
    REQUIRE(std::string(i18n::langCode(i18n::Lang::English)) == "en");
    REQUIRE(std::string(i18n::langLabel(i18n::Lang::Korean)) == u8"한국어");

    i18n::setLang(i18n::Lang::English);
    REQUIRE(std::string(i18n::L(i18n::S::ViewResetBtn)) == "Reset View");

    i18n::setLang(i18n::Lang::Korean);
    REQUIRE(i18n::getLang() == i18n::Lang::Korean);
    REQUIRE(std::string(i18n::L(i18n::S::ViewResetBtn)) == u8"시점 초기화");
    REQUIRE(std::string(i18n::L(static_cast<i18n::S>(999))).empty());
}

TEST_CASE("settings codec parses trimmed ini") {
    std::istringstream input(R"ini(
      ; comments and blank lines are ignored
      lang = en
      point_mode = cpu_point
      depth_w = 1024
      depth_h = invalid
      color_w = 1920
      color_h = 1080
      fps = 15
      show_ir = yes
      vsync = 0
    )ini");

    const auto settings = app_settings::loadFromStream(input);
    REQUIRE(settings.lang == i18n::Lang::English);
    REQUIRE(settings.pointMode == PointRenderMode::CpuPoint);
    REQUIRE(settings.stream.depthW == 1024);
    REQUIRE(settings.stream.depthH == 576);
    REQUIRE(settings.stream.colorW == 1920);
    REQUIRE(settings.stream.colorH == 1080);
    REQUIRE(settings.stream.fps == 15);
    REQUIRE(settings.showIr);
    REQUIRE_FALSE(settings.vsync);
}

TEST_CASE("settings codec serializes stable ini") {
    app_settings::AppSettings settings;
    settings.lang = i18n::Lang::Korean;
    settings.pointMode = PointRenderMode::GpuPoint;
    settings.stream.depthW = 512;
    settings.stream.depthH = 512;
    settings.stream.colorW = 640;
    settings.stream.colorH = 480;
    settings.stream.fps = 5;
    settings.showIr = true;
    settings.vsync = false;

    const std::string actual = app_settings::serialize(settings);
    const std::string expected = "lang=ko\n"
                                 "point_mode=point\n"
                                 "depth_w=512\n"
                                 "depth_h=512\n"
                                 "color_w=640\n"
                                 "color_h=480\n"
                                 "fps=5\n"
                                 "show_ir=1\n"
                                 "vsync=0\n";
    REQUIRE(actual == expected);
}

TEST_CASE("color pixel conversion handles rgb and bgr") {
    std::vector<uint8_t> out;
    const uint8_t rgb[] = {10, 20, 30, 40, 50, 60};
    REQUIRE(convertColorPixelsToRgb(rgb, RawFrameFormat::Rgb, 2, 1, out));
    REQUIRE(out == std::vector<uint8_t>{10, 20, 30, 40, 50, 60});

    const uint8_t bgr[] = {30, 20, 10, 60, 50, 40};
    REQUIRE(convertColorPixelsToRgb(bgr, RawFrameFormat::Bgr, 2, 1, out));
    REQUIRE(out == std::vector<uint8_t>{10, 20, 30, 40, 50, 60});

    REQUIRE_FALSE(convertColorPixelsToRgb(rgb, RawFrameFormat::Y8, 2, 1, out));
}

TEST_CASE("depth pixel conversion uses scale and palette") {
    std::vector<uint8_t> out;
    const uint16_t depthMm[] = {0, 250, 2625, 5000};
    REQUIRE(convertDepthPixelsToPseudoRgb(depthMm, RawFrameFormat::Z16, 4, 1, 1.0f, out));
    REQUIRE(out == std::vector<uint8_t>{0, 0, 0, 255, 255, 0, 127, 0, 127, 0, 255, 255});

    const uint16_t scaledDepth[] = {125};
    REQUIRE(convertDepthPixelsToPseudoRgb(scaledDepth, RawFrameFormat::Y16, 1, 1, 2.0f, out));
    REQUIRE(out == std::vector<uint8_t>{255, 255, 0});
}

TEST_CASE("ir pixel conversion handles y8 and y16") {
    std::vector<uint8_t> out;
    const uint16_t ir16[] = {0, 4095, 5000};
    REQUIRE(convertIrPixelsToGrayscaleRgb(ir16, RawFrameFormat::Y16, 3, 1, out));
    REQUIRE(out == std::vector<uint8_t>{0, 0, 0, 255, 255, 255, 255, 255, 255});

    const uint8_t ir8[] = {5, 200};
    REQUIRE(convertIrPixelsToGrayscaleRgb(ir8, RawFrameFormat::Y8, 2, 1, out));
    REQUIRE(out == std::vector<uint8_t>{5, 5, 5, 200, 200, 200});
}

TEST_CASE("usb controller formatting") {
    REQUIRE(normalizeUsbControllerName(
                "  Intel(R) USB 3.10   eXtensible Host Controller - 1.10 (Microsoft) ") ==
            "Intel(R) USB 3.10 xHCI");

    SystemUsbTopology topology;
    topology.controllers = {"pci-a", "pci-b"};
    REQUIRE(formatControllerDisplayName(topology, "pci-b", "Intel xHCI") == "#2 Intel xHCI");
    REQUIRE(formatControllerDisplayName(topology, "", "") == "Unknown Controller");
    REQUIRE(formatControllerDisplayName(topology, "pci-missing", "Fallback") == "Fallback");
}
