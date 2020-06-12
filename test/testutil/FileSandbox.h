#ifndef PFW_FILE_SANDBOX_H
#define PFW_FILE_SANDBOX_H

#include <filesystem>
#include <fstream>
#include <optional>
#include <random>

#include "catch_wrapper.h"

class FileSandbox
{
  public:
    FileSandbox()
    {
        size_t tries      = 0;
        bool   dirCreated = false;
        for (int i = 0; i < 26 && !dirCreated; ++i) {
            _path      = generateTemporaryPath(i);
            dirCreated = std::filesystem::create_directory(_path);
        }

        if (!dirCreated) {
            throw std::runtime_error("failed to create directory");
        }

        std::cout << "created filesandbox: '" << _path.string() << "'"
                  << std::endl;

        REQUIRE(isEmpty());
    }
    ~FileSandbox() { std::filesystem::remove_all(_path); }

    static std::filesystem::path generateTemporaryPath(size_t count)
    {
        if (count >= 26) {
            throw std::invalid_argument("count should not be higher then 26");
        }
        static std::random_device                 rd;
        static std::mt19937                       mt(rd());
        static std::uniform_int_distribution<int> dist(100000, 999999);

        std::string fileName = "nsfw-Sandbox-";
        fileName.push_back('a' + count);
        fileName += std::to_string(dist(mt));

        std::filesystem::path result = std::filesystem::temp_directory_path();
        result /= fileName;

        return result;
    }

    static std::string generateRandomData(const size_t bytes)
    {
        static std::random_device                 rd;
        static std::mt19937                       mt(rd());
        static std::uniform_int_distribution<int> dist(32, 254);

        std::string data;

        for (int i = 0; i < bytes; ++i) {
            data.push_back((char)dist(mt));
        }

        return data;
    }

    std::filesystem::path
    createFile(const std::filesystem::path &relativePath,
               std::optional<std::string>   content = std::nullopt)
    {
        std::filesystem::path absPath = _path / relativePath;

        std::fstream file(absPath.c_str(),
                          std::fstream::out | std::fstream::binary);
        if (content.has_value()) {
            file.write(content->c_str(), content->size());
        }
        file.close();

        REQUIRE(std::filesystem::exists(absPath));

        return absPath;
    }

    std::filesystem::path
    createRandomFile(const std::filesystem::path &relativePath,
                     const size_t                 fileSize)
    {
        return createFile(relativePath, generateRandomData(fileSize));
    }

    bool createSymLink(std::filesystem::path srcPath,
                       std::filesystem::path dstPath)
    {
        // not implemented yet
        REQUIRE(false);
        return false;
    }
    void modifyFile(const std::filesystem::path &relativePath,
                    const std::string &          content)
    {
        std::filesystem::path filePath = _path / relativePath;
        REQUIRE(std::filesystem::exists(filePath));

        std::ofstream ofs(filePath.c_str());
        ofs << content;

        REQUIRE(std::filesystem::exists(filePath));
    }
    void modifyFileRandomly(const std::filesystem::path &relativePath,
                            const size_t                 fileSize)
    {
        modifyFile(relativePath, generateRandomData(fileSize));
    }

    std::filesystem::path
    createDirectory(const std::filesystem::path &relativePath)
    {
        std::filesystem::path absPath = _path / relativePath;
        REQUIRE(std::filesystem::create_directory(absPath));
        REQUIRE(std::filesystem::exists(absPath));

        return absPath;
    }

    void touch(const std::filesystem::path &relativePath)
    {
        // not implemented yet
        REQUIRE(false);
    }
    void remove(const std::filesystem::path &relativePath)
    {
        std::filesystem::path filePath = _path / relativePath;
        REQUIRE(std::filesystem::remove_all(filePath) > 0);
        REQUIRE(!std::filesystem::exists(filePath));
    }

    std::filesystem::path rename(const std::filesystem::path &oldRelPath,
                                 const std::filesystem::path &newRelPath)
    {
        std::filesystem::rename(_path / oldRelPath, _path / newRelPath);

        return _path / newRelPath;
    }

    std::string read(const std::filesystem::path &relativePath)
    {
        std::string   content;
        std::string   line;
        std::ifstream file((_path / relativePath).string());
        if (file.is_open()) {
            while (std::getline(file, line)) {
                content += line;
                content.push_back('\n');
            }
            file.close();
        } else {
            REQUIRE(false);
        }

        return content;
    }

    std::filesystem::path path() const { return _path; }
    bool isEmpty() const { return std::filesystem::is_empty(_path); }

    bool contains(const std::filesystem::path &relativePath) const
    {
        return std::filesystem::exists(_path / relativePath);
    }
    bool containsFile(const std::filesystem::path &relativePath) const
    {
        return std::filesystem::is_regular_file(_path / relativePath);
    }
    bool containsDirectory(const std::filesystem::path &relativePath) const
    {
        return std::filesystem::is_directory(_path / relativePath);
    }

    std::vector<std::filesystem::path> list() const
    {
        std::vector<std::filesystem::path> result;

        for (auto &path :
             std::filesystem::recursive_directory_iterator(_path)) {
            result.push_back(path);
        }

        return result;
    }

    size_t entryCount() const { return list().size(); }

    void printListing() const
    {
        for (auto &p : list()) {
            std::cout << p << std::endl;
        }
    }

    std::string getFileSystemType() const
    {
        // not implemented yet
        REQUIRE(false);
        return "";
    }

  private:
    std::filesystem::path _path;
};

#endif  // PFW_FILE_SANDBOX_H
