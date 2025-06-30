const libavif = Deno.dlopen(
	"/home/gonzo/github/kvm_demo/deno/libavif-ffi.so",
	{
		transcode_jpeg_to_avif: {
			// size_t transcode_jpeg_to_avif(const uint8_t *src, size_t len,
			//     int quality, int speed, uint8_t **out);
			parameters: ["buffer", "usize", "i32", "i32", "pointer"],
			result: "u32",
		},
	} as const,
);
import { serve } from "./server.ts";
const jpeg = await fetch("https://deno.land/images/artwork/deno_city.jpeg");
const jpegData = new Uint8Array(
	await jpeg.arrayBuffer(),
);

serve(async () => {
	const ptrContainer = new BigUint64Array(1);
	const avifDataLen = libavif.symbols.transcode_jpeg_to_avif(
		jpegData.buffer, BigInt(jpegData.byteLength),
		65,  // quality
		6,  // speed
		Deno.UnsafePointer.of(ptrContainer), // uint8_t** pointer
	);
	if (avifDataLen === 0) {
		throw new Error("Failed to transcode JPEG to AVIF");
	}

	const avifDataPtr = Deno.UnsafePointer.create(ptrContainer[0]);
	const avifPtrView = Deno.UnsafePointerView.getArrayBuffer(avifDataPtr!, avifDataLen);
	const avifData = new Uint8Array(avifPtrView);

	return new Response(avifData, {
		headers: {
			"Content-Type": "image/avif",
			"Cache-Control": "public, max-age=31536000",
		},
	});
});
