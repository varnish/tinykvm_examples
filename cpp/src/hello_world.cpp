#include "kvm_api.hpp"
#include <unistd.h>

static void
on_get(const char *, const char *)
{
	/* POST to httpbin.org */
	const auto res =
		Curl::post("http://httpbin.org/post", "text/test", "Data here");

	/* Cache 200s for 10 seconds. */
	set_cacheable(res.status == 200, 10.0f, 0.0f, 0.0f);

	/* Respond with the httpbin.org content. */
	Backend::response(res.status, res.content_type, res.content,
		{ "X-Header: Hello World", "X-Hello: World" });
}

template <typename... Args>
static inline void Print(const char *fmt, Args&&... args)
{
	printf(fmt, std::forward<Args> (args)...);
	fflush(stdout);
}

static const char response[] =
	"HTTP/1.1 200 OK\r\n"
	"Server: Varnish Cache Edgerprise\r\n"
//	"Connection: Close\r\n"
	"Content-Type: text/plain\r\n"
	"Content-Length: 13\r\n"
	"\r\n"
	"Hello World!\n";

static int
on_connected(int fd, const char *remote, const char *argument)
{
	Print("* FD %d connected. Remote: %s Arg: %s\n",
		fd, remote, argument);

	write(fd, response, sizeof(response)-1);
	return 1;
}
static void
on_read(int fd, const uint8_t *data, size_t bytes)
{
	//Print("* FD %d data: %p, %zu bytes\n", fd, data, bytes);

	/* Assume request */
	write(fd, response, sizeof(response)-1);
}
/*static void
on_writable(int fd)
{
	Print("* FD %d writable (again)\n", fd);

	write(fd, "Last bit\n", 9);
	shutdown(fd, SHUT_RDWR);
}*/
static void
on_disconnect(int fd, const char *reason)
{
	Print("* FD %d disconnected: %s\n", fd, reason);
}

static void
on_socket_prepare(int thread)
{
	while (true) {
		std::array<kvm_socket_event, 8> events;
		int cnt = wait_for_socket_events_paused(events.data(), events.size());
		for (int i = 0; i < cnt; ++i) {
			auto& se = events[i];
			switch (se.event) {
			case SOCKET_CONNECT:
				//Print("Socket %d connected: %s\n", se.fd, se.remote);
				break;
			case SOCKET_READ:
				//Print("Socket %d read: %zu bytes\n", se.fd, se.data_len);
				break;
			case SOCKET_WRITABLE:
				//Print("Socket %d writable\n", se.fd);
				/* Write to the socket. */
				write(se.fd, response, sizeof(response)-1);
				break;
			case SOCKET_DISCONNECT:
				//Print("Socket %d disconnected: %s\n", se.fd, se.remote);
				break;
			}
		}
		/* Continue waiting for events. */
	}
}

int main(int argc, char **argv)
{
	Print("Hello Compute %s World!\n", argv[2]);

	set_backend_get(on_get);

	set_socket_prepare_for_pause(on_socket_prepare);
	set_socket_on_connect(on_connected);
	set_socket_on_read(on_read);
	//set_socket_on_writable(on_writable);
	set_socket_on_disconnect(on_disconnect);
	wait_for_requests();
}
