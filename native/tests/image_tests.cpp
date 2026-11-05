// Tests for images on the CPU: generating, flipping, and PNG files. Textures
// are uploaded from these, so a wrong row order here shows up as upside-down
// textures later.

#include "core/Paths.h"
#include "render/Image.h"

#include <doctest/doctest.h>

#include <cstdio>
#include <filesystem>

using namespace Engine;

TEST_SUITE("image") {
    TEST_CASE("a new image is transparent black with 4 bytes per pixel") {
        const Image image(3, 2);
        CHECK(image.pixels.size() == 3 * 2 * 4);
        CHECK_FALSE(image.empty());
        CHECK(image.at(2, 1)[0] == 0);
        CHECK(image.at(2, 1)[3] == 0);
        CHECK(Image().empty());
    }

    TEST_CASE("setPixel writes the pixel at (x, y), with y = 0 on top") {
        Image image(4, 4);
        image.setPixel(1, 2, 10, 20, 30, 40);
        const std::uint8_t* p = image.at(1, 2);
        CHECK(p[0] == 10);
        CHECK(p[1] == 20);
        CHECK(p[2] == 30);
        CHECK(p[3] == 40);
        // Row-major: (1, 2) is 2 rows of 4 pixels plus 1 pixel in.
        CHECK(p == image.pixels.data() + (2 * 4 + 1) * 4);
    }

    TEST_CASE("checkerboard cells alternate colors") {
        const Image board = makeCheckerboard(8, 2, Vec3(1.0f), Vec3(0.0f));
        // 2 x 2 cells of 4 x 4 pixels each.
        CHECK(board.at(0, 0)[0] == 255);
        CHECK(board.at(3, 3)[0] == 255);
        CHECK(board.at(4, 0)[0] == 0);
        CHECK(board.at(0, 4)[0] == 0);
        CHECK(board.at(7, 7)[0] == 255);
    }

    TEST_CASE("flipVertically swaps the top and bottom rows") {
        Image image(2, 3);
        image.setPixel(0, 0, 1, 0, 0);
        image.setPixel(0, 1, 2, 0, 0);
        image.setPixel(0, 2, 3, 0, 0);
        flipVertically(image);
        CHECK(image.at(0, 0)[0] == 3);
        CHECK(image.at(0, 1)[0] == 2); // the middle row of an odd height stays put
        CHECK(image.at(0, 2)[0] == 1);
    }

    TEST_CASE("saving and loading a PNG gives back the same pixels") {
        Image original(5, 3);
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 5; ++x) {
                original.setPixel(x, y, static_cast<std::uint8_t>(x * 50), static_cast<std::uint8_t>(y * 100), 7,
                                  static_cast<std::uint8_t>(255 - x));
            }
        }
        const std::string path = (std::filesystem::temp_directory_path() / "xordor_image_test.png").string();
        REQUIRE(savePng(path, original));

        std::string error;
        const auto loaded = loadImage(path, &error);
        std::remove(path.c_str());
        REQUIRE_MESSAGE(loaded.has_value(), error);
        CHECK(loaded->width == 5);
        CHECK(loaded->height == 3);
        CHECK(loaded->pixels == original.pixels); // PNG is lossless, so exactly equal
    }

    TEST_CASE("the sample crate texture loads from the assets folder") {
        std::string error;
        const auto crate = loadImage(assetPath("textures/crate.png"), &error);
        REQUIRE_MESSAGE(crate.has_value(), error);
        CHECK(crate->width == 128);
        CHECK(crate->height == 128);
        CHECK(crate->at(64, 64)[3] == 255);
    }

    TEST_CASE("loading a missing file returns nothing and says why") {
        std::string error;
        const auto missing = loadImage("no/such/file.png", &error);
        CHECK_FALSE(missing.has_value());
        CHECK(error.find("no/such/file.png") != std::string::npos);
    }
}
