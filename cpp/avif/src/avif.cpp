#include "kvm_api.hpp"
#include <avif/avif.h>
#include <nlohmann/json.hpp>
#include <turbojpeg.h>

EMBED_BINARY(rose_image, "../../../assets/rose.jpg");

static const uint8_t *current_img = NULL;
static size_t current_img_size = 0;

#if USE_LIBJPEG
#include <jpeglib.h>
#include <setjmp.h>

/* Some fucking ancient bullshit. */
struct jerror_mgr
{
	jpeg_error_mgr base;
	jmp_buf jmp;
};
METHODDEF(void) jerror_exit(j_common_ptr cinfo)
{
	jerror_mgr *err = (jerror_mgr *)cinfo->err;
	longjmp(err->jmp, 1);
}
METHODDEF(void) joutput_message(j_common_ptr) {}
#endif

/* For regular errors without the VM itself crashing, we can use this
   fallback function instead of the on_error callback. */
static void bail(const uint8_t *src, size_t len, const std::string& reason) {
	set_cacheable(false, 10.0f, 0.0, 0.0);
	Http::append(5, "X-Failed: " + reason);
	Backend::response(500, "image/jpeg", src, len);
}

static avifImage *image = nullptr;
static avifEncoder *encoder = nullptr;
static avifRWData avifOutput;

/* This function decodes a JPEG and encodes an AVIF, with medium quality. */
template <bool IsKVM>
void produce_image(const uint8_t *source_image, const size_t source_image_len, int quality = 75, int speed = 6)
{
#if USE_LIBJPEG
	struct jpeg_decompress_struct cinfo;
	jerror_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr.base);
	jerr.base.error_exit = jerror_exit;
	jerr.base.output_message = joutput_message;
	jpeg_create_decompress(&cinfo);

	jpeg_mem_src(&cinfo, source_image, source_image_len);
	if (!jpeg_read_header(&cinfo, TRUE))
		bail(source_image, source_image_len, "jpeg_read_header");

	cinfo.dct_method = JDCT_DEFAULT;
	
	if (!jpeg_start_decompress(&cinfo))
		bail(source_image, source_image_len, "jpeg_start_decompress");

	const unsigned W = cinfo.output_width;
	const unsigned H = cinfo.output_height;

	/* Induce an error that stops image conversion. This will
	   trigger on_error from outside the VM, for 1.0 seconds,
	   allowing a fallback response to be generated instead of
	   a generic 500/503 error. */
	//asm("ud2");

	/* Create RGB image buffer */
	avifRGBImage rgb;
	memset(&rgb, 0, sizeof(rgb));

	if (image) avifImageDestroy(image);
	image = avifImageCreate(W, H, 8, AVIF_PIXEL_FORMAT_YUV420);

	avifRGBImageSetDefaults(&rgb, image);
	rgb.format = AVIF_RGB_FORMAT_RGB;
	rgb.chromaUpsampling = AVIF_CHROMA_UPSAMPLING_AUTOMATIC;
	avifRGBImageAllocatePixels(&rgb);

	/* Decode JPEG into image buffer */
	JSAMPROW ptr[H];
	for (unsigned i = 0; i < H; i++) {
		ptr[i] = &rgb.pixels[i * W * cinfo.output_components];
	}
	while (cinfo.output_scanline < H) {
		/* This will decode around 2 or 4 scanlines at a time */
		const auto scanlines =
			jpeg_read_scanlines(&cinfo, ptr + cinfo.output_scanline, H);
		if (scanlines < 1)
			bail(source_image, source_image_len, "jpeg_read_scanlines");
	}
	if (!jpeg_finish_decompress(&cinfo))
		bail(source_image, source_image_len, "jpeg_finish_decompress");

	jpeg_destroy_decompress(&cinfo);

	/* Convert RGB to YUV */
	if (avifImageRGBToYUV(image, &rgb) != AVIF_RESULT_OK)
		bail(source_image, source_image_len, "avifImageRGBToYUV");
#else
	// Turbo JPEG - direct to YUV planes
	auto tj = tjInitDecompress();
	// How can you make a good API with this absolute shit?
	int W  = 0;
	int H = 0;
	int subsamp = 0;
	int colorspc = 0;
	tjDecompressHeader3(tj, source_image, source_image_len,
		&W, &H, &subsamp, &colorspc);

	// TODO: Use colorspc to determine YUV format bits
	if (image) avifImageDestroy(image);
	image = avifImageCreate(W, H, 8, AVIF_PIXEL_FORMAT_YUV420);

	const avifResult allocateResult = avifImageAllocatePlanes(image, AVIF_PLANES_ALL);
	if (allocateResult != AVIF_RESULT_OK)
		bail(source_image, source_image_len, avifResultToString(allocateResult));

	tjDecompressToYUVPlanes(tj, source_image, source_image_len,
		image->yuvPlanes, W, (int *)image->yuvRowBytes, H, 0);

	// ???
	memset(image->alphaPlane, 255, image->alphaRowBytes * image->height);


#endif

	/* Encode AVIF */
	if (encoder) avifEncoderDestroy(encoder);
	encoder = avifEncoderCreate();
	encoder->maxThreads = 1;
	encoder->speed = speed;
	encoder->quality = quality;
	encoder->minQuantizer = 0; // 0-63
	encoder->maxQuantizer = 63; // 0-63
	avifResult imgres =
		avifEncoderAddImage(encoder, image, 1, AVIF_ADD_IMAGE_FLAG_SINGLE);
	if (imgres != AVIF_RESULT_OK)
		bail(source_image, source_image_len, avifResultToString(imgres));

	if (avifOutput.data) avifRWDataFree(&avifOutput);
	avifOutput = AVIF_DATA_EMPTY;
	avifEncoderFinish(encoder, &avifOutput);

#if USE_LIBJPEG
	avifRGBImageFreePixels(&rgb);
#else
	tjDestroy(tj);
#endif

	if constexpr (IsKVM) {
		struct meminfo info;
		get_meminfo(&info);
		Http::append(RESP,
			std::string("X-Memory-Usage: ") + std::to_string(info.reqmem_current / 1024) + "KB");

		/* Respond with the image. */
		Backend::response(200, "image/avif", avifOutput.data, avifOutput.size);
	} else {
		fprintf(stdout, "Image produced, size: %zu\n", avifOutput.size);
		exit(0);
	}
}

static void
on_get(const char *url, const char *arg)
{
	Curl::Result image;
	int quality = 75;
	int speed = 6;
	auto arglen = strlen(arg);
	if (arglen > 0)
	{
		const auto j = nlohmann::json::parse(arg, arg + arglen, nullptr, true, true);

		if (j.contains("quality")) {
			quality = j["quality"].get<int>();
		}
		if (j.contains("speed")) {
			speed = j["speed"].get<int>();
		}

		std::vector<std::string> headers;
		if (j.contains("headers")) {
			headers = j["headers"].get<std::vector<std::string>>();
		}

		struct curl_options options = {};
		options.follow_location = true;
		options.dont_verify_host = true;

		image = Curl::fetch(url, headers, &options);
	}
	else {
		// Benchmarking mode
		image = {
			.status = 200,
			.content_type = "image/jpeg",
			.content = { rose_image, rose_image_size }
		};
	}

	if (image.status == 200)
	{
		/* For on_error fallback delivery. */
		current_img = (const uint8_t *)image.content.begin();
		current_img_size = image.content.size();

		produce_image<true>(current_img, current_img_size, quality, speed);
	}
	else {
		// Probably an error.
		Backend::response(503, "text/plain", "Failed to retrieve image asset");
	}
}

static void
on_post(const char *url, const char *arg, const char *, const uint8_t *src, size_t len)
{
	int quality = 75;
	int speed = 6;
	auto arglen = strlen(arg);
	if (arglen > 0)
	{
		const auto j = nlohmann::json::parse(arg, arg + arglen, nullptr, true, true);

		if (j.contains("quality")) {
			quality = j["quality"].get<int>();
		}
		if (j.contains("speed")) {
			speed = j["speed"].get<int>();
		}
	}

	/* You can POST a JPEG file and have it converted to AVIF. */
	current_img = src;
	current_img_size = len;
	produce_image<true>(src, len, quality, speed);
}

/* on_error can be used as a fallback function where we can
   still send something useful after *any* exception occured. */
static void
on_error(const char *url, const char *, const char *exception)
{
	set_cacheable(false, 10.0f, 0.0, 0.0);

	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "X-Error: %s", exception);
	http_append_str(RESP, buffer);
	/* Respond with the source image instead of AVIF. */
	Backend::response(200, "image/jpeg", current_img, current_img_size);
}

int main(int argc, char** argv)
{
	if (IS_LINUX_MAIN()) {
		produce_image<false> ((const uint8_t *)rose_image, rose_image_size);
		return 0;
	}

	/* GET method callback. */
	set_backend_get(on_get);
	/* POST method callback. */
	set_backend_post(on_post);
	/* Exception callback (short timeout). */
	set_on_error(on_error);
	/* Waiting for requests ensures we do not exit main. */
	wait_for_requests();
}
