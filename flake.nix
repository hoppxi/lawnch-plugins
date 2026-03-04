{
  description = "Lawnch Plugins";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        pluginApi = pkgs.stdenv.mkDerivation {
          pname = "lawnch-plugin-api";
          version = "0.2.0-alpha";
          src = ./.;
          nativeBuildInputs = [ pkgs.cmake ];
          meta = with pkgs.lib; {
            description = "API header for Lawnch plugins";
            license = licenses.mit;
          };
        };

        parsePinfoValue =
          file: key:
          let
            content = builtins.readFile file;
            lines = builtins.filter (l: l != "") (pkgs.lib.splitString "\n" content);
            matching = builtins.filter (l: pkgs.lib.hasPrefix "${key}=" l) lines;
            line = builtins.head matching;
          in
          pkgs.lib.removePrefix "${key}=" line;

        mkPlugin =
          name:
          let
            pinfoFile = ./plugins/${name}/pinfo;
            version = parsePinfoValue pinfoFile "version";

            depsFile = ./plugins/${name}/deps.nix;
            extraDeps = if builtins.pathExists depsFile then import depsFile { inherit pkgs; } else [ ];
          in
          {
            package = pkgs.stdenv.mkDerivation {
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

            devShell = pkgs.mkShell {
              nativeBuildInputs = [ pkgs.cmake ];
              buildInputs = [ pluginApi ] ++ extraDeps;
            };
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

        pluginResults = builtins.listToAttrs (
          map (name: {
            inherit name;
            value = mkPlugin name;
          }) pluginNames
        );

        pluginPackages = builtins.mapAttrs (_: r: r.package) pluginResults;
        pluginDevShells = builtins.mapAttrs (_: r: r.devShell) pluginResults;

      in
      {
        packages = pluginPackages // {
          plugin-api = pluginApi;
          default = pkgs.symlinkJoin {
            name = "lawnch-plugins";
            paths = [ pluginApi ] ++ builtins.attrValues pluginPackages;
            meta = with pkgs.lib; {
              description = "All available Lawnch plugins";
              license = licenses.mit;
            };
          };
        };

        devShells = pluginDevShells // {
          default = pkgs.mkShell {
            inputsFrom = builtins.attrValues pluginDevShells;
          };
        };
      }
    );

}
