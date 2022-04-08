#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/file.h>

#include "FileDescriptor.h"


FileDescriptor::FileDescriptor(const std::string& fn) {
    errno = 0;
    descr = open(fn.c_str(), O_RDONLY);
    if (descr < 0) {
        throw std::runtime_error(strerror(errno));
    }

    errno = 0;
    if (flock(descr, LOCK_EX)) {
        throw std::runtime_error(strerror(errno));
    }
}

FileDescriptor::~FileDescriptor() {
    flock(descr, LOCK_UN);
    close(descr);
}

int FileDescriptor::getDescriptor() const {
    return descr;
}

size_t FileDescriptor::getFileSize() const {
    struct stat statbuf = {};
    errno = 0;
    int ret_code = fstat(descr, &statbuf);

    if (ret_code != 0) {
        throw std::runtime_error(strerror(errno));
    }

    return statbuf.st_size;
}

void FileDescriptor::copyTo(const std::string& path) {

}

void FileDescriptor::copyDirectly(int destDescr) {  

}

void FileDescriptor::copyHardlink(int destDescr) {

}

void FileDescriptor::copySymlink(int destDescr) {

}
