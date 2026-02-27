{
  pkgs,
  lawnch-plugin-api,
  ...
}:

let
  api = lawnch-plugin-api;
  plugin-name = "google";
in
pkgs.stdenv.mkDerivation {
  pname = "lawnch-plugin-${plugin-name}";
  version = "0.1.0-alpha";

  src = ./.;

  nativeBuildInputs = [ pkgs.cmake ];
  buildInputs = [
    api
    pkgs.curl
  ];

  cmakeFlags = [
    "-DLawnchPluginApi_DIR=${api}/lib/cmake/LawnchPluginApi"
  ];

  installPhase = ''
    mkdir -p $out/share/lawnch/plugins/${plugin-name}
    cp *.so $out/share/lawnch/plugins/${plugin-name}/
    cp $src/pinfo $out/share/lawnch/plugins/${plugin-name}/pinfo
  '';
}
