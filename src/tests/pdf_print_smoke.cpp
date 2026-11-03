// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 rodioniurev
// All jobs in this test target temporary PDF files, never a physical printer.
#include "texstudio.h"
#include "configmanager.h"
#include "pdfviewer/PDFDocument.h"
#include "pdfviewer/pdfprinting.h"
#include <QtTest/QtTest>
#include <QPdfWriter>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QPrinterInfo>
#include <QContextMenuEvent>
#include <QMenu>
#include <QToolBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>
#include <memory>
#define CHECK(value) do { if (!(value)) { QTextStream(stderr) << "FAIL line " << __LINE__ << ": " << #value << '\n'; return 1; } } while (false)
static const QColor colors[] = {Qt::red, Qt::green, Qt::blue, Qt::cyan};
static bool checkTextPrinting(const QString &source, const QString &directory, int dpi)
{
    std::unique_ptr<Poppler::Document> input(Poppler::Document::load(source));
    if (!input || input->numPages() < 1) return false;
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(directory + "/text-output.pdf");
    printer.setPageSize(QPageSize(QPageSize::A4)); printer.setResolution(dpi);
    std::unique_ptr<Poppler::Page> original(input->page(0));
    const QSizeF size = original->pageSizeF();
    if (size.width() > size.height()) printer.setPageOrientation(QPageLayout::Landscape);
    printer.setPrintRange(QPrinter::CurrentPage);
    QString error;
    if (PdfPrinting::print(*input, printer, 0, error) != PdfPrinting::Result::Printed) return false;
    input->setRenderBackend(Poppler::Document::SplashBackend);
    const QImage reference = original->renderToImage(288, 288);
    std::unique_ptr<Poppler::Document> output(Poppler::Document::load(printer.outputFileName()));
    if (!output || reference.isNull()) return false;
    output->setRenderBackend(Poppler::Document::SplashBackend);
    std::unique_ptr<Poppler::Page> printed(output->page(0));
    const QImage actual = printed->renderToImage(144, 144);
    QImage expected(actual.size(), QImage::Format_RGB32); expected.fill(Qt::white);
    const QRectF points = printer.pageLayout().paintRectPoints();
    const QRectF area(points.topLeft()*2, points.size()*2);
    const qreal scale = qMin(area.width()/size.width(), area.height()/size.height());
    const QSizeF fitted = size * scale;
    QPainter painter(&expected); painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(QRectF(area.center() - QPointF(fitted.width()/2, fitted.height()/2), fitted), reference);
    painter.end();
    int ink = 0, matched = 0, actualInk = 0;
    for (int y=2; y<expected.height()-2; ++y) for (int x=2; x<expected.width()-2; ++x) {
        const QColor printedColor = actual.pixelColor(x,y);
        if (printedColor.red()<160 && printedColor.green()<160 && printedColor.blue()<160) ++actualInk;
        const QColor color = expected.pixelColor(x,y);
        if (color.red() >= 160 || color.green() >= 160 || color.blue() >= 160) continue;
        ++ink;
        bool found = false;
        for (int dy=-2; dy<=2 && !found; ++dy) for (int dx=-2; dx<=2 && !found; ++dx) {
            const QColor pixel = actual.pixelColor(x+dx,y+dy);
            found = pixel.red()<170 && pixel.green()<170 && pixel.blue()<170;
        }
        if (found) ++matched;
    }
    actual.save(directory + "/text-printed.png"); expected.save(directory + "/text-reference.png");
    QTextStream(stdout) << "Text ink match at " << dpi << " DPI: " << matched << '/' << ink << ", output ink: " << actualInk << '\n';
    return ink > 100 && matched >= ink * 0.95 && actualInk > ink * 0.75 && actualInk < ink * 1.25;
}
int main(int argc, char **argv)
{
    QApplication app(argc, argv); CHECK(argc >= 4);
    ConfigManager::configDirOverride = QString::fromLocal8Bit(argv[1]);
    ConfigManager::dontRestoreSession = true;
    Texstudio window;
    auto *config = dynamic_cast<ConfigManager *>(ConfigManagerInterface::getInstance()); CHECK(config);
    QTemporaryDir scratch; CHECK(scratch.isValid());
    const QString source = scratch.filePath(QString::fromUtf8("PDF test - пример.pdf"));
    {
        QPdfWriter writer(source); writer.setResolution(72); writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter paint(&writer); CHECK(paint.isActive());
        for (int page = 0; page < 4; ++page) {
            writer.setPageOrientation(page == 2 ? QPageLayout::Landscape : QPageLayout::Portrait);
            if (page) CHECK(writer.newPage());
            const QRect area(0, 0, writer.width(), writer.height());
            paint.fillRect(area, Qt::white);
            paint.fillRect(QRect(area.width()/4, area.height()/4, area.width()/2, area.height()/2), colors[page]);
            paint.setPen(Qt::black); paint.setFont(QFont("Arial", 20));
            paint.drawText(QPoint(30, 60), QString("PRINTMARKER%1").arg(page + 1));
        }
        CHECK(paint.end());
    }
    QFile file(source); CHECK(file.open(QIODevice::ReadOnly)); const QByteArray data = file.readAll(); file.close();
    QString error; int caseNumber = 0;
    auto checkPrint = [&](QPrinter::PrintRange range, const QList<int> &expected, int from = 1, int to = 4, int current = 0,
                          int copies = 1, bool collate = true, bool reverse = false, bool grayscale = false) {
        std::unique_ptr<Poppler::Document> input(Poppler::Document::loadFromData(data));
        QPrinter printer(QPrinter::HighResolution); printer.setOutputFormat(QPrinter::PdfFormat);
        const QString target = scratch.filePath(QString("output-%1.pdf").arg(++caseNumber));
        printer.setOutputFileName(target); printer.setResolution(144); printer.setPageSize(QPageSize(QPageSize::A4));
        printer.setFromTo(from, to); printer.setPrintRange(range); printer.setCopyCount(copies); printer.setCollateCopies(collate);
        printer.setPageOrder(reverse ? QPrinter::LastPageFirst : QPrinter::FirstPageFirst);
        printer.setColorMode(grayscale ? QPrinter::GrayScale : QPrinter::Color);
        const auto result = PdfPrinting::print(*input, printer, current, error);
        if (result != PdfPrinting::Result::Printed) { QTextStream(stderr) << error << '\n'; return false; }
        std::unique_ptr<Poppler::Document> output(Poppler::Document::load(target));
        if (!output || output->numPages() != expected.size()) { QTextStream(stderr) << "Page count: " << (output ? output->numPages() : -1) << " expected " << expected.size() << "\n"; return false; }
        for (int index = 0; index < expected.size(); ++index) {
            std::unique_ptr<Poppler::Page> page(output->page(index)); const QImage image = page->renderToImage(72, 72);
            if (image.isNull()) return false;
            const QColor actual = image.pixelColor(image.width()/2, image.height()/2);
            image.save(QString::fromLocal8Bit(argv[2]) + QString("/print-case-%1-page-%2.png").arg(caseNumber).arg(index));
            const QColor expectedColor = colors[expected.at(index) - 1];
            if (grayscale) { if (qAbs(actual.red()-actual.green())>2 || qAbs(actual.red()-actual.blue())>2) return false; }
            else if (qAbs(actual.red()-expectedColor.red())>10 || qAbs(actual.green()-expectedColor.green())>10 || qAbs(actual.blue()-expectedColor.blue())>10) { QTextStream(stderr) << "Center color " << actual.name() << " expected " << expectedColor.name() << "\n"; return false; }
        }
        return true;
    };
    CHECK(checkPrint(QPrinter::AllPages, {1,2,3,4}));
    CHECK(checkPrint(QPrinter::PageRange, {2,3}, 2, 3));
    CHECK(checkPrint(QPrinter::CurrentPage, {3}, 1, 4, 2));
    CHECK(checkPrint(QPrinter::AllPages, {4,3,2,1}, 1, 4, 0, 1, true, true));
    CHECK(checkPrint(QPrinter::PageRange, {2,3,2,3}, 2, 3, 0, 2, true));
    CHECK(checkPrint(QPrinter::PageRange, {2,2,3,3}, 2, 3, 0, 2, false));
    CHECK(checkPrint(QPrinter::AllPages, {1,2,3,4}, 1, 4, 0, 1, true, false, true));
    {
        std::unique_ptr<Poppler::Document> input(Poppler::Document::loadFromData(data));
        QPrinter printer; printer.setOutputFormat(QPrinter::PdfFormat); printer.setOutputFileName(scratch.filePath("cancelled.pdf"));
        CHECK(PdfPrinting::print(*input, printer, 0, error, [](int, int) { return false; }) == PdfPrinting::Result::Cancelled);
        CHECK(!QFile::exists(printer.outputFileName()));
        CHECK(PdfPrinting::print(*input, printer, 0, error, [](int done, int) { return done < 1; }) == PdfPrinting::Result::Cancelled);
        printer.setPrintRange(QPrinter::Selection);
        CHECK(PdfPrinting::print(*input, printer, 0, error) == PdfPrinting::Result::Failed);
        printer.setPrintRange(QPrinter::CurrentPage);
        CHECK(PdfPrinting::print(*input, printer, 99, error) == PdfPrinting::Result::Failed);
        printer.setPrintRange(QPrinter::AllPages); printer.setOutputFileName(scratch.filePath("missing/output.pdf"));
        CHECK(PdfPrinting::print(*input, printer, 0, error) == PdfPrinting::Result::Failed);
    }
    QTextStream(stdout) << "PASS: PDF printing preserves all/range/current/reverse pages, collated/uncollated copies, color/grayscale, cancellation and error handling.\n";
    CHECK(argc >= 5);
    CHECK(checkTextPrinting(QString::fromLocal8Bit(argv[4]), QString::fromLocal8Bit(argv[2]), 300));
    CHECK(checkTextPrinting(QString::fromLocal8Bit(argv[4]), QString::fromLocal8Bit(argv[2]), 1200));
    QTextStream(stdout) << "PASS: printed text and mathematics match the Splash reference at 300 and 1200 DPI.\n";
    {
        PDFDocument viewer(config->pdfDocumentConfig, true);
        QAction *action = viewer.printAction(); CHECK(action && !action->isEnabled());
        CHECK(!action->icon().isNull() && action->objectName() == "pdf/file/print");
        CHECK(action->shortcutContext() == Qt::WidgetWithChildrenShortcut);
        viewer.loadFile(source, QFileInfo(), PDFDocument::NoDisplayFlags); CHECK(action->isEnabled());
        bool toolbar = false;
        for (auto *bar : viewer.findChildren<QToolBar *>()) if (bar->actions().contains(action)) toolbar = true;
        CHECK(toolbar);
        viewer.resize(850, 700); viewer.show(); app.processEvents();
        bool contextHasPrint = false;
        QTimer::singleShot(0, [&] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (menu) {
                contextHasPrint = menu->actions().contains(action) && action->isEnabled();
                menu->grab().save(QString::fromLocal8Bit(argv[2]) + "/pdf-print-context.png"); menu->close();
            }
        });
        QContextMenuEvent context(QContextMenuEvent::Mouse, QPoint(60,60), viewer.widget()->mapToGlobal(QPoint(60,60)));
        QApplication::sendEvent(viewer.widget(), &context); CHECK(contextHasPrint);
        int externalJobs = 0; QObject::connect(&viewer, &PDFDocument::runCommandAsync, [&] { ++externalJobs; });
        bool cancelledDialog = false, disabledDuringPrint = false;
        QObject::connect(action, &QAction::changed, [&] { if (!action->isEnabled()) disabledDuringPrint = true; });
        QTimer cancel;
        QObject::connect(&cancel, &QTimer::timeout, [&] {
            for (auto *dialog : viewer.findChildren<QPrintDialog *>()) { cancelledDialog = true; dialog->reject(); }
        });
        cancel.start(50); action->trigger(); cancel.stop();
        CHECK(disabledDuringPrint && action->isEnabled() && externalJobs == 0);
        CHECK(cancelledDialog || QGuiApplication::platformName() == "offscreen");
        if (!cancelledDialog) QTextStream(stdout) << "INFO: Native Windows print dialogs cannot be displayed by the offscreen test platform.\n";
        QTextStream(stdout) << "PASS: embedded PDF toolbar/context action reaches native printing and restores state without external commands.\n";
    }
    {
        QTranslator translation; CHECK(translation.load(QString::fromLocal8Bit(argv[3]))); app.installTranslator(&translation);
        PDFDocument viewer(config->pdfDocumentConfig, false); viewer.loadFile(source, QFileInfo(), PDFDocument::NoDisplayFlags);
        CHECK(viewer.printAction()->isEnabled());
        CHECK(viewer.printAction()->text() == QString::fromUtf8("Печать PDF..."));
        auto *menu = viewer.findChild<QMenu *>("pdf/file"); CHECK(menu && menu->actions().contains(viewer.printAction()));
        QTextStream(stdout) << "PASS: windowed viewer exposes the same print action with its Russian translation.\n";
    }
    return 0;
}
