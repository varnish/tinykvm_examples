#include "kvm_api.hpp"
#include <cstdio>
#include <libgbc/machine.hpp>
#include <spng.h>
#include "file_helpers.cpp"

EMBED_BINARY(index_html, "../index.html");
EMBED_BINARY(rom, "../rom.gbc");
static std::string_view romdata { rom, rom_size };
static std::string statefile = "";

using PngData = std::pair<void*, size_t>;
using PaletteArray = struct spng_plte;
struct PixelState {
	PaletteArray palette;
};
struct InputState {
	void contribute(uint8_t keys) {
		for (size_t i = 0; i < keystate.size(); i++) {
			uint8_t idx = (current + i) % keystate.size();
			keystate.at(idx) |= keys;
		}
		contribs += 1;
	}
	uint8_t get() const {
		uint8_t keys = keystate.at(current);
		// Prevent buggy behavior from impossible dpad states
		if (keys & gbc::DPAD_UP) keys &= ~gbc::DPAD_DOWN;
		if (keys & gbc::DPAD_RIGHT) keys &= ~gbc::DPAD_LEFT;
		return keys;
	}
	auto contributors() const {
		return contribs;
	}
	void next() {
		keystate.at(current) = 0;
		current = (current + 1) % keystate.size();
		contribs = 0;
	}

	std::array<uint8_t, 4> keystate;
	uint8_t current = 0;
	uint16_t contribs;
};
static gbc::Machine* machine = nullptr;
static PixelState storage_state;
static PngData png;
struct Prediction {
	gbc::Machine* machine = nullptr;
	PaletteArray palette;
	uint8_t forwarded_keys = 0;
	PngData png {nullptr, 0};
	uint64_t predictions = 0;
	uint64_t predicted = 0;
};
static Prediction predict;

static PngData
generate_png(const std::vector<uint8_t>& pixels, PaletteArray& palette)
{
    const int size_x = 160;
    const int size_y = 144;

	// Render to PNG
	spng_ctx* enc = spng_ctx_new(SPNG_CTX_ENCODER);
	spng_set_option(enc, SPNG_ENCODE_TO_BUFFER, 1);
	spng_set_crc_action(enc, SPNG_CRC_USE, SPNG_CRC_USE);

	spng_ihdr ihdr;
	spng_get_ihdr(enc, &ihdr);
	ihdr.width = size_x;
	ihdr.height = size_y;
	ihdr.color_type = SPNG_COLOR_TYPE_INDEXED;
	ihdr.bit_depth = 8;

	spng_set_ihdr(enc, &ihdr);
	palette.n_entries = 64;
	spng_set_plte(enc, &palette);

	int ret =
		spng_encode_image(enc,
			pixels.data(), pixels.size(),
			SPNG_FMT_PNG, SPNG_ENCODE_FINALIZE);
	assert(ret == 0);

	size_t png_size = 0;
    void  *png_buf = spng_get_png_buffer(enc, &png_size, &ret);

	spng_ctx_free(enc);

	return {png_buf, png_size};
}

struct FrameState {
	double ts;
	InputState inputs;
	uint16_t contribs = 0;
};
static FrameState current_state;

static double time_now() {
	timespec t;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
	return t.tv_sec + t.tv_nsec / 1e9;
}
static double time_diff(double start_time, double end_time) {
	return end_time - start_time;
}

static void predict_next_frame(void* p, size_t plen)
{
	(void)p;
	(void)plen;

	if (predict.machine == nullptr) {
		predict.machine = new gbc::Machine(romdata);
	}
	predict.predictions ++;

	// Serialize state from main GBC and restore it to the prediction GBC
	std::vector<uint8_t> v;
	machine->serialize_state(v);
	predict.machine->restore_state(v);

	predict.machine->gpu.on_palchange([](const uint8_t idx, const uint16_t color) {
		auto& entry = predict.palette.entries[idx];
		*(uint32_t *)&entry = gbc::GPU::color15_to_rgba32(color);
	});
	predict.machine->set_inputs(predict.forwarded_keys);
	predict.machine->simulate_one_frame();

	// Encode predicted PNG
	std::free(predict.png.first);
	predict.png = generate_png(predict.machine->gpu.pixels(), predict.palette);
}

static void get_frame(size_t n, struct virtbuffer *vb, size_t res)
{
	assert(machine);

	// 1. Contribute inputs to current frame
	auto &inputs = *(uint8_t *)vb[0].data;
	current_state.inputs.contribute(inputs);

	// 2. Generate frame (predict first)
	auto t1 = time_now();
	bool do_predict_next_frame = false;
	static constexpr double TICKRATE = 0.0165;
	static constexpr double SKIPRATE = 0.050;

	while (time_diff(current_state.ts, t1) > TICKRATE)
	{
		auto keys = current_state.inputs.get();
		//vlogf("Frame %lu: %02x\n", machine->gpu.frame_count(), keys);

		if (false && predict.forwarded_keys == keys && predict.machine) {
			std::swap(machine, predict.machine);
			storage_state.palette = predict.palette;
			std::swap(png, predict.png);
			predict.predicted ++;
		} else {
			machine->set_inputs(keys);
			machine->gpu.on_palchange([](const uint8_t idx, const uint16_t color) {
				// The compiler will probably optimize this just fine if
				// we do it the proper hard way, anyway.
				auto& entry = storage_state.palette.entries[idx];
		        *(uint32_t *)&entry = gbc::GPU::color15_to_rgba32(color);
		    });
			machine->simulate_one_frame();

			// Encode new PNG
			png = generate_png(machine->gpu.pixels(), storage_state.palette);
			std::free(png.first);
		}

		do_predict_next_frame = false;

		if (time_diff(current_state.ts, t1) > SKIPRATE)
			current_state.ts = t1;
		else
			current_state.ts += TICKRATE;
	}

	if (do_predict_next_frame)
	{
		// Ensure we only make forward progress on inputs
		// once per update, regardless of time passed.
		current_state.contribs = current_state.inputs.contributors();

		// Predict next frame (after leaving)
		// This is done by queueing up the given function
		// on the queue of the storage VM, which happens
		// after we leave.
		predict.forwarded_keys = current_state.inputs.get();
		predict.palette = storage_state.palette;
		storage_task(predict_next_frame, &predict, sizeof(predict));
	}

	// Store state every X frames
	if (machine->gpu.frame_count() % 64 == 0)
	{
		auto state = create_serialized_state();
		file_writer(statefile, state);
	}

	current_state.inputs.next();
	storage_return(png.first, png.second);
}

static void
on_get(const char* url, const char *)
{
	if (strcmp(url, "/x") == 0) {
		set_cacheable(true, 3600.0, 3600.0, 0.0);
		const std::string& ctype = "text/html";
		Backend::response(200, ctype, index_html, index_html_size);
	}

	uint8_t inputs = 0;
	if (strchr(url, 'a')) inputs |= gbc::BUTTON_A;
	if (strchr(url, 'b')) inputs |= gbc::BUTTON_B;
	if (strchr(url, 'e')) inputs |= gbc::BUTTON_START;
	if (strchr(url, 's')) inputs |= gbc::BUTTON_SELECT;
	if (strchr(url, 'u')) inputs |= gbc::DPAD_UP;
	if (strchr(url, 'd')) inputs |= gbc::DPAD_DOWN;
	if (strchr(url, 'r')) inputs |= gbc::DPAD_RIGHT;
	if (strchr(url, 'l')) inputs |= gbc::DPAD_LEFT;

	// Disable client-side caching
	http_appendf(RESP, "cache-control: no-store");

	// Uncacheable or a short while
	set_cacheable(false, 0.01f, 0.0f, 0.0f);

	// Input: Input state from this request
	// Output: A PNG frame
	char output[16384];
	ssize_t output_len =
		storage_call(get_frame, &inputs, sizeof(inputs), output, sizeof(output));

	/*
	Http::append(RESP,
		"X-Predictions: " + std::to_string(predict.predictions));
	Http::append(RESP,
		"X-Predicted: " + std::to_string(predict.predicted));
	Http::append(RESP,
		"X-FrameCount: " + std::to_string(machine->gpu.frame_count()));
	Http::append(RESP,
		"X-Contributors: " + std::to_string(current_state.contribs));
	*/

	const std::string& ctype = "image/png";
	Backend::response(200, ctype, output, output_len);
}

static std::vector<uint8_t> create_serialized_state()
{
	std::vector<uint8_t> state;
	machine->serialize_state(state);
	state.insert(state.end(), (uint8_t*) &storage_state, (uint8_t*) &storage_state + sizeof(storage_state));
	state.insert(state.end(), (uint8_t*) &current_state, (uint8_t*) &current_state + sizeof(FrameState));
	return state;
}
static void restore_state_from(const std::vector<uint8_t> state)
{
	size_t off = machine->restore_state(state);
	if (state.size() >= off + sizeof(PixelState)) {
		storage_state = *(PixelState*) &state.at(off);
		off += sizeof(PixelState);
	}
	if (state.size() >= off + sizeof(FrameState)) {
		current_state = *(FrameState*) &state.at(off);
	}
	printf("State restored!\n");
	fflush(stdout);
}
static void do_serialize_state() {
	auto state = create_serialized_state();
	storage_return(state.data(), state.size());
}
static void do_restore_state(size_t len) {
	printf("State: %zu bytes\n", len);
	fflush(stdout);
	if (len == 0) return;

	// Restoration happens in two stages.
	// 1st stage: No data, but the length is provided.
	std::vector<uint8_t> state;
	state.resize(len);
	storage_return(state.data(), state.size());
	// 2nd stage: Do the actual restoration:
	restore_state_from(state);
}

int main(int argc, char** argv)
{
	(void)argc;

	if (IS_STORAGE())
	{
		/* GameBoy Color machine */
		machine = new gbc::Machine(romdata);

		current_state.ts = time_now();

		statefile = argv[1];
		try {
			auto state = file_loader(statefile);
			if (!state.empty()) {
				restore_state_from(state);
			}
		} catch (...) {
			fflush(stdout);
		}

		printf("GBC: Storage done\n");
		fflush(stdout);
	}

	STORAGE_ALLOW(get_frame);

	set_backend_get(on_get);
	set_on_live_update(do_serialize_state);
	set_on_live_restore(do_restore_state);
	wait_for_requests();
}
