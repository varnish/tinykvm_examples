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

static void
on_socket_prepare(int thread)
{
	std::vector<kvm_socket_event> write_events;
	while (true) {
		std::array<kvm_socket_event, 16> events;
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
				write_events.push_back({
					.fd = se.fd,
					.event = SOCKET_WRITABLE,
					.remote = nullptr,
					.arg = nullptr,
					.data = (const uint8_t *)response,
					.data_len = sizeof(response) - 1
				});
				break;
			case SOCKET_DISCONNECT:
				//Print("Socket %d disconnected: %s\n", se.fd, se.remote);
				break;
			}
		}
		if (!write_events.empty()) {
			/* Write to the socket. */
			sys_sockets_write(write_events.data(), write_events.size());
			write_events.clear();
		}
		/* Continue waiting for events. */
	}
}

int main(int argc, char **argv)
{
	Print("Hello Compute %s World!\n", getenv("KVM_TYPE"));

	set_backend_get(on_get);

	set_socket_prepare_for_pause(on_socket_prepare);
	wait_for_requests();
}
