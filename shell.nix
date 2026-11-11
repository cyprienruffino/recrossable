# Development shell for building the backend and running the AppLoad PC emulator
{ pkgs ? import <nixpkgs> { } }:

pkgs.mkShell {
  packages = with pkgs; [
    cmake
    ninja
    qt6.qtbase
    qt6.qtdeclarative
  ];

  RCC_EXECUTABLE = "${pkgs.qt6.qtbase}/libexec/rcc";
  QT_PLUGIN_PATH = "${pkgs.qt6.qtbase}/${pkgs.qt6.qtbase.qtPluginPrefix}";
  QML2_IMPORT_PATH = "${pkgs.qt6.qtdeclarative}/${pkgs.qt6.qtbase.qtQmlPrefix}";
}
