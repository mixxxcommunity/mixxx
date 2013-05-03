/***************************************************************************
                          dlgprefrecord.cpp  -  description
                             -------------------
    begin                : Thu Jun 19 2007
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

#include <QtCore>
#include <QtGui>
#include "dlgprefrecord.h"
#include "recording/defs_recording.h"
#include "controlobject.h"
#include "controlobjectthreadmain.h"
#include "recording/encoder.h"


DlgPrefRecord::DlgPrefRecord(QWidget * parent)
        : QWidget(parent),
          m_settings(){
    confirmOverwrite = false;
    radioFlac = 0;
    radioMp3 = 0;
    radioOgg = 0;
    radioAiff= 0;
    radioWav = 0;
    setupUi(this);

    //See RECORD_* #defines in defs_recording.h
    recordControl = new ControlObjectThreadMain(
        ControlObject::getControl(ConfigKey(RECORDING_PREF_KEY, "status")));

#ifdef __SHOUTCAST__
    radioOgg = new QRadioButton("Ogg Vorbis");
    radioMp3 = new QRadioButton(ENCODING_MP3);

    // Setting recordings path
    QString recordingsPath = m_settings.value("Recording/Directory").toString();
    if (recordingsPath == "") {
        // Initialize recordings path in config to old default path.
        // Do it here so we show current value in UI correctly.
        QString musicDir = m_settings.value("Library/Directory").toString();
        if (musicDir.isEmpty()) {
            musicDir = QDesktopServices::storageLocation(QDesktopServices::MusicLocation);
        }
        QDir recordDir(musicDir + "/Mixxx/Recordings");
        recordingsPath = recordDir.absolutePath();
    }
    LineEditRecordings->setText(recordingsPath);

    connect(PushButtonBrowseRecordings, SIGNAL(clicked()), this, SLOT(slotBrowseRecordingsDir()));
    connect(LineEditRecordings, SIGNAL(returnPressed()), this, SLOT(slotApply()));

    connect(radioOgg,SIGNAL(clicked()),
            this, SLOT(slotApply()));
    connect(radioMp3, SIGNAL(clicked()),
            this, SLOT(slotApply()));
    horizontalLayout->addWidget(radioOgg);
    horizontalLayout->addWidget(radioMp3);

#endif

    //AIFF and WAVE are supported by default
    radioWav = new QRadioButton(ENCODING_WAVE);
    connect(radioWav, SIGNAL(clicked()),
            this, SLOT(slotApply()));
    horizontalLayout->addWidget(radioWav);

    radioAiff = new QRadioButton(ENCODING_AIFF);
    connect(radioAiff, SIGNAL(clicked()),
            this, SLOT(slotApply()));
    horizontalLayout->addWidget(radioAiff);


#ifdef SF_FORMAT_FLAC
    radioFlac = new QRadioButton(ENCODING_FLAC);
    connect(radioFlac,SIGNAL(clicked()),
            this, SLOT(slotApply()));
    horizontalLayout->addWidget(radioFlac);
#endif

    //Read config and check radio button
    QString format = m_settings.value("Recording/Encoding").toString();
    if(format == ENCODING_WAVE)
        radioWav->setChecked(true);
#ifdef __SHOUTCAST__
    else if(format == ENCODING_OGG)
        radioOgg->setChecked(true);
    else if (format == ENCODING_MP3)
        radioMp3->setChecked(true);
#endif
#ifdef SF_FORMAT_FLAC
    else if (format == ENCODING_AIFF)
        radioAiff->setChecked(true);
#endif
    else { //Invalid, so set default and save
        //If no config was available, set to WAVE as default
        radioWav->setChecked(true);
        m_settings.setValue("Recording/Encoding", ENCODING_WAVE);
    }

    //Connections
    connect(SliderQuality,    SIGNAL(valueChanged(int)), this,  SLOT(slotSliderQuality()));
    connect(SliderQuality,    SIGNAL(sliderMoved(int)), this,   SLOT(slotSliderQuality()));
    connect(SliderQuality,    SIGNAL(sliderReleased()), this,   SLOT(slotSliderQuality()));
    connect(CheckBoxRecordCueFile, SIGNAL(stateChanged(int)), this, SLOT(slotEnableCueFile(int)));
    connect(comboBoxSplitting, SIGNAL(activated(int)),   this,   SLOT(slotChangeSplitSize()));

    slotApply();
    recordControl->slotSet(RECORD_OFF); //make sure a corrupt config file won't cause us to record constantly

    comboBoxSplitting->addItem(SPLIT_650MB);
    comboBoxSplitting->addItem(SPLIT_700MB);
    comboBoxSplitting->addItem(SPLIT_1024MB);
    comboBoxSplitting->addItem(SPLIT_2048MB);
    comboBoxSplitting->addItem(SPLIT_4096MB);

    QString fileSizeStr = m_settings.value("Recording/FileSize").toString();
    int index = comboBoxSplitting->findText(fileSizeStr);
    if(index > 0){
        //set file split size
        comboBoxSplitting->setCurrentIndex(index);
    }
    //Otherwise 650 MB will be default file split size

    //Read CUEfile info
    CheckBoxRecordCueFile->setChecked(m_settings.value("Recording/CueEnabled").toBool());

}

void DlgPrefRecord::slotSliderQuality()
{
    updateTextQuality();

    if (radioOgg && radioOgg->isChecked())
    {
        m_settings.setValue("Recording/OGG_Quality", SliderQuality->value());
    }
    else if (radioMp3 && radioMp3->isChecked())
    {
        m_settings.setValue("Recording/MP3_Quality", SliderQuality->value());
    }
}

int DlgPrefRecord::getSliderQualityVal()
{

    /* Commented by Tobias Rafreider
     * We always use the bitrate to denote the quality since it is more common to the users
     */
    return Encoder::convertToBitrate(SliderQuality->value());

}

void DlgPrefRecord::updateTextQuality()
{
    int quality = getSliderQualityVal();
    //QString encodingType = comboBoxEncoding->currentText();

    TextQuality->setText(QString( QString::number(quality) + tr("kbps")));


}

void DlgPrefRecord::slotEncoding()
{
    //set defaults
    groupBoxQuality->setEnabled(true);

    if (radioWav && radioWav->isChecked()) {
        m_settings.setValue("Recording/Encoding", ENCODING_WAVE);
        groupBoxQuality->setEnabled(false);
    } else if(radioFlac && radioFlac->isChecked()) {
        m_settings.setValue("Recording/Encoding", ENCODING_FLAC);
        groupBoxQuality->setEnabled(false);
    } else if(radioAiff && radioAiff->isChecked()) {
        m_settings.setValue("Recording/Encoding", ENCODING_AIFF);
        groupBoxQuality->setEnabled(false);
    } else if (radioOgg && radioOgg->isChecked()) {
        int value = m_settings.value("Recording/OGG_Quality").toInt();
        //if value == 0 then a default value of 128kbps is proposed.
        if(!value)
            value = 6; // 128kbps

        SliderQuality->setValue(value);
        m_settings.setValue("Recoding/Encoding", ENCODING_OGG);
    } else if (radioMp3 && radioMp3->isChecked()) {
        int value = m_settings.value("Recoding/MP3_Quality").toInt();
        //if value == 0 then a default value of 128kbps is proposed.
        if(!value)
            value = 6; // 128kbps

        SliderQuality->setValue(value);
        m_settings.setValue("Recording/Encoding", ENCODING_MP3);
    }
    else
        qDebug() << "Invalid recording encoding type in" << __FILE__ << "on line:" << __LINE__;
}

void DlgPrefRecord::setMetaData() {
    m_settings.setValue("Recording/Title", LineEditTitle->text());
    m_settings.setValue("Recording/Author", LineEditAuthor->text());
    m_settings.setValue("Recording/Album", LineEditAlbum->text());
}

void DlgPrefRecord::loadMetaData() {
    LineEditTitle->setText(m_settings.value("Recording/Title").toString());
    LineEditAuthor->setText(m_settings.value("Recording/Author").toString());
    LineEditAlbum->setText(m_settings.value("Recording/Album").toString());
}

DlgPrefRecord::~DlgPrefRecord() {
   delete recordControl;
}

void DlgPrefRecord::slotRecordPathChange() {
    confirmOverwrite = false;
    slotApply();
}

//This function updates/refreshes the contents of this dialog
void DlgPrefRecord::slotUpdate() {
    // Recordings path
    QString recordingsPath = m_settings.value("Recording/Directory").toString();
    LineEditRecordings->setText(recordingsPath);

    if (radioWav && radioWav->isChecked()) {
        m_settings.setValue("Recording/Encoding", ENCODING_WAVE);
    } else if (radioAiff && radioAiff->isChecked()) {
        m_settings.setValue("Recording/Encoding", ENCODING_AIFF);
    } else if (radioFlac && radioFlac->isChecked()) {
        m_settings.setValue("Recording/Encoding", ENCODING_FLAC);
    } else if (radioOgg && radioOgg->isChecked()) {
        m_settings.setValue("Recording/Encoding", ENCODING_OGG);
    } else if (radioMp3 && radioMp3->isChecked()) {
        m_settings.setValue("Recording/Encoding", ENCODING_MP3);
    }
    loadMetaData();
}

void DlgPrefRecord::slotBrowseRecordingsDir() {
    QString fd = QFileDialog::getExistingDirectory(this, tr("Choose recordings directory"),
                                                   m_settings.value("Recording/Directory").toString());
    if (fd != "") {
        LineEditRecordings->setText(fd);
    }
}

void DlgPrefRecord::slotApply() {
    setRecordingFolder();

    setMetaData();

    slotEncoding();
}

void DlgPrefRecord::setRecordingFolder() {
    if (LineEditRecordings->text() == "") {
        qDebug() << "Recordings path was empty in dialog";
        return;
    }
    if (LineEditRecordings->text() != m_settings.value("Recording/Directory").toString()) {
        qDebug() << "Saved recordings path" << LineEditRecordings->text();
        m_settings.setValue("Recording/Directory", LineEditRecordings->text());
    }
}

void DlgPrefRecord::slotEnableCueFile(int enabled) {
    m_settings.setValue("Recording/CueEnabled", enabled != Qt::Unchecked);
}

void DlgPrefRecord::slotChangeSplitSize() {
    m_settings.setValue("Recording/FileSize", comboBoxSplitting->currentText());
}
