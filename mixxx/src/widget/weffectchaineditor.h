#ifndef WEFFECTCHAINEDITOR_H
#define WEFFECTCHAINEDITOR_H

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>
#include "wpushbutton.h"
#include "wknob.h"
#include <QMenu>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <QDomNode>
#include <QHash>
#include <QStringList>
#include <QSharedPointer>

#include "effects/effectchain.h"
#include "effects/effectchainslot.h"
#include "effects/effectsmanager.h"

#include "controlobjectthreadwidget.h"

class WEffectHolder;
class WEffectUnit;
class WKnobUnit;

const unsigned int leastChainNumber = 0;
const int illegalChainNumber = -1;
const int illegalEffectSlotNumber = -1;

class WEffectChainEditor : public QWidget
{
    Q_OBJECT

public:
    WEffectChainEditor (QDomNode effectUnitConfig, 
                        QWidget *pParent = 0);
    ~WEffectChainEditor ();


public slots:
    // connected to EffectChainSlot
    void slotChainLoaded (EffectChainPointer, unsigned int);

    // connected to m_pWChainNumber on GUI
    void slotChainNumberChanged (int);

    // connected to m_pWSearchBox on GUI
    void slotSearchEffect (QString);

    // connected to m_pWEffectsList on GUI
    void slotProcessEffectAddition (QListWidgetItem*);


signals:
    // connected to m_pChainSlot
    void addEffectToChain (QString);


private:
    QString debugString() const {
        return QString("EffectChainEditor(%1)").arg(m_chainNumber);
    } 

    // sets up list of effects, search box and monitered chian number
    void setupWSidebarLayout ();

    // setup/update chain name display widget 
    void setupWChainName (EffectChainPointer, unsigned int); 
    
    // setup WEffectHolder
    void setupWEffectHolder ();

    unsigned int m_chainNumber;
    EffectsManager *m_pEffectsManager;
    EffectChainSlotPointer m_pChainSlot;
    QHash<QString, QString> m_effects;
    QStringList m_effectsStringList;

    QComboBox *m_pWChainNumber;
    QLineEdit *m_pWSearchBox;
    QListWidget *m_pWEffectsList;
    QLineEdit *m_pWChainName;
    QScrollArea *m_pWScroller;
    WEffectHolder *m_pWEffectHolder;

    QGridLayout *m_pWMainLayout;
    QVBoxLayout *m_pWSideBarLayout;
};

class WEffectHolder : public QWidget
{
    Q_OBJECT

public:
    WEffectHolder (QDomNode effectUnitConfig, 
                   unsigned int chainNumber,
                   QWidget *pParent = 0);
    ~WEffectHolder ();

public slots:    
    // connected to WEffectChainEditor
    void slotChainNumberChanged (int);

    // connected to currently monitered ChainSlot
    void slotEffectLoaded(EffectPointer, unsigned int chainNumber, unsigned int slotNumber);


private:
    QString debugString() const {
        return QString("EffectChainHolder(%1)").arg(m_chainNumber);
    } 

    void addNewEffectUnit (int);

    // to update individual effect unit
    void updateEffectUnits ();

    QDomNode m_effectUnitConfig;
    unsigned int m_chainNumber;
    EffectChainSlotPointer m_pChainSlot;

    QVBoxLayout *m_pWLayout;    
    QList<WEffectUnit*> m_WEffectUnits;
};

class WEffectUnit : public QWidget
{
    Q_OBJECT

public:
    WEffectUnit (QDomNode effectUnitConfig,                 
                 EffectSlotPointer pEffectSlot = EffectSlotPointer(), 
                 unsigned int chainNumber = illegalChainNumber,
                 unsigned int effectNumber = illegalEffectSlotNumber,
                 QWidget *pParent = 0);

    ~WEffectUnit ();

    void loadEffectSlot (EffectSlotPointer pEffectSlot, 
                           unsigned int chainNumber, 
                           unsigned int effectSlotNumber);

public slots:
    // connected to EffectSlot
    void slotEffectLoaded (EffectPointer, unsigned int);

private:
    QString debugString() const {
        return QString("EffectUnit(%1,%2)").arg(m_chainNumber).arg(m_effectSlotNumber);
    } 

    void setName(QString);
    void setKnobUnits(int);
    void reset();
    void disconnectWidget (QWidget*);
    void connectWidget (QWidget*, ConfigKey);

    QDomNode m_knobConfig;
    QDomNode m_btnDeleteConfig;
    QDomNode m_btnMoveUpOneConfig;
    QDomNode m_btnMoveDownOneConfig;
    EffectSlotPointer m_pEffectSlot;
    unsigned int m_chainNumber;
    unsigned int m_effectSlotNumber;
    QMap<QWidget*, ControlObjectThreadWidget*> m_widgetCOTWs;

    QHBoxLayout *m_pWLayout;
    QLabel *m_pWEffectName;
    WPushButton *m_pWBtnDelete;
    WPushButton *m_pWBtnMoveUpOne;
    WPushButton *m_pWBtnMoveDownOne;
    QList<WKnobUnit*> m_WKnobUnits;
};

class WKnobUnit : public WKnob
{
    Q_OBJECT

public:
    WKnobUnit (QDomNode knobConfig, WKnob *pParent = 0)
    {
        setup(knobConfig);
        
        m_pMenu = new QMenu (this);
        m_pMenu->addAction ("Connect to Parameter Knob", 
                            this, 
                            SLOT (slotConnectToParameterKnob()));
    }
    ~WKnobUnit ()
    {}

    void updateConfigKeys (ConfigKey parameterKnobKey, ConfigKey knobKey)
    {
        m_parameterKnobKey = parameterKnobKey;
        m_knobKey = knobKey;
    }

protected:
    void mousePressEvent(QMouseEvent *e)
    {
        switch (e->button()) 
        {
            case Qt::RightButton:
                qDebug() << debugString() << "rightButtonClicked";
                m_pMenu->popup (QCursor::pos());

                break;

            default:
                WKnob::mousePressEvent (e);

        }
    }

private slots:
    void slotConnectToParameterKnob()
    {
        if (ControlObject::connectControls (m_parameterKnobKey, m_knobKey))
            qDebug() << debugString() << "connected to parameter knob";
        else
            qDebug() << debugString() << "can NOT connect to parameter knob";
    }


private:
    QString debugString() const 
    {                                                        
      return QString("KnobUnit");
    }

    ConfigKey m_parameterKnobKey;
    ConfigKey m_knobKey;
    QMenu *m_pMenu;
    
};

#endif  //WEFFECTCHAINEDITOR_H
