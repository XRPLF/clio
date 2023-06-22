from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
import re

class Clio(ConanFile):
    name = 'clio'
    license = 'ISC'
    author = 'Alex Kremer <akremer@ripple.com>'
    url = 'https://github.com/xrplf/clio'
    description = 'Clio RPC server'
    settings = 'os', 'compiler', 'build_type', 'arch'
    options = {
        'assertions': [True, False],
        'coverage': [True, False],
        'fPIC': [True, False],
        'shared': [True, False],
        'static': [True, False],
        'tests': [True, False],
    }

    requires = [
        'clio-xrpl/1.11.0',
        'boost/1.77.0',
        'grpc/1.50.1',
        'openssl/1.1.1m',
        'protobuf/3.21.4',
        'cassandra-driver/2.16.2',
        'fmt/10.0.0',
        'gtest/1.13.0'
    ]

    default_options = {
        'assertions': False,
        'coverage': False,
        'fPIC': True,
        'shared': False,
        'static': True,
        'tests': False,

        'cassandra-driver/*:shared': False,
        'date/*:header_only': True,
        'grpc/*:shared': False,
        'grpc/*:secure': True,
        'libpq/*:shared': False,
        'lz4/*:shared': False,
        'openssl/*:shared': False,
        'protobuf/*:shared': False,
        'protobuf/*:with_zlib': True,
        'snappy/*:shared': False,
    }
        
    generators = ('cmake') # this may have to be done differently
    exports_sources = (
        'CMakeLists.txt', 'CMake/*', 'src/*'
    )

    def configure(self):
        if self.settings.compiler == 'apple-clang':
            self.options['boost'].visibility = 'global'

    def layout(self):
        cmake_layout(self)
        # Fix this setting to follow the default introduced in Conan 1.48
        # to align with our build instructions.
        self.folders.generators = 'build/generators'

    # generators = 'CMakeDeps'
    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables['tests'] = self.options.tests
        tc.variables['assert'] = self.options.assertions
        tc.variables['coverage'] = self.options.coverage
        tc.variables['BUILD_SHARED_LIBS'] = self.options.shared
        tc.variables['static'] = self.options.static
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.verbose = True
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.verbose = True
        cmake.install()
