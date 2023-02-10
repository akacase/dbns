{
  description = "dbns";

  inputs = {
    nixpkgs.url = github:NixOS/nixpkgs/nixos-22.05;
    utils.url = "github:numtide/flake-utils";
  };


  outputs = { self, nixpkgs, dns-compliance-testing-src, utils }: utils.lib.eachDefaultSystem
    (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [ self.overlay ];
        };
        gcc_musl = with nixpkgs; pkgs.wrapCCWith {
        cc = pkgs.gcc.cc;
          bintools = pkgs.wrapBintoolsWith {
            bintools = pkgs.binutils-unwrapped;
            libc = pkgs.musl;
          };
        };
        dbns = with pkgs; (overrideCC stdenv gcc_musl).mkDerivation {
          name = "dbns";
          src = ./src;
          nativeBuildInputs = [ clang zlib ninja ];
          buildInputs = [ openssl.dev ];

          buildPhase = ''
            ninja
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp ./src/dbnsd $out/bin/dbns
          '';

          meta = with lib; {
            description = "DNS protocol compliance of the servers they are delegating zones to.";
            homepage = https://gitlab.isc.org/isc-projects/DNS-Compliance-Testing;
            license = licenses.mpl20;
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
            clang
            zlib
            ninja
          ];
        };
      }) // {
    overlay = final: prev: {
      dbns = with final; (stdenv.mkDerivation {
        name = "dbns";
        src = ./src
        nativeBuildInputs = [ clang zlib ninja ];

        buildPhase = ''
          ninja
        '';

        installPhase = ''
          mkdir -p $out/bin
          cp ./src/dbnsd $out/bin/dbnsd
        '';

        meta = with lib; {
          description = "Dragonball MUD";
          homepage = https://capsulecorp.org;
          license = licenses.agpl;
          maintainers = with maintainers; [ case ];
        };
      });
    };
  };
}