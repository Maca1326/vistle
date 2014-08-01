#include "modulebrowser.h"
#include "ui_modulebrowser.h"

#include <QMimeData>
#include <QDebug>

namespace gui {

ModuleListWidget::ModuleListWidget(QWidget *parent)
   : QListWidget(parent)
{
}

QMimeData *ModuleListWidget::mimeData(const QList<QListWidgetItem *> dragList) const
{
   if (dragList.empty())
      return nullptr;

   QMimeData *md = QListWidget::mimeData(dragList);

   QByteArray encodedData;
   QDataStream stream(&encodedData, QIODevice::WriteOnly);
   for(QListWidgetItem *item: dragList) {
      stream << item->data(ModuleBrowser::hubRole()).toInt();
      stream << item->text();
   }
   md->setData(ModuleBrowser::mimeFormat(), encodedData);
   return md;
}

void ModuleListWidget::setFilter(QString filter) {

   m_filter = filter.trimmed();

   for (int idx=0; idx<count(); ++idx) {

      const auto i = item(idx);
      filterItem(i);
   }
}


void ModuleListWidget::filterItem(QListWidgetItem *item) const {

   const auto name = item->data(ModuleBrowser::nameRole()).toString();
   const bool match = name.contains(m_filter, Qt::CaseInsensitive);
   item->setHidden(!match);
}


const char *ModuleBrowser::mimeFormat() {
   return "application/x-modulebrowser";
}

int ModuleBrowser::nameRole() {

   return Qt::DisplayRole;
}

int ModuleBrowser::hubRole() {

   return Qt::UserRole;
}

ModuleBrowser::ModuleBrowser(QWidget *parent)
   : QWidget(parent)
   , ui(new Ui::ModuleBrowser)
{
   ui->setupUi(this);

   connect(filterEdit(), SIGNAL(textChanged(QString)), SLOT(setFilter(QString)));
   ui->moduleListWidget->setFocusProxy(filterEdit());
}

ModuleBrowser::~ModuleBrowser()
{
   delete ui;
}

void ModuleBrowser::addModule(int hub, QString module) {

    auto item = new QListWidgetItem(module);
    item->setData(hubRole(), hub);
    item->setData(Qt::ToolTipRole, hub);
    ui->moduleListWidget->addItem(item);
    ui->moduleListWidget->filterItem(item);
}

QLineEdit *ModuleBrowser::filterEdit() const {

   return ui->filterEdit;
}

void ModuleBrowser::setFilter(QString filter) {

   ui->moduleListWidget->setFilter(filter);
}

} // namespace gui
