#include "helpers.hpp"
#include <turbojpeg.h>
#include <webp/encode.h>
#include <vector>

#define USE_RGB 1 /* In contrast to YUV planes */
static std::vector<uint8_t> imageBuffer;

void decode_jpeg(WebPPicture& picture, const uint8_t *inp, size_t inp_len)
{
#if USE_RGB
	// Turbo JPEG - direct to YUV planes
	auto tj = tjInitDecompress();
	// How can you make a good API with this absolute shit?
	int W  = 0;
	int H  = 0;
	int subsamp = 0;
	int colorspc = 0;
	if (tjDecompressHeader3(tj, inp, inp_len, &W, &H, &subsamp, &colorspc) < 0)
		bail("tjDecompressHeader3");

	const int stride = 3 * W;

	/* Create RGB image buffer */
	imageBuffer.resize(stride * H);

	if (tjDecompress2(tj, (uint8_t *)inp, inp_len,
		imageBuffer.data(), W, 0, H, TJPF_RGB, TJFLAG_FASTDCT) < 0) {
		bail("tjDecompress");
	}

	tjDestroy(tj);

	/* Convert RGB to YUV */
	picture.width  = W;
	picture.height = H;

	if (!WebPPictureImportRGB(&picture, imageBuffer.data(), stride))
		bail("WebPPictureImportRGB");
#else
	// Turbo JPEG - direct to YUV planes
	auto tj = tjInitDecompress();
	// How can you make a good API with this absolute shit?
	int W = 0;
	int H = 0;
	int subsamp = 0;
	int colorspc = 0;
	if (tjDecompressHeader3(tj, inp, inp_len, &W, &H, &subsamp, &colorspc) < 0)
		bail("tjDecompressHeader3");

	picture.use_argb = false;
	picture.colorspace = WEBP_YUV420; /* TODO: Use colorspc */
	picture.width  = W;
	picture.height = H;
	WebPPictureAlloc(&picture);

	uint8_t* yuv[3] {
		picture.y,
		picture.u,
		picture.v,
	};
	int row_bytes[3] = {
		picture.y_stride,
		picture.uv_stride,
		picture.uv_stride,
	};
	if (tjDecompressToYUVPlanes(tj, inp, inp_len, yuv, W, row_bytes, H, TJFLAG_FASTDCT) < 0)
		bail("tjDecompressToYUVPlanes");

	tjDestroy(tj);
#endif
}
