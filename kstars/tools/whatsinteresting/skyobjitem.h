/***************************************************************************
                          skyobjitem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/21/06
    copyright            : (C) 2012 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#ifndef SKYOBJITEM_H
#define SKYOBJITEM_H

#include <QObject>
#include "skyobject.h"

class SkyObjItem : public QObject
{
    Q_OBJECT
public:
    enum SkyObjectRoles {DispNameRole = Qt::UserRole + 1 , CategoryRole, CategoryNameRole };
    enum Type {Planet, Star, Constellation, Galaxy, Cluster, Nebula};
    explicit SkyObjItem( SkyObject *so=0, QObject *parent = 0);
    QVariant data(int role);
    QHash<int, QByteArray> roleNames() const;
    inline QString getName() const { return m_Name; }
    inline QString getTypeName() const { return m_TypeName; }
    inline int getType() const { return int(m_Type); }
    inline QString getPosition() const { return m_Position; }
    inline SkyObject* getSkyObject() { return m_So; }
    QString getDesc() const;
    QString getMagnitude() const;
    void setPosition(SkyObject* so);

private:
    QString m_Name;
    QString m_TypeName;
    QString m_Position;
    Type m_Type;
    SkyObject* m_So;
};

#endif // SKYOBJITEM_H