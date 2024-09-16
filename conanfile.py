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
        'static': [True, False],              # static linkage
        'verbose': [True, False],
        'tests': [True, False],               # build unit tests; create `clio_tests` binary
        'integration_tests': [True, False],   # build integration tests; create `clio_integration_tests` binary
        'benchmark': [True, False],           # build benchmarks; create `clio_benchmarks` binary
        'docs': [True, False],                # doxygen API docs; create custom target 'docs'
        'packaging': [True, False],           # create distribution packages
        'coverage': [True, False],            # build for test coverage report; create custom target `clio_tests-ccov`
        'lint': [True, False],                # run clang-tidy checks during compilation
    }

    default_options = {
        'static': False,
        'verbose': False,
        'tests': False,
        'integration_tests': False,
        'benchmark': False,
        'packaging': False,
        'coverage': False,
        'lint': False,
        'docs': False,
        
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
        'benchmark/*:header_only': True, # Set options for benchmark if applicable
    }

    exports_sources = (
        'CMakeLists.txt', 'cmake/*', 'src/*'
    )

    def requirements(self):
        if self.options.tests or self.options.integration_tests:
            self.requires('gtest/1.14.0')
        if self.options.benchmark:
            self.requires('benchmark/1.8.3')

    def configure(self):
        if self.settings.compiler == 'apple-clang':
            self.options['boost'].visibility = 'global'

    def layout(self):
        cmake_layout(self)
        self.folders.generators = 'build/generators'

    generators = 'CMakeDeps'

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables['verbose'] = self.options.verbose
        tc.variables['static'] = self.options.static
        tc.variables['tests'] = self.options.tests
        tc.variables['integration_tests'] = self.options.integration_tests
        tc.variables['coverage'] = self.options.coverage
        tc.variables['lint'] = self.options.lint
        tc.variables['docs'] = self.options.docs
        tc.variables['packaging'] = self.options.packaging
        tc.variables['benchmark'] = self.options.benchmark
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
