#!/usr/bin/env python3
"""
Integration-test adapter for Samurai encoding with twoSixIndirect transport.

Addressing model (LD_CREATOR_TO_LOADER, "each node creates its own SEND hashtag"):
  * every node owns one hashtag it posts into:  H(role) = twoSixIndirect-<scenario>-<role>-send
  * a node receives by loading its PEER's hashtag: listener loads H(connector) and vice versa
  * names are deterministic, so no address has to be passed between nodes (no peer_context)

Scenario modes handled (race-cli flags in parentheses):
  send            connector sends, listener receives            (--send / --recv)
  send-recv       connector request, listener reply             (--send-recv / --recv-reply)
  client-connect  persistent socket proxy, both directions      (--client-connect / --server-connect)
"""

import json
import platform
import sys
from pathlib import Path

try:
    # generate_scenario.py puts raceboat/test/integration on sys.path before loading adapters.
    from adapter_types import NodeContribution, NodeRequest
except ImportError:
    # Standalone use: find a raceboat checkout in any parent directory of this plugin
    _integration_dir = next(
        (p / "raceboat" / "test" / "integration" for p in Path(__file__).resolve().parents
         if (p / "raceboat" / "test" / "integration").is_dir()), None)
    if _integration_dir is None:
        raise ImportError("adapter: could not find raceboat/test/integration") from None
    sys.path.insert(0, str(_integration_dir))
    from adapter_types import NodeContribution, NodeRequest  # noqa: E402

# Must match source/manifest.json ("compositions[].id")
DEFAULT_COMPOSITION = "samuraiTwoSixIndirectComposition"
KIT_NAME = "PluginSamuraiEncoding"

# twoSixIndirect is backed by a shared whiteboard
WHITEBOARD_HOSTNAME = "twosix-whiteboard"
WHITEBOARD_PORT = 5000

SIDECAR_SERVICES = {
    "twosix-redis": {
        "image": "redis:6.0.6",
        "healthcheck": {
            "test": ["CMD", "redis-cli", "ping"],
            "interval": "2s",
            "timeout": "2s",
            "retries": 15,
        },
    },
    "twosix-whiteboard": {
        "image": "ghcr.io/tst-race/race-core/twosix-whiteboard:main",
        "command": "-w 8",
        "hostname": WHITEBOARD_HOSTNAME,
        "depends_on": {"twosix-redis": {"condition": "service_healthy"}},
        "environment": {"REDIS_HOSTNAME": "twosix-redis"},
        "healthcheck": {
            "test": [
                "CMD",
                "python3",
                "-c",
                "import socket; socket.create_connection(('127.0.0.1', 5000), 2).close()",
            ],
            "interval": "2s",
            "timeout": "2s",
            "retries": 15,
        },
    },
}


def _detect_host_architecture() -> str:
    machine = platform.machine().lower()
    if machine in {"x86_64", "amd64"}:
        return "x86_64"
    if machine in {"arm64", "aarch64"}:
        return "arm64-v8a"
    raise ValueError(f"Unsupported host architecture: {platform.machine()}")


def kit_dir(role: str) -> Path:
    """Called with no scenario context, only to read this kit's manifest.json."""
    side = "client" if role == "connector" else "server"
    plugin_root = Path(__file__).resolve().parents[1]
    return (plugin_root / "kit" / "artifacts"
            / f"linux-{_detect_host_architecture()}-{side}" / KIT_NAME)


def hashtag_name(scenario_id: str, role: str) -> str:
    """The hashtag `role` posts into. Both nodes compute every name deterministically."""
    safe = scenario_id.replace(".", "-")
    return f"twoSixIndirect-{safe}-{role}-send"


def hashtag_address(name: str) -> str:
    """JSON text of a link address; must parse with source/transport/LinkAddress.cpp."""
    return json.dumps({"hashtag": name, "hostname": WHITEBOARD_HOSTNAME, "port": WHITEBOARD_PORT})


def action_file(mode: str, role: str) -> str:
    """Absolute path (inside the container) of the user-model schedule for this node.

    The file-based user model accepts absolute paths (starting with '/'). For send-only mode:
    - connector (sender) uses post-only actions from decomposed-exemplars kit
    - listener (receiver) uses fetch-only actions from THIS plugin's kit
    """
    decomposed_kit = "PluginCommsTwoSixStubDecomposed"
    samurai_kit = KIT_NAME
    if mode == "send":
        if role == "connector":
            # Connector posts: use post-only actions from decomposed-exemplars
            return f"/kits/{decomposed_kit}/config/actions-send-only.json"
        else:
            # Listener fetches: use fetch-only actions from samurai-encoding kit
            return f"/kits/{samurai_kit}/config/actions-fetch-only.json"
    else:
        # Bidirectional modes: use combined actions from decomposed-exemplars
        return f"/kits/{decomposed_kit}/config/actions-combined.json"


def generate_node_contribution(request: NodeRequest) -> NodeContribution:
    mode = request.scenario_mode
    if mode not in ("send", "send-recv", "client-connect"):
        raise ValueError(f"samurai-encoding adapter does not support scenario mode '{mode}'")
    scenario_id = request.scenario_id or "samurai-scenario"
    composition = request.composition_name or DEFAULT_COMPOSITION
    peer_role = "connector" if request.role == "listener" else "listener"

    # Which directions this node participates in for this mode.
    wants_send = mode != "send" or request.role == "connector"
    wants_recv = mode != "send" or request.role == "listener"
    # request.direction is None when ONE contribution serves both directions, or "send"/"recv"
    # when the scenario split the slot across two channels; then fill only that direction.
    wants_send &= request.direction in (None, "send")
    wants_recv &= request.direction in (None, "recv")

    contribution = NodeContribution(
        params={
            f"{composition}.save-encoded-output": "true",
            f"{composition}.actionFile": action_file(mode, request.role),
        },
        channel_name=composition,
        kit_dir=kit_dir(request.role),
        sidecar_services=dict(SIDECAR_SERVICES),
    )

    # Encoding parameters for Samurai steganography - passed as environment variables
    contribution.environment["SAMURAI_PASSWORD"] = "test_password_123"
    contribution.environment["SAMURAI_LEVEL"] = "low"
    contribution.environment["SAMURAI_CARRIER_WIDTH"] = "512"
    contribution.environment["SAMURAI_CARRIER_HEIGHT"] = "512"
    contribution.environment["SAMURAI_CARRIER_TYPE"] = "noise"
    contribution.environment["SAMURAI_SAVE_ENCODED_OUTPUT"] = "true"

    if wants_send:
        # This node POSTs to its own hashtag
        contribution.cli_flags["send-address"] = hashtag_address(hashtag_name(scenario_id, request.role))
    if wants_recv:
        # This node FETCHes from peer's hashtag
        contribution.cli_flags["recv-address"] = hashtag_address(hashtag_name(scenario_id, peer_role))

    return contribution