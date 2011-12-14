
#ifndef BANSHEEFEATURE_H
#define BANSHEEFEATURE_H

#include <QStringListModel>
#include <QtSql>
#include <QFuture>
#include <QtConcurrentRun>
#include <QFutureWatcher>

#include "library/libraryfeature.h"
#include "library/trackcollection.h"
#include "library/treeitemmodel.h"
#include "library/treeitem.h"

extern "C"
{
#include <gpod/itdb.h>
}

class BansheePlaylistModel;

class BansheeFeature : public LibraryFeature {
 Q_OBJECT
 public:
    BansheeFeature(QObject* parent, TrackCollection* pTrackCollection);
    virtual ~BansheeFeature();
    static bool isSupported();

    QVariant title();
    QIcon getIcon();

    bool dropAccept(QUrl url);
    bool dropAcceptChild(const QModelIndex& index, QUrl url);
    bool dragMoveAccept(QUrl url);
    bool dragMoveAcceptChild(const QModelIndex& index, QUrl url);

    TreeItemModel* getChildModel();

  public slots:
    void activate();
    void activate(bool forceReload);
    void activateChild(const QModelIndex& index);
    void onRightClick(const QPoint& globalPos);
    void onRightClickChild(const QPoint& globalPos, QModelIndex index);
    void onLazyChildExpandation(const QModelIndex& index);
    void onTrackCollectionLoaded();
    void slotAddToAutoDJ();
    void slotAddToAutoDJTop();
    void slotImportAsMixxxPlaylist();

  private:
    static QString getiTunesMusicPath();
    //returns the invisible rootItem for the sidebar model
    TreeItem* importLibrary();
    void addToAutoDJ(bool bTop);
    QString detectMountPoint(QString BansheeMountPoint);

    QAction* m_pAddToAutoDJAction;
    QAction* m_pAddToAutoDJTopAction;
    QAction* m_pImportAsMixxxPlaylistAction;

    QModelIndex m_lastRightClickedIndex;

    BansheePlaylistModel* m_pBansheePlaylistModel;
    TreeItemModel m_childModel;
    QStringList m_playlists;
    TrackCollection* m_pTrackCollection;
    //a new DB connection for the worker thread
    QSqlDatabase m_database;
    bool m_isActivated;
    QString m_dbfile;

    QFutureWatcher<TreeItem*> m_future_watcher;
    QFuture<TreeItem*> m_future;
    QString m_title;
    bool m_cancelImport;

    QString m_dbItunesRoot;
    QString m_mixxxItunesRoot;

    Itdb_iTunesDB* m_itdb;

    static const QString BANSHEE_MOUNT_KEY;
};

#endif /* BANSHEEFEATURE_H */
