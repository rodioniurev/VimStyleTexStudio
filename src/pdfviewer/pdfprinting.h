// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 rodioniurev
#ifndef Header_Pdf_Printing
#define Header_Pdf_Printing
#include <QString>
#include <functional>
class QPrinter;
namespace Poppler { class Document; }
namespace PdfPrinting {
enum class Result { Printed, Cancelled, Failed };
// currentPage is zero-based. The callback returns false to cancel.
Result print(Poppler::Document &document, QPrinter &printer, int currentPage,
             QString &error, const std::function<bool(int, int)> &progress = {});
}
#endif
