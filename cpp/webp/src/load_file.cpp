#include <stdexcept>
#include <unistd.h>
std::vector<uint8_t> load_file(const std::string& filename)
{
    size_t size = 0;
    FILE* f = fopen(filename.c_str(), "rb");
    if (f == NULL) throw std::runtime_error("Could not open file: " + filename);

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> result(size);
    if (size != fread(result.data(), 1, size, f))
    {
        fclose(f);
        throw std::runtime_error("Error when reading from file: " + filename);
    }
    fclose(f);
    return result;
}

bool write_file(const std::string& filename, const std::vector<uint8_t>& binary)
{
	FILE *f = fopen(filename.c_str(), "wb");
	if (f == NULL)
		return false;

	const size_t n = fwrite(binary.data(), 1, binary.size(), f);
	fclose(f);
	return n == binary.size();
}
