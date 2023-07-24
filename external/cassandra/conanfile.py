from conan import ConanFile, tools
from conan.tools.cmake import CMake, CMakeToolchain

class Cassandra(ConanFile):
    name = 'cassandra-cpp-driver'
    version = '2.16.2'
    license = 'Apache-2.0'
    url = 'https://github.com/conan-io/conan-center-index'
    homepage = 'https://docs.datastax.com/en/developer/cpp-driver/'
    description = 'Cassandra C++ Driver'
    topics = ('conan', 'cassandra', 'driver')

    settings = 'os', 'arch', 'compiler', 'build_type'
    options = {
        'shared': [True, False],
        'fPIC': [True, False],
        'install_header_in_subdir': [True, False],
        'use_atomic': [None, 'boost', 'std'],
        'with_openssl': [True, False],
        'with_zlib': [True, False],
        'with_kerberos': [True, False],
        'use_timerfd': [True, False],
    }
    default_options = {
        'shared': False,
        'fPIC': True,
        'install_header_in_subdir': False,
        'use_atomic': None,
        'with_openssl': True,
        'with_zlib': True,
        'with_kerberos': False,
        'use_timerfd': True,
    }

    def requirements(self):
        self.requires('libuv/1.44.1')
        self.requires('http_parser/2.9.4')
        if self.options.with_openssl:
            self.requires('openssl/1.1.1q')
        if self.options.with_zlib:
            self.requires('minizip/1.2.12')
            self.requires('zlib/1.2.13')
        if self.options.use_atomic == 'boost':
            self.requires('boost/1.79.0')

    exports_sources = ['CMakeLists.txt']

    def config_options(self):
        if self.settings.os == 'Windows':
            del self.options.fPIC

    def configure(self):
        self.options['libuv'].shared = self.options.shared

    def generate(self):
        tc = CMakeToolchain(self)
        if self.settings.get_safe('compiler.cppstd') == '20':
            tc.blocks['cppstd'].values['cppstd'] = 17
        tc.variables['CASS_BUILD_STATIC'] = not self.options.shared
        tc.variables['CASS_USE_STATIC_LIBS'] = not self.options.shared
        tc.variables['CASS_BUILD_SHARED'] = self.options.shared
        tc.variables['LIBUV_ROOT_DIR'] = self.deps_cpp_info['libuv'].rootpath
        if self.options.with_openssl:
            tc.variables['OPENSSL_ROOT_DIR'] = self.deps_cpp_info['openssl'].rootpath
        tc.generate()

    def source(self):
        tools.files.get(self, f'https://github.com/datastax/cpp-driver/archive/refs/tags/{self.version}.tar.gz', strip_root=True)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        if self.options.shared:
            self.cpp_info.libs = ['cassandra']
        else:
            self.cpp_info.libs = ['cassandra_static']
        if self.settings.os == 'Windows':
            self.cpp_info.libs.extend(['iphlpapi', 'psapi', 'wsock32', 'crypt32', 'ws2_32', 'userenv'])
            if not self.options.shared:
                self.cpp_info.defines = ['CASS_STATIC']
