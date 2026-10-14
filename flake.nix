{
  description = "Nix related things for Clio";

  inputs = {
    # Clio is built with the same toolchain as xrpld, so its dev shells are reused as is.
    # Keep the revision in sync with the `ghcr.io/xrplf/xrpld/nix-ubuntu` image used in CI,
    # so the local shell matches CI.
    # Its nixpkgs is deliberately not overridden with `follows`:
    # that would change the toolchain and lose the match with CI.
    xrpld.url = "github:XRPLF/rippled/ede8af8191d2bc9ff5c59b6a26d7b84e79b9ea94";
  };

  outputs =
    { xrpld, ... }:
    {
      inherit (xrpld) devShells formatter;
    };
}
