# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 rodioniurev
"""One request over stdin/stdout; document access stays in the Qt application."""
import asyncio
import json
import os
import sys
import tempfile


async def run(request):
    try:
        from copilot import CopilotClient
        from copilot.session import PermissionDecisionUserNotAvailable
    except ImportError:
        raise RuntimeError("Install Python 3.11+ and github-copilot-sdk>=1.0.28 in the selected Python environment, then provision its runtime with: python -m copilot download-runtime") from None
    with tempfile.TemporaryDirectory(prefix="texstudio-copilot-") as directory:
        options = dict(mode="empty", base_directory=directory, working_directory=directory)
        token = os.environ.get("COPILOT_GITHUB_TOKEN")
        if token:
            options.update(github_token=token, use_logged_in_user=False)
        async with CopilotClient(**options) as client:
            if request.get("operation") == "models":
                models = await client.list_models()
                return {"data": [{"id": item.id} for item in models]}
            config = dict(
                available_tools=[],
                tools=[],
                mcp_servers={},
                enable_skills=False,
                enable_file_hooks=False,
                enable_config_discovery=False,
                enable_host_git_operations=False,
                enable_session_store=False,
                coauthor_enabled=False,
                enable_file_change_tracking=False,
                enable_session_telemetry=False,
                working_directory=directory,
                on_permission_request=lambda *_: PermissionDecisionUserNotAvailable(),
                system_message={"mode": "append", "content": request["system"]},
            )
            if request.get("model"):
                config["model"] = request["model"]
            async with await client.create_session(**config) as session:
                event = await session.send_and_wait(request["prompt"], timeout=270)
                if event is None or not getattr(event.data, "content", None):
                    raise RuntimeError("Copilot returned no assistant text.")
                return {"text": event.data.content}


if __name__ == "__main__":
    try:
        data = sys.stdin.buffer.read(4_194_305)
        if len(data) > 4_194_304:
            raise RuntimeError("Copilot input exceeds 4 MB; use a smaller selection.")
        request = json.loads(data.decode("utf-8"))
        result = asyncio.run(run(request))
        print(json.dumps(result, ensure_ascii=True))
    except Exception as error:
        message = str(error)
        if "403" in message and ("authorized" in message or "Forbidden" in message):
            message = "Copilot access denied (HTTP 403). Check that the account/token has Copilot access and that organization policy allows this feature."
        for variable in ("COPILOT_GITHUB_TOKEN", "GH_TOKEN", "GITHUB_TOKEN"):
            token = os.environ.get(variable)
            if token:
                message = message.replace(token, "[redacted]")
        print(json.dumps({"error": message[:1200]}))
        sys.exit(1)
