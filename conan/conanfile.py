from conans import ConanFile, CMake, tools
from os import path

class PanoptesConan(ConanFile):
    name = "panoptes"
    version = "1.0.4"
    license = "MIT"
    url = "https://github.com/neXenio/panoptes.git"
    author = "Mathias Eggert <mathias.eggert@outlook.com>"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = "shared=False"
    description = "cross-platform filewatcher library for C++17 using std::filesystem and native interfaces"
    generators = "cmake"

    def source(self):
        g = tools.Git(folder="source")
        g.clone(self.url, branch="master")
        g.checkout("v" + self.version)

    def build(self):
        cmake = CMake(self)
        cmake.configure(source_folder=path.join(self.source_folder, "source"))
        cmake.build()
        cmake.install()

    def package(self):
        build_dir = path.join(self.source_folder, "source")
        self.copy(pattern="*", dst="include", src=path.join(build_dir, "include"))
        self.copy(pattern="*.lib", dst="lib", src=path.join(build_dir, "lib"), keep_path=False)
        self.copy(pattern="*.a", dst="lib", src=path.join(build_dir, "lib"), keep_path=False)
        self.copy(pattern="*.so", dst="lib", src=path.join(build_dir, "lib"), keep_path=False)
        self.copy(pattern="*.dylib", dst="lib", src=path.join(build_dir, "lib"), keep_path=False)
        self.copy(pattern="*.dll", dst="bin", src=path.join(build_dir, "bin"), keep_path=False)
        self.copy(pattern="LICENSE", dst='licenses', src=build_dir, ignore_case=True, keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["PanoptesFW"]