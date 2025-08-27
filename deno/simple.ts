import { http_get, http_set_req, http_set_resp, serve } from "./server.ts";
Object.assign(globalThis, { http_get, http_set_req, http_set_resp });

serve(async (req) => {
	const AsyncFunction = async function () { }.constructor;
	let f = new AsyncFunction('req', req.argument)
	await f(req)
	//await import(`data:,${req.argument}`);
	return new Response(`OK`);
});
