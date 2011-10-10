/***************************************************************************
                           trackcollection.h
                              -------------------
     begin                : 10/27/2008
     copyright            : (C) 2008 Albert Santoni
     email                : gamegod \a\t users.sf.net
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TRACKCOLLECTION_H
#define TRACKCOLLECTION_H

#include <QtSql>
#include <QList>
#include <QRegExp>
#include <QSharedPointer>
#include <QSqlDatabase>

#include "configobject.h"
#include "library/basetrackcache.h"
#include "library/dao/trackdao.h"
#include "library/dao/cratedao.h"
#include "library/dao/cuedao.h"
#include "library/dao/playlistdao.h"

class TrackInfoObject;

#define AUTODJ_TABLE "Auto DJ"

const QString MIXXX_DB_PATH = QDir::homePath().append("/").append(SETTINGS_PATH).append("mixxxdb.sqlite");

class BpmDetector;

/**
   @author Albert Santoni
*/
class TrackCollection : public QObject
{
    Q_OBJECT
  public:
    TrackCollection(ConfigObject<ConfigValue>* pConfig);
    ~TrackCollection();
    bool checkForTables();

    /** Import the files in a given diretory, without recursing into subdirectories */
    bool importDirectory(QString directory, TrackDAO &trackDao,
                         QList<TrackInfoObject*>& tracksToAdd);

    void resetLibaryCancellation();
    QSqlDatabase& getDatabase();

    CrateDAO& getCrateDAO();
    TrackDAO& getTrackDAO();
    PlaylistDAO& getPlaylistDAO();
    QSharedPointer<BaseTrackCache> getTrackSource(const QString name);
    void addTrackSource(const QString name, QSharedPointer<BaseTrackCache> trackSource);

  public slots:
    void slotCancelLibraryScan();

  signals:
    void startedLoading();
    void progressLoading(QString path);
    void finishedLoading();

  private:
    ConfigObject<ConfigValue>* m_pConfig;
    QSqlDatabase m_db;
    QHash<QString, QSharedPointer<BaseTrackCache> > m_trackSources;
    PlaylistDAO m_playlistDao;
    CueDAO m_cueDao;
    TrackDAO m_trackDao;
    CrateDAO m_crateDao;
    const QRegExp m_supportedFileExtensionsRegex;

    /** Flag to raise when library scan should be cancelled */
    int bCancelLibraryScan;
    QMutex m_libraryScanMutex;
};

#endif
