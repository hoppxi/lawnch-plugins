{
  pkgs,
  lawnch-plugin-api,
  ...
}:

let
  api = lawnch-plugin-api;
  plugin-name = "emoji";
in
pkgs.stdenv.mkDerivation {
  pname = "lawnch-plugin-${plugin-name}";
  version = "0.1.0-alpha";

  src = ./.;

  nativeBuildInputs = [ pkgs.cmake ];
  buildInputs = [
    api
    pkgs.nlohmann_json
  ];

  cmakeFlags = [
    "-DLawnchPluginApi_DIR=${api}/lib/cmake/LawnchPluginApi"
  ];

  installPhase = ''
    mkdir -p $out/share/lawnch/plugins/${plugin-name}
    cp *.so $out/share/lawnch/plugins/${plugin-name}/
    cp $src/pinfo $out/share/lawnch/plugins/${plugin-name}/pinfo

    mkdir -p $out/share/lawnch/plugins/${plugin-name}/assets
    cp $src/emoji.json $out/share/lawnch/plugins/${plugin-name}/assets/emoji.json
  '';
}
