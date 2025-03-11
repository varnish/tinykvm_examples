#include "../../kvm_api.h"
#include <algorithm>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
static std::unordered_map<std::string, unsigned> collection;
static std::mutex collection_mtx;

void storage_collect(const std::vector<std::string>& invec)
{
	std::scoped_lock lock(collection_mtx);

	for (const auto& string : invec) {
		auto p = collection.try_emplace(string, 1);
		if (p.second == false) {
			// Increment
			p.first->second ++;
		}
	}
}

void storage_report(std::function<void(const std::string&, unsigned)> callback)
{
	decltype(collection) copy;
	{
		std::scoped_lock lock(collection_mtx);
		copy = collection;
	}

	using P = std::pair<std::string, unsigned>;
	std::vector<P> pairs;

	for (auto itr = copy.begin(); itr != copy.end(); ++itr)
		pairs.push_back(*itr);

	std::sort(pairs.begin(), pairs.end(), [=](P& a, P& b) {
		return a.second < b.second;
	});

	for (const auto& it : pairs)
		callback(it.first, it.second);
}

int main()
{
	STORAGE_ALLOW(storage_collect);
	STORAGE_ALLOW(storage_report);

    wait_for_requests();
}
