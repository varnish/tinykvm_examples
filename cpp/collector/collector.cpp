#include "../src/kvm_api.hpp"
#include <functional>
#include <string>
#include <vector>
extern void storage_collect(const std::vector<std::string>&);
extern void storage_report(std::function<void(const std::string&, unsigned)>);
static std::vector<std::string> urls;
static constexpr size_t MAX_LOCAL_URLS = 8;

static void on_get(const char *url, const char*)
{
	if (strcmp(url, ":report:") == 0)
	{
		std::string report = "";
		storage_report([&] (const std::string& url, unsigned count) {
			report += std::to_string(count) + ": " + url + "\n";
		});
		const char ct[] = "text/plain";
		backend_response(200, ct, sizeof(ct)-1, report.c_str(), report.size());
	}

	urls.emplace_back(url);
	if (urls.size() >= MAX_LOCAL_URLS)
	{
		storage_collect(urls);
		urls.clear();
	}

    const char ct[] = "text/plain";
    backend_response(200, ct, sizeof(ct)-1, "OK", 2);
}

int main()
{
	urls.reserve(MAX_LOCAL_URLS);

    set_backend_get(on_get);
    wait_for_requests();
}
