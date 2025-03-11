#include "helpers.hpp"
#include <webp/encode.h>
#include <webp/decode.h>

void decode_webp(WebPPicture& picture, const uint8_t *inp, size_t inp_len)
{
	WebPBitstreamFeatures input;
	if (WebPGetFeatures(inp, inp_len, &input) != VP8_STATUS_OK)
		bail("WebP: invalid input image");

	const int stride = (input.has_alpha ? 4 : 3) * input.width;

	const int buff_size = stride * input.height;
	auto* buff = new uint8_t[buff_size];

	picture.width  = input.width;
	picture.height = input.height;

	if (input.has_alpha) {
		if (WebPDecodeRGBAInto(inp, inp_len, buff, buff_size, stride) == nullptr)
			bail("WebP: WebPDecodeRGBAInto failed");
		if (WebPPictureImportRGBA(&picture, buff, stride) < 0)
			bail("WebP: WebPPictureImportRGBA failed");
	} else {
		if (WebPDecodeRGBInto(inp, inp_len, buff, buff_size, stride) == nullptr)
			bail("WebP: WebPDecodeRGBInto failed");
		if (WebPPictureImportRGB(&picture, buff, stride) < 0)
			bail("WebP: WebPPictureImportRGB failed");
	}

	// delete[] buff;
}
