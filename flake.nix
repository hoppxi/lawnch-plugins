{
  description = "Lawnch Plugins";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs =
    { self, nixpkgs, ... }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      packages = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          lib = pkgs.lib;

          pluginApi = pkgs.stdenv.mkDerivation {
            pname = "lawnch-plugin-api";
            version = "0.3.0-alpha";

            src = ./.;

            nativeBuildInputs = [ pkgs.cmake ];

            meta = {
              description = "API header for Lawnch plugins";
              license = lib.licenses.mit;
            };
          };

          parsePinfoValue =
            file: key:
            let
              content = builtins.readFile file;
              lines = builtins.filter (l: l != "") (lib.splitString "\n" content);
              matching = builtins.filter (l: lib.hasPrefix "${key}=" l) lines;
              line = builtins.head matching;
            in
            lib.removePrefix "${key}=" line;

          mkPlugin =
            name:
            let
              pinfoFile = ./plugins/${name}/pinfo;
              version = parsePinfoValue pinfoFile "version";
              depsFile = ./plugins/${name}/deps.nix;
              extraDeps = if builtins.pathExists depsFile then import depsFile { inherit pkgs; } else [ ];
            in
            pkgs.stdenv.mkDerivation {
              pname = "lawnch-plugin-${name}";
              inherit version;
              src = ./plugins/${name};

              nativeBuildInputs = [ pkgs.cmake ];
              buildInputs = [ pluginApi ] ++ extraDeps;

              cmakeFlags = [
                "-DLawnchPluginApi_DIR=${pluginApi}/lib/cmake/LawnchPluginApi"
              ];

              preConfigure = ''
                cmakeFlagsArray+=("-DCMAKE_INSTALL_PREFIX=$out/share/lawnch/plugins/${name}")
              '';
            };

          pluginNames = [
            "calculator"
            "clipboard"
            "command"
            "emoji"
            "files"
            "files-nav"
            "google"
            "powermenu"
            "wallpapers"
            "youtube"
          ];

          pluginPkgs = lib.genAttrs pluginNames (name: mkPlugin name);

        in
        pluginPkgs
        // {
          plugin-api = pluginApi;
          default = pkgs.symlinkJoin {
            name = "lawnch-plugins";
            paths = [ pluginApi ] ++ (builtins.attrValues pluginPkgs);
            meta = {
              description = "All available Lawnch plugins";
              license = lib.licenses.mit;
            };
          };
        }
      );

      devShells = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          allPluginPkgs = builtins.attrValues self.packages.${system};
        in
        {
          default = pkgs.mkShell {
            inputsFrom = allPluginPkgs;
          };
        }
        // (builtins.mapAttrs (name: pkg: pkgs.mkShell { inputsFrom = [ pkg ]; }) (
          nixpkgs.lib.filterAttrs (n: _: n != "default" && n != "plugin-api") self.packages.${system}
        ))
      );
    };
}
