#define KVM_API_ALREADY_DEFINED
#include "kvm_api.hpp"
#include <string>

extern const uint8_t *original_img;
extern size_t         original_img_size;
extern std::string    original_content_type;

/* For regular errors without the VM itself crashing, we can use this
   fallback function instead of the on_error callback. */
inline void bail(const std::string& reason) {
	set_cacheable(false, 10.0f, 0.0, 0.0);
	Http::append(5, "X-Failed: " + reason);
	Backend::response(200, original_content_type, original_img, original_img_size);
}
