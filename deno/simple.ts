import { serve } from "./server.ts";

serve(async () => {
	return new Response(`Hello, World!`,
		{
			headers: {
				"Content-Type": "text/plain",
			},
			status: 200,
		},
	);
});
