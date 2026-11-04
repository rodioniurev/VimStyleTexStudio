# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 rodioniurev
"""Offline bridge contract tests against the installed official SDK signatures."""
import asyncio
import importlib.util
import inspect
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
import unittest
import copilot

spec = importlib.util.spec_from_file_location("texstudio_copilot_bridge", Path(__file__).with_name("copilot_bridge.py"))
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)
real_client = copilot.CopilotClient
captured = {}


class Session:
    async def __aenter__(self): return self
    async def __aexit__(self, *_): pass
    async def send_and_wait(self, prompt, *, timeout):
        captured["prompt"] = prompt
        return SimpleNamespace(data=SimpleNamespace(content='{"replacement":"new","summary":"done"}'))


class Client:
    def __init__(self, **options):
        inspect.signature(real_client).bind(**options)
        captured["client"] = options
    async def __aenter__(self): return self
    async def __aexit__(self, *_): pass
    async def list_models(self): return [SimpleNamespace(id="test-model")]
    async def create_session(self, **config):
        inspect.signature(real_client.create_session).bind(None, **config)
        captured["session"] = config
        return Session()


class BridgeTests(unittest.IsolatedAsyncioTestCase):
    async def test_generation_has_no_tools_or_project_discovery(self):
        with patch.object(copilot, "CopilotClient", Client):
            result = await bridge.run({"operation":"generate", "model":"test-model", "system":"edit source", "prompt":"source text"})
        self.assertIn("replacement", result["text"])
        config = captured["session"]
        self.assertEqual(config["available_tools"], [])
        self.assertEqual(config["tools"], [])
        self.assertEqual(config["mcp_servers"], {})
        for flag in ("enable_skills", "enable_file_hooks", "enable_config_discovery", "enable_host_git_operations", "enable_session_store", "coauthor_enabled", "enable_file_change_tracking", "enable_session_telemetry"):
            self.assertIs(config[flag], False)
        self.assertEqual(config["on_permission_request"](None, None).kind, "user-not-available")
        self.assertEqual(captured["client"]["mode"], "empty")
        self.assertFalse(Path(captured["client"]["base_directory"]).exists())
    async def test_models_are_reported_without_sending_document(self):
        with patch.object(copilot, "CopilotClient", Client):
            self.assertEqual(await bridge.run({"operation":"models"}), {"data":[{"id":"test-model"}]})


if __name__ == "__main__": unittest.main()
