/**
	QuickJS in Varnish

	Implement these functions to handle GET and POST:
	- function on_get(path) {}
	- function on_post(path, data) {}
	Implement this function in order to provide fallback responses:
	- function on_error(url, error) {}

	Send a HTTP response back to client:
	- varnish.response(code, content_type, content)

	Set a response as cached for 10 seconds (TTL, grace, keep):
	- varnish.set_cacheable(true, 10.0, 0.0, 0.0)

	Send a static resource back to client:
	- varnish.sendfile(path)

	Call a function in mutable storage with optional data arguments:
	- result = varnish.storage("storage_function")
	- result = varnish.storage("storage_function", data, ...)

	Storage is not ephemeral and will remember all changes as
	if we were running code in a normal program.

	Result value from the call to storage function is returned:
	- result = varnish.storage("storage_function")
	- function storage_function() {
	- 	return "data";
	- }
	*result* becomes "data".
	- result = varnish.storage("storage_function", "1", "2", "3", "4")
	- function storage_function(a1, a2, a3, a4) {
	- 	return a1 + a2 + a3 + a4;
	- }
	*result* becomes "1234".

	Fetch content of URL and return as string, or throw:
	- var resp = varnish.fetch(url)
	POST content:
	- var resp = varnish.post(url, "text/plain", "Hello World!");
	'resp' is an object that has these members: 
	  - resp.status == 200
	  - resp.type   == "text/plain"
	  - resp.content == "Hello World!"

	Logging and errors will show up in VSL.
	- varnish.log(text)

	scriptArgs[0] = "mydomain.com"
	scriptArgs[1] = "state.file"
	scriptArgs[2] = "request" or "storage"
**/

const state_file = scriptArgs[1]
varnish.log("Hello QuickJS World");

/* At the start we attempt to load previous text from disk */
var text = std.loadFile(state_file);
if (!text) text = "";

function on_get(path, arg)
{
	varnish.log("path: " + path);

	if (path == "" || path == "/") {
		varnish.sendfile("/index.html");
	} else if (path == "/example") {
		/* POST to httpbin.org that returns a JSON result. */
		var result = varnish.post("http://httpbin.org/post",
			"text/plain", path, ["X-Request: True", "X-Post: True"]);
		varnish.set("X-Hello: World");
		/* Intentional JS exception. */
		try {
			varnish.set(varnish.get("X-Other"));
		} catch (error) {
			/* Error: HTTP header field could not be found. */
			varnish.set("X-Error: " + error);
		}
		varnish.set_cacheable(false, 10.0, 0.0, 0.0);
		/* Add all the response headers to our response. */
		var c = "";
		result.headers.forEach (function(val){
			c = c + val + "\n";
		});
		/* Add the content at the end of the response. */
		c = c + result.content;
		varnish.response(200, "text/plain", c);
	} else if (path == "/get") {
		/* The KVM VMOD has mutable state in a storage VM that
		   every tenant has access to. Using that we can remember
		   things from request to request. */
		var result = varnish.storage("get_storage");
		varnish.set_cacheable(false, 10.0, 0.0, 0.0);
		varnish.response(200,
			"text/plain", result);
	}
	/* The QuickJS example has a built-in static site, using
	   the www folder. We can send files from the folder using
	   varnish.sendfile(path). */
	varnish.sendfile(path);
}

function on_post(path, arg, ctype, data)
{
	/* For demo purposes */
	if (path == "/example") {
		var resp = varnish.fetch("https://example.com");
		varnish.response(resp.status,
			resp.type,
			resp.content);
	}

	/* Make a call into storage @set_storage with data as argument */
	var result = varnish.storage("set_storage", data);

	/* The result is the updated text */
	varnish.response(201,
		"text/plain", result);
}

function get_storage()
{
	return text;
}
function set_storage(data)
{
	/* Modify text in storage */
	var json = JSON.parse(data);
	text += ">> " + json["text"] + "\n";

	/* "Persistence" using the state file from scriptArgs */
	var file = std.open(state_file, "wb");
	file.puts(text);
	file.close();

	/* Return the contents of current chat */
	return text;
}

/* Specially named functions that lets you
   keep the program state during updates. */
function on_live_update()
{
	return text;
}
function on_resume_update(new_text)
{
	text = new_text;
}

function on_error(url, arg, error)
{
	varnish.set("X-Error: " + error);
	varnish.response(500, "text/plain", "Oops!");
}
