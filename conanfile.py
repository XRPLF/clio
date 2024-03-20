from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout


class Clio(ConanFile):
    name = 'clio'
    license = 'ISC'
    author = 'Alex Kremer <akremer@ripple.com>, John Freeman <jfreeman@ripple.com>'
    url = 'https://github.com/xrplf/clio'
    description = 'Clio RPC server'
    settings = 'os', 'compiler', 'build_type', 'arch'
    options = {
        'fPIC': [True, False],
        'verbose': [True, False],
        'tests': [True, False],     # build unit tests; create `clio_tests` binary
        'docs': [True, False],      # doxygen API docs; create custom target 'docs'
        'packaging': [True, False], # create distribution packages
        'coverage': [True, False],  # build for test coverage report; create custom target `clio_tests-ccov`
        'lint': [True, False],      # run clang-tidy checks during compilation
    }

    requires = [
        'boost/1.82.0',
        'cassandra-cpp-driver/2.17.0',
        'fmt/10.1.1',
        'protobuf/3.21.12',
        'grpc/1.50.1',
        'openssl/1.1.1u',
        'xrpl/2.1.0',
        'libbacktrace/cci.20210118'
    ]

    default_options = {
        'fPIC': True,
        'verbose': False,
        'tests': False,
        'packaging': False,
        'coverage': False,
        'lint': False,
        'docs': False,
        
        'xrpl/*:tests': False,
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
        'CMakeLists.txt', 'CMake/*', 'src/*'
    )

    def requirements(self):
        if self.options.tests:
            self.requires('gtest/1.14.0')

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
        tc.variables['verbose'] = self.options.verbose
        tc.variables['tests'] = self.options.tests
        tc.variables['coverage'] = self.options.coverage
        tc.variables['lint'] = self.options.lint
        tc.variables['docs'] = self.options.docs
        tc.variables['packaging'] = self.options.packaging
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
