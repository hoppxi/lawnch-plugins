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
          version = "0.1.0-alpha";
          src = ./.;
          nativeBuildInputs = [ pkgs.cmake ];
          meta = with pkgs.lib; {
            description = "API header for Lawnch plugins";
            license = licenses.mit;
          };
        };

        newPlugin = name: pkgs.callPackage ./plugins/${name}/package.nix {
          lawnch-plugin-api = pluginApi;
        };

        plugins = {
          plugin-api = pluginApi;
          clipboard = newPlugin "clipboard";
          powermenu = newPlugin "powermenu";
          calculator = newPlugin "calculator";
          command = newPlugin "command";
          emoji = newPlugin "emoji";
          files = newPlugin "files";
          google = newPlugin "google";
          wallpapers = newPlugin "wallpapers";
          youtube = newPlugin "youtube";
        };

      in
      {
        packages = plugins // {
          default = pkgs.symlinkJoin {
            name = "lawnch-plugins";
            paths = builtins.attrValues plugins;
            meta = with pkgs.lib; {
              description = "All available Lawnch plugins";
              license = licenses.mit;
            };
          };
        };
      }
    );

}
