from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout


class ClioConan(ConanFile):
    name = 'clio'
    license = 'ISC'
    author = 'Alex Kremer <akremer@ripple.com>, John Freeman <jfreeman@ripple.com>, Ayaz Salikhov <asalikhov@ripple.com>'
    url = 'https://github.com/xrplf/clio'
    description = 'Clio RPC server'
    settings = 'os', 'compiler', 'build_type', 'arch'
    options = {}

    requires = [
        'boost/1.83.0',
        'cassandra-cpp-driver/2.17.0',
        'protobuf/3.21.12',
        'grpc/1.50.1',
        'openssl/1.1.1w',
        'xrpl/3.0.0-rc1',
        'zlib/1.3.1',
        'libbacktrace/cci.20210118',
        'spdlog/1.16.0',
    ]

    default_options = {
        'xrpl/*:tests': False,
        'xrpl/*:rocksdb': False,
        'cassandra-cpp-driver/*:shared': False,
        'date/*:header_only': True,
        'grpc/*:shared': False,
        'grpc/*:secure': True,
        'libpq/*:shared': False,
        'lz4/*:shared': False,
        'openssl/*:shared': False,
        'protobuf/*:shared': False,
        'protobuf/*:with_zlib': True,
        'snappy/*:shared': False,
        'gtest/*:no_main': True,
    }

    exports_sources = (
        'CMakeLists.txt', 'cmake/*', 'src/*'
    )

    def requirements(self):
        self.requires('gtest/1.14.0')
        self.requires('benchmark/1.9.4')
        self.requires('fmt/12.1.0', force=True)

    def configure(self):
        if self.settings.compiler == 'apple-clang':
            self.options['boost'].visibility = 'global'

    def layout(self):
        cmake_layout(self)
        # Fix this setting to follow the default introduced in Conan 1.48
        # to align with our build instructions.
        self.folders.generators = 'build/generators'

    generators = 'CMakeDeps'

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
