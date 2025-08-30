#include "include/libplatform/libplatform.h"
#include "include/v8-context.h"
#include "include/v8-exception.h"
#include "include/v8-initialization.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-primitive.h"
#include "include/v8-script.h"
#include "include/v8-template.h"

#include "../../kvm_api.h"
#undef NDEBUG
#include <cassert>

static v8::Isolate* isolate;
static v8::Isolate::CreateParams create_params;
static v8::Local<v8::Script> script;

struct Response {
	uint16_t status;
	std::string content_type;
	std::string content;
};

static Response invoke(const char *url, const char *arg)
{
	v8::TryCatch try_catch(isolate);

	v8::Local<v8::Context> context(isolate->GetCurrentContext());

	// Run the script to get the result.
	v8::Local<v8::Value> result;
	if (!script->Run(context).ToLocal(&result))
	{
		v8::String::Utf8Value error(isolate, try_catch.Exception());
		return Response {
			503,
			"text/plain",
			*error,
		};
	}

	// Convert the content to an UTF8 string, for now.
	v8::String::Utf8Value utf8(isolate, result);
	return Response {
		200,
		"text/plain",
		*utf8,
	};
}

static void
on_get(const char *url, const char *arg)
{
	const auto resp = invoke(url, arg);

	backend_response(resp.status,
		resp.content_type.data(), resp.content_type.size(),
		resp.content.c_str(), resp.content.length());
}

static int local_test()
{
	{
		v8::Isolate::Scope isolate_scope(isolate);

		// Create a stack-allocated handle scope.
		v8::HandleScope handle_scope(isolate);

		// Create a new context.
		v8::Local<v8::Context> context = v8::Context::New(isolate);

		// Enter the context for compiling and running the hello world script.
		v8::Context::Scope context_scope(context);

		{
			// Create a string containing the JavaScript source code.
			v8::Local<v8::String> source =
				v8::String::NewFromUtf8Literal(isolate, "'Hello' + ', World!'");

			// Compile the source code.
			v8::Local<v8::Script> script =
				v8::Script::Compile(context, source).ToLocalChecked();

			// Run the script to get the result.
			v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

			// Convert the result to an UTF8 string and print it.
			v8::String::Utf8Value utf8(isolate, result);
			printf("%s\n", *utf8);
		}
	}

	// Dispose the isolate and tear down V8.
	isolate->Dispose();
	v8::V8::Dispose();
	v8::V8::DisposePlatform();
	delete create_params.array_buffer_allocator;
	return 0;
}

static const char* ToCString(const v8::String::Utf8Value& value)
{
	return *value ? *value : "<string conversion failed>";
}

static void v8sys_Print(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	bool first = true;
	for (int i = 0; i < info.Length(); i++) {
		v8::HandleScope handle_scope(info.GetIsolate());
		if (first) {
			first = false;
		} else {
			printf(" ");
		}
		v8::String::Utf8Value str(info.GetIsolate(), info[i]);
		const char* cstr = ToCString(str);
		printf("%s", cstr);
	}
	printf("\n");
	fflush(stdout);
}

static void v8sys_Version(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	info.GetReturnValue().Set(
		v8::String::NewFromUtf8(info.GetIsolate(), "v0.0.1")
			.ToLocalChecked());
}

static v8::Local<v8::Context> VarnishAPI_context(v8::Isolate* isolate)
{
	v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);

	global->Set(isolate, "print", v8::FunctionTemplate::New(isolate, v8sys_Print));
	global->Set(isolate, "version", v8::FunctionTemplate::New(isolate, v8sys_Version));

	return v8::Context::New(isolate, NULL, global);
}

int main(int argc, char* argv[])
{
	// Initialize V8.
	v8::V8::InitializeICUDefaultLocation(argv[0]);
	v8::V8::InitializeExternalStartupData(argv[0]);
	v8::V8::SetFlagsFromString("--predictable --single-threaded --single-threaded-gc");
	std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
	v8::V8::InitializePlatform(platform.get());
	v8::V8::Initialize();

	// Create a new Isolate and make it the current one.
	create_params.array_buffer_allocator =
		v8::ArrayBuffer::Allocator::NewDefaultAllocator();
	isolate = v8::Isolate::New(create_params);

	if (IS_LINUX_MAIN())
	{
		return local_test();
	}
	else
	{
		v8::Isolate::Scope isolate_scope(isolate);

		// Create a stack-allocated handle scope.
		v8::HandleScope handle_scope(isolate);

		// Create a new context.
		v8::Local<v8::Context> context = VarnishAPI_context(isolate);

		// Enter the context for compiling and running the hello world script.
		v8::Context::Scope context_scope(context);

		// The JavaScript code:
		const char *code = argv[1];
		printf("Code: %s\n", code);
		fflush(stdout);

		// Create a string containing the JavaScript source code.
		v8::MaybeLocal<v8::String> source =
			v8::String::NewFromUtf8(isolate, code, v8::NewStringType::kNormal, strlen(code));
		v8::Local<v8::String> local_source;
		if (!source.ToLocal(&local_source)) {
			vlogf("Failed to initialize script from main argument");
			return 1;
		}

		// Compile the source code.
		script = v8::Script::Compile(context, local_source).ToLocalChecked();

		// Warmup
		(void)invoke("/", "");

		if constexpr (true)
		{
			while (true) {
				// Wait for requests
				struct kvm_request req;
				wait_for_requests_paused(&req);

				// Invoke the script and produce a response
				const auto resp = invoke("", "");
				backend_response(resp.status,
					resp.content_type.data(), resp.content_type.size(),
					resp.content.c_str(), resp.content.length());
			}
		} else {
			set_backend_get(on_get);
			wait_for_requests();
		}
	}
}
