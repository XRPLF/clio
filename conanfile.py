from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout


class ClioConan(ConanFile):
    name = 'clio'
    license = 'ISC'
    author = 'Alex Kremer <akremer@ripple.com>, John Freeman <jfreeman@ripple.com>, Ayaz Salikhov <asalikhov@ripple.com>'
    url = 'https://github.com/xrplf/clio'
    description = 'Clio RPC server'
    settings = 'os', 'compiler', 'build_type', 'arch'
    options = {
        'tests': [True, False],
        'benchmark': [True, False],
    }

    requires = [
        'boost/1.83.0',
        'cassandra-cpp-driver/2.17.0',
        'fmt/11.2.0',
        'protobuf/3.21.12',
        'grpc/1.50.1',
        'openssl/1.1.1w',
        'xrpl/2.6.0',
        'zlib/1.3.1',
        'libbacktrace/cci.20210118',
        'spdlog/1.15.3',
    ]

    default_options = {
        'tests': False,
        'benchmark': False,

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
        if self.options.tests or self.options.integration_tests:
            self.requires('gtest/1.14.0')
        if self.options.benchmark:
            self.requires('benchmark/1.9.4')

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
        for option_name, option_value in self.options.items():
            tc.variables[option_name] = option_value
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
