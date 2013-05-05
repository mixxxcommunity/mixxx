#include "configfileparser.h"
#include <QDebug>
#include <QTextStream>
#include <QString>
#include <QRegExp>

bool readConfig(QIODevice &device, QSettings::SettingsMap &map){
    qDebug() << "kain88 reading file";

    QTextStream text(&device);
    int group = 0;
    QString groupStr, line;
    text.setCodec("UTF-8");

    while (!text.atEnd()) {
        line = text.readLine().trimmed();

        if (line.length() != 0) {
            if (line.startsWith("[") && line.endsWith("]")) {
                group++;
                groupStr = line;
                groupStr.chop(1);
                groupStr = groupStr.remove(0,1);
                // TODO kain88 check regexp
                //groupStr = line.remove(QRegExp("\[.*?\]"));
            } else if (group>0) {
                QString key;
                QTextStream(&line) >> key;
                key = groupStr+"/"+key;
                QString val = line.right(line.length() - key.length()).trimmed();
                qDebug() << "parser: Key="<<key<<", value="<<val;
                map[key] = val;
            }
        } 
    }


    return true;
}

bool writeConfig(QIODevice &device, const  QSettings::SettingsMap &map) {
    qDebug() << "kain88 writing file";
    return true;
}
