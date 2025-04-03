#include "kvm_api.hpp"
#include <libdeflate.h>
#include <nlohmann/json.hpp>

static libdeflate_decompressor* decompressor;
static uint8_t* compression_buffer = nullptr;
static size_t compression_buffer_size = 0;
static constexpr size_t INITIAL_BUFFER_SIZE = 16UL << 20; // 16MB

static void on_post(const char* url, const char* arg,
	const char* ctype, const uint8_t* content, size_t content_len)
{
	bool action_compress = true;
	int compression_level = 6;

	/* Optional JSON document */
	const auto arglen = strlen(arg);
	if (arglen > 0)
	{
		const auto j = nlohmann::json::parse(arg, arg + arglen, nullptr, true, true);

		if (j.contains("action")) {
			const auto action = j["action"].get<std::string>();
			if (action == "compress") {
				action_compress = true;
			}
			else if (action == "decompress") {
				action_compress = false;
			}
			else {
				Backend::response(400, "text/plain", "Invalid action");
				return;
			}
		}
		if (j.contains("level")) {
			compression_level = j["level"];
			if (compression_level < 1 || compression_level > 12) {
				Backend::response(400, "text/plain", "Invalid compression level");
				return;
			}
		}
	}

	if (action_compress) // Compression
	{
		// Set the compression level
		libdeflate_compressor* compressor = libdeflate_alloc_compressor(compression_level);
		if (!compressor) {
			Backend::response(500, "text/plain", "Failed to allocate compressor");
			return;
		}

		// Determine the size of the output buffer
		const size_t out_size = libdeflate_gzip_compress_bound(compressor, content_len);
		if (out_size > compression_buffer_size) {
			uint8_t* buffer = (uint8_t*)realloc(compression_buffer, out_size);
			if (!buffer) {
				libdeflate_free_compressor(compressor);
				Backend::response(500, "text/plain", "Failed to allocate compression buffer");
				return;
			}
			compression_buffer = buffer;
			compression_buffer_size = out_size;
		}

		const size_t actual_out_size =
			libdeflate_gzip_compress(compressor, content, content_len, compression_buffer, compression_buffer_size);
		// Immediately free the compressor
		libdeflate_free_compressor(compressor);
		if (actual_out_size == 0) {
			Backend::response(500, "text/plain", "Compression failed");
			return;
		}

		// Retain the original content type, but add a content-encoding header
		// to indicate that the content is compressed.
		std::vector<std::string> headers {
			"Content-Encoding: gzip",
		};
		Backend::response(200, ctype, compression_buffer, actual_out_size, headers);
	}
	else // Decompression
	{
		// Create initial buffer for decompression
		if (compression_buffer_size == 0) {
			compression_buffer = (uint8_t*)malloc(INITIAL_BUFFER_SIZE);
			if (!compression_buffer) {
				libdeflate_free_decompressor(decompressor);
				Backend::response(500, "text/plain", "Failed to allocate decompression buffer");
				return;
			}
			compression_buffer_size = INITIAL_BUFFER_SIZE;
		}

		// Try to decompress the content with the current buffer size
		size_t actual_out_nbytes_ret = 0;
		const libdeflate_result result =
			libdeflate_gzip_decompress(decompressor, content, content_len,
				compression_buffer, compression_buffer_size, &actual_out_nbytes_ret);
		// Resize the buffer if necessary
		if (result == LIBDEFLATE_INSUFFICIENT_SPACE) {
			compression_buffer = (uint8_t*)realloc(compression_buffer, actual_out_nbytes_ret);
			if (!compression_buffer) {
				libdeflate_free_decompressor(decompressor);
				Backend::response(500, "text/plain", "Failed to allocate decompression buffer");
				return;
			}
			compression_buffer_size = actual_out_nbytes_ret;

			// Try to decompress again with the new buffer size
			const libdeflate_result result =
				libdeflate_gzip_decompress(decompressor, content, content_len,
					compression_buffer, compression_buffer_size, &actual_out_nbytes_ret);
			if (result != LIBDEFLATE_SUCCESS) {
				libdeflate_free_decompressor(decompressor);
				Backend::response(500, "text/plain", "Decompression failed");
				return;
			}
		} else if (result != LIBDEFLATE_SUCCESS) {
			libdeflate_free_decompressor(decompressor);
			Backend::response(500, "text/plain", "Decompression failed");
			return;
		}

		// Retain the original content type, but remove the content-encoding header
		Http::set(RESP, "Content-Encoding");

		// Return the decompressed data
		Backend::response(200, ctype, compression_buffer, actual_out_nbytes_ret);
	}

	Backend::response(500, "text/plain", "Deflate program failed");
}

int main()
{
	decompressor = libdeflate_alloc_decompressor();
	if (!decompressor) {
		fprintf(stderr, "Failed to allocate libdeflate decompressor\n");
		return 1;
	}

	set_backend_post(on_post);
	wait_for_requests();
}
