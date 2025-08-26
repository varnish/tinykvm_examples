import { http_get, http_set_req, http_set_resp, serve } from "./server.ts";

serve(async (req) => {
	if (req.argument === "") {
		req.argument = `
		// We can get/set from HTTP request
		const [key, value] = http_get("X-JS");

		// These will be forwarded to backend_fetch (bereq)
		http_set_req("X-Deno", \`Hello from Deno URL=\${req.url} Method=\${req.method}\`);
		http_set_req("X-Deno2", \`ARG=\${req.argument}\`);
		`;
	}

	const AsyncFunction = async function () { }.constructor;
	let f = new AsyncFunction('req', 'http_get', 'http_set_req', 'http_set_resp', req.argument)
	await f(req, http_get, http_set_req, http_set_resp)
	return new Response(`OK`);
});
