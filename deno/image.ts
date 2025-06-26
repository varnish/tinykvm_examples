import { serve } from "./server.ts";
import {
	IMagickImage,
	ImageMagick,
	initialize,
	MagickFormat,
} from "https://deno.land/x/imagemagick_deno@0.0.31/mod.ts";

await initialize(); // ImageMagick-WASM initialization

const jpegData = Deno.readFileSync(
	"/home/gonzo/github/kvm_demo/deno/deno_city.jpeg",
);

serve(async () => {
	let buffer = await new Promise<Uint8Array>((resolve) => {
		ImageMagick.read(jpegData, (img: IMagickImage) => {
			//img.resize(200, 200);
			//img.blur(20, 6);
			img.format = MagickFormat.Avif;
			img.quality = 50; // set quality from 0 to 100
			img.speed = 10;   // set speed from 0 to 10

			img.write(
				(data: Uint8Array) => resolve(data));
		});
	});
	return new Response(buffer, {
		headers: {
			"Content-Type": "image/avif",
			"Cache-Control": "public, max-age=31536000",
		},
	});
});
