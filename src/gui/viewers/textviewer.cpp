#include "textviewer.h"

#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QScrollBar>
#include <QStringDecoder>

#include "settings.h"

#ifdef USE_KSYNTAXHIGHLIGHTING
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/SyntaxHighlighter>
#include <KSyntaxHighlighting/Theme>
#else
#include "textsyntaxhighlighter.h"
#endif

TextViewer::TextViewer(QWidget *parent) : QWidget(parent) {
    setAccessibleName("textViewer");
    layout.setContentsMargins(0, 0, 0, 0);
    layout.setSpacing(0);
    setLayout(&layout);

    noticeLabel = new QLabel(this);
    noticeLabel->setAlignment(Qt::AlignCenter);
    noticeLabel->setContentsMargins(4, 4, 4, 4);
    noticeLabel->hide();
    layout.addWidget(noticeLabel);

    textEdit = new QPlainTextEdit(this);
    textEdit->setReadOnly(true);
    textEdit->setFrameShape(QFrame::NoFrame);
    // keep keyboard focus with the main window so all navigation shortcuts
    // (prev/next file, folder view, ...) stay live; mouse selection still works
    textEdit->setFocusPolicy(Qt::NoFocus);
    textEdit->setTextInteractionFlags(Qt::TextSelectableByMouse);
    textEdit->document()->setDocumentMargin(14);
    layout.addWidget(textEdit);

    readSettings();
    connect(settings, &Settings::settingsChanged, this, &TextViewer::readSettings);
}

void TextViewer::readSettings() {
    const ColorScheme &scheme = settings->colorScheme();
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSizeF(qMax(9.0, QFont().pointSizeF()));
    textEdit->setFont(mono);
    QPalette pal = textEdit->palette();
    pal.setColor(QPalette::Base, scheme.background);
    pal.setColor(QPalette::Text, scheme.text);
    pal.setColor(QPalette::Highlight, scheme.accent);
    pal.setColor(QPalette::HighlightedText, scheme.background);
    textEdit->setPalette(pal);
    noticeLabel->setStyleSheet(QString("color: %1; background-color: %2;")
                               .arg(scheme.text_lc.name(), scheme.widget.name()));
}

bool TextViewer::showFile(const QString &filePath) {
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly)) {
        clear();
        noticeLabel->setText(tr("Could not open file"));
        noticeLabel->show();
        return false;
    }
    const qint64 fileSize = file.size();
    QByteArray raw = file.read(maxDisplayBytes);
    file.close();

    // decode as utf-8 (replacement chars for invalid sequences keep this safe
    // for any input); strip a BOM if present
    auto decoder = QStringDecoder(QStringDecoder::Utf8);
    QString content = decoder.decode(raw);
    if(!content.isEmpty() && content.front() == QChar(0xFEFF))
        content.remove(0, 1);

    // word wrap for prose formats, no wrap for everything else (code, data)
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    static const QSet<QString> proseSuffixes = {"txt", "md", "markdown", "rst", "adoc", "org", ""};
    textEdit->setLineWrapMode(proseSuffixes.contains(suffix) ? QPlainTextEdit::WidgetWidth
                                                             : QPlainTextEdit::NoWrap);

    delete highlighter;
    highlighter = nullptr;
    textEdit->setPlainText(content);
    applyHighlighter(filePath);
    textEdit->verticalScrollBar()->setValue(0);
    textEdit->horizontalScrollBar()->setValue(0);

    if(fileSize > maxDisplayBytes) {
        noticeLabel->setText(tr("Preview of the first %1 KiB (file is %2 KiB)")
                             .arg(maxDisplayBytes / 1024).arg(fileSize / 1024));
        noticeLabel->show();
    } else {
        noticeLabel->hide();
    }
    return true;
}

void TextViewer::applyHighlighter(const QString &filePath) {
    const ColorScheme &scheme = settings->colorScheme();
    qreal lum = 0.299 * scheme.background.redF() +
                0.587 * scheme.background.greenF() +
                0.114 * scheme.background.blueF();
    bool darkTheme = (lum < 0.5);
#ifdef USE_KSYNTAXHIGHLIGHTING
    static KSyntaxHighlighting::Repository repository;
    auto *hl = new KSyntaxHighlighting::SyntaxHighlighter(textEdit->document());
    hl->setTheme(repository.defaultTheme(darkTheme ? KSyntaxHighlighting::Repository::DarkTheme
                                                   : KSyntaxHighlighting::Repository::LightTheme));
    hl->setDefinition(repository.definitionForFileName(filePath));
    highlighter = hl;
#else
    highlighter = new TextSyntaxHighlighter(textEdit->document(), filePath, darkTheme);
#endif
}

void TextViewer::clear() {
    delete highlighter;
    highlighter = nullptr;
    textEdit->clear();
    noticeLabel->hide();
}
