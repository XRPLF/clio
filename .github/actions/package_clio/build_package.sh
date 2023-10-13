source /opt/rh/devtoolset-11/enable
source /opt/rh/rh-python38/enable

if ! command -v conan &> /dev/null
then
    echo "Conan could not be found"
    pip install "conan<2"
fi

SRC_DIR=/clio
BUILD_DIR="${SRC_DIR}/build"
BUILD_CONFIG=Release
TESTS=False
NPROC=$(nproc)
PKG=${PKG-deb}

git config --global --add safe.directory $PWD

conan remote add --insert 0 conan-non-prod http://18.143.149.228:8081/artifactory/api/conan/conan-non-prod || true
conan install ${SRC_DIR} \
    --build b2  \
    --build missing \
    --install-folder ${BUILD_DIR} \
    --output-folder ${BUILD_DIR} \
    --options tests=${TESTS} \
    --settings build_type=${BUILD_CONFIG}

cmake -S ${SRC_DIR} \
    -B ${BUILD_DIR} \
    -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${BUILD_DIR}/build/generators/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=${BUILD_CONFIG} \
    -DPKG=$PKG

cmake --build ${BUILD_DIR} \
    --target package \
    --parallel $NPROC
