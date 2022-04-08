#include <iostream>
#include <unistd.h>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/file.h>

#include "FileDescriptor.h"


constexpr size_t READ_CHUNK_SIZE = 4 * 1024;


int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "You need to provide source and destination." << std::endl;
        return EXIT_FAILURE;
    }

    FileDescriptor fd(argv[1]);
    size_t fsize = fd.getFileSize();
    size_t current_size = 0;
    
    std::vector<char> file(fsize);

    lockf(fd.getDescriptor(), F_LOCK, fsize);

    while(current_size < fsize) {
        size_t want_to_read = std::min(READ_CHUNK_SIZE, fsize - current_size);
        errno = 0;
        ssize_t readed = read(fd.getDescriptor(), file.data() + current_size, want_to_read);
        sleep(5);
        if (readed == -1) {
            switch (errno) {
            case EAGAIN:
            case EINTR:
                continue;
            
            default:
                std::cerr << "Something wrong with file: " << strerror(errno) << std::endl;
                return EXIT_FAILURE;
            }
        } else if (readed == 0) {
            std::cerr << "File trunkated!\nCurrent size: " << current_size << "\nExpected size: " << fsize << std::endl;
            return EXIT_FAILURE;
        }

        current_size += readed;

        if (readed != want_to_read) {
            std::cout << "WARNING: Readed " << readed << std::endl;
        }
    }

    lockf(fd.getDescriptor(), F_UNLCK, fsize);

    // for (const char &c: file) {
    //     std::cout << c;
    // }
    std::cout << "Readed: " << current_size << std::endl;
}