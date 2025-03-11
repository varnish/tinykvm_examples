#include "kvm_api.h" // Order matters
#include "helpers.hpp"
#include "load_file.cpp"

#include <nlohmann/json.hpp>
#include <webp/encode.h>

const uint8_t *original_img = NULL;
size_t         original_img_size = 0;
std::string    original_content_type;

extern void decode_webp(WebPPicture&, const uint8_t *, size_t len);
extern void decode_jpeg(WebPPicture&, const uint8_t *, size_t len);

/* This function decodes a JPEG and encodes an AVIF, with medium quality. */
template <bool KVM>
void produce_image(const nlohmann::json& j,
	std::string_view content_type,
	const uint8_t *source_image, const size_t source_image_len)
{
	WebPPicture picture;
	WebPPictureInit(&picture);

	WebPConfig config;
	WebPConfigInit(&config);
	config.quality  = 50.0f;
	config.method   = 3; /* 1=Fastest, 6=Slowest */
	config.filter_type  = 0;
	config.thread_level = 0;

	if (j.contains("quality")) {
		config.quality = j["quality"];
	}
	if (j.contains("method")) {
		config.method = j["method"];
	}

	/* Decode source image, based on (partial) magic signature. */
	const uint16_t magic = *(uint16_t *)source_image;
	switch (magic)
	{
		case 0xD8FF: // JPEG
			decode_jpeg(picture, source_image, source_image_len);
			config.lossless = false;
			break;
		case 0x5089: // PNG
			bail("Unimplemented image format: PNG");
			config.lossless = true;
			break;
		case 0x4952: // WebP
			decode_webp(picture, source_image, source_image_len);
			config.lossless = true;
			break;
		default:
			bail("Unrecognized image format");
	}

	if (!WebPValidateConfig(&config))
		bail("WebP configuration failed validation");

	/* Image transformations */
	if (j.contains("crop")) {
		std::vector<int> crop = j["crop"];
		int crop_x = 0;
		int crop_y = 0;
		int crop_w = picture.width;
		int crop_h = picture.height;
		if (crop.size() == 2) {
			crop_w = crop[0];
			crop_h = crop[1];
		}
		else if (crop.size() == 4) {
			crop_x = crop[0];
			crop_y = crop[1];
			crop_w = crop[2];
			crop_h = crop[3];
		}
		else {
			bail("WebP: Invalid crop array count: " + std::to_string(crop.size()));
		}
		if (WebPPictureCrop(&picture, crop_x, crop_y, crop_w, crop_h) < 0)
			bail("WebP: Failed to crop image");
	}

	if (j.contains("resize")) {
		std::vector<size_t> resize = j["resize"];
		int w = resize.at(0);
		int h = 0;
		if (resize.size() >= 2)
			h = resize.at(1);

		/* Maintains aspect ratio if either w or h is 0. */
		if (WebPPictureRescale(&picture, w, h) < 0)
			bail("WebP: Failed to resize image to w=" + std::to_string(w) + ", h=" + std::to_string(h));
	}

	/* Encode final WebP image */
	std::vector<uint8_t> buffer;
	picture.user_data = &buffer;
	picture.writer =
	[] (const uint8_t* data, size_t data_size, const auto* picture) -> int
	{
		auto* vec = (decltype(buffer)*) picture->user_data;
		vec->insert(vec->end(), data, data + data_size);
		return data_size;
	};

	if (!WebPEncode(&config, &picture))
		bail("WebPEncode failed: " + std::to_string(picture.error_code));

	if constexpr (KVM)
	{
		/* Compute HTTP response */
		struct meminfo info;
		get_meminfo(&info);
		Http::append(RESP,
			std::string("X-Memory-Usage: ") + std::to_string(info.reqmem_current / 1024) + "KB");

		/* Respond with the image (always WebP at this point). */
		Backend::response(200, "image/webp", buffer.data(), buffer.size());
	}
	else
	{
		static int counter = 0;
		/* Linux test output */
		write_file("output" + std::to_string(counter++) + ".webp", buffer);
	}
}

static void
on_get(const char *url, const char *arg)
{
	const auto j = nlohmann::json::parse(arg, arg + strlen(arg), nullptr, true, true);

	std::vector<std::string> headers;
	if (j.contains("headers")) {
		headers = j["headers"].get<std::vector<std::string>>();
	}

	struct curl_options options = {};
	options.follow_location = true;
	options.dont_verify_host = true;

	auto image = Curl::fetch(url, headers, &options);

	if (image.status == 200)
	{
		/* For on_error fallback delivery. */
		original_img = (const uint8_t *)image.content.begin();
		original_img_size = image.content.size();

		produce_image<true>(j, image.content_type, original_img, original_img_size);
	}
	else {
		// Probably an error.
		Backend::response(503, "text/plain", "Failed to retrieve image asset");
	}
}

template <bool KVM> static void
on_post(const char *url, const char *arg, const char *ctype, const uint8_t *src, size_t len)
{
	/* Deliver the original image in case of errors. */
	original_img = src;
	original_img_size = len;
	original_content_type = ctype;

	const auto j = nlohmann::json::parse(arg, arg + strlen(arg), nullptr, true, true);
	produce_image<KVM>(j, ctype, src, len);
}

/* on_error can be used as a fallback function where we can
   still send something useful, after any exception occured. */
static void
on_error(const char *url, const char *, const char *exception)
{
	//set_cacheable(false, 10.0f, 0.0, 0.0);

	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "X-Error: %s", exception);
	http_append_str(RESP, buffer);
	/* Respond with the source image instead of WebP. */
	Backend::response(200, original_content_type, original_img, original_img_size);
}

int main(int, char** argv)
{
	if (IS_LINUX_MAIN())
	{
		// Test1: Convert JPEG to WebP
		auto f1 = load_file("../../assets/rose.jpg");
		on_post<false>("", "{}", "image/jpeg", f1.data(), f1.size());

		// Test2: Convert WebP to WebP
		auto f2 = load_file("output0.webp");
		on_post<false>("", "{}", "image/webp", f2.data(), f2.size());

		return 0;
	}

	/* GET method callback. */
	set_backend_get(on_get);
	/* POST method callback. */
	set_backend_post(on_post<true>);
	/* Exception callback (short timeout). */
	set_on_error(on_error);
	/* Waiting for requests ensures we do not exit main. */
	wait_for_requests();
}
