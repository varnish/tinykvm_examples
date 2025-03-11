#include "kvm_api.hpp"

#include <cstring>
#include <nlohmann/json.hpp>
#include <tinyxml2.h>

template <typename T>
static void transform(T& j,
	const char *xml, size_t len)
{
	tinyxml2::XMLDocument doc;
	doc.Parse((const char*)xml, len);

	if (doc.Error())
	{
		vlogf("Error: %s\n", doc.ErrorStr());
		Http::set(RESP, "X-Error: " + std::to_string(doc.ErrorLineNum()));
		Backend::response(503, "text/plain", doc.ErrorStr());
	}

	Backend::response(200, "text/xml", std::string_view(xml, len));
}

static void
on_get(const char *xml, const char* arg)
{
	// Parse JSON with comments enabled
	const auto j = nlohmann::json::parse(arg, arg + strlen(arg), nullptr, true, true);

	transform(j, xml, strlen(xml));
}

static void
on_post(const char *url, const char *arg, const char *ctype, const uint8_t *data, size_t len)
{
	// Parse JSON with comments enabled
	const auto j = nlohmann::json::parse(arg, arg + strlen(arg), nullptr, true, true);

	transform(j, (const char *)data, len);
}

int main()
{
	set_backend_get(on_get);
	set_backend_post(on_post);
	wait_for_requests();
}
