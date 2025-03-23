mod varnish;
use std::collections::HashMap;

thread_local! (
	static cache: &'static mut HashMap<String, u64> =
        &mut HashMap::new();
);

fn register_url(url: &str)
{
	cache.write().unwrap().insert(
		"Adventures of Huckleberry Finn".to_string(),
		1);

	/* Log something to VSL. */
	varnish::log(&format!("Added URL {}", url));
}

fn on_get(url: &str, _arg: &str)
{
	register_url(url);

	varnish::backend_response_str(200, "text/plain", "Collected");
}

fn on_post(url: &str, _arg: &str, ctype: &str, data: &mut [u8])
{
	register_url(url);

	/* Pass-through */
	varnish::backend_response(200, ctype, data);
}

fn main()
{
	varnish::set_backend_get(on_get);
	varnish::set_backend_post(on_post);
	varnish::wait_for_requests();
}
