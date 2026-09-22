{
  description = "CFAT File System: a FUSE-based FAT32-like filesystem";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
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
        pkgs = import nixpkgs { inherit system; };
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "cfs";
          version = "0.1.0";

          src = ./.;

          nativeBuildInputs = [ pkgs.pkg-config ];
          buildInputs = [ pkgs.fuse ];

          installPhase = ''
            mkdir -p $out/bin
            cp cfs $out/bin/
          '';
        };

        devShells.default = pkgs.mkShell {
          packages = [
            pkgs.gcc
            pkgs.gnumake
            pkgs.pkg-config
            pkgs.fuse
          ];
        };

        # `nix shell` uses this by default
        legacyPackages = pkgs;
      }
    );
}
