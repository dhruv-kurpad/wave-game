cmake_minimum_required(VERSION 3.16)
project(WaveGame LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Third-party: KissFFT (vendored, C sources) ---
# We only need the real FFT (kiss_fftr) for audio analysis later.
add_library(kissfft STATIC
  third_party/kissfft/kiss_fft.c
  third_party/kissfft/kiss_fftr.c
)
target_include_directories(kissfft PUBLIC third_party/kissfft)
# kiss_fft.h expects this for float (default) real-FFT helpers.
target_compile_definitions(kissfft PUBLIC kiss_fft_scalar=float)

# --- System deps via pkg-config / CMake packages ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

add_executable(wavegame
  src/main.cpp
  src/audio/AudioInput.cpp
  src/dsp/DSPProcessor.cpp
)

add_executable(test_ring_buffer
  tests/test_ring_buffer.cpp
)
target_include_directories(test_ring_buffer PRIVATE src)

add_executable(test_dsp_math
  tests/test_dsp_math.cpp
)
target_include_directories(test_dsp_math PRIVATE src)


target_include_directories(wavegame PRIVATE
  ${PORTAUDIO_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIRS}
  ${SDL2_TTF_INCLUDE_DIRS}
  src
)

# pkg-config often returns -lfoo without -L/path on Homebrew; add dirs explicitly.
target_link_directories(wavegame PRIVATE
  ${PORTAUDIO_LIBRARY_DIRS}
  ${SDL2_LIBRARY_DIRS}
  ${SDL2_TTF_LIBRARY_DIRS}
)

target_link_libraries(wavegame PRIVATE
  kissfft
  ${PORTAUDIO_LIBRARIES}
  ${SDL2_LIBRARIES}
  ${SDL2_TTF_LIBRARIES}
)

target_compile_options(wavegame PRIVATE
  ${PORTAUDIO_CFLAGS_OTHER}
  ${SDL2_CFLAGS_OTHER}
  ${SDL2_TTF_CFLAGS_OTHER}
)

# On macOS, pkg-config sometimes omits framework flags SDL needs.
if(APPLE)
  target_link_libraries(wavegame PRIVATE
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreAudio"
    "-framework AudioToolbox"
    "-framework CoreFoundation"
    "-framework ForceFeedback"
    "-framework Carbon"
    "-framework Metal"
    "-framework QuartzCore"
    "-framework GameController"
  )
endif()

# Copy config next to the binary so runtime can find defaults easily.
add_custom_command(TARGET wavegame POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:wavegame>/config
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/config/settings.json
    $<TARGET_FILE_DIR:wavegame>/config/settings.json
)
