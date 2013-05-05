#ifndef CONFIGFILEPARSER_H
#define CONFIGFILEPARSER_H

#include <QSettings>
#include <QIODevice>

bool readConfig(QIODevice &device, QSettings::SettingsMap &map);
bool writeConfig(QIODevice &device, const QSettings::SettingsMap &map);

#endif // CONFIGFILEPARSER_H
