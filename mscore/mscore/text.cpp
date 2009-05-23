//=============================================================================
//  MuseScore
//  Linux Music Score Editor
//  $Id$
//
//  Copyright (C) 2002-2009 Werner Schweer and others
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

#include "globals.h"
#include "text.h"
#include "xml.h"
#include "style.h"
#include "mscore.h"
#include "canvas.h"
#include "score.h"
#include "utils.h"
#include "page.h"
#include "textpalette.h"
#include "sym.h"
#include "symbol.h"
#include "textline.h"
#include "preferences.h"
#include "system.h"
#include "measure.h"
#include "textproperties.h"

TextPalette* textPalette;

//---------------------------------------------------------
//   TextBase
//---------------------------------------------------------

TextBase::TextBase()
      {
      _refCount     = 1;
      _doc          = new QTextDocument(0);
      _doc->setUseDesignMetrics(true);
      _doc->setUndoRedoEnabled(true);

      _hasFrame     = false;
      _frameWidth   = 0.35;         // default line width
      _paddingWidth = 0.0;
      _frameColor   = preferences.defaultColor;
      _frameRound   = 25;
      _circle       = false;
      _layoutWidth  = -1;

      QTextOption to = _doc->defaultTextOption();
      to.setUseDesignMetrics(true);
      to.setWrapMode(QTextOption::NoWrap);
      _doc->setDefaultTextOption(to);
      }

//---------------------------------------------------------
//   TextBase
//---------------------------------------------------------

TextBase::TextBase(const TextBase& t)
      {
      _doc          = t._doc->clone(0);
      _frameWidth   = t._frameWidth;
      _paddingWidth = t._paddingWidth;
      _frameColor   = t._frameColor;
      _frameRound   = t._frameRound;
      _circle       = t._circle;
      _hasFrame     = t._hasFrame;
      frame         = t.frame;
      _bbox         = t._bbox;
      _layoutWidth  = t._layoutWidth;
      _doc->documentLayout()->setPaintDevice(pdev);
      layout(_layoutWidth);
      }

//---------------------------------------------------------
//   isSimpleText
//    Return true if this is simple text and conforms to
//    the text style.
//    This kind of text is reponsive to style changes and
//    can be saved as simple text string.
//---------------------------------------------------------

bool TextBase::isSimpleText(TextStyle* style, double spatium) const
      {
      if (_doc->blockCount() > 1) {
            //printf("blocks > 1: %s\n", qPrintable(getText()));
            return false;
            }

      int fragments = 0;
      QTextBlock b = _doc->begin();
      QTextCharFormat cf;
      for (QTextBlock::iterator i = b.begin(); !i.atEnd(); ++i) {
            if (i.fragment().isValid())
                  ++fragments;
            if (fragments > 1) {
                  return false;
                  }
            cf = i.fragment().charFormat();
            }
      if (!style) {
            // printf("no style: %s\n", qPrintable(getText()));
            return false;
            }
      //
      // the style font has another size than the actual
      // font used in cf (due to rounding errors?), but
      // QFontInfo gives the right size
      //

      QFontInfo fi(style->font(spatium));

      QFont f(cf.font());
      if (fi.family() == f.family()
         && fi.pointSize() == f.pointSize()
         && fi.bold() == f.bold()
         && style->font(spatium).underline() == f.underline()
         && fi.italic() == f.italic())
            return true;

//      printf("bad font: %s %f <%s><%s> %d %d\n", qPrintable(getText()), spatium,
//         qPrintable(fi.family()), qPrintable(f.family()), fi.pointSize(), f.pointSize());
//      printf("%s\n", qPrintable(style->font(spatium).toString()));
//      printf("%s\n", qPrintable(f.toString()));

      return false;
      }

//---------------------------------------------------------
//   setDoc
//---------------------------------------------------------

void TextBase::setDoc(const QTextDocument& d)
      {
      if (_doc)
            delete _doc;
      _doc = d.clone(0);
      }

QFont TextBase::defaultFont() const
      {
      QTextCursor cursor(_doc);
      cursor.movePosition(QTextCursor::Start);
      return cursor.charFormat().font();
      }

void TextBase::setDefaultFont(QFont f)
      {
      _doc->setDefaultFont(f);
      QTextCursor cursor(_doc);
      cursor.movePosition(QTextCursor::Start);
      cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
      QTextCharFormat cf = cursor.charFormat();
      cf.setFont(f);
      cursor.setCharFormat(cf);
      }

//---------------------------------------------------------
//   setText
//---------------------------------------------------------

void TextBase::setText(const QString& s, Align align)
      {
      _doc->clear();
      QTextCursor cursor(_doc);
      cursor.movePosition(QTextCursor::Start);
      if (align & (ALIGN_HCENTER | ALIGN_RIGHT)) {
            Qt::Alignment a;
            if (align & ALIGN_HCENTER)
                  a = Qt::AlignHCenter;
            else if (align & ALIGN_RIGHT)
                  a = Qt::AlignRight;
            QTextBlockFormat bf = cursor.blockFormat();
            bf.setAlignment(a);
            cursor.setBlockFormat(bf);
            }
      QTextCharFormat tf = cursor.charFormat();
      tf.setFont(_doc->defaultFont());
      cursor.setBlockCharFormat(tf);
      cursor.insertText(s);
      }

//---------------------------------------------------------
//   setHtml
//---------------------------------------------------------

void TextBase::setHtml(const QString& s)
      {
      _doc->clear();
      _doc->setHtml(s);
      }

//---------------------------------------------------------
//   getText
//---------------------------------------------------------

QString TextBase::getText() const
      {
      return _doc->toPlainText();
      }

//---------------------------------------------------------
//   getHtml
//---------------------------------------------------------

QString TextBase::getHtml() const
      {
      return _doc->toHtml("utf-8");
      }

//---------------------------------------------------------
//   writeProperties
//---------------------------------------------------------

void TextBase::writeProperties(Xml& xml, TextStyle* ts, double spatium, bool writeText) const
      {
      // write all properties which are different from style

      if (_hasFrame) {
            if (ts == 0 || _frameWidth != ts->frameWidth)
                  xml.tag("frameWidth", _frameWidth);
            if (ts == 0 || _paddingWidth != ts->paddingWidth)
                  xml.tag("paddingWidth", _paddingWidth);
            if (ts == 0 || _frameColor != ts->frameColor)
                  xml.tag("frameColor", _frameColor);
            if (ts == 0 || _frameRound != ts->frameRound)
                  xml.tag("frameRound", _frameRound);
            if (ts == 0 || _circle != ts->circle)
                  xml.tag("circle", _circle);
            }
      if (writeText) {
            if (isSimpleText(ts, spatium))
                  xml.tag("text", _doc->toPlainText());
            else {
                  xml.stag("html-data");
                  xml.writeHtml(_doc->toHtml("utf-8"));
                  xml.etag();
                  }
            }
      }

//---------------------------------------------------------
//   readProperties
//---------------------------------------------------------

bool TextBase::readProperties(QDomElement e)
      {
      QString tag(e.tagName());
      QString val(e.text());

      if (tag == "data")                  // obsolete
            _doc->setHtml(val);
      else if (tag == "html") {
            QString s = Xml::htmlToString(e);
            _doc->setHtml(s);
            }
      else if (tag == "text")
            _doc->setPlainText(val);
      else if (tag == "html-data") {
            QString s = Xml::htmlToString(e.firstChildElement());
            _doc->setHtml(s);
            }
      else if (tag == "frameWidth") {
            _frameWidth = val.toDouble();
            _hasFrame   = true;
            }
      else if (tag == "paddingWidth")
            _paddingWidth = val.toDouble();
      else if (tag == "frameColor")
            _frameColor = readColor(e);
      else if (tag == "frameRound")
            _frameRound = val.toInt();
      else if (tag == "circle")
            _circle = val.toInt();
      else
            return false;
      return true;
      }

//---------------------------------------------------------
//   layout
//---------------------------------------------------------

void TextBase::layout(double w)
      {
      _layoutWidth = w;
      if (!_doc->isModified())
            return;
      _doc->documentLayout()->setPaintDevice(pdev);
      if (w <= 0.0)
            w = _doc->idealWidth();
      else {
            QTextOption to = _doc->defaultTextOption();
            to.setUseDesignMetrics(true);
            to.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
            _doc->setDefaultTextOption(to);
            }
      _doc->setTextWidth(w);   // to make alignment work

      if (_hasFrame) {
            frame = QRectF();
            for (QTextBlock tb = _doc->begin(); tb.isValid(); tb = tb.next()) {
                  QTextLayout* tl = tb.layout();
                  int n = tl->lineCount();
                  for (int i = 0; i < n; ++i)
                        // frame |= tl->lineAt(0).naturalTextRect().translated(tl->position());
                        frame |= tl->lineAt(0).rect().translated(tl->position());
                  }
            if (_circle) {
                  if (frame.width() > frame.height()) {
                        frame.setY(frame.y() + (frame.width() - frame.height()) * -.5);
                        frame.setHeight(frame.width());
                        }
                  else {
                        frame.setX(frame.x() + (frame.height() - frame.width()) * -.5);
                        frame.setWidth(frame.height());
                        }
                  }
            double w = (_paddingWidth + _frameWidth * .5) * DPMM;
            frame.adjust(-w, -w, w, w);
            w = _frameWidth * DPMM;
            _bbox = frame.adjusted(-w, -w, w, w);
            }
      else {
            _bbox = _doc->documentLayout()->frameBoundingRect(_doc->rootFrame());
            }
      _doc->setModified(false);
      }

//---------------------------------------------------------
//   TextBase::draw
//---------------------------------------------------------

void TextBase::draw(QPainter& p, QTextCursor* cursor) const
      {
      QAbstractTextDocumentLayout::PaintContext c;
      c.cursorPosition = -1;
      if (cursor) {
            c.cursorPosition = cursor->position();
            QAbstractTextDocumentLayout::Selection sel;
            sel.cursor = *cursor;
            sel.format.setFontUnderline(true);
            c.selections.append(sel);
            }
      QColor color = p.pen().color();
      c.palette.setColor(QPalette::Text, color);

      _doc->documentLayout()->setProperty("cursorWidth", QVariant(int(lrint(2.0*DPI/PDPI))));
      _doc->documentLayout()->draw(&p, c);

      // draw frame
      if (_hasFrame) {
            p.setPen(QPen(QBrush(_frameColor), _frameWidth * DPMM));
            p.setBrush(QBrush(Qt::NoBrush));
            if (_circle)
                  p.drawArc(frame, 0, 5760);
            else {
                  double mag = 1.0; // DPI/PDPI;

                  int r2 = _frameRound * lrint((frame.width() / frame.height()));
                  if (r2 > 99)
                        r2 = 99;
                  QRectF fr(frame.x(), frame.y(), frame.width() * mag, frame.height() * mag);
                  p.drawRoundRect(fr, _frameRound, r2);
                  }
            }
      }

//---------------------------------------------------------
//   Text
//---------------------------------------------------------

TextB::TextB(Score* s)
   : Element(s)
      {
      editMode   = false;
      cursorPos  = 0;
      cursor     = 0;
      _movable   = true;
      _textStyle = -1;
      }

TextB::TextB(const TextB& e)
   : Element(e)
      {
      _movable                = e._movable;
      _sizeIsSpatiumDependent = e._sizeIsSpatiumDependent;
      editMode                = e.editMode;
      cursorPos               = e.cursorPos;
      _textStyle              = e._textStyle;
      cursor                  = 0;
      }

//---------------------------------------------------------
//   TextC
//---------------------------------------------------------

TextC::TextC(Score* s)
   : TextB(s)
      {
      _tb = new TextBase();
      setSubtype(TEXT_STAFF);
      }

TextC::TextC(const TextC& e)
   : TextB(e)
      {
      _tb    = e._tb;
      _tb->incRefCount();
      cursor = 0;
      baseChanged();
      }

TextC* TextC::clone() const
      {
      TextC* t = new TextC(*this);
      _tb->decRefCount();
      t->_tb = new TextBase(*_tb);
      return t;
      }

TextC::~TextC()
      {
      _tb->decRefCount();
      if (_tb->refCount() <= 0)
            delete _tb;
      }

//---------------------------------------------------------
//   baseChanged
//---------------------------------------------------------

void TextC::baseChanged()
      {
      if (editMode) {
            if (cursor)
                  delete cursor;
            cursor = new QTextCursor(textBase()->doc());
            cursor->setPosition(cursorPos);
            }
      else
            cursor = 0;
      }

//---------------------------------------------------------
//   Text
//---------------------------------------------------------

Text::Text(Score* s)
   : TextB(s)
      {
      _tb = new TextBase;
      setSubtype(0);
      }

Text::Text(const Text& e)
   : TextB(e)
      {
      _tb = new TextBase(*(e._tb));
      if (editMode) {
            cursor = new QTextCursor(textBase()->doc());
            cursor->setPosition(cursorPos);
            cursor->setBlockFormat(e.cursor->blockFormat());
            cursor->setCharFormat(e.cursor->charFormat());
            }
      else
            cursor = 0;
      }

Text::~Text()
      {
      delete _tb;
      }

//---------------------------------------------------------
//   setDoc
//---------------------------------------------------------

void TextB::setDoc(const QTextDocument& d)
      {
      textBase()->setDoc(d);
      cursorPos = 0;
      }

//---------------------------------------------------------
//   setAbove
//---------------------------------------------------------

void TextB::setAbove(bool /*val*/)
      {
      // setYoff(val ? -2.0 : 7.0);
      }

//---------------------------------------------------------
//   subtypeName
//---------------------------------------------------------

const QString TextB::subtypeName() const
      {
      switch (subtype()) {
            case TEXT_TITLE:            return "Title";
            case TEXT_SUBTITLE:         return "Subtitle";
            case TEXT_COMPOSER:         return "Composer";
            case TEXT_POET:             return "Poet";
            case TEXT_TRANSLATOR:       return "Translator";
            case TEXT_MEASURE_NUMBER:   return "MeasureNumber";
            case TEXT_PAGE_NUMBER_ODD:  return "PageNoOdd";
            case TEXT_PAGE_NUMBER_EVEN: return "PageNoEven";
            case TEXT_COPYRIGHT:        return "Copyright";
            case TEXT_FINGERING:        return "Fingering";
            case TEXT_INSTRUMENT_LONG:  return "InstrumentLong";
            case TEXT_INSTRUMENT_SHORT: return "InstrumentShort";
            case TEXT_INSTRUMENT_EXCERPT: return "InstrumentExcerpt";
            case TEXT_TEMPO:            return "Tempo";
            case TEXT_LYRIC:            return "Lyric";
            case TEXT_TUPLET:           return "Tuplet";
            case TEXT_SYSTEM:           return "System";
            case TEXT_STAFF:            return "Staff";
            case TEXT_CHORD:            return "";     // "Chordname";
            case TEXT_REHEARSAL_MARK:   return "RehearsalMark";
            case TEXT_REPEAT:           return "Repeat";
            case TEXT_VOLTA:            return "Volta";
            case TEXT_FRAME:            return "Frame";
            case TEXT_TEXTLINE:         return "TextLine";
            case TEXT_STRING_NUMBER:    return "StringNumber";
            default:
                  printf("unknown text subtype %d\n", subtype());
                  break;
            }
      return "?";
      }

//---------------------------------------------------------
//   setSubtype
//---------------------------------------------------------

void TextB::setSubtype(const QString& s)
      {
      int st = 0;
      if (s == "Title")
            st = TEXT_TITLE;
      else if (s == "Subtitle")
            st = TEXT_SUBTITLE;
      else if (s == "Composer")
            st = TEXT_COMPOSER;
      else if (s == "Poet")
            st = TEXT_POET;
      else if (s == "Translator")
            st = TEXT_TRANSLATOR;
      else if (s == "MeasureNumber")
            st = TEXT_MEASURE_NUMBER;
      else if (s == "PageNoOdd")
            st = TEXT_PAGE_NUMBER_ODD;
      else if (s == "PageNoEven")
            st = TEXT_PAGE_NUMBER_EVEN;
      else if (s == "Copyright")
            st = TEXT_COPYRIGHT;
      else if (s == "Fingering")
            st = TEXT_FINGERING;
      else if (s == "InstrumentLong")
            st = TEXT_INSTRUMENT_LONG;
      else if (s == "InstrumentShort")
            st = TEXT_INSTRUMENT_SHORT;
      else if (s == "InstrumentExcerpt")
            st = TEXT_INSTRUMENT_EXCERPT;
      else if (s == "Tempo")
            st = TEXT_TEMPO;
      else if (s == "Lyric")
            st = TEXT_LYRIC;
      else if (s == "Tuplet")
            st = TEXT_TUPLET;
      else if (s == "System")
            st = TEXT_SYSTEM;
      else if (s == "Staff")
            st = TEXT_STAFF;
      else if (s == "Chordname")
            st = TEXT_CHORD;
      else if (s == "RehearsalMark")
            st = TEXT_REHEARSAL_MARK;
      else if (s == "Repeat")
            st = TEXT_REPEAT;
      else if (s == "Volta")
            st = TEXT_VOLTA;
      else if (s == "Frame")
            st = TEXT_FRAME;
      else if (s == "TextLine")
            st = TEXT_TEXTLINE;
      else if (s == "StringNumber")
            st = TEXT_STRING_NUMBER;
      else
            printf("setSubtype: unknown type <%s>\n", qPrintable(s));
      setSubtype(st);
      }

//---------------------------------------------------------
//   resetMode
//---------------------------------------------------------

void TextB::resetMode()
      {
      editMode = 0;
      if (cursor) {
            delete cursor;
            cursor = 0;
            }
      }

//---------------------------------------------------------
//   isEmpty
//---------------------------------------------------------

bool TextB::isEmpty() const
      {
      return doc()->isEmpty();
      }

//---------------------------------------------------------
//   layout
//---------------------------------------------------------

void TextB::layout()
      {
      if (parent() && parent()->type() == HBOX)
            textBase()->layout(parent()->width());
      else
            textBase()->layout(-1.0);
      setbbox(textBase()->bbox());

      Element::layout();      // process alignment

      if (_align & ALIGN_VCENTER && subtype() == TEXT_TEXTLINE) {
            // special case: vertically centered text with TextLine needs to
            // take into account the line width
            TextLineSegment* tls = (TextLineSegment*)parent();
            TextLine* tl = (TextLine*)(tls->line());
            qreal textlineLineWidth = point(tl->lineWidth());
            setYpos(pos().y() - textlineLineWidth * .5);
            }

      if (parent() == 0)
            return;
      if (parent()->type() == MEASURE) {
            Measure* m = static_cast<Measure*>(parent());
            double y = track() < 0 ? 0.0 : m->system()->staff(track() / VOICES)->y();
            double x = (tick() < 0) ? 0.0 : m->tick2pos(tick());
            setPos(ipos() + QPointF(x, y));
            }
      }

//---------------------------------------------------------
//   draw
//---------------------------------------------------------

void TextB::draw(QPainter& p) const
      {
      textBase()->draw(p, cursor);
      }

//---------------------------------------------------------
//   setTextStyle
//---------------------------------------------------------

void TextB::setTextStyle(int idx)
      {
      _textStyle   = idx;
      TextStyle* s = score()->textStyle(idx);
      doc()->setDefaultFont(s->font(spatium()));

      _align         = s->align;
      _xoff          = s->xoff;
      _yoff          = s->yoff;
      _rxoff         = s->rxoff;
      _ryoff         = s->ryoff;
      _offsetType    = s->offsetType;
      _sizeIsSpatiumDependent = s->sizeIsSpatiumDependent;
      setSystemFlag(s->systemFlag);

      textBase()->setFrameWidth(s->frameWidth);
      textBase()->setHasFrame(s->hasFrame);
      textBase()->setPaddingWidth(s->paddingWidth);
      textBase()->setFrameColor(s->frameColor);
      textBase()->setFrameRound(s->frameRound);
      textBase()->setCircle(s->circle);
      }

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void TextB::write(Xml& xml) const
      {
      write(xml, "Text");
      }

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void TextB::write(Xml& xml, const char* name) const
      {
      if (doc()->isEmpty())
            return;
      xml.stag(name);
      writeProperties(xml, true);
      xml.etag();
      }

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void TextB::read(QDomElement e)
      {
      _align  = 0;
      for (e = e.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
            if (!readProperties(e))
                  domError(e);
            }
      cursorPos = 0;
      }

//---------------------------------------------------------
//   writeProperties
//---------------------------------------------------------

void TextB::writeProperties(Xml& xml, bool writeText) const
      {
      Element::writeProperties(xml);

      // write all properties which are different from style

      if (_textStyle >= 0)
            xml.tag("style", _textStyle);

      TextStyle* st = score()->textStyle(_textStyle);
      if (st == 0 || _align != st->align) {
            if (_align & (ALIGN_RIGHT | ALIGN_HCENTER)) {          // default is ALIGN_LEFT
                  xml.tag("halign", (_align & ALIGN_RIGHT) ? "right" : "center");
                  }
            if (_align & (ALIGN_BOTTOM | ALIGN_VCENTER | ALIGN_BASELINE)) {        // default is ALIGN_TOP
                  if (_align & ALIGN_BOTTOM)
                        xml.tag("valign", "bottom");
                  else if (_align & ALIGN_VCENTER)
                        xml.tag("valign", "center");
                  else if (_align & ALIGN_BASELINE)
                        xml.tag("valign", "baseline");
                  }
            }
      if (st == 0 || _xoff != st->xoff)
            xml.tag("xoffset", _xoff);
      if (st == 0 || _yoff != st->yoff)
            xml.tag("yoffset", _yoff);
      if (st == 0 || _rxoff != st->rxoff)
            xml.tag("rxoffset", _rxoff);
      if (st == 0 || _ryoff != st->ryoff)
            xml.tag("ryoffset", _ryoff);

      if (st == 0 || _offsetType != st->offsetType) {
            const char* p = 0;
            switch(_offsetType) {
                  case OFFSET_SPATIUM:                    break;
                  case OFFSET_ABS:        p = "absolute"; break;
                  }
            if (p)
                  xml.tag("offsetType", p);
            }

      if (st == 0 || (!_sizeIsSpatiumDependent && _sizeIsSpatiumDependent != st->sizeIsSpatiumDependent))
            xml.tag("spatiumSizeDependent", _sizeIsSpatiumDependent);
      if (subtype() == TEXT_MEASURE_NUMBER)
            return;
      textBase()->writeProperties(xml, st, score()->spatium(), writeText);
      }

//---------------------------------------------------------
//   textStyleChanged
//    do not touch values which are modified by user
//    (== different from old style)
//---------------------------------------------------------

void TextB::textStyleChanged(const QVector<TextStyle*>& styles)
      {
      if (_textStyle < 0)
            return;
      TextStyle* ns = styles[_textStyle];
      TextStyle* os = score()->textStyle(_textStyle);

      if (_align == os->align)
            _align = ns->align;
      if (_xoff == os->xoff)
            _xoff = ns->xoff;
      if (_yoff == os->yoff)
            _yoff = ns->yoff;
      if (_rxoff == os->rxoff)
            _rxoff = ns->rxoff;
      if (_ryoff == os->ryoff)
            _ryoff = ns->ryoff;
      if (_offsetType == os->offsetType)
            _offsetType = ns->offsetType;
      if (_sizeIsSpatiumDependent == os->sizeIsSpatiumDependent)
            _sizeIsSpatiumDependent = ns->sizeIsSpatiumDependent;
      if (frameWidth() == os->frameWidth)
            setFrameWidth(ns->frameWidth);
      if (paddingWidth() == os->paddingWidth)
            setPaddingWidth(ns->paddingWidth);
      if (frameRound() == os->frameRound)
            setFrameRound(ns->frameRound);
      if (frameColor() == os->frameColor)
            setFrameColor(ns->frameColor);
      if (circle() == os->circle)
            setCircle(ns->circle);
      if (systemFlag() == os->systemFlag)
            setSystemFlag(ns->systemFlag);
      double _spatium = spatium();
      if (textBase()->isSimpleText(os, _spatium))
            setDefaultFont(ns->font(_spatium));
      if (textBase()->hasFrame() == os->hasFrame)
            textBase()->setHasFrame(ns->hasFrame);
      }

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void TextB::spatiumChanged(double oldValue, double newValue)
      {
      if (!_sizeIsSpatiumDependent)
            return;
      TextStyle* style = score()->textStyle(_textStyle);
      if (textBase()->isSimpleText(style, oldValue))
            setDefaultFont(style->font(newValue));
      }

//---------------------------------------------------------
//   readProperties
//---------------------------------------------------------

bool TextB::readProperties(QDomElement e)
      {
      QString tag(e.tagName());
      QString val(e.text());

      if (tag == "style") {
            int i = val.toInt();
            if (i >= 0)
                  setTextStyle(i);
            }
      else if (tag == "align")            // obsolete
            _align = Align(val.toInt());
      else if (tag == "halign") {
            if (val == "center")
                  _align |= ALIGN_HCENTER;
            else if (val == "right")
                  _align |= ALIGN_RIGHT;
            else
                  printf("Text::readProperties: unknown alignment: <%s>\n", qPrintable(val));
            }
      else if (tag == "valign") {
            if (val == "center")
                  _align |= ALIGN_VCENTER;
            else if (val == "bottom")
                  _align |= ALIGN_BOTTOM;
            else if (val == "baseline")
                  _align |= ALIGN_BASELINE;
            else
                  printf("Text::readProperties: unknown alignment: <%s>\n", qPrintable(val));
            }
      else if (tag == "xoffset")
            _xoff = val.toDouble();
      else if (tag == "yoffset")
            _yoff = val.toDouble();
      else if (tag == "rxoffset")
            _rxoff = val.toDouble();
      else if (tag == "ryoffset")
            _ryoff = val.toDouble();
      else if (tag == "offsetType") {
            if (val == "absolute")
                  _offsetType = OFFSET_ABS;
            else if (val == "spatium")
                  _offsetType = OFFSET_SPATIUM;
            else
                  printf("Text::readProperties: unknown offset type: <%s>\n", qPrintable(val));
            }
      else if (tag == "spatiumSizeDependent")
            _sizeIsSpatiumDependent = val.toInt();
      else if (textBase()->readProperties(e))
            ;
      else if (!Element::readProperties(e))
            return false;
      return true;
      }

//---------------------------------------------------------
//   startEdit
//---------------------------------------------------------

bool TextB::startEdit(Viewer* view, const QPointF& p)
      {
      if ((type() == TEXT) && (subtype() == TEXT_MEASURE_NUMBER))
            return false;
      mscore->textTools()->show();
      cursor = new QTextCursor(doc());
      cursor->setPosition(cursorPos);
      editMode = true;
      setCursor(p);
      cursorPos = cursor->position();

      if (cursorPos == 0 && _align) {
            QTextBlockFormat bf = cursor->blockFormat();
            Qt::Alignment alignment = 0;
            if (_align & ALIGN_HCENTER)
                  alignment |= Qt::AlignHCenter;
            else if (_align & ALIGN_LEFT)
                  alignment |= Qt::AlignLeft;
            else if (_align & ALIGN_RIGHT)
                  alignment |= Qt::AlignRight;
            bf.setAlignment(alignment);
            setBlockFormat(bf);
            }
      qreal w = 8.0 / view->matrix().m11();
      score()->addRefresh(abbox().adjusted(-w, -w, w, w));
      return true;
      }

//---------------------------------------------------------
//   edit
//    return true if event is accepted
//---------------------------------------------------------

bool TextB::edit(Viewer* view, int /*grip*/, int key, Qt::KeyboardModifiers modifiers, const QString& s)
      {
      if (debugMode)
            printf("TextB::edit\n");
      bool lo = (subtype() == TEXT_INSTRUMENT_SHORT) || (subtype() == TEXT_INSTRUMENT_LONG);
      score()->setLayoutAll(lo);
      qreal w = 8.0 / view->matrix().m11();
      score()->addRefresh(abbox().adjusted(-w, -w, w, w));

      if (modifiers == Qt::ControlModifier) {
            switch (key) {
                  case Qt::Key_B:   // toggle bold face
                        {
                        QTextCharFormat f = cursor->charFormat();
                        f.setFontWeight(f.fontWeight() == QFont::Bold ? QFont::Normal : QFont::Bold);
                        mscore->textTools()->setCharFormat(f);
                        cursor->setCharFormat(f);
                        }
                        break;
                  case Qt::Key_I:   // toggle italic
                        {
                        QTextCharFormat f = cursor->charFormat();
                        f.setFontItalic(!f.fontItalic());
                        mscore->textTools()->setCharFormat(f);
                        cursor->setCharFormat(f);
                        }
                        break;
                  case Qt::Key_U:   // toggle underline
                        {
                        QTextCharFormat f = cursor->charFormat();
                        f.setFontUnderline(!f.fontUnderline());
                        mscore->textTools()->setCharFormat(f);
                        cursor->setCharFormat(f);
                        }
                        break;
                  case Qt::Key_Up:
                        {
                        QTextCharFormat f = cursor->charFormat();
                        if (f.verticalAlignment() == QTextCharFormat::AlignNormal)
                              f.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
                        else if (f.verticalAlignment() == QTextCharFormat::AlignSubScript)
                              f.setVerticalAlignment(QTextCharFormat::AlignNormal);
                        cursor->setCharFormat(f);
                        }
                        break;

                  case Qt::Key_Down:
                        {
                        QTextCharFormat f = cursor->charFormat();
                        if (f.verticalAlignment() == QTextCharFormat::AlignNormal)
                              f.setVerticalAlignment(QTextCharFormat::AlignSubScript);
                        else if (f.verticalAlignment() == QTextCharFormat::AlignSuperScript)
                              f.setVerticalAlignment(QTextCharFormat::AlignNormal);
                        cursor->setCharFormat(f);
                        }
                        break;
                  }
            if (key != Qt::Key_Space && key != Qt::Key_Minus)
                  return true;
            }
      QTextCursor::MoveMode mm = (modifiers & Qt::ShiftModifier)
         ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor;
      switch (key) {
            case Qt::Key_Return:
                  cursor->insertText(QString("\r"));
                  break;

            case Qt::Key_Backspace:
                  cursor->deletePreviousChar();
                  break;

            case Qt::Key_Delete:
                  cursor->deleteChar();
                  break;

            case Qt::Key_Left:
                  if (!cursor->movePosition(QTextCursor::Left, mm))
                        return false;
                  break;

            case Qt::Key_Right:
                  if (!cursor->movePosition(QTextCursor::Right, mm))
                        return false;
                  break;

            case Qt::Key_Up:
                  cursor->movePosition(QTextCursor::Up, mm);
                  break;

            case Qt::Key_Down:
                  cursor->movePosition(QTextCursor::Down, mm);
                  break;

            case Qt::Key_Home:
                  cursor->movePosition(QTextCursor::Start, mm);
                  break;

            case Qt::Key_End:
                  cursor->movePosition(QTextCursor::End, mm);
                  break;

            case Qt::Key_Space:
                  cursor->insertText(" ");
                  break;

            case Qt::Key_Minus:
                  cursor->insertText("-");
                  break;

            default:
                  cursor->insertText(s);
                  break;
            }
      if (key == Qt::Key_Return || key == Qt::Key_Space || key == Qt::Key_Tab) {
            replaceSpecialChars();
            }
      mscore->textTools()->setCharFormat(cursor->charFormat());
      mscore->textTools()->setBlockFormat(cursor->blockFormat());
      layout();
      score()->addRefresh(abbox().adjusted(-w, -w, w, w));
      return true;
      }

//---------------------------------------------------------
//   replaceSpecialChars
//---------------------------------------------------------

bool TextB::replaceSpecialChars() {
      QTextCursor startCur = *cursor;
      foreach (const char* s, charReplaceMap.keys()) {
            SymCode sym = *charReplaceMap.value(s);
            switch (sym.type) {
                  case SYMBOL_COPYRIGHT:
                        if (!preferences.replaceCopyrightSymbol || subtype() != TEXT_COPYRIGHT)
                              continue;
                        break;
                  case SYMBOL_FRACTION:
                        if (!preferences.replaceFractions)
                              continue;
                        break;
                  default:
                        ;
                  }
            QTextCursor cur = doc()->find(s, cursor->position() - 1 - strlen(s),
                  QTextDocument::FindWholeWords);
            if (cur.isNull())
                  continue;
            // do not go beyond the cursor
            if (cur.selectionEnd() > cursor->selectionEnd())
                  continue;
            addSymbol(sym, &cur);
            return true;
            }
      return false;
      }

//---------------------------------------------------------
//   moveCursorToEnd
//---------------------------------------------------------

void TextB::moveCursorToEnd()
      {
      if (cursor)
            cursor->movePosition(QTextCursor::End);
      }

//---------------------------------------------------------
//   moveCursor
//---------------------------------------------------------

void TextB::moveCursor(int col)
      {
      if (cursor)
            cursor->setPosition(col);
      }

//---------------------------------------------------------
//   endEdit
//---------------------------------------------------------

void TextB::endEdit()
      {
      cursorPos = cursor->position();
      if (textPalette)
            textPalette->hide();
      mscore->textTools()->hide();
      delete cursor;
      cursor = 0;
      editMode = false;

      if (subtype() == TEXT_COPYRIGHT)
            score()->undoChangeCopyright(doc()->toHtml("UTF-8"));
      }

//---------------------------------------------------------
//   shape
//---------------------------------------------------------

QPainterPath TextB::shape() const
      {
      QPainterPath pp;

      for (QTextBlock tb = doc()->begin(); tb.isValid(); tb = tb.next()) {
            QTextLayout* tl = tb.layout();
            int n = tl->lineCount();
            for (int i = 0; i < n; ++i)
                  pp.addRect(tl->lineAt(0).naturalTextRect().translated(tl->position()));
            }
      return pp;
      }

//---------------------------------------------------------
//   baseLine
//    returns ascent of first text line in first block
//---------------------------------------------------------

qreal TextB::baseLine() const
      {
      for (QTextBlock tb = doc()->begin(); tb.isValid(); tb = tb.next()) {
            const QTextLayout* tl = tb.layout();
            if (tl->lineCount())
                  return tl->lineAt(0).ascent() + tl->position().y();
            }
      return 0.0;
      }

//---------------------------------------------------------
//   lineSpacing
//---------------------------------------------------------

double TextB::lineSpacing() const
      {
      return QFontMetricsF(doc()->defaultFont(), pdev).lineSpacing();
      }

//---------------------------------------------------------
//   lineHeight
//    HACK
//---------------------------------------------------------

double TextB::lineHeight() const
      {
      QFontMetricsF fm(doc()->defaultFont(), pdev);
      return fm.height();
      }

//---------------------------------------------------------
//   addSymbol
//---------------------------------------------------------

void TextB::addSymbol(const SymCode& s, QTextCursor* cur)
      {
      if (cur == 0)
            cur = cursor;
// printf("Text: addSymbol(%x)\n", s.code);
      QTextCharFormat oFormat = cur->charFormat();
      if (s.fontId >= 0) {
            QTextCharFormat oFormat = cur->charFormat();
            QTextCharFormat nFormat(oFormat);
            nFormat.setFontFamily(fontId2font(s.fontId).family());
            cur->setCharFormat(nFormat);
            cur->insertText(s.code);
            cur->setCharFormat(oFormat);
            }
      else
            cur->insertText(s.code);
      score()->setLayoutAll(true);
      score()->end();
      }

//---------------------------------------------------------
//   setCharFormat
//---------------------------------------------------------

void TextB::setCharFormat(const QTextCharFormat& f)
      {
      if (!cursor)
            return;
      cursor->setCharFormat(f);
      }

//---------------------------------------------------------
//   setBlockFormat
//---------------------------------------------------------

void TextB::setBlockFormat(const QTextBlockFormat& bf)
      {
      if (!cursor)
            return;
      cursor->setBlockFormat(bf);
      score()->setLayoutAll(true);
      }

//---------------------------------------------------------
//   setCursor
//---------------------------------------------------------

bool TextB::setCursor(const QPointF& p)
      {
      QPointF pt  = p - canvasPos();
      if (!bbox().contains(pt))
            return false;
      int idx = doc()->documentLayout()->hitTest(pt, Qt::FuzzyHit);
      if (idx == -1)
            return true;
      cursor->setPosition(idx);
      return true;
      }

//---------------------------------------------------------
//   mousePress
//    set text cursor
//---------------------------------------------------------

bool TextB::mousePress(const QPointF& p, QMouseEvent* ev)
      {
      if (!setCursor(p))
            return false;

      if (ev->button() == Qt::MidButton)
            paste();
      return true;
      }

//---------------------------------------------------------
//   paste
//---------------------------------------------------------

void TextB::paste()
      {
#ifdef __MINGW32__
      QString txt = QApplication::clipboard()->text(QClipboard::Clipboard);
#else
      QString txt = QApplication::clipboard()->text(QClipboard::Selection);
#endif
      if (debugMode)
            printf("TextB::paste() <%s>\n", qPrintable(txt));
      cursor->insertText(txt);
      layout();
      bool lo = (subtype() == TEXT_INSTRUMENT_SHORT) || (subtype() == TEXT_INSTRUMENT_LONG);
      score()->setLayoutAll(lo);
      score()->setUpdateAll();
      score()->end();
      }

//---------------------------------------------------------
//   genPropertyMenu
//---------------------------------------------------------

bool Text::genPropertyMenu(QMenu* popup) const
      {
      QAction* a;
      if (visible())
            a = popup->addAction(tr("Set Invisible"));
      else
            a = popup->addAction(tr("Set Visible"));
      a->setData("invisible");
      a = popup->addAction(tr("Text Properties..."));
      a->setData("props");
      return true;
      }

//---------------------------------------------------------
//   propertyAction
//---------------------------------------------------------

void Text::propertyAction(const QString& s)
      {
      if (s == "props") {
            Text* nText = new Text(*this);
            TextProperties tp(nText, 0);
            int rv = tp.exec();
            if (rv) {
                  score()->undoChangeElement(this, nText);
                  }
            else
                  delete nText;
            }
      else
            Element::propertyAction(s);
      }

//---------------------------------------------------------
//   dragAnchor
//---------------------------------------------------------

QLineF TextB::dragAnchor() const
      {
      QPointF p1;

      if (parent()->type() == MEASURE) {
            Measure* m     = static_cast<Measure*>(parent());
            System* system = m->system();
            double yp      = system->staff(staffIdx())->y() + system->y();
            double xp      = m->tick2pos(tick()) + m->canvasPos().x();
            p1 = QPointF(xp, yp);
            }
      else {
            p1 = QPointF(parent()->abbox().topLeft());
            }
      double tw = width();
      double th = height();
      double x  = 0.0;
      double y  = 0.0;
      if (_align & ALIGN_BOTTOM)
            y = th;
      else if (_align & ALIGN_VCENTER)
            y = (th * .5);
      else if (_align & ALIGN_BASELINE)
            y = baseLine();
      if (_align & ALIGN_RIGHT)
            x = tw;
      else if (_align & ALIGN_HCENTER)
            x = (tw * .5);
      return QLineF(p1, abbox().topLeft() + QPointF(x, y));
      }

//---------------------------------------------------------
//   TextProperties
//---------------------------------------------------------

TextProperties::TextProperties(TextB* t, QWidget* parent)
   : QDialog(parent)
      {
      setWindowTitle(tr("MuseScore: Text Properties"));
      QGridLayout* layout = new QGridLayout;
      tp = new TextProp;
      layout->addWidget(tp, 0, 1);
      QLabel* l = new QLabel;
      l->setPixmap(QPixmap(":/data/bg1.jpg"));
      layout->addWidget(l, 0, 0, 2, 1);
      QDialogButtonBox* bb = new QDialogButtonBox(
         QDialogButtonBox::Ok | QDialogButtonBox::Cancel
         );
      layout->addWidget(bb, 1, 0, 1, 2);
      setLayout(layout);

      tb = t;
      tp->set(tb);
      connect(bb, SIGNAL(accepted()), SLOT(accept()));
      connect(bb, SIGNAL(rejected()), SLOT(reject()));
      }

//---------------------------------------------------------
//   accept
//---------------------------------------------------------

void TextProperties::accept()
      {
      tp->get(tb);
      QDialog::accept();
      }

