#ifndef BANSHEEDBCONNECTION_H
#define BANSHEEDBCONNECTION_H

#include <QSqlDatabase>

class BansheeDbConnection
{
public:
    BansheeDbConnection();
    virtual ~BansheeDbConnection();

    static QString getDatabaseFile();

    bool open(const QString& databaseFile);
    int getSchemaVersion();
    QList<QPair<QString, QString> > getPlaylists();

private:
    QSqlDatabase m_database;

};

#endif // BANSHEEDBCONNECTION_H
