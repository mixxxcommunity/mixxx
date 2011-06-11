#include <QtDebug>
#include <QtGui>

#include "dlgprefautodj.h"

DlgPrefAutoDJ::DlgPrefAutoDJ(QWidget *parent, ConfigObject<ConfigValue> *_config) :
    QWidget(parent), Ui::DlgPrefAutoDJDlg() {
    setupUi(this);
}

DlgPrefAutoDJ::~DlgPrefAutoDJ(){

}

void DlgPrefAutoDJ::slotApply(){
}

void DlgPrefAutoDJ::slotUpdate(){
}
