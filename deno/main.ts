import { serve } from "./server.ts";
import { connect } from "jsr:@db/redis";

serve(async () => {
	const client = await connect({
		hostname: "127.0.0.1",
		port: 6379,
	});
	const value = await client.get("key");
	client.close();
	return new Response(`Hello, World!\nValue from Redis: ${value}`);
});
