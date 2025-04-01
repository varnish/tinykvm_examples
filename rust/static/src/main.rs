mod varnish;
use crate::varnish::*;

fn on_get(_url: &str, _arg: &str)
{
	let response = "Hello, world!";

	varnish::backend_response_str(200, "text/plain", response);
}

fn on_post(_url: &str, _arg: &str, ctype: &str, data: &mut [u8])
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

			let header1 = ResponseHeader {
				data: "X-Header: Header value".as_ptr(),
				len: "X-Header: Header value".len(),
			};
			let header2 = ResponseHeader {
				data: "X-Header2: Header value".as_ptr(),
				len: "X-Header2: Header value".len(),
			};
			let headers = [header1, header2];
			let extra = ExtraResponseData {
				headers: headers.as_ptr(),
				num_headers: headers.len(),
				cached: false,
				ttl: 10.0,
				grace: 0.0,
				keep: 0.0,
			};
			varnish::backend_response_full(200, "text/plain", resp.as_bytes(), &extra);
		}
	}
}
