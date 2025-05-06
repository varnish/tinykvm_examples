use tokio::io::AsyncReadExt;
use tokio::io::AsyncWriteExt;
use tokio::net::TcpStream;

#[allow(dead_code)]
mod varnish;

#[tokio::main(flavor = "current_thread")]
async fn main() -> Result<(), std::io::Error>  {
	loop {
		let _request = varnish::wait_for_requests_paused();
		// python3 -m http.server
		let mut stream = TcpStream::connect("127.0.0.1:8000").await?;
		stream.write_all(
			b"HEAD / HTTP/1.1\r\n\
			Host: 127.0.0.1:8080\r\n\
			Connection: close\r\n\
			\r\n"
		).await?;
		let mut buf = Vec::<u8>::new();
		stream.read_to_end(&mut buf).await?;
		varnish::backend_response(200, "text/plain", &buf);
	}
}
