#include <cstdio>
#include <cstdlib>
#include <print>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::println(stderr, "Invalid usage. Check supported commands here...");
        exit(1);
    }

    std::string command = argv[1];
    if (command == "build") {
        std::println("bulding");
    } else if (command == "run") {
        std::println("running");
    }

    return 0;
}
