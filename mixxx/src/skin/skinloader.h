#ifndef SKINLOADER_H
#define SKINLOADER_H

#include <QWidget>
#include <QSettings>

#include "configobject.h"

class MixxxKeyboard;
class PlayerManager;
class ControllerManager;
class Library;
class MixxxView;
class VinylControlManager;

class SkinLoader {
  public:
    SkinLoader(ConfigObject<ConfigValue>* pConfig);
    ~SkinLoader();
    QWidget* loadDefaultSkin(QWidget* pParent,
                             MixxxKeyboard* pKeyboard,
                             PlayerManager* pPlayerManager,
                             ControllerManager* pControllerManager,
                             Library* pLibrary,
                             VinylControlManager* pVCMan);

    QString getConfiguredSkinPath();

  private:
    ConfigObject<ConfigValue>* m_pConfig;
    QSettings m_settings;
};


#endif /* SKINLOADER_H */

