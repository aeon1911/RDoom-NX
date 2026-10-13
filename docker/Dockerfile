# RDoom NX build environment.
#
# Base: the official devkitPro devkitA64 image, pinned by digest (tag 20260219:
# devkitA64 GCC 15.2.0, libnx 4.12.0, switch-portlibs).
#
# On top of it:
#   * Mesa 26.2.4 "switch-unified-horizon-sdk" (danfromtico/mesa-switch CI
#     artifact 11484346771, commit 606441c90e459028a50fca352daebfc20211f890).
#     It replaces the old switch-mesa 20.1 EGL/GLES archives with one Mesa
#     that provides loaderless NVK Vulkan plus EGL/GL/GLES on the same Horizon
#     backend. The archive is verified against its SHA-256 before use.
#   * libsndfile 1.2.2, which devkitPro does not package (needed by ZMusic).
#
# Build context: the repository's third_party/ directory
#   docker build -f docker/Dockerfile -t rdoom-nx-build third_party
FROM devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282

ARG MESA_SDK_ZIP=mesa-sdk/mesa-26.2.4-switch-unified-horizon-sdk-606441c9.zip
ARG MESA_SDK_SHA256=937cbda82d411c3ca909aa977b3ced82d4e6070046ab016911b1ce84e58ca392
ARG LIBSNDFILE_TAG=1.2.2

ENV DEVKITPRO=/opt/devkitpro \
    DEVKITA64=/opt/devkitpro/devkitA64 \
    PATH=/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:/opt/devkitpro/portlibs/switch/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

# Host-side packages: the engine's native configure step (which only builds
# the code generators zipdir/re2c/lemon) looks for these.
RUN apt-get update \
 && apt-get install -y --no-install-recommends libsdl2-dev libglib2.0-dev libopenal-dev libvpx-dev libbz2-dev \
      zlib1g-dev libgtk-3-dev \
 && rm -rf /var/lib/apt/lists/*

COPY ${MESA_SDK_ZIP} /tmp/mesa-sdk.zip
RUN echo "${MESA_SDK_SHA256}  /tmp/mesa-sdk.zip" | sha256sum -c - \
 && cd / && unzip -q -o /tmp/mesa-sdk.zip \
 && rm /tmp/mesa-sdk.zip \
 # The SDK archives come from an MSYS2 llvm-ar whose symbol index leaves out
 # the Rust core/alloc/std members (13400 of 18602 symbols in libvulkan.a), so
 # GNU ld cannot resolve NAK's runtime. Rebuild every index with GNU ranlib.
 && for a in /opt/devkitpro/portlibs/switch/lib/libvulkan.a /opt/devkitpro/portlibs/switch/lib/libEGL.a \
             /opt/devkitpro/portlibs/switch/lib/libnak_rs.a /opt/devkitpro/portlibs/switch/lib/libGL.a \
             /opt/devkitpro/portlibs/switch/lib/libGLESv2.a /opt/devkitpro/portlibs/switch/lib/libglapi.a; do \
      aarch64-none-elf-ranlib "$a"; done \
 && test -f /opt/devkitpro/portlibs/switch/lib/libvulkan.a \
 && test -f /opt/devkitpro/portlibs/switch/include/vulkan/vulkan_vi.h

RUN git clone -q --depth 1 --branch ${LIBSNDFILE_TAG} https://github.com/libsndfile/libsndfile.git /tmp/libsndfile \
 && git -C /tmp/libsndfile rev-parse HEAD > /opt/devkitpro/portlibs/switch/libsndfile.REVISION \
 && . /opt/devkitpro/switchvars.sh \
 && unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS \
 && cmake -S /tmp/libsndfile -B /tmp/libsndfile/build-nx -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
      -DCMAKE_INSTALL_PREFIX=/opt/devkitpro/portlibs/switch -DBUILD_SHARED_LIBS=OFF \
      -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DENABLE_CPACK=OFF \
      -DENABLE_EXTERNAL_LIBS=ON -DENABLE_MPEG=OFF -DINSTALL_MANPAGES=OFF -DBUILD_REGTEST=OFF \
 && ninja -C /tmp/libsndfile/build-nx install \
 && rm -rf /tmp/libsndfile \
 && ( [ -f /opt/devkitpro/portlibs/switch/lib/libdl.a ] || aarch64-none-elf-ar rcs /opt/devkitpro/portlibs/switch/lib/libdl.a )

WORKDIR /src
