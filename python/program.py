import varnish
import uuid
import gzip

while True:
	req = varnish.wait_for_requests()
	#print(req)
	req_uuid = str(uuid.uuid4())
	varnish.http_set('X-Compute-Request-ID: ' + req_uuid)
	agent = varnish.http_get('User-Agent')
	if agent:
		agent = agent.partition(': ')[-1]
		varnish.http_set('X-Compute-User-Agent: ' + agent)

	data = b'Hello Python Compute World'
	compressed_data = gzip.compress(data)
	varnish.http_set("Content-Encoding: gzip")

	varnish.backend_response(200, 'text/plain', compressed_data)
