// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 rodioniurev
#include "pdfprinting.h"
#include <QCoreApplication>
#include <QPainter>
#include <QPicture>
#include <QPrinter>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#if QT_VERSION_MAJOR > 5
#include "poppler-qt6.h"
#else
#include "poppler-qt5.h"
#endif

namespace {
QString message(const char *text) { return QCoreApplication::translate("PDFDocument", text); }
}

PdfPrinting::Result PdfPrinting::print(Poppler::Document &document, QPrinter &printer, int currentPage,
                                     QString &error, const std::function<bool(int, int)> &progress)
{
    error.clear();
    if (document.isLocked() || !document.okToPrint()) {
        error = message("Printing is not permitted for this PDF."); return Result::Failed;
    }
    QVector<int> pages;
    for (int page = 0; page < document.numPages(); ++page) {
        bool selected = printer.printRange() == QPrinter::AllPages;
        if (printer.printRange() == QPrinter::CurrentPage) selected = page == currentPage;
        if (printer.printRange() == QPrinter::PageRange) {
#if QT_VERSION_MAJOR > 5
            selected = printer.pageRanges().contains(page + 1);
#else
            selected = page + 1 >= printer.fromPage() && page + 1 <= printer.toPage();
#endif
        }
        if (selected) pages.append(page);
    }
    if (pages.isEmpty()) { error = message("No pages selected for printing."); return Result::Failed; }
    if (printer.pageOrder() == QPrinter::LastPageFirst) std::reverse(pages.begin(), pages.end());
    const int copies = printer.supportsMultipleCopies() ? 1 : qMax(1, printer.copyCount());
    const qint64 count = qint64(pages.size()) * copies;
    if (count > std::numeric_limits<int>::max()) { error = message("Too many pages selected for printing."); return Result::Failed; }
    const int total = int(count);
    if (progress && !progress(0, total)) return Result::Cancelled;

    const bool highResolution = document.okToPrintHighRes();
    const bool vectorOutput = highResolution && Poppler::Document::availableRenderBackends().contains(Poppler::Document::QPainterBackend);
    document.setRenderBackend(vectorOutput ? Poppler::Document::QPainterBackend : Poppler::Document::SplashBackend);
    document.setRenderHint(Poppler::Document::Antialiasing, true);
    document.setRenderHint(Poppler::Document::TextAntialiasing, true);
    printer.setFullPage(false);
    QPainter painter;
    if (!painter.begin(&printer)) { error = message("Could not start the print job."); return Result::Failed; }
    auto fail = [&](const QString &reason) {
        error = reason; printer.abort(); painter.end(); return Result::Failed;
    };
    for (int index = 0; index < total; ++index) {
        if (progress && !progress(index, total)) { printer.abort(); painter.end(); return Result::Cancelled; }
        if (index > 0 && !printer.newPage()) return fail(message("Could not create the next printed page."));
        const int pageNumber = pages.at(printer.collateCopies() ? index % pages.size() : index / copies);
        std::unique_ptr<Poppler::Page> page(document.page(pageNumber));
        if (!page) return fail(message("Could not render PDF page %1.").arg(pageNumber + 1));
        const QSizeF size = page->pageSizeF();
        if (!(size.width() > 0 && size.height() > 0) || !std::isfinite(size.width()) || !std::isfinite(size.height()))
            return fail(message("Could not render PDF page %1.").arg(pageNumber + 1));
        // With fullPage=false the painter origin is already the printable area's origin.
        const QSize area = printer.pageLayout().paintRectPixels(printer.resolution()).size();
        if (area.isEmpty()) return fail(message("The printer has no printable page area."));
        const qreal scale = qMin(area.width() / size.width(), area.height() / size.height());
        painter.save();
        painter.translate((area.width() - size.width() * scale) / 2, (area.height() - size.height() * scale) / 2);
        painter.scale(scale, scale);
        bool rendered = false;
        if (vectorOutput) {
            // Poppler replaces its painter's world transform. Record first so
            // fitting/centering remains under the print painter's control.
            QPicture picture;
            QPainter recording(&picture);
            rendered = page->renderToPainter(&recording, 72, 72);
            recording.end();
            if (rendered) {
                painter.scale(qreal(picture.logicalDpiX()) / printer.logicalDpiX(),
                              qreal(picture.logicalDpiY()) / printer.logicalDpiY());
                painter.drawPicture(QPointF(), picture);
            }
        }
        else {
            // Limit fallback memory to one 24-megapixel page, including oversized PDFs.
            const double dpi = qMin(double(highResolution ? qMin(600, qMax(300, printer.resolution())) : 150),
                                    72.0 * std::sqrt(24000000.0 / (size.width() * size.height())));
            const QImage image = page->renderToImage(dpi, dpi);
            if (!image.isNull()) { painter.drawImage(QRectF(QPointF(), size), image); rendered = true; }
        }
        painter.restore();
        if (!rendered) return fail(message("Could not render PDF page %1.").arg(pageNumber + 1));
        if (printer.printerState() == QPrinter::Error) return fail(message("The printer reported an error."));
    }
    if (!painter.end()) { error = message("Could not finish the print job."); return Result::Failed; }
    if (progress) progress(total, total);
    return Result::Printed;
}
