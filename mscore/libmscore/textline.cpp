//=============================================================================
//  MuseScore
//  Music Composition & Notation
//  $Id$
//
//  Copyright (C) 2002-2011 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#include "textline.h"
#include "style.h"
#include "system.h"
#include "measure.h"
#include "xml.h"
#include "utils.h"
#include "score.h"
#include "sym.h"
#include "text.h"
#include "mscore.h"

//---------------------------------------------------------
//   TextLineSegment
//---------------------------------------------------------

TextLineSegment::TextLineSegment(Score* s)
   : LineSegment(s)
      {
      _text = 0;
      }

TextLineSegment::TextLineSegment(const TextLineSegment& seg)
   : LineSegment(seg)
      {
      _text = 0;
      layout();    // set right _text
      }

//---------------------------------------------------------
//   setSelected
//---------------------------------------------------------

void TextLineSegment::setSelected(bool f)
      {
      Element::setSelected(f);
      if (_text) {
            if (subtype() == SEGMENT_SINGLE || subtype() == SEGMENT_BEGIN) {
                  if (textLine()->beginText())
                        _text->setSelected(f);
                  }
            else if (textLine()->continueText())
                  _text->setSelected(f);
            }
      }

//---------------------------------------------------------
//   draw
//---------------------------------------------------------

void TextLineSegment::draw(QPainter* painter) const
      {
      TextLine* tl    = textLine();
      qreal _spatium = spatium();

      qreal textlineLineWidth    = tl->lineWidth().val() * _spatium;
      qreal textlineTextDistance = _spatium * .5;

      QPointF pp2(pos2());

      QColor color;
      bool normalColor = false;
      if (selected() && !(score() && score()->printing()))
            color = MScore::selectColor[0];
      else if (!visible())
            color = Qt::gray;
      else {
            color = curColor();
            normalColor = true;
            }

      qreal l = 0.0;
      int sym = subtype() == SEGMENT_MIDDLE ? tl->continueSymbol() : tl->beginSymbol();
      if (_text) {
            SpannerSegmentType st = subtype();
            if (
               ((st == SEGMENT_SINGLE || st == SEGMENT_BEGIN) && (tl->beginTextPlace() == PLACE_LEFT))
               || ((st == SEGMENT_MIDDLE || st == SEGMENT_END) && (tl->continueTextPlace() == PLACE_LEFT))
               ) {
                  QRectF bb(_text->bbox());
                  l = _text->pos().x() + bb.width() + textlineTextDistance;
                  }
            painter->translate(_text->pos());
            painter->setPen(normalColor ? _text->curColor() : color);
            _text->draw(painter);
            painter->translate(-_text->pos());
            }
      else if (sym != -1) {
            const QRectF& bb = symbols[score()->symIdx()][sym].bbox(magS());
            qreal h = bb.height() * .5;
            QPointF o = tl->beginSymbolOffset() * _spatium;
            painter->setPen(color);
            symbols[score()->symIdx()][sym].draw(painter, 1.0, QPointF(o.x(), h + o.y()));
            l = bb.width() + textlineTextDistance;
            }

      QPen pen(normalColor ? tl->lineColor() : color, textlineLineWidth);
      pen.setStyle(tl->lineStyle());
      painter->setPen(pen);

      if (subtype() == SEGMENT_SINGLE || subtype() == SEGMENT_END) {
            if (tl->endSymbol() != -1) {
                  int sym = tl->endSymbol();
                  const QRectF& bb = symbols[score()->symIdx()][sym].bbox(magS());
                  qreal h = bb.height() * .5;
                  QPointF o = tl->endSymbolOffset() * _spatium;
                  pp2.setX(pp2.x() - bb.width() + textlineTextDistance);
                  symbols[score()->symIdx()][sym].draw(painter, 1.0, QPointF(pp2.x() + textlineTextDistance + o.x(), h + o.y()));
                  }
            }

      QPointF pp1(l, 0.0);

      if (tl->beginHook() && tl->beginHookType() == HOOK_45)
            pp1.rx() += fabs(tl->beginHookHeight().val() * _spatium * .4);
      if (tl->endHook() && tl->endHookType() == HOOK_45)
            pp2.rx() -= fabs(tl->endHookHeight().val() * _spatium * .4);
      painter->drawLine(QLineF(pp1.x(), pp1.y(), pp2.x(), pp2.y()));

      if (tl->beginHook()) {
            qreal hh = tl->beginHookHeight().val() * _spatium;
            if (subtype() == SEGMENT_SINGLE || subtype() == SEGMENT_BEGIN) {
                  if (tl->beginHookType() == HOOK_45)
                        painter->drawLine(QLineF(pp1.x(), pp1.y(), pp1.x() - fabs(hh * .4), pp1.y() + hh));
                  else
                        painter->drawLine(QLineF(pp1.x(), pp1.y(), pp1.x(), pp1.y() + hh));
                  }
            }
      if (tl->endHook()) {
            qreal hh = tl->endHookHeight().val() * _spatium;
            if (subtype() == SEGMENT_SINGLE || subtype() == SEGMENT_END) {
                  if (tl->endHookType() == HOOK_45)
                        painter->drawLine(QLineF(pp2.x(), pp2.y(), pp2.x() + fabs(hh * .4), pp2.y() + hh));
                  else
                        painter->drawLine(QLineF(pp2.x(), pp2.y(), pp2.x(), pp2.y() + hh));
                  }
            }
      }

//---------------------------------------------------------
//   layout
//---------------------------------------------------------

void TextLineSegment::layout()
      {
      TextLine* tl = static_cast<TextLine*>(line());
      if (!tl->diagonal())
            _userOff2.setY(0);
      switch (subtype()) {
            case SEGMENT_SINGLE:
            case SEGMENT_BEGIN:
                  if (tl->beginText()) {
                        if (_text == 0) {
                              _text = new Text(*tl->beginText());
                              _text->setFlag(ELEMENT_MOVABLE, false);
                              _text->setParent(this);
                              }
                        }
                  else {
                        delete _text;
                        _text = 0;
                        }
                  break;
            case SEGMENT_MIDDLE:
            case SEGMENT_END:
                  if (tl->continueText()) {
                        if (_text == 0) {
                              _text = new Text(*tl->continueText());
                              _text->setFlag(ELEMENT_MOVABLE, false);
                              _text->setParent(this);
                              }
                        }
                  else {
                        delete _text;
                        _text = 0;
                        }
                  break;
            }
      if (_text)
            _text->layout();

      QPointF pp1;
      QPointF pp2(pos2());

      if (!_text && pp2.y() != 0) {
            setbbox(QRectF(pp1, pp2).normalized());
            return;
            }
      qreal y1 = point(-textLine()->lineWidth());
      qreal y2 = -y1;

      int sym = textLine()->beginSymbol();
      if (_text) {
            qreal h = _text->height();
            if (textLine()->beginTextPlace() == PLACE_ABOVE)
                  y1 = -h;
            else if (textLine()->beginTextPlace() == PLACE_BELOW)
                  y2 = h;
            else {
                  y1 = -h * .5;
                  y2 = h * .5;
                  }
            }
      else if (sym != -1) {
            qreal hh = symbols[score()->symIdx()][sym].height(magS()) * .5;
            y1 = -hh;
            y2 = hh;
            }
      if (textLine()->endHook()) {
            qreal h = point(textLine()->endHookHeight());
            if (h > y2)
                  y2 = h;
            else if (h < y1)
                  y1 = h;
            }
      if (textLine()->beginHook()) {
            qreal h = point(textLine()->beginHookHeight());
            if (h > y2)
                  y2 = h;
            else if (h < y1)
                  y1 = h;
            }
      setbbox(QRectF(.0, y1, pp2.x(), y2 - y1));
      }

//---------------------------------------------------------
//   clearText
//---------------------------------------------------------

void TextLineSegment::clearText()
      {
      delete _text;
      _text = 0;
      }

//---------------------------------------------------------
//   TextLine
//---------------------------------------------------------

TextLine::TextLine(Score* s)
   : SLine(s)
      {
      _beginText         = 0;
      _continueText      = 0;

      _beginHookHeight   = Spatium(1.5);
      _endHookHeight     = Spatium(1.5);
      _beginHook         = false;
      _endHook           = false;
      _beginHookType     = HOOK_90;
      _endHookType       = HOOK_90;

      _lineWidth         = Spatium(0.15);
      _lineStyle         = Qt::SolidLine;
      _beginTextPlace    = PLACE_LEFT;
      _continueTextPlace = PLACE_LEFT;
      _lineColor         = Qt::black;
      _mxmlOff2          = 0;

      _beginSymbol       = -1;
      _continueSymbol    = -1;
      _endSymbol         = -1;
      _sp                = 0;
      }

TextLine::TextLine(const TextLine& e)
   : SLine(e)
      {
      _lineWidth            = e._lineWidth;
      _lineColor            = e._lineColor;
      _lineStyle            = e._lineStyle;
      _beginTextPlace       = e._beginTextPlace;
      _continueTextPlace    = e._continueTextPlace;

      _beginHook            = e._beginHook;
      _endHook              = e._endHook;
      _beginHookType        = e._beginHookType;
      _endHookType          = e._endHookType;
      _beginHookHeight      = e._beginHookHeight;
      _endHookHeight        = e._endHookHeight;

      _beginSymbol          = e._beginSymbol;
      _continueSymbol       = e._continueSymbol;
      _endSymbol            = e._endSymbol;
      _beginSymbolOffset    = e._beginSymbolOffset;
      _continueSymbolOffset = e._continueSymbolOffset;
      _endSymbolOffset      = e._endSymbolOffset;
      _mxmlOff2             = e._mxmlOff2;
      _beginText            = 0;
      _continueText         = 0;
      if (e._beginText) {
            _beginText = e._beginText->clone(); // deep copy
            _beginText->setParent(this);
            }
      if (e._continueText) {
            _continueText = e._continueText->clone();
            _continueText->setParent(this);
            }
      _sp = 0;
      }

//---------------------------------------------------------
//   setBeginText
//---------------------------------------------------------

void TextLine::setBeginText(const QString& s, const TextStyle& textStyle)
      {
      if (!_beginText) {
            _beginText = new Text(score());
            _beginText->setParent(this);
            }
      _beginText->setTextStyle(textStyle);
      _beginText->setText(s);
      }

//---------------------------------------------------------
//   setContinueText
//---------------------------------------------------------

void TextLine::setContinueText(const QString& s, const TextStyle& textStyle)
      {
      if (!_continueText) {
            _continueText = new Text(score());
            _continueText->setParent(this);
            }
      _continueText->setTextStyle(textStyle);
      _continueText->setText(s);
      }

//---------------------------------------------------------
//   setBeginText
//---------------------------------------------------------

void TextLine::setBeginText(Text* v)
      {
      delete _beginText;
      _beginText = v;
      }

//---------------------------------------------------------
//   setContinueText
//---------------------------------------------------------

void TextLine::setContinueText(Text* v)
      {
      delete _continueText;
      _continueText = v;
      }

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void TextLine::write(Xml& xml) const
      {
      xml.stag(QString("%1 id=\"%2\"").arg(name()).arg(id()));
      writeProperties(xml);
      xml.etag();
      }

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void TextLine::read(const QDomElement& de)
      {
      foreach(SpannerSegment* seg, spannerSegments())
            delete seg;
      spannerSegments().clear();
      setId(de.attribute("id", "-1").toInt());
      for (QDomElement e = de.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
            if (!readProperties(e))
                  domError(e);
            }
      }

//---------------------------------------------------------
//   writeProperties
//    write properties different from prototype
//---------------------------------------------------------

void TextLine::writeProperties(Xml& xml, const TextLine* proto) const
      {
      if (_beginHook) {
            if (proto == 0 || proto->beginHookHeight() != _beginHookHeight)
                  xml.tag("beginHookHeight", _beginHookHeight.val());
            if (proto == 0 || proto->beginHookType() != _beginHookType)
                  xml.tag("beginHookType", int(_beginHookType));
            }
      if (_endHook) {
            if (proto == 0 || proto->endHookHeight() != _endHookHeight)
                  xml.tag("endHookHeight", _endHookHeight.val());
            if (proto == 0 || proto->endHookType() != _endHookType)
                  xml.tag("endHookType", int(_endHookType));
            }

      if (proto == 0 || proto->lineWidth() != _lineWidth)
            xml.tag("lineWidth", _lineWidth.val());
      if (proto == 0 || proto->lineStyle() != _lineStyle)
            xml.tag("lineStyle", _lineStyle);
      if (proto == 0 || proto->lineColor() != _lineColor)
            xml.tag("lineColor", _lineColor);
      if (proto == 0 || proto->beginTextPlace() != _beginTextPlace)
            xml.pTag("beginTextPlace", _beginTextPlace);
      if (proto == 0 || proto->continueTextPlace() != _continueTextPlace)
            xml.pTag("continueTextPlace", _continueTextPlace);

      SLine::writeProperties(xml);
      if (_beginText) {
            xml.stag("beginText");
            _beginText->writeProperties(xml);
            xml.etag();
            }
      if (_continueText) {
            xml.stag("continueText");
            _continueText->writeProperties(xml);
            xml.etag();
            }
      if (_beginSymbol != -1) {
            xml.tag("beginSymbol", _beginSymbol);   // symbols[_symbol].name();
            xml.tag("beginSymbolOffset", _beginSymbolOffset);
            }
      if (_continueSymbol != -1) {
            xml.tag("continueSymbol", _continueSymbol);
            xml.tag("continueSymbolOffset", _continueSymbolOffset);
            }
      if (_endSymbol != -1) {
            xml.tag("endSymbol", _endSymbol);
            xml.tag("endSymbolOffset", _endSymbolOffset);
            }
      }

//---------------------------------------------------------
//   readProperties
//---------------------------------------------------------

bool TextLine::readProperties(const QDomElement& e)
      {
      const QString& tag(e.tagName());
      const QString& text(e.text());

      if (tag == "beginHookHeight") {
            _beginHookHeight = Spatium(text.toDouble());
            _beginHook = true;
            }
      else if (tag == "beginHookType")
            _beginHookType = HookType(text.toInt());
      else if (tag == "endHookHeight" || tag == "hookHeight") { // hookHeight is obsolete
            _endHookHeight = Spatium(text.toDouble());
            _endHook = true;
            }
      else if (tag == "endHookType")
            _endHookType = HookType(text.toInt());
      else if (tag == "hookUp")           // obsolete
            _endHookHeight *= qreal(-1.0);
      else if (tag == "beginSymbol" || tag == "symbol")     // "symbol" is obsolete
            _beginSymbol = text.toInt();
      else if (tag == "continueSymbol")
            _continueSymbol = text.toInt();
      else if (tag == "endSymbol")
            _endSymbol = text.toInt();
      else if (tag == "beginSymbolOffset")
            _beginSymbolOffset = readPoint(e);
      else if (tag == "continueSymbolOffset")
            _continueSymbolOffset = readPoint(e);
      else if (tag == "endSymbolOffset")
            _endSymbolOffset = readPoint(e);
      else if (tag == "lineWidth")
            _lineWidth = Spatium(text.toDouble());
      else if (tag == "lineStyle")
            _lineStyle = Qt::PenStyle(text.toInt());
      else if (tag == "beginTextPlace")
            _beginTextPlace = readPlacement(e);
      else if (tag == "continueTextPlace")
            _continueTextPlace = readPlacement(e);
      else if (tag == "lineColor")
            _lineColor = readColor(e);
      else if (tag == "beginText") {
            _beginText = new Text(score());
            _beginText->setParent(this);
            _beginText->read(e);
            }
      else if (tag == "continueText") {
            _continueText = new Text(score());
            _continueText->setParent(this);
            _continueText->read(e);
            }
      else if (!SLine::readProperties(e)) {
            printf(" ==readSLineProps: failed\n");
            return false;
            }
      return true;
      }

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

LineSegment* TextLine::createLineSegment()
      {
      return new TextLineSegment(score());
      }

//---------------------------------------------------------
//   layout
//    compute segments from tick() to _tick2
//---------------------------------------------------------

void TextLine::layout()
      {
      setPos(0.0, 0.0);
      if (_beginText)
            _beginText->layout();
      if (_continueText)
            _continueText->layout();
      SLine::layout();
      }

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void TextLineSegment::spatiumChanged(qreal ov, qreal nv)
      {
      textLine()->spatiumChanged(ov, nv);
      if (_text)
            _text->spatiumChanged(ov, nv);
      }

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void TextLine::spatiumChanged(qreal ov, qreal nv)
      {
      if (_beginText)
            _beginText->spatiumChanged(ov, nv);
      if (_continueText)
            _continueText->spatiumChanged(ov, nv);
      }

