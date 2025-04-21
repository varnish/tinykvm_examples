import varnish

while True:
	req = varnish.wait_for_requests()
	#print(req)
	varnish.http_set('X-Compute-Request-ID: 123456')
	agent = varnish.http_get('User-Agent')
	varnish.http_set('X-Compute-User-Agent: ' + agent)
	varnish.backend_response_str(200, 'text/plain', 'Hello Python Compute World')
