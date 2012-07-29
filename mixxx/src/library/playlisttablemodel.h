#ifndef PLAYLISTTABLEMODEL_H
#define PLAYLISTTABLEMODEL_H

#include <QtSql>
#include <QItemDelegate>
#include <QtCore>

#include "library/basesqltablemodel.h"
#include "library/dao/playlistdao.h"
#include "library/dao/trackdao.h"
#include "library/librarytablemodel.h"
#include "configobject.h"

class TrackCollection;

class PlaylistTableModel : public BaseSqlTableModel {
    Q_OBJECT
  public:
    PlaylistTableModel(QObject* parent, TrackCollection* pTrackCollection,
                    QString settingsNamespace, 
                    ConfigObject<ConfigValue>* pConfig,
                    QStringList availableDirs,
                    bool showAll=false);
    virtual ~PlaylistTableModel();
    void setPlaylist(int playlistId,QString name);
    int getPlaylist() const {
        return m_iPlaylistId;
    }
    virtual TrackPointer getTrack(const QModelIndex& index) const;

    virtual void search(const QString& searchText);
    virtual bool isColumnInternal(int column);
    virtual bool isColumnHiddenByDefault(int column);
    virtual void removeTrack(const QModelIndex& index);
    virtual void removeTracks(const QModelIndexList& indices);
    virtual bool addTrack(const QModelIndex& index, QString location);
    virtual bool appendTrack(int trackId);
    // Adding multiple tracks at one to a playlist. Returns the number of
    // successful additions.
    virtual int addTracks(const QModelIndex& index, QList<QString> locations);
    virtual void moveTrack(const QModelIndex& sourceIndex, const QModelIndex& destIndex);
    virtual void shuffleTracks(const QModelIndex& shuffleStartIndex);

    TrackModel::CapabilitiesFlags getCapabilities() const;

  private slots:
    void slotSearch(const QString& searchText);
    void slotConfigChanged(QString,QString);
    void slotAvailableDirsChanged(QStringList, QString);

  signals:
    void doSearch(const QString& searchText);

  private:
    TrackCollection* m_pTrackCollection;
    PlaylistDAO& m_playlistDao;
    TrackDAO& m_trackDao;
    int m_iPlaylistId;
    ConfigObject<ConfigValue>* m_pConfig;
    QStringList m_availableDirs;
    bool m_showAll;
};

#endif
