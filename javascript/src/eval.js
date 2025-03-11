/**
	QuickJS in Varnish

	$ curl -D - -X POST -d "varnish.response(200, 'text/plain', 'Executing random JavaScript');" http://127.0.0.1:8080
	What is going on here?
**/

function on_get(path)
{
	varnish.response(200,
		"text/plain", "This example uses POST requests");
}

function on_post(path, data)
{
	eval(data)
}

function on_error(url, error)
{
	varnish.set("X-Error: " + error);
	varnish.response(500, "text/plain", "Oops!");
}
