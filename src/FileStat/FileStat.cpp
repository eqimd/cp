#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/file.h>
#include <filesystem>
#include "FileStat.h"


FileStat::FileStat(const char* fn, int flags) {
    errno = 0;
    descr = open(fn, flags);
    if (descr < 0) {
        throw std::runtime_error(strerror(errno));
    }

    errno = 0;
    if (flock(descr, LOCK_EX)) {
        throw std::runtime_error(strerror(errno));
    }

    errno = 0;
    int ret_code = fstat(descr, &statbuf);

    if (ret_code != 0) {
        throw std::runtime_error(strerror(errno));
    }
}

FileStat::~FileStat() {
    flock(descr, LOCK_UN);
    close(descr);
}

int FileStat::getDescriptor() const {
    return descr;
}

unsigned int FileStat::getMode() const {
    return statbuf.st_mode;
}

size_t FileStat::getFileSize() const {
    return statbuf.st_size;
}
