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
const jpegData = Deno.readFileSync(
	"/home/gonzo/github/kvm_demo/deno/deno_city.jpeg",
);

serve(async () => {
	const ptrContainer = new BigUint64Array(1);
	const avifDataLen = libavif.symbols.transcode_jpeg_to_avif(
		jpegData.buffer, jpegData.byteLength,
		50,  // quality
		10,  // speed
		Deno.UnsafePointer.of(ptrContainer), // output pointer
	);
	if (avifDataLen === 0) {
		throw new Error("Failed to transcode JPEG to AVIF");
	}

	const avifDataPtr = Deno.UnsafePointer.create(ptrContainer[0]);
	const avifPtrView = new Deno.UnsafePointerView(avifDataPtr);

	const avifData = new Uint8Array(avifDataLen);
	avifPtrView.copyInto(avifData);

	return new Response(avifData, {
		headers: {
			"Content-Type": "image/avif",
			"Cache-Control": "public, max-age=31536000",
		},
	});
});
