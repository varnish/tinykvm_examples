#include "kvm_api.hpp"
#include "Simd/SimdLib.hpp"
#include "Simd/SimdImageLoad.h"
#include "Simd/SimdImageSave.h"
#include <nlohmann/json.hpp>
#include <map>

static std::map<std::string, size_t> sizes = {
    {"tiny",   75},
    {"small",  150},
    {"medium", 400},
};

static void
on_get(const char *url, const char *arg)
{
	// Parse JSON with comments enabled
	const auto j = nlohmann::json::parse(arg, arg + strlen(arg), nullptr, true, true);

    std::vector<std::string> headers;
	if (j.contains("headers")) {
		headers = j["headers"].get<std::vector<std::string>>();
    }

    /* In the presence of ? we can automatically downsize. */
    const char *q = strchr(url, '?');
    if (q != nullptr) {
		/* Thumbnail map from optional {"sizes": {}} object in JSON */
		if (j.contains("sizes"))
		{
			sizes = j.at("sizes").get<std::map<std::string, size_t>>();
		}

        /* String to image size. */
        const auto new_size = sizes.at(q + 1);
		/* Base URL */
		std::string_view base_url { url, size_t(q - url) };

        /* Construct new image of @new_size, from image in cache */
		auto resp = Curl::fetch(base_url, headers);
		if (resp.failed()) {
			Backend::response(503, "text/plain", "Failed to retrieve asset from cache");
		}

        typedef Simd::View<Simd::Allocator> View;
        View decodedImage;
        // Decode image from memory.
        decodedImage.Load((const uint8_t *)resp.content.data(), resp.content.size(), View::Bgr24);
        // Must be same pixelformat (24-bit BGR). Retain aspect.
		const float aspect = float(decodedImage.height) / decodedImage.width;
        View resizedImage(new_size, new_size * aspect, View::Bgr24);

        // Bilinear downsizing.
        Simd::Resize(decodedImage, resizedImage, SimdResizeMethodBilinear);

        // Encode back to 24-bit JPEG.
        size_t size = 0;
        uint8_t *data = Simd::Avx2::ImageSaveToMemory(
            resizedImage.data, resizedImage.stride,
            resizedImage.width, resizedImage.height,
            SimdPixelFormatType::SimdPixelFormatRgb24,
            SimdImageFileType::SimdImageFileJpeg, 90, &size);

        Backend::response(200, "image/jpeg", data, size);
    }

    /* Original image, no resizing. */
	struct curl_options options = {};
	options.follow_location = true;
	options.dont_verify_host = true;

	auto resp = Curl::fetch(j.at("url").get<std::string>(), headers, &options);
	if (resp.failed()) {
		Backend::response(503, "text/plain", "Failed to retrieve asset from backend");
	}

    /* Respond with the image. */
    Backend::response(resp.status, resp.content_type, resp.content);
}

int main(int argc, char** argv)
{
	if (IS_LINUX_MAIN())
	{
		printf("Main of thumbnails demo\n");
		exit(0);
	}

    set_backend_get(on_get);
    wait_for_requests();
}
