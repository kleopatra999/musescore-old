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

#include "navigate.h"
#include "element.h"
#include "clef.h"
#include "score.h"
#include "note.h"
#include "rest.h"
#include "chord.h"
#include "system.h"
#include "segment.h"
#include "lyrics.h"
#include "harmony.h"
#include "utils.h"
#include "input.h"
#include "measure.h"
#include "page.h"

//---------------------------------------------------------
//   nextChordRest
//    return next NOTE or REST, skip grace notes
//    element - current element
//---------------------------------------------------------

ChordRest* nextChordRest(ChordRest* cr)
      {
      if (!cr)
            return 0;
      int track = cr->track();
      for (Segment* seg = cr->segment()->next1(SegChordRest); seg; seg = seg->next1(SegChordRest)) {
            if (seg->measure()->multiMeasure() < 0)
                  continue;
            Element* e = seg->element(track);
            if (e && e->isChordRest())
                  return static_cast<ChordRest*>(e);
            }
      return 0;
      }

//---------------------------------------------------------
//   prevChordRest
//    return previous Chord or Rest
//---------------------------------------------------------

ChordRest* prevChordRest(ChordRest* cr)
      {
      if (!cr)
            return 0;
      int track = cr->track();
      for (Segment* seg = cr->segment()->prev1(); seg; seg = seg->prev1()) {
            if (seg->subtype() != SegChordRest)
                  continue;
            if (seg->measure()->multiMeasure() < 0)
                  continue;
            Element* e = seg->element(track);
            if (e && e->isChordRest())
                  return static_cast<ChordRest*>(e);
            }
      return 0;
      }

//---------------------------------------------------------
//   noteLessThan
//---------------------------------------------------------

static bool noteLessThan(const Note* n1, const Note* n2)
      {
      return n1->pitch() <= n2->pitch();
      }

//---------------------------------------------------------
//   upAlt
//    select next higher pitched note in chord
//---------------------------------------------------------

Note* Score::upAlt(Element* element)
      {
      Element* re = 0;
      if (element->type() == REST) {
            if (_is.track() <= 0)
                  return 0;
            _is.setTrack(_is.track() - 1);
            re = searchNote(static_cast<Rest*>(element)->tick(), _is.track());
            }
      else if (element->type() == NOTE) {
            // find segment
            Chord* chord     = static_cast<Note*>(element)->chord();
            Segment* segment = chord->segment();

            // collect all notes for this segment in noteList:
            QList<Note*> rnl;
            int tracks = nstaves() * VOICES;
            for (int track = 0; track < tracks; ++track) {
                  Element* el = segment->element(track);
                  if (!el || el->type() != CHORD)
                        continue;
                  rnl.append(static_cast<Chord*>(el)->notes());
                  qSort(rnl.begin(), rnl.end(), noteLessThan);
                  int idx = rnl.indexOf(static_cast<Note*>(element));
                  if (idx < rnl.size()-1)
                        ++idx;
                  re = rnl.value(idx);
                  }
            }
      if (re == 0)
            return 0;
      if (re->type() == CHORD)
            re = ((Chord*)re)->notes().front();
      return (Note*)re;
      }

//---------------------------------------------------------
//   upAltCtrl
//    select top note in chord
//---------------------------------------------------------

Note* Score::upAltCtrl(Note* note) const
      {
      return note->chord()->upNote();
      }

//---------------------------------------------------------
//   downAlt
//    goto next note with lower pitch in chord or to
//    top note in next staff
//---------------------------------------------------------

Note* Score::downAlt(Element* element)
      {
      Element* re = 0;
      int staves = nstaves();
      if (element->type() == REST) {
            if ((_is.track() + 1) >= staves * VOICES)
                  return 0;
            _is.setTrack(_is.track() + 1);
            re = searchNote(static_cast<Rest*>(element)->tick(), _is.track());
            }
      else if (element->type() == NOTE) {
            // find segment
            Chord* chord     = static_cast<Note*>(element)->chord();
            Segment* segment = chord->segment();

            // collect all notes for this segment in noteList:
            QList<Note*> rnl;
            int tracks = nstaves() * VOICES;
            for (int track = 0; track < tracks; ++track) {
                  Element* el = segment->element(track);
                  if (!el || el->type() != CHORD)
                        continue;
                  rnl.append(static_cast<Chord*>(el)->notes());
                  qSort(rnl.begin(), rnl.end(), noteLessThan);
                  int idx = rnl.indexOf(static_cast<Note*>(element));
                  if (idx)
                        --idx;
                  re = rnl.value(idx);
                  }
            }

      if (re == 0)
            return 0;
      if (re->type() == CHORD)
            re = static_cast<Chord*>(re)->notes().back();
      return (Note*)re;
      }

//---------------------------------------------------------
//   downAltCtrl
//    niedrigste Note in Chord selektieren
//---------------------------------------------------------

Note* Score::downAltCtrl(Note* note) const
      {
      return note->chord()->downNote();
      }

//---------------------------------------------------------
//   upStaff
//---------------------------------------------------------

ChordRest* Score::upStaff(ChordRest* cr)
      {
      Segment* segment = cr->segment();

      if (cr->staffIdx() == 0)
            return cr;

      for (int track = (cr->staffIdx() - 1) * VOICES; track >= 0; --track) {
            Element* el = segment->element(track);
            if (!el)
                  continue;
            if (el->type() == NOTE)
                  el = static_cast<Note*>(el)->chord();
            if (el->isChordRest())
                  return static_cast<ChordRest*>(el);
            }
      return 0;
      }

//---------------------------------------------------------
//   downStaff
//---------------------------------------------------------

ChordRest* Score::downStaff(ChordRest* cr)
      {
      Segment* segment = cr->segment();
      int tracks = nstaves() * VOICES;

      if (cr->staffIdx() == nstaves() - 1)
            return cr;

      for (int track = (cr->staffIdx() + 1) * VOICES; track < tracks; --track) {
            Element* el = segment->element(track);
            if (!el)
                  continue;
            if (el->type() == NOTE)
                  el = static_cast<Note*>(el)->chord();
            if (el->isChordRest())
                  return static_cast<ChordRest*>(el);
            }
      return 0;
      }

//---------------------------------------------------------
//   nextMeasure
//---------------------------------------------------------

ChordRest* Score::nextMeasure(ChordRest* element, bool selectBehavior)
      {
      if (!element)
            return 0;

      MeasureBase* mb = element->measure()->next();
      while (mb && ((mb->type() != MEASURE) || (mb->type() == MEASURE && static_cast<Measure*>(mb)->multiMeasure() < 0)))
            mb = mb->next();

      Measure* measure = static_cast<Measure*>(mb);

      int endTick = element->measure()->last()->nextChordRest(element->track(), true)->tick();
      bool last = false;

      if (selection().state() == SEL_RANGE) {
            if (element->tick() != endTick && selection().tickEnd() <= endTick) {
                  measure = element->measure();
                  last = true;
                  }
            else if (element->tick() == endTick && selection().isEndActive())
                  last = true;
            }
      else if (element->tick() != endTick && selectBehavior) {
            measure = element->measure();
            last = true;
            }
      if (!measure) {
            measure = element->measure();
            last = true;
            }
      int staff = element->staffIdx();

      Segment* startSeg = last ? measure->last() : measure->first();
      for (Segment* seg = startSeg; seg; seg = last ? seg->prev() : seg->next()) {
            int etrack = (staff+1) * VOICES;
            for (int track = staff * VOICES; track < etrack; ++track) {
                  Element* pel = seg->element(track);

                  if (pel && pel->isChordRest())
                        return static_cast<ChordRest*>(pel);
                  }
            }
      return 0;
      }

//---------------------------------------------------------
//   prevMeasure
//---------------------------------------------------------

ChordRest* Score::prevMeasure(ChordRest* element)
      {
      if (!element)
            return 0;

      MeasureBase* mb = element->measure()->prev();
      while (mb && ((mb->type() != MEASURE) || (mb->type() == MEASURE && static_cast<Measure*>(mb)->multiMeasure() < 0)))
            mb = mb->prev();

      Measure* measure = static_cast<Measure*>(mb);

      int startTick = element->measure()->first()->nextChordRest(element->track())->tick();
      bool last = false;

      if ((selection().state() == SEL_RANGE)
         && selection().isEndActive() && selection().startSegment()->tick() <= startTick)
            last = true;
      else if (element->tick() != startTick) {
            measure = element->measure();
            }
      if (!measure) {
            measure = element->measure();
            last = false;
            }

      int staff = element->staffIdx();

      Segment* startSeg = last ? measure->last() : measure->first();
      for (Segment* seg = startSeg; seg; seg = last ? seg->prev() : seg->next()) {
            int etrack = (staff+1) * VOICES;
            for (int track = staff * VOICES; track < etrack; ++track) {
                  Element* pel = seg->element(track);

                  if (pel && pel->isChordRest())
                        return static_cast<ChordRest*>(pel);
                  }
            }
      return 0;
      }

