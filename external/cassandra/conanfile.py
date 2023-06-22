from conans import ConanFile, CMake, tools
import os


class CassandraConan(ConanFile):
    name = "cassandra-driver"
    version = "2.16.2"
    description = "Cassandra C++ Driver"
    topics = ("conan", "cassandra", "driver")
    url = "https://github.com/bincrafters/conan-cassandra-driver"
    homepage = "https://github.com/datastax/cpp-driver"
    license = "Apache-2.0"

    exports_sources = ["CMakeLists.txt"]
    generators = "cmake"

    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {'shared': False, 'fPIC': True}

    _source_subfolder = "source_subfolder"

    requires = (
        "libuv/1.44.1",
        "http_parser/2.9.4",
        "rapidjson/cci.20211112",
        "openssl/1.1.1q",
    )

    def config_options(self):
        if self.settings.os == 'Windows':
            del self.options.fPIC

    def configure(self):
        self.options["libuv"].shared = self.options.shared

    def source(self):
        url: ""
        sha256 = "de60751bd575b5364c2c5a17a24a40f3058264ea2ee6fef19de126ae550febc9"
        source_url = " https://github.com/datastax/cpp-driver/archive/refs/tags/"
        extracted_name = "cpp-driver-{}".format(self.version)
        full_filename = "{}.tar.gz".format(extracted_name)
        tools.get("{0}/{1}.tar.gz".format(source_url, self.version), filename=full_filename, sha256=sha256)
        os.rename(extracted_name, self._source_subfolder)
        self.source_subfolder = self._source_subfolder

    def _configure_cmake(self):
        cmake = CMake(self)
        cmake.definitions['CASS_BUILD_STATIC'] = not self.options.shared
        cmake.definitions['CASS_USE_STATIC_LIBS'] = not self.options.shared
        cmake.definitions['CASS_BUILD_SHARED'] = self.options.shared
        cmake.definitions['LIBUV_ROOT_DIR'] = self.deps_cpp_info["libuv"].rootpath
        if self.settings.os == 'Windows' and self.options.shared:
            if self.settings.compiler == 'gcc':
                libuv_library = os.path.join(self.deps_cpp_info["libuv"].rootpath, "bin", "libuv.dll")
            else:
                libuv_library = os.path.join(self.deps_cpp_info["libuv"].rootpath, "lib", "libuv.dll.lib")
            cmake.definitions['LIBUV_LIBRARY'] = libuv_library
        cmake.definitions['OPENSSL_ROOT_DIR'] = self.deps_cpp_info["openssl"].rootpath
        cmake.configure(source_folder = self._source_subfolder)
        self._cmake = cmake

    def build(self):
        if self.settings.os == 'Windows' and self.settings.compiler == 'gcc' and self.options.shared:
            tools.replace_in_file(os.path.join(self._source_subfolder, "cmake", "modules", "CppDriver.cmake"),
                                  "if(WIN32)\n      install",
                                  "if(WIN32 AND NOT MINGW)\n      install")
            tools.replace_in_file(os.path.join(self._source_subfolder, "cmake", "modules", "CppDriver.cmake"),
                                  'elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")',
                                  'elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")\n' +
                                  'set(CASS_LIBS ${CASS_LIBS} iphlpapi psapi wsock32 crypt32 ws2_32 userenv)')
        self._configure_cmake()
        self._cmake.build()

    def package(self):
        self.copy(pattern="LICENSE.txt", dst="license", src=self._source_subfolder)
        self._cmake.install()

    def package_info(self):
        if self.options.shared:
            self.cpp_info.libs = ['cassandra']
        else:
            self.cpp_info.libs = ['cassandra_static']
        if self.settings.os == "Windows":
            self.cpp_info.libs.extend(["iphlpapi", "psapi", "wsock32", "crypt32", "ws2_32", "userenv"])
            if not self.options.shared:
                self.cpp_info.defines = ["CASS_STATIC"]
        elif self.settings.os == "Linux":
            self.cpp_info.libs.extend(["pthread", "rt"])
            if self.settings.compiler == 'clang' and self.settings.arch == 'x86':
                self.cpp_info.libs.extend(["atomic"])

# # from conans import ConanFile, CMake, tools
# from conans.errors import ConanInvalidConfiguration
# import os
# required_conan_version = ">=1.33.0"
# class CassandraCppDriverConan(ConanFile):
#     version = "2.16.2"
#     name = "cassandra-cpp-driver"
#     license = "Apache-2.0"
#     url = "https://github.com/conan-io/conan-center-index"
#     homepage = "https://docs.datastax.com/en/developer/cpp-driver/"
#     description = "DataStax C/C++ Driver for Apache Cassandra and DataStax Products"
#     topics = ("cassandra", "cpp-driver", "database", "conan-recipe")
#     settings = "os", "compiler", "build_type", "arch"
#     options = {
#         "shared": [True, False],
#         "fPIC": [True, False],
#         "install_header_in_subdir": [True, False],
#         "use_atomic": [None, "boost", "std"],
#         "with_openssl": [True, False],
#         "with_zlib": [True, False],
#         "with_kerberos": [True, False],
#         "use_timerfd": [True, False],
#     }
#     default_options = {
#         "shared": False,
#         "fPIC": True,
#         "install_header_in_subdir": False,
#         "use_atomic": None,
#         "with_openssl": True,
#         "with_zlib": True,
#         "with_kerberos": False,
#         "use_timerfd": True,
#     }
#     short_paths = True
#     generators = "cmake"
#     exports_sources = [
#         "CMakeLists.txt",
#         "patches/*"
#     ]
#     _cmake = None
#     @property
#     def _source_subfolder(self):
#         return ""
#     def config_options(self):
#         if self.settings.os == "Windows":
#             del self.options.fPIC
#             del self.options.use_timerfd
#     def configure(self):
#         if self.options.shared:
#             del self.options.fPIC
#     def requirements(self):
#         self.requires("libuv/1.44.1")
#         self.requires("http_parser/2.9.4")
#         self.requires("rapidjson/cci.20211112")
#         if self.options.with_openssl:
#             self.requires("openssl/1.1.1q")
#         if self.options.with_zlib:
#             self.requires("minizip/1.2.12")
#             self.requires("zlib/1.2.12")
#         if self.options.use_atomic == "boost":
#             self.requires("boost/1.79.0")
#     def validate(self):
#         if self.options.use_atomic == "boost":
#             # Compilation error on Linux
#             if self.settings.os == "Linux":
#                 raise ConanInvalidConfiguration(
#                     "Boost.Atomic is not supported on Linux at the moment")
#         if self.options.with_kerberos:
#             raise ConanInvalidConfiguration(
#                 "Kerberos is not supported at the moment")
#     def source(self):
#         tools.get(**self.conan_data["sources"][self.version],
#                   destination=self._source_subfolder, strip_root=True)
#     def _patch_sources(self):
#         for patch in self.conan_data.get("patches", {}).get(self.version, []):
#             tools.patch(**patch)
#         tools.replace_in_file(os.path.join(self._source_subfolder, "CMakeLists.txt"),
#                               "\"${CMAKE_CXX_COMPILER_ID}\" STREQUAL \"Clang\"",
#                               "\"${CMAKE_CXX_COMPILER_ID}\" STREQUAL \"Clang\" OR \"${CMAKE_CXX_COMPILER_ID}\" STREQUAL \"AppleClang\"")
#     def _configure_cmake(self):
#         if self._cmake:
#             return self._cmake
#         self._cmake = CMake(self)
#         self._cmake.definitions["VERSION"] = self.version
#         self._cmake.definitions["CASS_BUILD_EXAMPLES"] = False
#         self._cmake.definitions["CASS_BUILD_INTEGRATION_TESTS"] = False
#         self._cmake.definitions["CASS_BUILD_SHARED"] = self.options.shared
#         self._cmake.definitions["CASS_BUILD_STATIC"] = not self.options.shared
#         self._cmake.definitions["CASS_BUILD_TESTS"] = False
#         self._cmake.definitions["CASS_BUILD_UNIT_TESTS"] = False
#         self._cmake.definitions["CASS_DEBUG_CUSTOM_ALLOC"] = False
#         self._cmake.definitions["CASS_INSTALL_HEADER_IN_SUBDIR"] = self.options.install_header_in_subdir
#         self._cmake.definitions["CASS_INSTALL_PKG_CONFIG"] = False
#         if self.options.use_atomic == "boost":
#             self._cmake.definitions["CASS_USE_BOOST_ATOMIC"] = True
#             self._cmake.definitions["CASS_USE_STD_ATOMIC"] = False
#         elif self.options.use_atomic == "std":
#             self._cmake.definitions["CASS_USE_BOOST_ATOMIC"] = False
#             self._cmake.definitions["CASS_USE_STD_ATOMIC"] = True
#         else:
#             self._cmake.definitions["CASS_USE_BOOST_ATOMIC"] = False
#             self._cmake.definitions["CASS_USE_STD_ATOMIC"] = False
#         self._cmake.definitions["CASS_USE_OPENSSL"] = self.options.with_openssl
#         self._cmake.definitions["CASS_USE_STATIC_LIBS"] = False
#         self._cmake.definitions["CASS_USE_ZLIB"] = self.options.with_zlib
#         self._cmake.definitions["CASS_USE_LIBSSH2"] = False
#         # FIXME: To use kerberos, its conan package is needed. Uncomment this when kerberos conan package is ready.
#         # self._cmake.definitions["CASS_USE_KERBEROS"] = self.options.with_kerberos
#         if self.settings.os == "Linux":
#             self._cmake.definitions["CASS_USE_TIMERFD"] = self.options.use_timerfd
#         self._cmake.configure()
#         return self._cmake
#     def build(self):
#         # self._patch_sources()
#         cmake = self._configure_cmake()
#         cmake.build()
#     def package(self):
#         self.copy(pattern="LICENSE.txt", dst="licenses", src=self._source_subfolder)
#         cmake = self._configure_cmake()
#         cmake.install()
#     def package_info(self):
#         self.cpp_info.libs = tools.collect_libs(self)
#         if self.settings.os == "Windows":
#             self.cpp_info.system_libs.extend(["iphlpapi", "psapi", "wsock32",
#                 "crypt32", "ws2_32", "userenv", "version"])
#             if not self.options.shared:
#                 self.cpp_info.defines = ["CASS_STATIC"]

