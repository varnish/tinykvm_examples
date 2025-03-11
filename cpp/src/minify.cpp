#include "kvm_api.hpp"
#include <simdjson/minify.h>

static std::vector<char> buffer(16ULL << 20); // 16MB buffer

static void
on_get(const char *json, const char *)
{
    /* Minify the JSON at 6GB/s */
    size_t dst_len = dst.size();
    simdjson::minify(json, strlen(json), dst.data(), dst_len);

    /* Respond with the minified JSON */
    Backend::response(200, "application/json", {dst, dst_len});
}

static void
on_post(const char *, const char *,
	const char *content_type, const uint8_t* data, const size_t len)
{
    /* Minify the JSON at 6GB/s */
    size_t dst_len = dst.size();
    simdjson::minify((const char *)data, len, dst.data(), dst_len);

    /* Respond with the minified JSON */
    Backend::response(200, "application/json", {dst.data(), dst_len});
}

int main(int argc, char **argv)
{
    set_backend_get(on_get);
    set_backend_post(on_post);
    wait_for_requests();
}
