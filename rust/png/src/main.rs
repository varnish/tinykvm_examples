mod varnish;
use std::io::BufWriter;

fn on_get(url: &str, _arg: &str) -> !
{
	/* Generate luxurious and expensive blue color. */
	const IMG_WIDTH: usize = 1024;
	const IMG_HEIGHT: usize = 1024;
	const SCANLINE: usize = IMG_WIDTH * 3;
	let mut image: Vec<u8> = vec![0; (SCANLINE * IMG_HEIGHT) as usize];

	for y in 0..IMG_HEIGHT {
		for x in 0..IMG_WIDTH {
			image[x*3+0 + y * SCANLINE] = 44;
			image[x*3+1 + y * SCANLINE] = 144;
			image[x*3+2 + y * SCANLINE] = 244;
		}
	}

	/* Encode losslessly in beautiful PNG. */
	let mut bufwriter = BufWriter::new(Vec::new());
	{
		let mut encoder = png::Encoder::new(&mut bufwriter, IMG_WIDTH as u32, IMG_WIDTH as u32);
		encoder.set_depth(png::BitDepth::Eight);
		encoder.set_color(png::ColorType::RGB);
		let mut writer = encoder.write_header().unwrap();
		writer.write_image_data(&image).unwrap();
	}

	/* Set a response header field. */
	let hello = format!("X-Hello: {}", url);
	varnish::append(varnish::RESP, &hello);

	/* Log something to VSL. */
	varnish::log(&format!("The URL is {}", url));

	/* Uncacheable for 1 second. */
	varnish::set_cacheable(false, 1.0, 0.0, 0.0);

	varnish::backend_response(200, "image/png",
		&bufwriter.into_inner().expect(""));
}

fn on_post(_url: &str, _arg: &str, ctype: &str, data: &mut [u8]) -> !
{
	varnish::set_cacheable(false, 1.0, 0.0, 0.0);
	varnish::backend_response(200, ctype, data);
}

fn main()
{
	varnish::set_backend_get(on_get);
	varnish::set_backend_post(on_post);
	varnish::wait_for_requests();
}
