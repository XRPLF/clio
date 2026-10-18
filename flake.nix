{
  description = "Nix related things for Clio";

  inputs = {
    # Clio is built with the same toolchain as xrpld, so its dev shells are reused as is.
    # Keep the revision in sync with the `ghcr.io/xrplf/xrpld/nix-ubuntu` image used in CI,
    # so the local shell matches CI.
    # Its nixpkgs is deliberately not overridden with `follows`:
    # that would change the toolchain and lose the match with CI.
    xrpld.url = "github:XRPLF/rippled/de5053ae0d8905d8abfa6c59764774aeb2cef801";
  };

  outputs =
    { xrpld, ... }:
    {
      inherit (xrpld) devShells formatter;
    };
}
