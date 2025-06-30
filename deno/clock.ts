import { serve } from "./server.ts";

serve(async () => {
	const timeNow = new Date().toISOString();
	const secsNow = performance.now() / 1000.0;
	const clockSvg = `<svg xmlns="http://www.w3.org/2000/svg"
		width="480" height="480">
		<circle cx="100" cy="100" r="160" fill="lightgray" />
		<text x="100" y="120" font-size="24"
			text-anchor="right" fill="black">
			${timeNow}
			<br />
			${secsNow} seconds
		</text>
	</svg>`;
	const encoder = new TextEncoder();
	const svgData = encoder.encode(clockSvg);

	return new Response(svgData, {
		headers: {
			"Content-Type": "image/svg+xml",
			"Cache-Control": "public, max-age=31536000",
		},
	});
});
