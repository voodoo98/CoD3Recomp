#include "log.h"

#include <Windows.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

// Launch without inherited standard handles, like the window-only game.
// The caller enforces a timeout: the old Finish deadlocks on an idle pipe.
int main(int argc, char** argv)
{
    if (argc != 2) return 1;
    _putenv_s("COD3_LOG", argv[1]);
    Log::Start();
    printf("stdout marker\n");
    fprintf(stderr, "stderr marker\n");
    fflush(stdout);
    fflush(stderr);
    Sleep(100); // The copier is now waiting for the next line in ReadFile.
    const auto start = std::chrono::steady_clock::now();
    Log::Finish();
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(1)) return 2;
    std::ifstream file(argv[1]);
    const std::string contents{ std::istreambuf_iterator<char>(file), {} };
    if (contents.find("stdout marker") == std::string::npos) return 3;
    if (contents.find("stderr marker") == std::string::npos) return 4;
    return 0;
}
