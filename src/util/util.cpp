#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/file.h>
#include <filesystem>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include "FileStat.h"
#include "util.h"


void copyDirectly(const char* src, const char* dst) {
    FileStat fdSrc(src, O_RDONLY);
    FileStat fdDst(dst, O_WRONLY | O_CREAT | O_TRUNC);
    int srcDescr = fdSrc.getDescriptor();
    int dstDescr = fdDst.getDescriptor();

    size_t srcSize = fdSrc.getFileSize();
    size_t current_size = 0;

    std::vector<char> fileBuf(RW_CHUNK_SIZE);

    std::string progress;
    while(current_size < srcSize) {
        std::cout << std::string(' ', progress.length()) << "\r";
        size_t want_to_rw = std::min(RW_CHUNK_SIZE, srcSize - current_size);

        errno = 0;
        ssize_t readed = read(srcDescr, fileBuf.data(), want_to_rw);
        if (readed == -1) {
            switch (errno) {
                case EAGAIN:
                case EINTR:
                    continue;
            
                default:
                    throw std::runtime_error("Something wrong with source file: " + std::string(strerror(errno)));
            }
        } else if (readed == 0) {
            throw std::runtime_error(
                "Source file trunkated!\nCurrent size: " + std::to_string(current_size) + 
                "\nExpected size: " + std::to_string(srcSize)
            );
        }

        errno = 0;
        ssize_t writed = write(dstDescr, fileBuf.data(), want_to_rw);

        if (writed == -1) {
            switch (errno) {
                case EAGAIN:
                case EINTR:
                    continue;

                default:
                    throw std::runtime_error("Something wrong with destination file: " + std::string(strerror(errno)));
            }
        } else if (writed == 0) {
            throw std::runtime_error(
                "Destination file trunkated!\nCurrent size: " + std::to_string(current_size) +
                "\nExpected size: " + std::to_string(srcSize)
            );
        }

        current_size += readed;

        if (readed != want_to_rw) {
            std::cout << "WARNING: Readed " << readed << std::endl;
        }
        if (writed != want_to_rw) {
            std::cout << "WARNING: Writed " << writed << std::endl;
        }

        progress = "Current progres: " + std::to_string(current_size) + " / " + std::to_string(srcSize);
        std::cout << progress;
    }
    std::cout << "\nDone copying. " << "Readed: " << current_size << std::endl;
}

void copySymlink(const char* src, const char* dst) {
    char* actualpath = realpath(src, NULL);
    errno = 0;
    int ret_code = symlink(actualpath, dst);
    if (ret_code != 0) {
        throw std::runtime_error(strerror(errno));
    }
}

void copyMain(const char* src, const char* dst) {
    if (fs::is_symlink(src)) {
        copySymlink(src, dst);
        std::cout << "Source is a symlink; created symlink." << std::endl;
    } else {
        errno = 0;
        int ret_code = linkat(0, src, 0, dst, 0);
        if (ret_code != 0) {
            switch (errno) {
                case EXDEV:
                    std::cout << "Source and destination are in different file systems; copying full file..." << std::endl;
                    copyDirectly(src, dst);
            }
        } else {
            std::cout << "Source is not a symlink, but is a hardlink; created hardlink." << std::endl;
        }
    }
}

void copySrcToDst(const fs::path& src, const fs::path& dst) {
    if (!fs::exists(src)) {
        throw std::runtime_error("Source does not exist!");
    }
    if (fs::is_directory(src)) {
        throw std::runtime_error("Source is a directory!");
    }
    if (dst.empty()) {
        throw std::runtime_error("Destination path is empty!");
    }

    fs::path fullPath = dst;
    fs::path tempPath;
    fs::path existingPath;
    fs::path nonExistingPathBegin;

    if (fs::exists(dst)) {
        if (fs::is_directory(dst)) {
            fullPath /= src.filename();
            tempPath = fullPath;
        } else {
            tempPath = fullPath.string() + ".tmp";
        }
    } else {
        std::vector<std::string> paths;
        boost::split(paths, dst.string(), boost::is_any_of("/"));

        if (dst.filename().empty()) {
            fullPath /= src.filename();
        } else {
            paths.pop_back();
        }

        for (auto& p : paths) {
            try {
                if (!fs::create_directory(p)) {
                    existingPath /= p;

                } else {
                    if (nonExistingPathBegin.empty()) {
                        nonExistingPathBegin = p;
                    }
                    std::cout << "Created directory: " << existingPath / p << std::endl;
                }
            } catch (const fs::filesystem_error& e) {
                if (!nonExistingPathBegin.empty()) {
                    fs::remove_all(existingPath / nonExistingPathBegin);
                }
                throw;
            }
        }
        tempPath = fullPath;
    }

    try {
        copyMain(fs::absolute(src).string().c_str(), fs::absolute(tempPath).string().c_str());
    } catch (const std::runtime_error& e) {
        if (!nonExistingPathBegin.empty()) {
            fs::remove_all(existingPath / nonExistingPathBegin);
        } else {
            if (tempPath.filename() != fullPath.filename()) {
                fs::remove(tempPath);
            }
        }
        throw;
    }

}