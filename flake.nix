{
  description = "Fluctus Dock — macOS-style animated task manager dock for KDE Plasma 6.8+";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (pkgs: {
        default = pkgs.stdenv.mkDerivation {
          pname = "fluctus-plasma";
          version = "0.1.0";
          src = self;

          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
            pkgs.kdePackages.extra-cmake-modules
            pkgs.kdePackages.wrapQtAppsHook
          ];

          buildInputs = [
            pkgs.kdePackages.qtbase
            pkgs.kdePackages.qtdeclarative
            pkgs.kdePackages.kcoreaddons
            pkgs.kdePackages.ki18n
            pkgs.kdePackages.kwindowsystem
            pkgs.kdePackages.kglobalaccel
            pkgs.kdePackages.kservice
            pkgs.kdePackages.kio
            pkgs.kdePackages.libplasma
            pkgs.kdePackages.plasma-workspace
          ];

          cmakeFlags = [
            "-DFLUCTUS_REQUIRE_ECM=ON"
            "-DFLUCTUS_BUILD_TESTS=ON"
          ];

          doCheck = true;
          checkPhase = ''
            runHook preCheck
            QT_QPA_PLATFORM=offscreen ctest --output-on-failure
            runHook postCheck
          '';

          meta = with pkgs.lib; {
            description = "macOS-style animated task manager dock for KDE Plasma 6.8+";
            homepage = "https://github.com/kogical/fluctus-plasma";
            license = licenses.mit;
            platforms = platforms.linux;
          };
        };
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ self.packages.${pkgs.system}.default ];
          packages = [
            pkgs.just
            pkgs.ccache
            pkgs.lld
            pkgs.clang-tools
          ];
        };
      });
    };
}
