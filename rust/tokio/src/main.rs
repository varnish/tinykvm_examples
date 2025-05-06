use tokio::io::AsyncReadExt;
use tokio::io::AsyncWriteExt;
use tokio::net::TcpStream;

#[allow(dead_code)]
mod varnish;

#[tokio::main(flavor = "current_thread")]
async fn main() -> Result<(), std::io::Error>  {
	loop {
		println!("Before: wait_for_requests_paused");
		let _request = varnish::wait_for_requests_paused();
		println!("After: wait_for_requests_paused");
		let mut buf = Vec::<u8>::new();
		{
			// python3 -m http.server
			println!("Before: TcpStream::connect(...).await?");
			let mut stream = TcpStream::connect("127.0.0.1:8000").await?;
			println!("After: TcpStream::connect(...).await?");
			println!("Before: stream.write_all.await?");
			stream.write_all(
				b"HEAD / HTTP/1.1\r\n\
				Host: 127.0.0.1:8080\r\n\
				Connection: close\r\n\
				\r\n"
			).await?;
			println!("After: stream.write_all.await?");
			println!("Before: stream.read_to_end.await?");
			stream.read_to_end(&mut buf).await?;
			println!("After: stream.read_to_end.await?");
		}
		println!("Before: backend_response");
		varnish::backend_response(200, "text/plain", &buf);
		println!("After: backend_response");
	}
}
