{
  description = "Foptim";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem
      (system:
        let
          overlays = [];
          pkgs = import nixpkgs {
            inherit system overlays;
          };
          nativeBuildInputs = with pkgs; [
            # IMPORTANT CLANG TOOLS AT START
            llvmPackages_20.clang-tools

            #build tools
            cmake
            ninja
            llvmPackages_20.clang

            #for testing stuff not real dependencies
            cmakeCurses
            graphviz
            nnd
            gdb
          ];

          buildInputs = [ ];
        in
        with pkgs;
        {
          devShells.default = mkShell {
            inherit buildInputs nativeBuildInputs;
            shellHook = ''
                export NIX_ENFORCE_NO_NATIVE=0
            '';
          };
        }
      );
}
