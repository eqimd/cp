#include <sys/types.h>
#include <string>
#include <sys/stat.h>

class FileStat {
public:
    explicit FileStat(const char* fn, int flags, mode_t mode = 0);

    ~FileStat();
    int getDescriptor() const;
    size_t getFileSize() const;
    unsigned int getMode() const;

private:
    int descr;
    struct stat statbuf = {};
};