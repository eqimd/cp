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
#include <signal.h>


class CopyContext {
public:
    CopyContext(const fs::path& src, const fs::path& dst) {
        _src = src;
        _dst = dst;

        if (!fs::exists(_src)) {
            throw std::runtime_error("Source does not exist!");
        }
        if (fs::is_directory(_src)) {
            throw std::runtime_error("Source is a directory!");
        }
        if (_dst.empty()) {
            throw std::runtime_error("Destination path is empty!");
        }
    }

    ~CopyContext() {
        try {
            if (inCopyProcess) {
                if (!_nonExPath.empty()) {
                    fs::remove_all(_nonExPath);
                } else {
                    fs::remove(_fullPath);
                    if (!_backupPath.empty()) {
                        errno = 0;
                        int retCode = linkat(
                            0, fs::absolute(_backupPath).c_str(),
                            0, fs::absolute(_fullPath).c_str(),
                            0
                        );
                        if (retCode != 0) {
                            std::cerr << "Could not restore from backup;"
                                      << "check backup at "
                                      << _backupPath
                                      << std::endl
                                      << strerror(errno)
                                      << std::endl;
                        } else {
                            fs::remove(_backupPath);
                        }
                    }
                }
            }
        } catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    void makeCopy() {
        preparePathsAndBackup();
        fs::remove(_fullPath);

        inCopyProcess = true;
        copyMain(fs::absolute(_src).c_str(), fs::absolute(_fullPath).c_str());
        inCopyProcess = false;

        fs::remove(_backupPath);
    }

private:
    fs::path _src;
    fs::path _dst;
    
    fs::path _fullPath;
    fs::path _backupPath;
    fs::path _nonExPath;

    bool inCopyProcess = false;

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
            size_t want_to_read = std::min(RW_CHUNK_SIZE, srcSize - current_size);

            errno = 0;
            ssize_t readed = read(srcDescr, fileBuf.data(), want_to_read);
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
            ssize_t writed = write(dstDescr, fileBuf.data(), readed);

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

            if (readed != want_to_read) {
                std::cout << "WARNING: Readed " << readed << std::endl;
            }
            if (writed != want_to_read) {
                std::cout << "WARNING: Writed " << writed << std::endl;
            }

            progress = "Current progres: " + std::to_string(current_size) + " / " + std::to_string(srcSize);
            std::cout << progress;
        }
        std::cout << "\nDone copying. " << "Readed: " << current_size << std::endl;
    }

    void copyMain(const char* src, const char* dst) {
        if (fs::is_symlink(src)) {
            errno = 0;
            char* actualpath = realpath(src, NULL);
            if (actualpath == nullptr) {
                throw std::runtime_error(strerror(errno));
            }
            errno = 0;
            int retCode = symlink(actualpath, dst);
            if (retCode != 0) {
                throw std::runtime_error(strerror(errno));
            }
            std::cout << "Source is a symlink; created symlink." << std::endl;
            return;
        }

        errno = 0;
        int retCode = linkat(0, src, 0, dst, 0);
        if (retCode != 0) {
            if (errno == EXDEV) {
                std::cout << "Source and destination are in different file systems; "
                          << "copying full file..."
                          << std::endl;
                copyDirectly(src, dst);
            } else {
                throw std::runtime_error(strerror(errno));
            }
        }
        std::cout << "Source is not a symlink, but is a hardlink; "
                  << "created hardlink."
                  << std::endl;
    }

    void createNewDirectories() {
        std::vector<std::string> paths;
        boost::split(paths, _fullPath.string(), boost::is_any_of("/"));
        paths.pop_back();

        auto pathIter = paths.end();
        fs::path curPath = _fullPath.parent_path();

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

        _nonExPath = existingPath / nonExistingPathBegin;
    }

    void preparePathsAndBackup() {
        _fullPath = _dst;

        if (fs::exists(_dst)) {
            if (fs::is_directory(_dst)) {
                _fullPath /= _src.filename();
            } else {
                _backupPath = _fullPath.parent_path() / ("." + _fullPath.filename().string() + ".bk");
                errno = 0;
                int retCode = linkat(
                    0, fs::absolute(_fullPath).c_str(),
                    0, fs::absolute(_backupPath).c_str(),
                    0
                );
                if (retCode != 0) {
                    std::cerr << "Could not create backup." << std::endl;
                    throw std::runtime_error(strerror(errno));
                }
            }
        } else {
            if (_dst.filename().empty()) {
                _fullPath /= _src.filename();
            }
            createNewDirectories();
        }
    }
};

void copySrcToDst(const fs::path& src, const fs::path& dst) {
    CopyContext copyCont(src, dst);
    copyCont.makeCopy();
}