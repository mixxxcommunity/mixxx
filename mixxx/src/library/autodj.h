#ifndef AUTODJ_H
#define AUTODJ_H

#include <QObject>

#include "dlgautodj.h"

class PlayerManager;

class AutoDJ : public QObject {
    Q_OBJECT

public:
    AutoDJ(QObject* parent, DlgAutoDJ* dlgAutoDJ, PlayerManager* playerManager);
    ~AutoDJ();

public slots:
    void setEnabled(bool);
    void receiveNextTrack(TrackPointer nextTrack);

signals:
    void needNextTrack();

private:

    void refreshPlayerStates();

    DlgAutoDJ* m_pDlgAutoDJ;
    PlayerManager* m_pPlayerManager;
    bool m_bEnabled;


};

#endif // AUTODJ_H
