#include "kvm_api.hpp"

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

int main(int argc, char **argv)
{
	Print("Hello Compute World!\n");

	set_backend_get(on_get);
	wait_for_requests();
}
