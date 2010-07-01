//=============================================================================
//  MuseScore
//  Linux Music Score Editor
//  $Id: trill.cpp 3229 2010-06-27 14:55:28Z wschweer $
//
//  Copyright (C) 2010 Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================

#include "fingering.h"

//---------------------------------------------------------
//   Fingering
//---------------------------------------------------------

Fingering::Fingering(Score* s)
  : Text(s)
      {
      setSubtype(TEXT_FINGERING);
      setTextStyle(TEXT_STYLE_FINGERING);
      }

//---------------------------------------------------------
//   layout
//---------------------------------------------------------

void Fingering::layout()
      {
      Text::layout();
      }

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void Fingering::write(Xml& xml) const
      {
      xml.stag(QString("%1").arg(name()));
      Text::writeProperties(xml);
      xml.etag();
      }

//---------------------------------------------------------
//   Trill::read
//---------------------------------------------------------

void Fingering::read(QDomElement e)
      {
      for (e = e.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
            QString tag(e.tagName());
            QString val(e.text());
            if (tag == "Accidental") {
                  }
            else if (!Text::readProperties(e))
                  domError(e);
            }
      }
