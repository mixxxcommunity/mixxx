#include <QSet>

#include "controlobject.h"
#include "weffectchaineditor.h"
#include "effects/effectmanifest.h"

WEffectChainEditor::WEffectChainEditor (QDomNode effectUnitConfig, 
                                        QWidget *pParent)
    : QWidget (pParent),
      m_chainNumber (illegalChainNumber),
      m_pEffectsManager (EffectsManager::getEffectsManager ()),
      m_pChainSlot (NULL),
      m_effects (),
      m_effectsStringList (),

      m_pWChainNumber (new QComboBox (this)),
      m_pWSearchBox (new QLineEdit (this)),
      m_pWEffectsList (new QListWidget (this)),
      m_pWChainName (new QLineEdit (this)),
      m_pWScroller (new QScrollArea (this)),
      m_pWEffectHolder (new WEffectHolder (effectUnitConfig, 
                                           m_chainNumber, 
                                           this)),

      m_pWMainLayout (new QGridLayout (this)),
      m_pWSideBarLayout (new QVBoxLayout (this))
{

    qDebug() << debugString() << "SHASHANK ctor here.......................";
    //
    // setup non-GUI elements
    //
    QSet<QString> setEffects = m_pEffectsManager->getAvailableEffects ();
    QSet<QString>::const_iterator iterEffects = setEffects.constBegin ();
    while (iterEffects != setEffects.constEnd())
    {
        EffectManifestPointer pManifest = m_pEffectsManager->getEffectManifest (*iterEffects);
        m_effects.insert (pManifest->name(), *iterEffects);

        iterEffects++;
    }

    m_effectsStringList = QStringList (m_effects.keys ());

    //
    // setup GUI elements
    //
    setupWEffectHolder ();
    setupWSidebarLayout ();
    
    m_pWMainLayout->addLayout (m_pWSideBarLayout, 0, 0, 2, 1);
    m_pWMainLayout->addWidget (m_pWChainName, 0, 1, 1, 3);
    m_pWMainLayout->addWidget (m_pWScroller, 1, 1, 1, 3);
    m_pWMainLayout->setColumnStretch (0, 10);
    m_pWMainLayout->setColumnStretch (2, 30);

    setLayout (m_pWMainLayout);

    qDebug() << debugString() << "SHASHANK ctor ends here.......................";

}

WEffectChainEditor::~WEffectChainEditor ()
{}

void WEffectChainEditor::slotChainLoaded (EffectChainPointer pChain, unsigned int chainNumber)
{
    qDebug() << debugString() << "New chain loaded in " <<  chainNumber;

    // update widget to display chain name
    setupWChainName (pChain, chainNumber);
}

void WEffectChainEditor::slotChainNumberChanged (int newChainNumber)
{
   
    qDebug() << debugString() << "Change monitered slot to " << m_chainNumber+1;
   
    EffectChainSlotPointer pNewChainSlot = m_pEffectsManager->getEffectChainSlot (newChainNumber);

    // disconnect old connections
    disconnect (this, 0, m_pChainSlot.data(), 0);
    disconnect (m_pChainSlot.data(), 0, this, 0);

    // make new connections
    connect (this, SIGNAL (addEffectToChain (QString)),
         pNewChainSlot.data(), SLOT (slotAddEffectToChain (QString)));

    connect (pNewChainSlot.data(), SIGNAL (effectChainLoaded (EffectChainPointer, unsigned int)),
            this, SLOT (slotChainLoaded (EffectChainPointer, unsigned int)));

    // explicit call to slotChainLoaded to update related widgets
    slotChainLoaded (pNewChainSlot->getEffectChain (), newChainNumber);

    // WARNING: this update must be done after updating every
    // other widget.
    //
    // update other variables
    m_chainNumber = newChainNumber;
    m_pChainSlot = pNewChainSlot;

    // TODO (shanxS) maybe this is a good place to refresh
    // available effects ?

}

void WEffectChainEditor::slotSearchEffect (QString str)
{
    qDebug() << debugString() << "Search string" << str;

    // clear displayed output
    m_pWEffectsList->clear ();

    // if no search string - display all effects available
    if (str.size() == 0)
    {
        for (int i=0; i<m_effectsStringList.size(); i++)
            m_pWEffectsList->addItem (m_effectsStringList.at (i));

        return;
    }

    // search for name
    QStringList result = m_effectsStringList.filter (str, Qt::CaseInsensitive);

    // load results
    for (int i=0; i<result.size(); i++)
        m_pWEffectsList->addItem (result.at (i));

}

void WEffectChainEditor::slotProcessEffectAddition (QListWidgetItem *item)
{
    emit (addEffectToChain (m_effects.value (item->text ())));
}

void WEffectChainEditor::setupWSidebarLayout ()
{
    //
    // setup ComboBox to choose monitered chain number
    //
    QString chainNum = "Moniter Chain Number %1";
    unsigned int numChainSlots = m_pEffectsManager->numEffectChainSlots ();
    for (int i=1; i<=numChainSlots; i++)
        m_pWChainNumber->addItem (chainNum.arg (i));
    
    m_pWSideBarLayout->addWidget (m_pWChainNumber);
    connect (m_pWChainNumber, SIGNAL (currentIndexChanged (int)),
             this, SLOT (slotChainNumberChanged (int)));
    slotChainNumberChanged (leastChainNumber);
    connect (m_pWChainNumber, SIGNAL (currentIndexChanged (int)),
             m_pWEffectHolder, SLOT (slotChainNumberChanged (int)));
    m_pWEffectHolder->slotChainNumberChanged (leastChainNumber);

    //
    // setup search box
    //
    m_pWSearchBox->setPlaceholderText ("Effects Search Box");
    m_pWSideBarLayout->addWidget (m_pWSearchBox);
    connect (m_pWSearchBox, SIGNAL (textChanged (QString)),
             this, SLOT (slotSearchEffect (QString)));

    //
    // setup available effects list
    //
    for (int i=0; i<m_effectsStringList.size(); i++)
        m_pWEffectsList->addItem (m_effectsStringList.at (i));
    
    m_pWSideBarLayout->addWidget (m_pWEffectsList);
    connect (m_pWEffectsList, SIGNAL (itemClicked (QListWidgetItem*)),
             this, SLOT (slotProcessEffectAddition (QListWidgetItem*)));
}

void WEffectChainEditor::setupWChainName (EffectChainPointer pNewChain, unsigned int newChainNumber)
{
    // WARNING: on event of ChainSlot change
    // this widget must be updated before data members of
    // this class are updated

    // check if ChianSlot has changed
    if (newChainNumber != m_chainNumber)
    {

        // disconnect older connections
        disconnect (m_pWChainName, 0, m_pChainSlot.data(), 0);
        disconnect (m_pChainSlot.data(), 0, m_pWChainName, 0);

        // get new ChainSlot
        EffectChainSlotPointer pNewChainSlot = m_pEffectsManager->getEffectChainSlot (newChainNumber);

        // update connections    
        connect (m_pWChainName, SIGNAL (textEdited (QString)),
             pNewChainSlot.data(), SLOT (slotRenameChain (QString)));
        connect (pNewChainSlot.data(), SIGNAL (chainRenamed (QString)),
             m_pWChainName, SLOT (setText (QString)));

    }
                                                                   
    // reset text
    if (pNewChain.isNull())
    {
        m_pWChainName->setText ("");
        m_pWChainName->setPlaceholderText ("No Chain Loaded");
    }
    else
    {
        m_pWChainName->setText (pNewChain->name());
        m_pWChainName->setPlaceholderText ("Chain Unnamed");
    }
                                                                   
}

void WEffectChainEditor::setupWEffectHolder ()
{
    m_pWScroller->setWidgetResizable (true);
    m_pWScroller->setWidget (m_pWEffectHolder);



}

WEffectHolder::WEffectHolder (QDomNode effectUnitConfig,
                              unsigned int chainNumber,
                              QWidget *pParent)
    : QWidget (pParent),
      m_effectUnitConfig (effectUnitConfig),
      m_chainNumber (chainNumber),
      m_pChainSlot (NULL),

      m_pWLayout (new QVBoxLayout (this)),      
      m_WEffectUnits ()
{
    // TODO (shanxS) should alignment be customised from
    // skin ?
    m_pWLayout->setAlignment (Qt::AlignTop);

    setLayout (m_pWLayout);
}

WEffectHolder::~WEffectHolder()
{}

void WEffectHolder::slotChainNumberChanged (int newChainNumber)
{
    qDebug() << debugString() << "slotChainNumberChanged" << newChainNumber;

    // return if previous ChainSlot is same as
    // current ChainSlot
    if (newChainNumber == m_chainNumber)
        return;
    
    // disconnect from previous ChainSlot
    disconnect (m_pChainSlot.data(), 0, this, 0);
    disconnect (this, 0, m_pChainSlot.data(), 0);

    // get new ChainSlot
    m_chainNumber = newChainNumber;
    m_pChainSlot = (EffectsManager::getEffectsManager())->getEffectChainSlot (m_chainNumber);

    // update connections
    connect (m_pChainSlot.data(), SIGNAL (effectLoaded (EffectPointer, unsigned int, unsigned int)),
             this, SLOT (slotEffectLoaded (EffectPointer, unsigned int, unsigned int)));

    // add more EffectUnits if needed
    int requiredUnits = 0;
    if (!m_pChainSlot.isNull())
        requiredUnits = m_pChainSlot->numSlots();

    int newUnits = requiredUnits - m_WEffectUnits.size();
    addNewEffectUnit (newUnits);

    // update EffectUnits
    updateEffectUnits ();
}

void WEffectHolder::slotEffectLoaded(EffectPointer pEffect, unsigned int chainNumber, unsigned int effectSlotNumber)
{
    // if more EffectUnits are needed create them and init. them
    if (effectSlotNumber >= m_WEffectUnits.size())
        // since indicies reported by EffectChainSlot is 0 based
        // we need to check for equality.
        //
        // TODO (shanxS) should we add more than required slots
        // the way std::vecot behaves
        addNewEffectUnit ((effectSlotNumber - m_WEffectUnits.size()) + 1);
}

void WEffectHolder::addNewEffectUnit (int count)
{
    while (count)
    {
        int effectSlotNumber = m_WEffectUnits.size();

        WEffectUnit *pEffectUnit = new WEffectUnit (m_effectUnitConfig,
                                              m_pChainSlot->getEffectSlot (effectSlotNumber),
                                              m_chainNumber, 
                                              effectSlotNumber);
        m_WEffectUnits.append (pEffectUnit);
        m_pWLayout->addWidget (pEffectUnit);
        --count;
    }
}

void WEffectHolder::updateEffectUnits()
{
    if (m_pChainSlot.isNull())
    {
        qWarning() << debugString() << "updateEffectUnits"
                   << "attempt to update using NULL ChainSlot";
        return;           
    }

    for (int i=0; i<m_WEffectUnits.size(); i++)
    {
        WEffectUnit *pEffectUnit = m_WEffectUnits[i];
        pEffectUnit->loadEffectSlot (m_pChainSlot->getEffectSlot (i),
                               m_chainNumber,
                               i);
    }
}

WEffectUnit::WEffectUnit (QDomNode effectUnitConfig, 
                          EffectSlotPointer pEffectSlot,
                          unsigned int chainNumber, 
                          unsigned int effectSlotNumber, 
                          QWidget *pParent)
    : QWidget (pParent),
      m_knobConfig (effectUnitConfig.firstChildElement ("Knob")),
      m_btnDeleteConfig (effectUnitConfig.firstChildElement ("DeleteButton")),
      m_btnMoveUpOneConfig (effectUnitConfig.firstChildElement ("MoveUpOneButton")),
      m_btnMoveDownOneConfig (effectUnitConfig.firstChildElement ("MoveDownOneButton")),
      m_pEffectSlot (pEffectSlot),
      m_chainNumber (chainNumber),
      m_effectSlotNumber (effectSlotNumber),
      m_widgetCOTWs (),

      m_pWLayout (new QHBoxLayout (this)),
      m_pWEffectName (new QLabel (this)),
      m_pWBtnDelete (new WPushButton (this)),
      m_pWBtnMoveUpOne (new WPushButton (this)),
      m_pWBtnMoveDownOne (new WPushButton (this)),
      m_WKnobUnits ()
{
    // TODO (shanxS) should this be customised from skin ?
    m_pWEffectName->setFixedSize (100, 20);

    m_pWBtnDelete->setup (m_btnDeleteConfig);
    ControlObjectThreadWidget *pDeleteCOTW = NULL;
    m_widgetCOTWs.insert (m_pWBtnDelete, pDeleteCOTW);

    m_pWBtnMoveUpOne->setup (m_btnMoveUpOneConfig);
    ControlObjectThreadWidget *pMoveUpOneCOTW = NULL;
    m_widgetCOTWs.insert (m_pWBtnMoveUpOne, pMoveUpOneCOTW);

    m_pWBtnMoveDownOne->setup (m_btnMoveDownOneConfig);
    ControlObjectThreadWidget *pMoveDownOneCOTW = NULL;
    m_widgetCOTWs.insert (m_pWBtnMoveDownOne, pMoveDownOneCOTW);

    // TODO (shanxS) should alignment be customised from
    // skin ?
    m_pWLayout->setAlignment (Qt::AlignLeft);
    m_pWLayout->addWidget (m_pWEffectName);
    m_pWLayout->addWidget (m_pWBtnDelete);
    m_pWLayout->addWidget (m_pWBtnMoveUpOne);
    m_pWLayout->addWidget (m_pWBtnMoveDownOne);
    setLayout (m_pWLayout);

    loadEffectSlot (m_pEffectSlot, m_chainNumber, m_effectSlotNumber);
}

WEffectUnit::~WEffectUnit ()
{
    foreach (WKnobUnit *pKnobUnit, m_WKnobUnits)
        delete pKnobUnit;
}

void WEffectUnit::loadEffectSlot (EffectSlotPointer pEffectSlot,
                               unsigned int chainNumber, 
                               unsigned int effectSlotNumber)
{
    qDebug() << debugString() << "loadEffectSlot" 
                              << chainNumber 
                              << effectSlotNumber;
    
    // disconnect from previous EffectSlot
    disconnect (m_pEffectSlot.data(), 0, this, 0);   
    disconnect (this, 0, m_pEffectSlot.data(), 0);

    disconnectWidget (m_pWBtnDelete);
    disconnectWidget (m_pWBtnMoveUpOne);
    disconnectWidget (m_pWBtnMoveDownOne);

    if (m_pEffectSlot.isNull())
    {
        reset();
        return;
    }

    connect (pEffectSlot.data(), SIGNAL (effectLoaded (EffectPointer, unsigned int)),
             this, SLOT (slotEffectLoaded (EffectPointer, unsigned int)));

    ConfigKey deleteKey = ConfigKey (EffectSlot::formatGroupString (m_chainNumber, m_effectSlotNumber), "delete");
    connectWidget (m_pWBtnDelete, deleteKey);
    
    ConfigKey moveUpOneKey = ConfigKey (EffectSlot::formatGroupString (m_chainNumber, m_effectSlotNumber), "move_effect_up_one");
    connectWidget (m_pWBtnMoveUpOne, moveUpOneKey);

    ConfigKey moveDownOneKey = ConfigKey (EffectSlot::formatGroupString (m_chainNumber, m_effectSlotNumber), "move_effect_down_one");
    connectWidget (m_pWBtnMoveDownOne, moveDownOneKey);

    m_pEffectSlot = pEffectSlot;
    m_chainNumber = chainNumber;
    m_effectSlotNumber = effectSlotNumber;

    slotEffectLoaded (m_pEffectSlot->getEffect(), m_effectSlotNumber);
}

void WEffectUnit::slotEffectLoaded (EffectPointer pEffect, unsigned int effectSlotNumber)
{
    qDebug() << debugString() << "slotEffectLoaded";

    if (effectSlotNumber != m_effectSlotNumber)
    {
        qWarning() << debugString() << "receiving signal from wrong slot" << effectSlotNumber;
        return;
    }

    QString name;
    int numKnobUnits = 0;

    if (pEffect.isNull())
    {
        this->setVisible(false);
    }
    else
    {
        name = m_pEffectSlot->name();
        numKnobUnits = m_pEffectSlot->numSlots();
        this->setVisible (true);
    }

    setName (name);
    setKnobUnits (numKnobUnits);
}

void WEffectUnit::setName (QString name)
{
    m_pWEffectName->setText(name);
}

void WEffectUnit::setKnobUnits (int numKnobUnits)
{
    // add new KnobUnits if needed
    while (m_WKnobUnits.size() < numKnobUnits)
    {
        // make new KnobUnits
        WKnobUnit *pKnobUnit = new WKnobUnit (m_knobConfig);
        m_WKnobUnits.append (pKnobUnit);

        // make entries for COTWs of thses KnowUnits
        ControlObjectThreadWidget *pKnobUnitCOTW = NULL;
        m_widgetCOTWs.insert (pKnobUnit, pKnobUnitCOTW);

        // add KnobUnit to GUI
        m_pWLayout->addWidget (pKnobUnit);
    }

    // update connections and visiblilty of KnobUnits
    for (int i=0; i<m_WKnobUnits.size(); i++)
    {
        WKnobUnit *pKnobUnit = m_WKnobUnits[i];
        disconnectWidget (pKnobUnit);
        
        if (i<numKnobUnits)
        {
            ConfigKey knobUnitKey = ConfigKey (EffectParameterSlot::formatGroupString (m_chainNumber, m_effectSlotNumber, i), "value_normalized");
            connectWidget (pKnobUnit, knobUnitKey);

            pKnobUnit->updateConfigKeys (ConfigKey (EffectChainSlot::formatGroupString (m_chainNumber), "parameter"), knobUnitKey);
            pKnobUnit->setVisible(true);
        }
        else
            pKnobUnit->setVisible(false);

    }
}

void WEffectUnit::reset()
{
    this->setVisible(false);

    m_pEffectSlot.clear();
    m_chainNumber = illegalChainNumber;
    m_effectSlotNumber = illegalEffectSlotNumber;

    setName("");
    // TODO disconnect all knobs
}

void WEffectUnit::disconnectWidget (QWidget *pWidget)
{
    ControlObjectThreadWidget *pCOTW = m_widgetCOTWs.value (pWidget);
    delete pCOTW;

    pCOTW = NULL;
    m_widgetCOTWs.insert (pWidget, pCOTW);
}

void WEffectUnit::connectWidget (QWidget *pWidget, ConfigKey key)
{
    ControlObject *pCO = ControlObject::getControl (key);
    if (pCO == NULL)
    {
        qWarning() << debugString() 
                   << "ControlObject is null for" 
                   << key.item;
        return;
    }

    ControlObjectThreadWidget *pCOTW = new ControlObjectThreadWidget (pCO, pWidget);
    pCOTW->setWidget (pWidget, 
                      true, 
                      true, 
                      ControlObjectThreadWidget::EMIT_ON_PRESS_AND_RELEASE,
                      Qt::NoButton);

    m_widgetCOTWs.insert (pWidget, pCOTW);
}
