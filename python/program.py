import varnish
import uuid

while True:
	req = varnish.wait_for_requests()
	#print(req)
	req_uuid = str(uuid.uuid4())
	varnish.http_set('X-Compute-Request-ID: ' + req_uuid)
	agent = varnish.http_get('User-Agent')
	if agent:
		agent = agent.partition(': ')[-1]
		varnish.http_set('X-Compute-User-Agent: ' + agent)
	varnish.backend_response_str(200, 'text/plain', 'Hello Python Compute World')
