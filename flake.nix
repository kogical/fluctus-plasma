{
  description = "Fluctus: a fluid dock and real Liquid Glass for KDE Plasma 6.8+";

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
          pname = "fluctus";
          version = "0.2.0";
          src = self;

          # Vendored Rust dependencies for the liquidglass engine (offline, from Cargo.lock).
          cargoDeps = pkgs.rustPlatform.importCargoLock { lockFile = ./Cargo.lock; };

          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
            pkgs.cargo
            pkgs.rustc
            pkgs.rustPlatform.cargoSetupHook
            pkgs.kdePackages.extra-cmake-modules
            pkgs.kdePackages.qtshadertools
            pkgs.kdePackages.wrapQtAppsHook
          ];

          buildInputs = [
            pkgs.kdePackages.qtbase
            pkgs.kdePackages.qtdeclarative
            pkgs.kdePackages.kcoreaddons
            pkgs.kdePackages.kconfig
            pkgs.kdePackages.kguiaddons
            pkgs.kdePackages.ki18n
            pkgs.kdePackages.kwindowsystem
            pkgs.kdePackages.kglobalaccel
            pkgs.kdePackages.kservice
            pkgs.kdePackages.kio
            pkgs.kdePackages.kcmutils
            pkgs.kdePackages.libplasma
            pkgs.kdePackages.plasma-workspace
            pkgs.kdePackages.kwin
            pkgs.kdePackages.kdecoration
          ];

          cmakeFlags = [
            "-DFLUCTUS_REQUIRE_ECM=ON"
            "-DFLUCTUS_BUILD_TESTS=ON"
          ];

          dontWrapQtApps = true; # plugins only, nothing to wrap

          doCheck = true;
          checkPhase = ''
            runHook preCheck
            QT_QPA_PLATFORM=offscreen ctest --output-on-failure
            runHook postCheck
          '';

          meta = with pkgs.lib; {
            description = "Animated dock and Liquid Glass KWin effect for KDE Plasma 6.8+";
            homepage = "https://github.com/kogical/fluctus";
            license = [ licenses.asl20 licenses.gpl3Plus ];
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
            pkgs.clippy
          ];
        };
      });
    };
}
