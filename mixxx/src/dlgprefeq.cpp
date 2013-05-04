/***************************************************************************
                          dlgprefeq.cpp  -  description
                             -------------------
    begin                : Thu Jun 7 2007
    copyright            : (C) 2007 by John Sully
    email                : jsully@scs.ryerson.ca
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "dlgprefeq.h"
#include "engine/enginefilteriir.h"
#include <qlineedit.h>
#include <qwidget.h>
#include <qslider.h>
#include <qlabel.h>
#include <qstring.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <qgraphicsscene.h>

#include <assert.h>

static const QString CONFIG_KEY="MixerProfile";

const int kFrequencyUpperLimit = 20050;
const int kFrequencyLowerLimit = 16;

DlgPrefEQ::DlgPrefEQ(QWidget *pParent)
        : QWidget(pParent),
          m_settings(),
#ifndef __LOFI__
          m_COTLoFreq(ControlObject::getControl(ConfigKey(CONFIG_KEY, "LoEQFrequency"))),
          m_COTHiFreq(ControlObject::getControl(ConfigKey(CONFIG_KEY, "HiEQFrequency"))),
          m_COTLoFi(ControlObject::getControl(ConfigKey(CONFIG_KEY, "LoFiEQs")))
#endif
{

    setupUi(this);

    // Connection
#ifndef __LOFI__
    connect(SliderHiEQ, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateHiEQ()));
    connect(SliderHiEQ, SIGNAL(sliderMoved(int)), this, SLOT(slotUpdateHiEQ()));
    connect(SliderHiEQ, SIGNAL(sliderReleased()), this, SLOT(slotUpdateHiEQ()));

    connect(SliderLoEQ, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateLoEQ()));
    connect(SliderLoEQ, SIGNAL(sliderMoved(int)), this, SLOT(slotUpdateLoEQ()));
    connect(SliderLoEQ, SIGNAL(sliderReleased()), this, SLOT(slotUpdateLoEQ()));

    connect(CheckBoxLoFi, SIGNAL(stateChanged(int)), this, SLOT(slotLoFiChanged()));
#else
    CheckBoxLoFi->setChecked(true);
    slotLoFiChanged();
    CheckBoxLoFi->setEnabled(false);
#endif
    connect(PushButtonReset, SIGNAL(clicked(bool)), this, SLOT(reset()));

    m_lowEqFreq = 0;
    m_highEqFreq = 0;

    loadSettings();
}

DlgPrefEQ::~DlgPrefEQ() {
}

void DlgPrefEQ::loadSettings() {
    QString highEqCourse = m_settings.value(CONFIG_KEY+"/HiEQFrequency").toString();
    QString highEqPrecise = m_settings.value(CONFIG_KEY+"/HiEQFrequencyPrecise").toString();
    QString lowEqCourse = m_settings.value(CONFIG_KEY+"/LoEQFrequency").toString();
    QString lowEqPrecise = m_settings.value(CONFIG_KEY+"/LoEQFrequencyPrecise").toString();

    double lowEqFreq = 0.0;
    double highEqFreq = 0.0;

    // Precise takes precedence over course.
    lowEqFreq = lowEqCourse.isEmpty() ? lowEqFreq : lowEqCourse.toDouble();
    lowEqFreq = lowEqPrecise.isEmpty() ? lowEqFreq : lowEqPrecise.toDouble();
    highEqFreq = highEqCourse.isEmpty() ? highEqFreq : highEqCourse.toDouble();
    highEqFreq = highEqPrecise.isEmpty() ? highEqFreq : highEqPrecise.toDouble();

    if (lowEqFreq == 0.0 || highEqFreq == 0.0 || lowEqFreq == highEqFreq) {
        CheckBoxLoFi->setChecked(true);
        setDefaultShelves();
        lowEqFreq = m_settings.value(CONFIG_KEY+"/LoEQFrequencyPrecise").toDouble();
        highEqFreq = m_settings.value(CONFIG_KEY+"/HiEQFrequencyPrecise").toDouble();
    }

    SliderHiEQ->setValue(getSliderPosition(highEqFreq,
                         SliderHiEQ->minimum(),
                         SliderHiEQ->maximum()));
    SliderLoEQ->setValue(getSliderPosition(lowEqFreq,
                         SliderLoEQ->minimum(),
                         SliderLoEQ->maximum()));

    if (m_settings.value(CONFIG_KEY+"/LoFiEQs").toString() == QString("yes")) {
        CheckBoxLoFi->setChecked(true);
    } else {
        CheckBoxLoFi->setChecked(false);
    }

    slotUpdate();
    slotApply();
}

void DlgPrefEQ::setDefaultShelves() {
    m_settings.setValue(CONFIG_KEY+"/HiEQFrequency", 2500);
    m_settings.setValue(CONFIG_KEY+"/LoEQFrequency", 250);
    m_settings.setValue(CONFIG_KEY+"/HiEQFrequencyPrecise", 2500);
    m_settings.setValue(CONFIG_KEY+"/LoEQFrequencyPrecise", 250);
}

// Resets settings, leaves LOFI box checked asis.
void DlgPrefEQ::reset() {
    setDefaultShelves();
    loadSettings();
}

void DlgPrefEQ::slotLoFiChanged() {
    GroupBoxHiEQ->setEnabled(!CheckBoxLoFi->isChecked());
    GroupBoxLoEQ->setEnabled(!CheckBoxLoFi->isChecked());
    if(CheckBoxLoFi->isChecked()) {
        m_settings.setValue(CONFIG_KEY+"/LoFiEQs", "yes");
    } else {
        m_settings.setValue(CONFIG_KEY+"/LoFiEQs", "no");
    }
    slotApply();
}

void DlgPrefEQ::slotUpdateHiEQ() {
    if (SliderHiEQ->value() < SliderLoEQ->value()) {
        SliderHiEQ->setValue(SliderLoEQ->value());
    }
    m_highEqFreq = getEqFreq(SliderHiEQ->value(),
                             SliderHiEQ->minimum(),
                             SliderHiEQ->maximum());
    validate_levels();
    if (m_highEqFreq < 1000) {
        TextHiEQ->setText( QString("%1 Hz").arg((int)m_highEqFreq));
    } else {
        TextHiEQ->setText( QString("%1 kHz").arg((int)m_highEqFreq / 1000.));
    }
    m_settings.setValue(CONFIG_KEY+"/HiEQFrequency", m_highEqFreq);
    m_settings.setValue(CONFIG_KEY+"/HiEQFrequencyPrecise", m_highEqFreq);

    slotApply();
}

void DlgPrefEQ::slotUpdateLoEQ() {
    if (SliderLoEQ->value() > SliderHiEQ->value()) {
        SliderLoEQ->setValue(SliderHiEQ->value());
    }
    m_lowEqFreq = getEqFreq(SliderLoEQ->value(),
                            SliderLoEQ->minimum(),
                            SliderLoEQ->maximum());
    validate_levels();
    if (m_lowEqFreq < 1000) {
        TextLoEQ->setText(QString("%1 Hz").arg((int)m_lowEqFreq));
    } else {
        TextLoEQ->setText(QString("%1 kHz").arg((int)m_lowEqFreq / 1000.));
    }
    m_settings.setValue(CONFIG_KEY+"/LoEQFrequency", m_lowEqFreq);
    m_settings.setValue(CONFIG_KEY+"/LoEQFrequencyPrecise", m_lowEqFreq);

    slotApply();
}

int DlgPrefEQ::getSliderPosition(double eqFreq, int minValue, int maxValue) {
    if (eqFreq >= kFrequencyUpperLimit) {
        return maxValue;
    } else if (eqFreq <= kFrequencyLowerLimit) {
        return minValue;
    }
    double dsliderPos = (eqFreq - kFrequencyLowerLimit) / (kFrequencyUpperLimit-kFrequencyLowerLimit);
    dsliderPos = pow(dsliderPos, 1./4.) * (maxValue - minValue) + minValue;
    return dsliderPos;
}


void DlgPrefEQ::slotApply() {
#ifndef __LOFI__
    m_COTLoFreq.slotSet(m_lowEqFreq);
    m_COTHiFreq.slotSet(m_highEqFreq);
    m_COTLoFi.slotSet(CheckBoxLoFi->isChecked());
#endif
}

void DlgPrefEQ::slotUpdate() {
    slotUpdateLoEQ();
    slotUpdateHiEQ();
    slotLoFiChanged();
}

double DlgPrefEQ::getEqFreq(int sliderVal, int minValue, int maxValue) {
    // We're mapping f(x) = x^4 onto the range kFrequencyLowerLimit,
    // kFrequencyUpperLimit with x [minValue, maxValue]. First translate x into
    // [0.0, 1.0], raise it to the 4th power, and then scale the result from
    // [0.0, 1.0] to [kFrequencyLowerLimit, kFrequencyUpperLimit].
    double normValue = static_cast<double>(sliderVal - minValue) /
            (maxValue - minValue);
    // Use a non-linear mapping between slider and frequency.
    normValue = normValue * normValue * normValue * normValue;
    double result = normValue * (kFrequencyUpperLimit-kFrequencyLowerLimit) +
            kFrequencyLowerLimit;
    return result;
}

void DlgPrefEQ::validate_levels() {
    m_highEqFreq = math_max(math_min(m_highEqFreq, kFrequencyUpperLimit),
                            kFrequencyLowerLimit);
    m_lowEqFreq = math_max(math_min(m_lowEqFreq, kFrequencyUpperLimit),
                           kFrequencyLowerLimit);
    if (m_lowEqFreq == m_highEqFreq) {
        if (m_lowEqFreq == kFrequencyLowerLimit) {
            ++m_highEqFreq;
        } else if (m_highEqFreq == kFrequencyUpperLimit) {
            --m_lowEqFreq;
        } else {
            ++m_highEqFreq;
        }
    }
}
