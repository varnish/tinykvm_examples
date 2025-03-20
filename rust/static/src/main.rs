mod varnish;

fn on_get(_url: &str, _arg: &str) -> !
{
	let response = "Hello, world!";

	varnish::backend_response_str(200, "text/plain", response);
}

fn on_post(_url: &str, _arg: &str, ctype: &str, data: &mut [u8]) -> !
{
	varnish::set_cacheable(false, 1.0, 0.0, 0.0);
	varnish::backend_response(200, ctype, data);
}

fn main()
{
	if false {
		varnish::set_backend_get(on_get);
		varnish::set_backend_post(on_post);
		varnish::wait_for_requests();
	}
	else {
		loop {
			let request = varnish::wait_for_requests_paused();
			let mut resp : String = "Hello, world! URL=".to_string();
			resp.push_str(&request.url());

			varnish::backend_response_str(200, "text/plain", &resp);
		}
	}
}
