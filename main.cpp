#include <iostream>
#include "util.h"
#include <filesystem>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "You need to provide source and destination." << std::endl;
        return EXIT_FAILURE;
    }

    try {
        copySrcToDst(argv[1], argv[2]);
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }
}
