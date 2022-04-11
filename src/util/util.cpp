#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/file.h>
#include <filesystem>
#include <vector>
#include "FileStat.h"
#include "util.h"
#include <csignal>

namespace {
    volatile std::sig_atomic_t signalTerminated = 0;
}

void interruptHandler(int sig) {
    signalTerminated = 1;
}

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
        if (inCopyProcess) {
            std::cout << "Something went wrong while copying; "
                      << "restoring from backup..."
                      << std::endl;
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
                        std::cerr << "Could not restore from backup; "
                                    << "check backup at "
                                    << _backupPath
                                    << std::endl
                                    << strerror(errno)
                                    << std::endl;
                    } else {
                        std::cout << "Restored." << std::endl;
                        fs::remove(_backupPath);
                    }
                }
            }
        }
    }

    void makeCopy() {
        std::signal(SIGINT, interruptHandler);
        std::signal(SIGABRT, interruptHandler);
        
        prepareFullPath();
        if (_fullPath.lexically_normal() == fs::absolute(_src).lexically_normal()) {
            std::cerr << "Can not copy equivalent name in same folder." << std::endl;
            return;
        }

        createNewDirectories();
        if (signalTerminated == 1) {
            throw std::runtime_error("Terminated!");
        }

        createBackup(
            _fullPath.parent_path() / ("." + _fullPath.filename().string() + ".bk")
        );
        if (signalTerminated == 1) {
            throw std::runtime_error("Terminated!");
        }

        fs::remove(_fullPath);

        inCopyProcess = true;
        copyMain(fs::absolute(_src).c_str(), fs::absolute(_fullPath).c_str());
        if (signalTerminated == 1) {
            throw std::runtime_error("Terminated!");
        }
        
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
        int mode = fdSrc.getMode();
        FileStat fdDst(dst, O_WRONLY | O_CREAT | O_TRUNC, mode);
        int srcDescr = fdSrc.getDescriptor();
        int dstDescr = fdDst.getDescriptor();

        size_t srcSize = fdSrc.getFileSize();
        size_t current_size = 0;

        std::vector<char> fileBuf(READ_CHUNK_SIZE);

        while(current_size < srcSize) {
            size_t want_to_read = std::min(READ_CHUNK_SIZE, srcSize - current_size);

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

            size_t wroteSize = 0;
            while (wroteSize < readed) {
                size_t want_to_write = std::min(READ_CHUNK_SIZE, WRITE_CHUNK_SIZE);
                want_to_write = std::min(want_to_write, readed - wroteSize);
                errno = 0;
                ssize_t writed = write(dstDescr, fileBuf.data(), want_to_write);
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

                wroteSize += writed;
                if (writed != want_to_write) {
                    std::cout << "WARNING: Writed " << writed << std::endl;
                }
            }

            current_size += readed;

            if (readed != want_to_read) {
                std::cout << "WARNING: Readed " << readed << std::endl;
            }

            std::cout << "Current progres: " + std::to_string(current_size) + " / " + std::to_string(srcSize) << "\r";
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
                return;
            } else {
                throw std::runtime_error(strerror(errno));
            }
        }
        std::cout << "Source is not a symlink, but is a hardlink; "
                  << "created hardlink."
                  << std::endl;
    }

    void createNewDirectories() {
        fs::path curPath;

        for (auto p : _fullPath.parent_path()) {
            curPath /= p;
            if (!fs::exists(curPath)) {
                if (_nonExPath.empty()) {
                    _nonExPath = curPath;
                }
                try {
                    fs::create_directory(curPath);
                    std::cout << "Created directory: " << curPath << std::endl;
                } catch (...) {
                    std::cerr << "Error while creating directory " << curPath << std::endl;
                    throw;
                }
            }
        }
    }

    void createBackup(const fs::path& backupPath) {
        if (!fs::exists(_fullPath)) {
            return;
        }

        _backupPath = backupPath;
        errno = 0;
        int retCode = linkat(
            0, fs::absolute(_fullPath).c_str(),
            0, fs::absolute(_backupPath).c_str(),
            0
        );
        if (retCode != 0) {
            std::cerr << "Could not create backup." << std::endl;
            fs::remove(_backupPath);
            throw std::runtime_error(strerror(errno));
        }
    }

    void prepareFullPath() {
        _fullPath = fs::absolute(_dst);

        if (
            (fs::exists(_fullPath) && fs::is_directory(_fullPath)) ||
            _fullPath.filename().empty()
        ) {
                _fullPath /= _src.filename();
        }
    }
};

void copySrcToDst(const fs::path& src, const fs::path& dst) {
    CopyContext copyCont(src, dst);
    copyCont.makeCopy();
}