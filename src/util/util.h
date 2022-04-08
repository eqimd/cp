#include <filesystem>

namespace fs = std::filesystem;

static constexpr size_t RW_CHUNK_SIZE = 4 * 1024;

void copyDirectly(const char* src, const char* dst);
void copySymlink(const char* src, const char* dst);
void copyMain(const char* src, const char* dst);
void copySrcToDst(const fs::path& src, const fs::path& dst);