ADD_SUBDIRECTORY(brotli)
ADD_SUBDIRECTORY(pugixml)
ADD_SUBDIRECTORY(sqlite)
ADD_SUBDIRECTORY(zlib)
ADD_SUBDIRECTORY(zstd)

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v2.13.2)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 8.0.1
)

FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.0.0
)

FetchContent_MakeAvailable(Catch2 CLI11 fmt)