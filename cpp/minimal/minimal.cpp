#include "../../kvm_api.h"
extern void storage_add_counter(uint64_t);
extern uint64_t storage_counter();
static constexpr uint64_t COMMIT_COUNT = 16;
static uint64_t local_counter = 0;

static void on_get(const char *url, const char*)
{
	local_counter ++;
	if (local_counter >= COMMIT_COUNT) {
		storage_add_counter(local_counter);
		local_counter = 0;
	}
	const auto counter = local_counter + storage_counter();

	const char ct[] = "text/plain";
	char co[] = "Hell0 World, 000!";
	co[ 4] = '0' + (counter / 1000) % 10;
	co[13] = '0' + (counter / 100) % 10;
	co[14] = '0' + (counter / 10) % 10;
	co[15] = '0' + (counter / 1) % 10;
	backend_response(200, ct, sizeof(ct)-1, co, sizeof(co)-1);
}

static void on_post(const char *url, const char *arg, const char *ctype, const uint8_t *content, size_t content_len)
{
	set_cacheable(false, 0.01, 0.0, 0.0);
	const char ct[] = "text/plain";
	backend_response(200, ct, sizeof(ct)-1, content, content_len);
}

extern "C" void _start()
{
	set_backend_get(on_get);
	set_backend_post(on_post);
	wait_for_requests();
}

/**
 * $ ./wrk -t8 -c8 -d10s http://127.0.0.1:8080/min
Running 10s test @ http://127.0.0.1:8080/min
  8 threads and 8 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
	Latency    41.65us    6.45us 532.00us   71.81%
	Req/Sec    23.86k   801.69    27.34k    77.85%
  1918042 requests in 10.10s, 495.71MB read
Requests/sec: 189920.47
Transfer/sec:     49.08MB
 *
 * $ ./wrk -t8 -c8 -d10s http://127.0.0.1:8080/file
Running 10s test @ http://127.0.0.1:8080/file
  8 threads and 8 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
	Latency    36.49us    7.74us 547.00us   79.30%
	Req/Sec    27.24k     1.32k   33.00k    70.54%
  2189600 requests in 10.10s, 590.42MB read
Requests/sec: 216792.04
Transfer/sec:     58.46MB
**/
