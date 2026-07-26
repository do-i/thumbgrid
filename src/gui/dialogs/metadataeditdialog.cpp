#include "metadataeditdialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "sourcecontainers/documentinfo.h"

MetadataEditDialog::MetadataEditDialog(const QString &fileName,
                                       const QMap<QString, QString> &current,
                                       QWidget *parent)
    : QDialog(parent), mOriginal(current)
{
    setWindowTitle(tr("Edit metadata"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);

    auto *heading = new QLabel(fileName, this);
    heading->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(heading);

    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    for(const QString &key : DocumentInfo::editableTagKeys()) {
        auto *field = new QLineEdit(current.value(key), this);
        // objectName is the exiv2 key so a test (or a stylesheet) can address a
        // specific field without depending on the row order.
        field->setObjectName(key);
        if(key == QLatin1String("Exif.Image.DateTime"))
            field->setPlaceholderText(QStringLiteral("YYYY:MM:DD HH:MM:SS"));
        mFields.insert(key, field);
        form->addRow(DocumentInfo::editableTagLabel(key) + QStringLiteral(":"), field);
    }
    layout->addLayout(form);

    auto *hint = new QLabel(tr("Clearing a field removes that tag from the file."), this);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("metadataEditHint"));
    layout->addWidget(hint);

    mError = new QLabel(this);
    mError->setObjectName(QStringLiteral("metadataEditError"));
    mError->setWordWrap(true);
    mError->hide();
    layout->addWidget(mError);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Save"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttons, &QDialogButtonBox::accepted, this, &MetadataEditDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    if(auto *first = mFields.value(DocumentInfo::editableTagKeys().first()))
        first->setFocus();
}

QMap<QString, QString> MetadataEditDialog::editedValues() const {
    QMap<QString, QString> changed;
    for(auto it = mFields.cbegin(); it != mFields.cend(); ++it) {
        const QString before = mOriginal.value(it.key());
        const QString after = it.value()->text();
        if(after != before)
            changed.insert(it.key(), after);
    }
    return changed;
}

void MetadataEditDialog::onAccept() {
    const QString dateKey = QStringLiteral("Exif.Image.DateTime");
    if(auto *dateField = mFields.value(dateKey)) {
        const QString value = dateField->text();
        // Empty is fine - that removes the tag. Anything else must be the one
        // form Exif allows, or the file ends up with a date other tools ignore.
        if(!value.isEmpty() && !DocumentInfo::isValidExifDateTime(value)) {
            mError->setText(tr("Date/Time must look like 2026:07:26 10:30:00."));
            mError->show();
            dateField->setFocus();
            dateField->selectAll();
            return;
        }
    }
    accept();
}
