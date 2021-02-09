with import <nixpkgs> {};

stdenv.mkDerivation {
  name = "dbnsd";
  src = ./src;

  buildInputs = [ clang ninja ];

  buildPhase = "ninja";

  installPhase = ''
    mkdir -p $out/bin
    cp dbnsd $out/bin/
  '';
}
