#include <filesystem>

namespace fs = std::filesystem;

static constexpr size_t READ_CHUNK_SIZE = 4 * 1024;
static constexpr size_t WRITE_CHUNK_SIZE = 4 * 1024;

void copySrcToDst(const fs::path& src, const fs::path& dst);