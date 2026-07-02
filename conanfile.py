from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout

from conan import ConanFile


class ClioConan(ConanFile):
    name = "clio"
    license = "ISC"
    author = "Alex Kremer <akremer@ripple.com>, John Freeman <jfreeman@ripple.com>, Ayaz Salikhov <asalikhov@ripple.com>"
    url = "https://github.com/xrplf/clio"
    description = "Clio RPC server"
    settings = "os", "compiler", "build_type", "arch"
    options = {}

    requires = [
        "cassandra-cpp-driver/2.17.0",
        "fmt/12.1.0",
        "libbacktrace/cci.20210118",
        "spdlog/1.17.0",
        "xrpl/3.2.0",
    ]

    default_options = {
        "cassandra-cpp-driver/*:shared": False,
        "date/*:header_only": True,
        "grpc/*:secure": True,
        "grpc/*:shared": False,
        "gtest/*:no_main": True,
        "libpq/*:shared": False,
        "lz4/*:shared": False,
        "openssl/*:shared": False,
        "protobuf/*:shared": False,
        "protobuf/*:with_zlib": True,
        "snappy/*:shared": False,
        "xrpl/*:rocksdb": True,  # TODO: revert to false when includes are fixed in libxrpl
        "xrpl/*:tests": False,
    }

    exports_sources = ("CMakeLists.txt", "cmake/*", "src/*")

    def requirements(self):
        self.requires("gtest/1.17.0")
        self.requires("benchmark/1.9.5")
        # Clio's own code includes grpc (<grpcpp/...>) and openssl (via
        # <boost/asio/ssl>) headers directly, but xrpl does not re-export them
        # (only boost/date/xxhash are required with transitive_headers=True).
        # So they must be direct requirements of clio to get their include dirs;
        # the version pins match xrpl's, so this does not change any package_id.
        self.requires("grpc/1.78.1")
        self.requires("openssl/3.6.3", force=True)
        # Pin the remaining transitive deps to the exact versions xrpl uses.
        # override=True only sets the version when the package appears
        # transitively (it does not make them direct deps), and matches xrpl's
        # force=True boost pin that overrides nudb's `boost < 1.91.0` cap.
        self.requires("boost/1.91.0", override=True)
        self.requires("zlib/1.3.2", override=True)

    def configure(self):
        if self.settings.compiler == "apple-clang":
            self.options["boost"].visibility = "global"

    def layout(self):
        cmake_layout(self)
        # Fix this setting to follow the default introduced in Conan 1.48
        # to align with our build instructions.
        self.folders.generators = "build/generators"

    generators = "CMakeDeps"

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
