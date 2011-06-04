#ifndef AUTODJ_H
#define AUTODJ_H

#include <QObject>
class PlayerManager;

class AutoDJ : public QObject {
    Q_OBJECT

public:
    AutoDJ(QObject* parent, PlayerManager* playerManager);
    ~AutoDJ();

public slots:
    void setEnabled(bool);

signals:
    void needNextTrack();

private:
    PlayerManager* m_pPlayerManager;
    bool m_bEnabled;


};

#endif // AUTODJ_H
