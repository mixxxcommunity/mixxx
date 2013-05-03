/***************************************************************************
                          dlgprefplaylist.cpp  -  description
                             -------------------
    begin                : Thu Apr 17 2003
    copyright            : (C) 2003 by Tue & Ken Haste Andersen
    email                : haste@diku.dk
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "dlgprefplaylist.h"
#ifdef __PROMO__
#include "library/promotracksfeature.h"
#endif
#include "soundsourceproxy.h"
//#include "plugindownloader.h"
#include <QtCore>
#include <QtGui>

#define MIXXX_ADDONS_URL "http://www.mixxx.org/wiki/doku.php/add-ons"

DlgPrefPlaylist::DlgPrefPlaylist(QWidget * parent)
        :  QWidget(parent),
         m_settings(parent) {
    setupUi(this);
    slotUpdate();
    checkbox_ID3_sync->setVisible(false);

    /*
    m_pPluginDownloader = new PluginDownloader(this);

    //Disable the M4A button if the plugin is present on disk.
    setupM4AButton();

    //Disable M4A Button after download completes successfully.
    connect(m_pPluginDownloader, SIGNAL(downloadFinished()),
            this, SLOT(slotM4ADownloadFinished()));

    connect(m_pPluginDownloader, SIGNAL(downloadProgress(qint64, qint64)),
            this, SLOT(slotM4ADownloadProgress(qint64, qint64)));
    */

    // Connection
    connect(PushButtonBrowsePlaylist, SIGNAL(clicked()),       this,      SLOT(slotBrowseDir()));
    connect(LineEditSongfiles,        SIGNAL(returnPressed()), this,      SLOT(slotApply()));
    //connect(pushButtonM4A, SIGNAL(clicked()), this, SLOT(slotM4ACheck()));
    connect(pushButtonExtraPlugins, SIGNAL(clicked()), this, SLOT(slotExtraPlugins()));

    bool enablePromoGroupbox = false;
#ifdef __PROMO__
    enablePromoGroupbox = PromoTracksFeature::isSupported(config);
#endif
    if (!enablePromoGroupbox) {
        groupBoxBundledSongs->hide();
    }

    // plugins are loaded in src/main.cpp way early in boot so this is safe
    // here, doesn't need done at every slotUpdate
    QStringList plugins(SoundSourceProxy::supportedFileExtensionsByPlugins());
    if (plugins.length() > 0) {
        pluginsLabel->setText(plugins.join(", "));
    }
}

DlgPrefPlaylist::~DlgPrefPlaylist() {
}

void DlgPrefPlaylist::slotExtraPlugins() {
    QDesktopServices::openUrl(QUrl(MIXXX_ADDONS_URL));
}

void DlgPrefPlaylist::slotUpdate() {
    // Song path
    LineEditSongfiles->setText(m_settings.value("Library/Directory").toString());
    //Bundled songs stat tracking
    checkBoxPromoStats->setChecked(m_settings.value("Promo/StatTracking").toBool());
    checkBox_library_scan->setChecked(m_settings.value("Library/RescanOnStartup",false).toBool());
    checkbox_ID3_sync->setChecked(m_settings.value("Library/WriteAudioTags").toBool());
    checkBox_use_relative_path->setChecked(m_settings.value("Library/UseRelativePathOnExport").toBool());
    checkBox_show_rhythmbox->setChecked(m_settings.value("Library/ShowRhythmboxLibrary").toBool());
    checkBox_show_itunes->setChecked(m_settings.value("Library/ShowITunesLibrary").toBool());
    checkBox_show_traktor->setChecked(m_settings.value("Library/ShowTraktorLibrary").toBool());
}

void DlgPrefPlaylist::slotBrowseDir() {
    QString fd = QFileDialog::getExistingDirectory(this, tr("Choose music library directory"),
                                                   m_settings.value("Library/Directory").toString());
    if (fd != "") {
        LineEditSongfiles->setText(fd);
    }
}

void DlgPrefPlaylist::slotApply() {
    m_settings.setValue("Promo/StatTracking", checkBoxPromoStats->isChecked());
    m_settings.setValue("Library/RescanOnStartup", checkBox_library_scan->isChecked());
    m_settings.setValue("Library/WriteAudioTags",checkbox_ID3_sync->isChecked());
    m_settings.setValue("Library/UseRelativePathOnExport", checkBox_use_relative_path->isChecked());
    m_settings.setValue("Library/ShowRhythmboxLibrary", checkBox_show_rhythmbox->isChecked());
    m_settings.setValue("Library/ShowITunesLibrary", checkBox_show_itunes->isChecked());
    m_settings.setValue("Library.ShowTraktorLibrary", checkBox_show_traktor->isChecked());

    // Update playlist if path has changed
    if (LineEditSongfiles->text() != m_settings.value("Library/Directory").toString()) {
        // Check for valid directory and put up a dialog if invalid!!!

        m_settings.setValue("Library/Directory", LineEditSongfiles->text());

        // Emit apply signal
        emit(apply());
    }
}
