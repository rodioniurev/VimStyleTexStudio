# Printing compiled PDF documents

After Build & View, right-click a page in the built-in PDF viewer and choose
**Print PDF...**. The viewer also has a printer button on its toolbar. In a
separate viewer window, the command is available under **File > Print PDF...**.
Ctrl+P invokes it when the PDF viewer has focus. The source editor's separate
**Print Source Code...** command is unchanged.

The system print dialog selects the printer, all pages/a page range/the current
page, copies and printer properties. Pages fit inside the selected paper's
printable area, preserving proportions. Copy collation, reverse page order,
color/grayscale and duplex settings are passed through the Qt printer API.
The print job uses the PDF's original colors, independent of dark/inverted
preview colors. It does not compile the source automatically.

A separate in-memory snapshot is taken from the actual viewer filename before
the dialog opens. Recompilation can replace that file without invalidating the
print job. Printing uses Qt PrintSupport and the bundled Poppler library; it
does not require Ghostscript or an external PDF application.

Pages are rendered with Poppler's Splash backend before they reach the printer,
so embedded Type 1 fonts do not depend on native printer font support. Output
is rasterized at 300-600 DPI, capped at 24 megapixels per page (about 500 DPI
on A4). Oversized pages use lower resolution to stay within this limit; PDFs
that allow only low-resolution printing are limited to 150 DPI. Printed PDF
copies contain page images rather than selectable text. Password-protected PDFs may ask
for the password again; printing permissions are respected. Errors are shown
in the application, and long jobs can be cancelled between pages.

## Existing upstream discussions

- [#201: Be able to print from the internal viewer](https://github.com/texstudio-org/texstudio/issues/201)
  remains open as checked on 2026-11-03. Maintainers explained that the old
  Windows implementation depended on Ghostscript and was disabled because of
  reliability and maintenance concerns. The fork implements a native printer
  path rather than re-enabling that legacy external-command handler.
- [#3710: Printing document prints the codebase instead of the compiled document](https://github.com/texstudio-org/texstudio/issues/3710)
  is closed. Its discussion distinguishes printing source code from printing
  the compiled PDF; the default source-print shortcut was removed upstream.
  The new action is explicitly named Print PDF and its embedded-viewer shortcut
  is limited to the viewer's focus scope.

No comments or pull requests were posted to these upstream discussions.

## Verification

```powershell
python scripts/vimstyle/test_pdf_print_windows.py C:/msys64/ucrt64 . build-windows
```

The isolated check generates a four-page PDF with colored markers and mixed
orientations, prints to temporary PDF files, and inspects the rendered output.
It covers page selection/order, copies, grayscale, cancellation and failures.
A separate LaTeX fixture with embedded Type 1 fonts checks the visible text,
Cyrillic and mathematics against a Splash reference at 300 and 1200 printer DPI,
including missing glyphs and excess ink from distorted font rendering.
It also checks embedded/windowed viewer menus, toolbar integration, shortcut
scope and the Russian label. Physical printers are never used. Native Windows
dialogs cannot be displayed by the offscreen test platform; their entry point
and state restoration are checked there.

For a local document check, set `TXS_PRINT_TEST_PDF` to an existing PDF before
running the script. Its contents and screenshots must remain local and must
not be included in published test artifacts.
