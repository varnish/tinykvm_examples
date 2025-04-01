#include "kvm_api.hpp"
#include <simdjson/minify.h>

static std::vector<char> buffer(16ULL << 20); // 16MB buffer

static void
on_get(const char *json, const char *)
{
    /* Minify the JSON at 6GB/s */
    size_t buffer_len = buffer.size();
    (void)simdjson::minify(json, strlen(json), buffer.data(), buffer_len);

    /* Respond with the minified JSON */
    Backend::response(200, "application/json", buffer.data(), buffer_len);
}

static void
on_post(const char *, const char *,
	const char *content_type, const uint8_t* data, const size_t len)
{
    /* Minify the JSON at 6GB/s */
    size_t buffer_len = buffer.size();
    (void)simdjson::minify((const char *)data, len, buffer.data(), buffer_len);

    /* Respond with the minified JSON */
    Backend::response(200, "application/json", buffer.data(), buffer_len);
}

int main(int argc, char **argv)
{
    set_backend_get(on_get);
    set_backend_post(on_post);
    wait_for_requests();
}
