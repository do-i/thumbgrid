#include "shortcutcreatordialog.h"
#include "ui_shortcutcreatordialog.h"

#include <QBoxLayout>
#include <QPushButton>
#include "dialogbuttonrow.h"

ShortcutCreatorDialog::ShortcutCreatorDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ShortcutCreatorDialog)
{
    ui->setupUi(this);
    if(auto box = qobject_cast<QBoxLayout *>(ui->sequenceEdit->parentWidget()->layout())) {
        const int at = box->indexOf(ui->sequenceEdit);
        if(at >= 0) {
            QPushButton *gestures = ui->sequenceEdit->createMouseGestureButton(this);
            box->insertWidget(at + 1, gestures);
            // created in code, so it would otherwise tab last, after Cancel
            setTabOrder(ui->sequenceEdit, gestures);
            setTabOrder(gestures, ui->okButton);
        }
    }

    // Each shortcut belongs to one context. Global bindings run in every screen;
    // view-specific bindings override them for the same key.
    ui->contextComboBox->addItem(tr("Global"), ActionManager::contextToString(MODE_GLOBAL));
    ui->contextComboBox->addItem(tr("Document"), ActionManager::contextToString(MODE_DOCUMENT));
    ui->contextComboBox->addItem(tr("Grid"), ActionManager::contextToString(MODE_FOLDERVIEW));
    ui->contextComboBox->addItem(tr("Copy / Move"), ActionManager::contextToString(MODE_COPYMOVE));
    ui->contextComboBox->setCurrentIndex(0);

    actionList = appActions->getList();
    scriptList = scriptManager->scriptNames();

    ui->actionsComboBox->addItems(actionList);
    ui->actionsComboBox->setCurrentIndex(0);

    ui->scriptsComboBox->addItems(scriptList);
    ui->scriptsComboBox->setCurrentIndex(0);

    // accept-first order is laid out in the .ui
    DialogButtonRow::styleButtons(ui->okButton, ui->cancelButton);
    connect(ui->okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

ShortcutCreatorDialog::~ShortcutCreatorDialog() {
    delete ui;
}

QString ShortcutCreatorDialog::selectedAction() {
    if(ui->actionsRadioButton->isChecked())
        return ui->actionsComboBox->currentText();
    else
        return "s:"+ui->scriptsComboBox->currentText();
}

QString ShortcutCreatorDialog::selectedShortcut() {
    return ui->sequenceEdit->sequence();
}

ViewMode ShortcutCreatorDialog::selectedContext() {
    return ActionManager::contextFromString(ui->contextComboBox->currentData().toString());
}

void ShortcutCreatorDialog::onShortcutEdited() {
    // A shortcut only conflicts within the same context, so scope the check.
    QString action = actionManager->actionForShortcut(selectedContext(), ui->sequenceEdit->sequence());
    if(!action.isEmpty())
        ui->warningLabel->setText(tr("This shortcut is used for action: %1. Replace?").arg(action));
    else
        ui->warningLabel->setText("");
}

void ShortcutCreatorDialog::setAction(QString action) {
    auto cbox = ui->actionsComboBox;
    if(action.startsWith("s:")) {
        action = action.remove(0,2);
        cbox = ui->scriptsComboBox;
        ui->scriptsRadioButton->setChecked(true);
    }
    int index = cbox->findText(action);
    if(index != -1)
       cbox->setCurrentIndex(index);
}

void ShortcutCreatorDialog::setContext(ViewMode context) {
    int index = ui->contextComboBox->findData(ActionManager::contextToString(context));
    if(index != -1)
        ui->contextComboBox->setCurrentIndex(index);
}
