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
    int retCode = symlink(actualpath, dst);
    if (retCode != 0) {
        throw std::runtime_error(strerror(errno));
    }
}

void copyHardlink(const char* src, const char* dst) {
    errno = 0;
    int retCode = linkat(0, src, 0, dst, 0);
    if (retCode != 0) {
        throw std::runtime_error(strerror(errno));
    }
}

void copyMain(const char* src, const char* dst) {
    if (fs::is_symlink(src)) {
        copySymlink(src, dst);
        std::cout << "Source is a symlink; created symlink." << std::endl;
    } else {
        try {
            copyHardlink(src, dst);
            std::cout << "Source is not a symlink, but is a hardlink; created hardlink." << std::endl;
        } catch (const std::runtime_error& e) {
            if (errno == EXDEV) {
                std::cout << "Source and destination are in different file systems; copying full file..." << std::endl;
                copyDirectly(src, dst);
            } else {
                throw;
            }
        }
    }
}

fs::path createNewDirectories(const fs::path& path) {
    std::vector<std::string> paths;
    boost::split(paths, path.string(), boost::is_any_of("/"));
    paths.pop_back();

    auto pathIter = paths.end();
    fs::path curPath = path.parent_path();

    while (pathIter != paths.begin() && !fs::exists(curPath)) {
        --pathIter;
        curPath /= "..";
        curPath = curPath.lexically_normal();
    }

    fs::path existingPath = curPath;
    fs::path nonExistingPathBegin;
    if (pathIter != paths.end()) {
        nonExistingPathBegin = *pathIter;
    }
    for (pathIter; pathIter != paths.end(); ++pathIter) {
        curPath /= *pathIter;
        try {
            fs::create_directory(curPath);
            std::cout << "Created directory: " << curPath << std::endl;
        } catch (const fs::filesystem_error& e) {
            fs::remove_all(existingPath / nonExistingPathBegin);
            throw;
        }
    }

    return existingPath / nonExistingPathBegin;
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
    fs::path backupPath;
    fs::path nonExPath;

    if (fs::exists(dst)) {
        if (fs::is_directory(dst)) {
            fullPath /= src.filename();
        } else {
            backupPath = fullPath.parent_path() / ("." + fullPath.filename().string() + ".bk");
            copyHardlink(fs::absolute(fullPath).c_str(), fs::absolute(backupPath).c_str());
        }
    } else {
        if (dst.filename().empty()) {
            fullPath /= src.filename();
        }
        nonExPath = createNewDirectories(fullPath);     
    }

    fs::remove(fullPath);

    try {
        copyMain(fs::absolute(src).c_str(), fs::absolute(fullPath).c_str());
        fs::remove(backupPath);
    } catch (const std::runtime_error& e) {
        if (!nonExPath.empty()) {
            fs::remove_all(nonExPath);
        } else {
            fs::remove(fullPath);
            if (!backupPath.empty()) {
                copyHardlink(fs::absolute(backupPath).c_str(), fs::absolute(fullPath).c_str());
                fs::remove(backupPath);
            }
        }
        throw;
    }
}