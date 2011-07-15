#include <QtDebug>
#include <QtGui>

#include "dlgprefautodj.h"

#define CONFIG_KEY "[Auto DJ]"

DlgPrefAutoDJ::DlgPrefAutoDJ(QWidget *parent, ConfigObject<ConfigValue> *config) :
    QWidget(parent),
    m_pConfig(config),
    Ui::DlgPrefAutoDJDlg() {

    setupUi(this);

    connect(SliderFadeLength, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateFadeLength()));
    connect(SliderFadeLength, SIGNAL(sliderMoved(int)), this, SLOT(slotUpdateFadeLength()));
    connect(SliderFadeLength, SIGNAL(sliderReleased()), this, SLOT(slotUpdateFadeLength()));

    loadSettings();
}

DlgPrefAutoDJ::~DlgPrefAutoDJ(){
}

void DlgPrefAutoDJ::loadSettings(){
    if (m_pConfig->getValueString(
                ConfigKey(CONFIG_KEY, "CrossfadeLength")) == QString("")){
        // No value set, apply our defaults
        setDefaults();
    }
    SliderFadeLength->setValue(
        m_pConfig->getValueString(ConfigKey(CONFIG_KEY, "CrossfadeLength")).toInt());

    slotUpdate();
    slotApply();
}

void DlgPrefAutoDJ::setDefaults(){
    m_pConfig->set(ConfigKey(CONFIG_KEY, "CrossfadeLength"), ConfigValue(5));
}

void DlgPrefAutoDJ::slotApply(){
}

void DlgPrefAutoDJ::slotUpdate(){
    slotUpdateFadeLength();
}

void DlgPrefAutoDJ::slotUpdateFadeLength(){
    m_fadeLength = SliderFadeLength->value();
    TextFadeLength->setText(QString("%1 Seconds").arg(m_fadeLength));
    m_pConfig->set(ConfigKey(CONFIG_KEY, "CrossfadeLength"), ConfigValue(m_fadeLength));
}
