{pkgs}: {
  deps = [
    pkgs.catch2
    pkgs.gtest
    pkgs.yaml-cpp
    pkgs.nlohmann_json
    pkgs.spdlog
    pkgs.clang-tools
    pkgs.doxygen
    pkgs.pkg-config
    pkgs.ninja
    pkgs.clang
    pkgs.gcc
    pkgs.cmake
  ];
}
