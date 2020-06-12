#include "pfw/FileSystemWatcher.h"

#include <bitset>
#include <chrono>
#include <filesystem>
#include <memory>

class Listener
{
  public:
    Listener(std::filesystem::path path)
    {
        _watcher = std::make_unique<pfw::FileSystemWatcher>(
            path, std::chrono::milliseconds(1),
            std::bind(&Listener::listenerFunction, this,
                      std::placeholders::_1));
    }
    void listenerFunction(std::vector<pfw::EventPtr> events)
    {
        for (const auto &event : events) {
            std::bitset<16> typeBits(event->type);
            std::cout << event->relativePath << " with the type: " << typeBits
                      << std::endl;
        }
    }

  private:
    std::unique_ptr<pfw::FileSystemWatcher> _watcher;
};

int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cout << "one input parameter is needed" << std::endl;
    }

    const auto path = std::filesystem::path(argv[1]);

    std::cout << "observed path is '" << path.string() << "'" << std::endl;

    auto listenerInstance = Listener(path);

    std::cout << "Press any key to finish the observation!" << std::endl;
    std::cin.ignore();

    return 0;
}
