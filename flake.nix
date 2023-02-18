{
  description = "dbns";

  inputs = {
    nixpkgs.url = github:NixOS/nixpkgs/nixos-22.11;
    utils.url = "github:numtide/flake-utils";
  };


  outputs = { self, nixpkgs, utils }: utils.lib.eachDefaultSystem
    (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [ self.overlay ];
        };
        dbns = with pkgs; stdenv.mkDerivation {
          name = "dbns";
          src = ./src;
          nativeBuildInputs = [ gnumake clang zlib ninja ];

          buildPhase = ''
            ninja
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp dbnsd $out/bin/
          '';

          meta = with lib; {
            description = "Dragonball MUD";
            homepage = https://capsulecorp.org;
            license = licenses.agpl3;
            maintainers = with maintainers; [ case ];
          };
        };
      in
      rec
      {
        packages.${system} = dbns;
        defaultPackage = dbns;
        devShell = pkgs.mkShell {
          buildInputs = with pkgs; [
            gnumake
            clang
            zlib
            ninja
          ];
        };
      }) // {
    overlay = final: prev: {
      dbns = with final; (stdenv.mkDerivation {
        name = "dbns";
        src = ./src;
        nativeBuildInputs = [ gnumake clang zlib ninja ];

        buildPhase = ''
          ninja
        '';

        installPhase = ''
          mkdir -p $out/bin
          cp dbnsd $out/bin/
        '';

        meta = with lib; {
          description = "Dragonball MUD";
          homepage = https://capsulecorp.org;
          license = licenses.agpl3;
          maintainers = with maintainers; [ case ];
        };
      });
    };
  };
}