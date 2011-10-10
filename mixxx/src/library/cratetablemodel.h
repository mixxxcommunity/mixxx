// cratetablemodel.h
// Created 10/25/2009 by RJ Ryan (rryan@mit.edu)

#ifndef CRATETABLEMODEL_H
#define CRATETABLEMODEL_H

#include <QItemDelegate>
#include <QSqlTableModel>

#include "library/basesqltablemodel.h"

class TrackCollection;

class CrateTableModel : public BaseSqlTableModel {
    Q_OBJECT
  public:
    CrateTableModel(QObject* parent, TrackCollection* pTrackCollection);
    virtual ~CrateTableModel();

    void setCrate(int crateId);

    // From TrackModel
    virtual TrackPointer getTrack(const QModelIndex& index) const;
    virtual void search(const QString& searchText);
    virtual bool isColumnInternal(int column);
    virtual bool isColumnHiddenByDefault(int column);
    virtual void removeTrack(const QModelIndex& index);
    virtual void removeTracks(const QModelIndexList& indices);
    virtual bool addTrack(const QModelIndex& index, QString location);
    virtual void moveTrack(const QModelIndex& sourceIndex,
                           const QModelIndex& destIndex);
    TrackModel::CapabilitiesFlags getCapabilities() const;
    virtual QItemDelegate* delegateForColumn(const int i);

  private slots:
    void slotSearch(const QString& searchText);

  signals:
    void doSearch(const QString& searchText);

  private:
    TrackCollection* m_pTrackCollection;
    int m_iCrateId;
};

#endif /* CRATETABLEMODEL_H */
