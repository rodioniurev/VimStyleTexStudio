# AI editing with review and undo

Select source text and press the **Edit with AI** toolbar button, choose the
same action directly in the source editor's right-click menu, or press
**Ctrl+Alt+Enter**. Write what should change in the compact prompt window.
The existing AI Chat dialog remains available separately.

## Workflow

1. Set up the provider once under **Options > Configure TeXstudio > AI Editing**.
   Choose a provider/model, enter its key if required, and use **Check connection /
   load models** to query availability without sending document text. Press **OK**.
2. Select a fragment in the editor, then open **Edit with AI**. With no selection,
   the action targets the current document. Type a natural-language instruction.
3. Leave **Allow related changes elsewhere in this document** enabled when the
   task needs definitions or usages outside the selection. This sends the whole
   current file as context and permits related edits there. Turn it off to limit
   changes to the selection, with the preamble supplied as read-only context.
4. Press **Send** or **Ctrl+Enter**. Nothing is applied while generating.
5. Review **Original** and **Proposed replacement** after the response arrives.
   Related edits highlight changed original/result lines and list original line
   numbers. The proposed result remains editable. Press **Apply changes** when
   satisfied, then use Build & View to inspect the compiled PDF. Changes remain
   unsaved and one Undo restores the original document.

The **AI settings...** button opens the same Configuration page. Provider, model,
keys and limits are kept out of the everyday prompt window. Color and Beamer
presets remain available under **Wizards > AI Editing** and prefill an editable
instruction.

For example, select the hardcoded month name and ask:

> Introduce \newcommand{\lang}{es}. Derive \calMonthName from \calMonth
> (1–12) and \lang (es/en). Make all weekday names depend on \lang too.
> Preserve the calendar structure and update related definitions where needed.

Keep related changes enabled for this task. The model returns exact source
fragments to replace; missing, ambiguous or overlapping fragments are rejected.
It can propose changes in the current file's preamble, including dependencies.

Closing the dialog or stopping a request leaves the document unchanged. A
response cannot be applied if the original editor was closed, became read-only,
or its document changed after opening the dialog. Empty, malformed and visibly
truncated responses are rejected rather than inserted. The raw provider text is
available on the **Provider response** tab for diagnosis.

## Providers and availability

| Provider | Endpoint / prerequisite | Authentication |
| --- | --- | --- |
| Ollama | `http://127.0.0.1:11434/api/chat` | None for the local server |
| OpenAI API | `https://api.openai.com/v1/chat/completions` | OpenAI API key / `OPENAI_API_KEY` |
| Anthropic Claude | `https://api.anthropic.com/v1/messages` | Anthropic API key / `ANTHROPIC_API_KEY` |
| OpenAI-compatible / LM Studio | e.g. `http://127.0.0.1:1234/v1/chat/completions` | Server-dependent |
| GitHub Copilot SDK | Python 3.11+ with the official SDK and runtime | GitHub Copilot authentication or `COPILOT_GITHUB_TOKEN` |

Model IDs are editable and retrieved from the provider, rather than a fixed list
of models that may become obsolete. A listed model must support chat/text output;
an account's list can also contain models unsuitable for this task. Availability
and quotas depend on the account. An OpenAI API key is distinct from signing in
to the ChatGPT application. Claude and Copilot likewise require the appropriate
API/account entitlement; a key from one provider does not authenticate another.

Keys entered in Configuration are password-masked. By default they are kept
only in process memory; environment variables are also supported. On Windows,
**Remember key securely for this Windows account** opts into DPAPI-encrypted
storage in `ai-edit.ini`. This encrypted value is tied to the Windows account;
it is not a portable plaintext key. Cancel in Configuration discards changes.
Provider, endpoint, model and limits are saved in the same file. The existing
AI Chat feature has separate settings and persistence.

The new editing workflow exposes no editor tools or executable macros to HTTP
models. Copilot runs through an embedded Python bridge in an isolated temporary
directory with an empty client, no tools, disabled configuration discovery,
file hooks, skills, session storage and host Git operations. It returns text
for review; the Qt application performs the explicit replacement.

### Copilot setup

Use a dedicated Python environment if desired:

```powershell
python -m venv "$env:LOCALAPPDATA\Programs\TeXstudio-AI-Python"
& "$env:LOCALAPPDATA\Programs\TeXstudio-AI-Python\Scripts\python.exe" -m pip install github-copilot-sdk==1.0.28
& "$env:LOCALAPPDATA\Programs\TeXstudio-AI-Python\Scripts\python.exe" -m copilot download-runtime
```

Choose that environment's full `python.exe` path in the **Endpoint / Python
executable** field for Copilot. The SDK can download its pinned runtime on first
use if it has not been provisioned. Authenticate with GitHub Copilot or enter a
GitHub token that has Copilot access. This integration uses the official SDK;
it does not reuse an undocumented OpenAI-compatible Copilot endpoint. A GitHub
Models token/endpoint is a separate service and is not presented as Copilot.

The official authentication documentation supports fine-grained personal access
tokens (`github_pat_`) and OAuth/app user tokens (`gho_`, `ghu_`); classic `ghp_`
personal tokens are not supported. A successful GitHub sign-in alone does not
establish access to Copilot model requests. HTTP 403 from model discovery means
the current account/token or organization policy does not allow the request.
OpenAI and Claude keys are entered for their own providers.

## Local model choices

For a Windows laptop with 16 GB RAM and an RTX 3060 Laptop GPU with 6 GB VRAM,
`qwen3:4b-instruct` (Q4_K_M, approximately 2.5 GB model download) is a reasonable
starting choice that leaves more memory for context. This describes memory
fit, not a guarantee of LaTeX editing quality. In local checks it completed
small color and Beamer tasks, but a month/weekday-language task degenerated
into repeated TeX conditionals and timed out without changing the document.
Review every proposal; complex transformations may need a stronger model.

```powershell
ollama pull qwen3:4b-instruct
```

`qwen2.5-coder:7b` (Q4_K_M, approximately 4.7 GB download) is another option for
source editing, with tighter GPU memory headroom. An existing `qwen2.5:7b` can
also be used if enough memory is available.
Model weights, context/KV cache and runtime buffers all use memory. Large
contexts may cause CPU offloading and slower generation on this hardware.

Ollama context tokens and output tokens are adjustable. The context must fit
both the source and the generated replacement. For short selections start with
4096-8192 context tokens. Whole-document rewrites need more context and output
space; split large documents into selections when necessary. The defaults are
8192 context tokens and a 4096-token output limit. Copilot uses SDK/model
limits instead of these HTTP-provider output settings.

## Scope and limitations

- Source text is sent, not PDF pages or screenshots. Models do not inspect the
  actual rendered layout. Recompile and review the PDF after applying.
- Related changes send the whole current file; selection-only edits send the
  selection and preamble. Included files, images and bibliographies are not
  automatically read or modified.
- With related changes off, a selection edit cannot alter package imports
  elsewhere. Enable related changes when the task needs preamble edits.
- Model-generated colors, layouts, content preservation and compilability need
  review. Source structure checks do not prove semantic correctness.
- Generation is cancellable. Ollama streams progress and received character
  counts; other providers return the complete response. Request timeout is
  configurable from 30 to 1800 seconds, default 600; model discovery has a
  one-minute limit. A slow CPU model can take several minutes. Input is limited to one
  million characters of serialized source/context, responses to 4 MB for HTTP.
- Endpoint redirects are not followed automatically. API keys require HTTPS
  for remote endpoints; loopback development endpoints can use HTTP.

## Upstream discussions checked on 2026-11-04

- [#4355: AI Chat Feature Expansion](https://github.com/texstudio-org/texstudio/issues/4355)
  is open and asks for better context and interaction with local models.
- [#3652: Add Github Copilot](https://github.com/texstudio-org/texstudio/issues/3652)
  is open. The 2024 discussion noted the lack of a usable integration API;
  the official Copilot SDK now provides a supported integration route.
- [#4385: Anthropic API Support](https://github.com/texstudio-org/texstudio/issues/4385)
  is closed; Claude support exists upstream. This editing workflow sends the
  native Messages payload with `system` and `max_tokens` serialized correctly.
- [#4054: Selected text presentation](https://github.com/texstudio-org/texstudio/issues/4054),
  [#3807: Custom endpoint model discovery](https://github.com/texstudio-org/texstudio/issues/3807),
  [#4130: Empty keys with local models](https://github.com/texstudio-org/texstudio/issues/4130)
  are related closed discussions addressed by explicit scope and endpoint-based
  model discovery in the new workflow.

No upstream comments, pull requests or releases were posted for this preview.
Publication requires the user's test and explicit confirmation.

## Official integration references

- [OpenAI API authentication](https://developers.openai.com/api/reference/overview)
- [Claude Messages API](https://platform.claude.com/docs/en/api/messages/create)
- [Ollama chat API](https://docs.ollama.com/api/chat)
- [LM Studio compatibility endpoints](https://lmstudio.ai/docs/developer/openai-compat)
- [Official Copilot Python SDK](https://github.com/github/copilot-sdk/tree/main/python)
- [Copilot authentication](https://docs.github.com/en/copilot/how-tos/copilot-sdk/auth)
- [Qwen3 4B Instruct model](https://ollama.com/library/qwen3:4b-instruct)
- [Qwen2.5 Coder 7B model](https://ollama.com/library/qwen2.5-coder:7b)

## Verification commands

```powershell
python scripts/vimstyle/test_ai_edit_windows.py C:/msys64/ucrt64 . build-windows
python scripts/vimstyle/test_copilot_bridge.py  # in an environment with the SDK
```

Mock HTTP checks cover the quick prompt, direct context action, toolbar button,
Configuration page, provider formats, model discovery, related edits and scope
restrictions, explicit application/undo, stale documents, truncated/malformed
replies, complete/incomplete Ollama streams, cancellation and encrypted key
persistence/restoration. Regional Russian catalog lookup is checked separately. `TXS_TEST_LIVE_OLLAMA=1`
enables synthetic color, Beamer and language-dependent calendar edits with local `qwen3:4b-instruct` (override
with `TXS_TEST_OLLAMA_MODEL`);
`TXS_TEST_LATEX` can specify pdflatex and `TXS_TEST_PDFTOTEXT` pdftotext for
compilation and language/month output checks. No private document
is used by these tests. Cloud account/model access requires the user's own key
and is not established by mock protocol tests.
