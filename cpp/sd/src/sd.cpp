#include "kvm_api.hpp"

#include <cstring>
#include <nlohmann/json.hpp>
#include <sys/random.h>
#include "stable-diffusion.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "stb_image_write.h"

static StableDiffusion* sd = nullptr;

static void stable_diffusion(const std::string& prompt,
	const std::string& negative_prompt)
{
	const float cfg_scale = 7.0f;
	const int w = 128;
	const int h = 128;
	const SampleMethod sample_method = EULAR_A;
	const int sample_steps = 12;
	int seed = 42;
	getrandom(&seed, sizeof(seed), 0);

	vlogf("Prompt: %s  Seed: %d\n", prompt.c_str(), seed);

	std::vector<uint8_t> img = sd->txt2img(
		prompt,
		negative_prompt,
		cfg_scale,
		w,
		h,
		sample_method,
		sample_steps,
		seed);

	int png_len = 0;
	const auto *png =
		stbi_write_png_to_mem(img.data(), 0, w, h, 3, &png_len);

	Backend::response(200, "image/png", std::string_view((const char*)png, png_len));
}

static void
on_get(const char *prompt, const char *neg_prompt)
{
	stable_diffusion(prompt, neg_prompt);
}

static void
on_post(const char *url, const char *arg, const char *ctype, const uint8_t *data, size_t len)
{
}

int main(int argc, char** argv)
{
	if (strcmp(argv[2], "request") == 0)
	{
		const int n_threads = 1;
		const bool vae_decode_only = true;

		sd = new StableDiffusion(n_threads, vae_decode_only, true);
		if (!sd->load_from_file("/tmp/model.f16")) {
			return 1;
		}

		const auto sysinfo = sd_get_system_info();
		vlogf("%s\n", sysinfo.c_str());
	}

	set_backend_get(on_get);
	set_backend_post(on_post);
	wait_for_requests();
}
