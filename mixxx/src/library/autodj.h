#ifndef AUTODJ_H
#define AUTODJ_H

#include <QObject>
class PlayerManager;

class AutoDJ : public QObject
{
    Q_OBJECT

public:
    AutoDJ(QObject* parent, PlayerManager* playerManager);
    ~AutoDJ();

public slots:

signals:

private:

    PlayerManager* m_pPlayerManager;



};

#endif // AUTODJ_H
