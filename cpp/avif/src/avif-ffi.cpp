#include <avif/avif.h>
#include <nlohmann/json.hpp>
#include <turbojpeg.h>
#define USE_LIBJPEG 0
#define ENCODE_VIA_RGB 1

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

static void bail(const uint8_t *src, size_t len, const std::string& reason) {
	fprintf(stderr, "Error: %s\n", reason.c_str());
	abort();
}

static avifImage *image = nullptr;
static avifEncoder *encoder = nullptr;
static avifRWData avifOutput;

/* This function decodes a JPEG and encodes an AVIF, with medium quality. */
static void produce_image(const uint8_t *source_image, const size_t source_image_len,
	uint8_t *&out_data, size_t &out_size,
	int quality = 75, int speed = 6)
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

#if ENCODE_VIA_RGB
	avifRGBImage rgb;
	memset(&rgb, 0, sizeof(rgb));

	avifRGBImageSetDefaults(&rgb, image);
	rgb.format = AVIF_RGB_FORMAT_RGB;
	rgb.chromaUpsampling = AVIF_CHROMA_UPSAMPLING_AUTOMATIC;
	avifRGBImageAllocatePixels(&rgb);

	tjDecompress2(tj, source_image, source_image_len,
		rgb.pixels, W, rgb.rowBytes, H, TJPF_RGB, 0);

	avifImageRGBToYUV(image, &rgb);
#else
	const avifResult allocateResult = avifImageAllocatePlanes(image, AVIF_PLANES_ALL);
	if (allocateResult != AVIF_RESULT_OK)
		bail(source_image, source_image_len, avifResultToString(allocateResult));

	tjDecompressToYUVPlanes(tj, source_image, source_image_len,
		image->yuvPlanes, W, (int *)image->yuvRowBytes, H, 0);

	// ???
	memset(image->alphaPlane, 255, image->alphaRowBytes * image->height);
#endif // ENCODE_VIA_RGB

#endif // USE_LIBJPEG

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

	out_data = avifOutput.data;
	out_size = avifOutput.size;
}

extern "C" size_t __attribute__((visibility("default")))
transcode_jpeg_to_avif(const uint8_t *src, size_t len, int quality, int speed,
	uint8_t **out_data)
{
	uint8_t *data = NULL;
	size_t size = 0;
	produce_image(src, len, data, size, quality, speed);
	*out_data = data;
	return size;
}
