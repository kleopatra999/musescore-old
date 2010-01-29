//=============================================================================
//  MuseScore
//  Linux Music Score Editor
//  $Id$
//
//  Copyright (C) 2008-2010 Werner Schweer and others
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

#include "transposedialog.h"
#include "score.h"
#include "mscore.h"
#include "chord.h"
#include "note.h"
#include "key.h"
#include "staff.h"
#include "harmony.h"
#include "part.h"
#include "pitchspelling.h"
#include "measure.h"
#include "undo.h"
#include "keysig.h"
#include "utils.h"

//---------------------------------------------------------
//   TransposeDialog
//---------------------------------------------------------

TransposeDialog::TransposeDialog(QWidget* parent)
   : QDialog(parent)
      {
      setupUi(this);
      connect(transposeByKey, SIGNAL(clicked(bool)), SLOT(transposeByKeyToggled(bool)));
      connect(transposeByInterval, SIGNAL(clicked(bool)), SLOT(transposeByIntervalToggled(bool)));
      }

//---------------------------------------------------------
//   transposeByKeyToggled
//---------------------------------------------------------

void TransposeDialog::transposeByKeyToggled(bool val)
      {
      transposeByInterval->setChecked(!val);
      }

//---------------------------------------------------------
//   transposeByIntervalToggled
//---------------------------------------------------------

void TransposeDialog::transposeByIntervalToggled(bool val)
      {
      transposeByKey->setChecked(!val);
      }

//---------------------------------------------------------
//   mode
//---------------------------------------------------------

TransposeMode TransposeDialog::mode() const
      {
      return transposeByKey->isChecked() ? TRANSPOSE_BY_KEY : TRANSPOSE_BY_INTERVAL;
      }

//---------------------------------------------------------
//   direction
//---------------------------------------------------------

TransposeDirection TransposeDialog::direction() const
      {
      if (mode() == TRANSPOSE_BY_KEY) {
            if (closestKey->isChecked())
                  return TRANSPOSE_CLOSEST;
            if (upKey->isChecked())
                  return TRANSPOSE_UP;
            return TRANSPOSE_DOWN;
            }
      if (upInterval->isChecked())
            return TRANSPOSE_UP;
      return TRANSPOSE_DOWN;
      }

//---------------------------------------------------------
//   transpose
//---------------------------------------------------------

void Score::transpose()
      {
      if (last() == 0)     // empty score?
            return;
      if (selection()->state() == SEL_NONE) {
            QMessageBox::StandardButton sb = QMessageBox::question(mscore,
               tr("MuseScore: transpose"),
               tr("There is nothing selected. Transpose whole score?"),
               QMessageBox::Yes | QMessageBox::Cancel,
               QMessageBox::Yes
            );
            if (sb == QMessageBox::Cancel)
                  return;
            //
            // select all
            //
            _selection.setState(SEL_SYSTEM);
            _selection.setStartSegment(tick2segment(0));
            _selection.setEndSegment(
               tick2segment(last()->tick() + last()->tickLen())
               );
            _selection.setStaffStart(0);
            _selection.setStaffEnd(nstaves());
            }
      TransposeDialog td;
      td.enableTransposeKeys(_selection.state() == SEL_SYSTEM);
      int startStaffIdx = 0;
      int startTick = 0;
      if (selection()->state() == SEL_STAFF || selection()->state() == SEL_SYSTEM) {
            startStaffIdx = selection()->staffStart();
            startTick     = selection()->tickStart();
            }
      KeyList* km = staff(startStaffIdx)->keymap();
      int key     = km->key(startTick).accidentalType;
      td.setKey(key);
      if (!td.exec())
            return;

      int semitones;
      if (td.mode() == TRANSPOSE_BY_KEY)
            semitones = -1;    // TODO
      else {
            semitones = intervalList[td.transposeInterval()].semitones;
            if (td.direction() == TRANSPOSE_DOWN)
                  semitones = -semitones;
            }
      bool trKeys              = td.getTransposeKeys();
      bool transposeChordNames = td.getTransposeChordNames();

      if (_selection.state() != SEL_SYSTEM)
            trKeys = false;
      int d = semitones < 0 ? -semitones : semitones;
      bool fullOctave = (d % 12) == 0;
      if (fullOctave && td.mode() != TRANSPOSE_BY_KEY) {
            trKeys = false;
            transposeChordNames = false;
            }

      if (_selection.state() == SEL_SINGLE || _selection.state() == SEL_MULT) {
            foreach(Element* e, _selection.elements()) {
                  if (e->type() != NOTE)
                        continue;
                  Note* n = static_cast<Note*>(e);
                  if (td.mode() == TRANSPOSE_BY_KEY)
                        transposeByKey(n, td.transposeKey(), td.direction());
                  else
                        transposeByInterval(n, td.transposeInterval(), td.direction());
                  }
            return;
            }

      int startTrack = _selection.staffStart() * VOICES;
      int endTrack   = _selection.staffEnd() * VOICES;
      if (_selection.state() == SEL_SYSTEM) {
            startTrack = 0;
            endTrack   = nstaves() * VOICES;
            }

      for (int st = startTrack; st < endTrack; ++st) {
            for (Segment* segment = _selection.startSegment(); segment && segment != _selection.endSegment(); segment = segment->next1()) {
                  Element* e = segment->element(st);
                  if (!e || e->type() != CHORD)
                        continue;
                  Chord* chord = static_cast<Chord*>(e);
                  NoteList* notes = chord->noteList();

                  // we have to operate on a list copy because
                  // change pitch changes chord->noteList():
                  QList<Note*> nl;
                  for (iNote i = notes->begin(); i != notes->end(); ++i)
                        nl.append(i->second);
                  foreach(Note* n, nl) {
                        if (td.mode() == TRANSPOSE_BY_KEY)
                              transposeByKey(n, td.transposeKey(), td.direction());
                        else
                              transposeByInterval(n, td.transposeInterval(), td.direction());
                        }
                  }
            }

      if (trKeys) {
            transposeKeys(_selection.staffStart(), _selection.staffEnd(),
               _selection.tickStart(), _selection.tickEnd(),
               td.mode(), td.transposeKey(), semitones);
            }

      if (transposeChordNames) {
            Measure* sm = _selection.startSegment()->measure();
            Measure* em = _selection.endSegment()->measure();
            int stick   = _selection.startSegment()->tick();
            int etick   = _selection.endSegment()->tick();

            for (Measure* m = sm; m; m = m->nextMeasure()) {
                  foreach (Element* e, *m->el()) {
                        if (e->type() != HARMONY || e->tick() < stick)
                              continue;
                        Harmony* harmony = static_cast<Harmony*>(e);
                        if (harmony->tick() >= etick)
                              break;
                        if (td.mode() == TRANSPOSE_BY_KEY)
                              ; // TODO
                        else {
                              int rootTpc = transposeTpc(harmony->rootTpc(),
                                 td.transposeInterval(), td.direction());
                              int baseTpc = transposeTpc(harmony->baseTpc(),
                                 td.transposeInterval(), td.direction());
                              undoTransposeHarmony(harmony, rootTpc, baseTpc);
                              }
                        }
                  if (m == em)
                        break;
                  }
            }
      setLayoutAll(true);
      }

//---------------------------------------------------------
//   transposeStaff
//---------------------------------------------------------

void Score::cmdTransposeStaff(int staffIdx, int diff)
      {
      int startTrack = staffIdx * VOICES;
      int endTrack   = startTrack + VOICES;

      transposeKeys(staffIdx, staffIdx+1, 0, lastSegment()->tick(), TRANSPOSE_BY_INTERVAL, 0, diff);

     for (Segment* segment = firstSegment(); segment; segment = segment->next1()) {
           if (segment->subtype() != Segment::SegChordRest)
                 continue;
           for (int st = startTrack; st < endTrack; ++st) {
                 Element* e = segment->element(st);
                 if (!e)
                       continue;
                 Chord* chord = static_cast<Chord*>(e);
                 NoteList* notes = chord->noteList();

                 // we have to operate on a list copy because
                 // change pitch changes chord->noteList():
                 QList<Note*> nl;
                 for (iNote i = notes->begin(); i != notes->end(); ++i)
                       nl.append(i->second);
                 foreach(Note* n, nl)
                       transposeBySemitones(n, diff);
                 }
           }
#if 0
      for (Measure* m = firstMeasure(); m; m = m->nextMeasure()) {
            foreach (Element* e, *m->el()) {
                  if (e->type() != HARMONY)
                        continue;
                  Harmony* harmony = static_cast<Harmony*>(e);
                  undoTransposeHarmony(harmony, diff);
                  }
            }
#endif
//    spell(staffIdx, staffIdx+1, _selection.startSegment(), _selection.endSegment());
      spell();
      }

//---------------------------------------------------------
//   cmdConcertPitchChanged
//---------------------------------------------------------

void Score::cmdConcertPitchChanged(bool flag)
      {
      _undo->push(new ChangeConcertPitch(this, flag));

      foreach(Staff* staff, _staves) {
            Instrument* instr = staff->part()->instrument();
            int offset = instr->pitchOffset;
            if (offset == 0)
                  continue;
            if (!flag)
                 offset = -offset;
            cmdTransposeStaff(staff->idx(), offset);
            }
      }

//---------------------------------------------------------
//   transpose
//    transpose by semitones
//---------------------------------------------------------

void Score::transposeBySemitones(Note* n, int diff)
      {
      int npitch = n->pitch() + diff;
      int d      = diff < 0 ? -diff : diff;
      KeySigEvent key = n->staff()->key(n->chord()->tick());
      int tpc    = (d % 12) == 0 ? n->tpc() : pitch2tpc(npitch, key.accidentalType);
      int acc    = (d % 12) == 0 ? n->userAccidental() : 0;
      undoChangePitch(n, npitch, tpc, acc);
      }

//---------------------------------------------------------
//   transposeByKey
//    keysig -   -7(Cb) - +7(C#)
//    TODO
//---------------------------------------------------------

void Score::transposeByKey(Note* n, int nKey, TransposeDirection dir)
      {
      KeyList* km = n->staff()->keymap();
      int oKey    = km->key(n->chord()->tick()).accidentalType;
      int semitones;
      int cofSteps;     // circle of fifth steps
      if (nKey > oKey)
            cofSteps = nKey - oKey;
      else
            cofSteps = 12 - (oKey - nKey);
      semitones = (cofSteps * 7) % 12;
      int steps = (cofSteps * 4) % 7;

printf("transposeByKey %p %d %d-%d cofSteps %d, semitones %d steps %d\n",
   n, n->chord()->tick(), oKey, nKey, cofSteps, semitones, steps);

      if ((dir == TRANSPOSE_CLOSEST) && (semitones > 6))
            dir = TRANSPOSE_DOWN;

      if (dir == TRANSPOSE_DOWN) {
            semitones -= 12;
            }

      int interval = searchInterval(steps, semitones);
      if (interval == -1) {
            printf("transposeByKey: cannot determine interval %d %d\n", steps, semitones);
            return;
            }
      transposeByInterval(n, interval, dir);
      }

//---------------------------------------------------------
//   transposeByInterval
//---------------------------------------------------------

void Score::transposeByInterval(Note* n, int interval, TransposeDirection dir)
      {
      int npitch;
      int ntpc;
      transposeInterval(n->pitch(), n->tpc(), &npitch, &ntpc, interval, dir);
      undoChangePitch(n, npitch, ntpc, 0);
      }

//---------------------------------------------------------
//   transposeKeys
//    key -   -7(Cb) - +7(C#)
//---------------------------------------------------------

void Score::transposeKeys(int staffStart, int staffEnd, int tickStart, int tickEnd,
   TransposeMode mode, int key, int semitones)
      {
printf("transpose keys\n");
      for (int staffIdx = staffStart; staffIdx < staffEnd; ++staffIdx) {
            KeyList* km = staff(staffIdx)->keymap();
            for (iKeyList ke = km->lower_bound(tickStart);
               ke != km->lower_bound(tickEnd); ++ke) {
                  KeySigEvent oKey  = ke->second;
                  int tick  = ke->first;
                  int nKey = 0;
                  if (mode == TRANSPOSE_BY_KEY)
                        nKey  = key;
                  else
                        nKey  = transposeKey(oKey.accidentalType, semitones);
                  undoChangeKey(staff(staffIdx), tick, oKey, KeySigEvent(nKey));
                  }
            for (Segment* s = firstSegment(); s; s = s->next1()) {
                  if (s->subtype() != Segment::SegKeySig)
                        continue;
                  if (s->tick() < tickStart)
                        continue;
                  if (s->tick() >= tickEnd)
                        break;
                  KeySig* ks = static_cast<KeySig*>(s->element(staffIdx));
                  if (ks) {
                        KeySigEvent key  = km->key(s->tick());
                        KeySigEvent okey = km->key(s->tick() - 1);
                        key.naturalType  = okey.accidentalType;
                        _undo->push(new ChangeKeySig(ks, key));
                        }
                  }
            }
      }

