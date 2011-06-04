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
    void setEnabled(bool);

signals:

private:
    bool m_bEnabled;
    PlayerManager* m_pPlayerManager;



};

#endif // AUTODJ_H
