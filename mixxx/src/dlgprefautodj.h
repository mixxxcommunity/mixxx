#ifndef DLGPREFAUTODJ_H
#define DLGPREFAUTODJ_H

#include "ui_dlgprefautodjdlg.h"
#include "controlobject.h"
#include "configobject.h"

class QWidget;

class DlgPrefAutoDJ : public QWidget, public Ui::DlgPrefAutoDJDlg {
    Q_OBJECT

public:
    DlgPrefAutoDJ(QWidget *parent, ConfigObject<ConfigValue> *_config);
    ~DlgPrefAutoDJ();
public slots:
    void slotUpdateFadeLength();
    // Apply changes to widget
    void slotApply();
    void slotUpdate();

private:
    void loadSettings();
    void setDefaults();
    int m_fadeLength;
    ConfigObject<ConfigValue> *m_pConfig;
};

#endif // DLGPREFAUTODJ_H
