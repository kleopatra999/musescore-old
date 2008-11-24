//=============================================================================
//  MusE Score
//  Linux Music Score Editor
//  $Id: exportly.cpp,v 1.71 2006/03/28 14:58:58 wschweer Exp $
//
//  Copyright (C) 2007 Werner Schweer and others
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
//
// Lilypond export.
// For HISTORY, NEWS and TODOS: see end of file
//
// Some revisions by olagunde@start.no As I have no prior knowledge of
// C or C++, I have to resort to techniques well known in standard
// Pascal as it was known in 1983, when I got my very short
// programming education in the "Programming for Poets" course.
// Olav.

#include "barline.h"
#include "beam.h"
#include "config.h"
#include "dynamics.h"
#include "score.h"
#include "globals.h"
#include "part.h"
#include "hairpin.h"
#include "segment.h"
#include "measure.h"
#include "staff.h"
#include "layout.h"
#include "chord.h"
#include "note.h"
#include "rest.h"
#include "timesig.h"
#include "clef.h"
#include "keysig.h"
#include "key.h"
#include "page.h"
#include "text.h"
#include "tempotext.h"
#include "ottava.h"
#include "sym.h"
#include "pedal.h" 
#include "slur.h"
#include "style.h"
#include "tuplet.h"
#include "articulation.h"
#include "volta.h"
#include <string.h>

const int MAX_SLURS = 8;



//---------------------------------------------------------
//   ExportLy
//---------------------------------------------------------

class ExportLy {
  Score* score;
  QFile f;
  QTextStream os;
  int level;        // indent level
  int curTicks;
  Direction stemDirection;
  QString  staffid[32];
  int indx;
  int timeabove, timebelow;
  int  n, z1, z2, z3, z4; //timesignatures
  int barlen;
  bool slur;
  bool pickup; // must be preserved from voice to voice, but deleted before each new measure....
  bool graceswitch;
  int prevpitch, staffpitch, chordpitch, oktavdiff;
  int measurenumber, lastind, taktnr;
  bool repeatactive;
  bool firstalt,secondalt;
  enum voltatype {startending, endending, startrepeat, endrepeat, bothrepeat, doublebar, brokenbar, endbar, none};
  struct  voltareg { voltatype voltart; int barno; };
  struct voltareg  voltarray[255];
  int tupletcount;
  bool pianostaff;
  const Slur* slurre[MAX_SLURS];
  bool started[MAX_SLURS];
  int findSlur(const Slur* s) const;
  QString voicename[VOICES], partshort[32];
  const char *relativ, *staffrelativ;
  bool voiceActive[VOICES];
  QString  partname[32];
  QString cleannote, prevnote;
  struct InstructionAnchor 
  {
    Element* instruct;  // the element containing the instruction
    Element* anchor;    // the element it is attached to
    bool     start;     // whether it is attached to start or end
    int      tick;      // the timestamp
  };          
  
  int nextAnchor;
  struct InstructionAnchor anker;
  struct InstructionAnchor anchors[256];
  void storeAnchor(struct InstructionAnchor);
  void initAnchors();
  void removeAnchor(int);
  void resetAnchor(struct InstructionAnchor &ank);
  bool findSpecificMatchInMeasure(int, Staff*, Measure*, int, int);
  bool findMatchInMeasure(int, Staff*, Measure*, int, int);
  bool findMatchInPart(int, Staff*, Score*, Part*, int, int);
  bool findSpecificMatchInPart(int, Staff*, bool, Score*, int, int);


  void symbol(Symbol * sym, int staff);
  void tempoText(TempoText *, int);
  void words(Text *, int);
  void hairpin(Hairpin* hp, int staff, int tick);
  void ottava(Ottava* ot, int staff, int tick);
  void pedal(Pedal* pd, int staff, int tick);
  void dynamic(Dynamic* dyn, int staff);
  void textLine(TextLine* tl, int staff, int tick);
  //from xml's class directionhandler:
  void buildInstructionList(Part* p, int strack, int etrack);
  void buildInstructionList(Measure* m, bool dopart, Part* p, int strack, int etrack);
  void handleElement(Element* el, int sstaff, bool start);
 
  void indent();
  int getLen(int ticks, int* dots);
  void writeLen(int);
  QString tpc2name(int tpc);
  QString tpc2purename(int tpc);

  void writeScore();
  void writeVoiceMeasure(Measure*, Staff*, int, int);
  void writeKeySig(int);
  void writeTimeSig(TimeSig*);
  void writeClef(int);
  void writeChord(Chord*);
  void writeRest(int, int, int);
  void findVolta();
  void writeBarline(Measure *);
  int  voltaCheckBar(Measure *, int);
  void writeVolta(int, int);
  void findTuplets(Note *);
  void writeArticulation(Chord*);
  void writeScoreBlock();
  void checkSlur(Chord* chord);
  void doSlurStart(Chord* chord);
  void doSlurStop(Chord* chord);
  void anchorList();

public:
  ExportLy(Score* s) 
  {
    score  = s;
    level  = 0;
    curTicks = division;
    slur   = false;
    stemDirection = AUTO;
  }
  bool write(const QString& name);
};


//---------------------------------------------------------
// abs num value
//---------------------------------------------------------
int numval(int num)
{  if (num <0) return -num;
  return num;
}

void ExportLy::anchorList()
{ 
  // for debugging.
  int ix=1;
  printf("start of acnhor-list\n");
  printf("nr 0. ticks %d", anchors[0].tick);
  printf(" instruct->type %d\n", anchors[0].instruct->type());
    for (ix=0; ix<nextAnchor; ix++)
      {if anchors[ix]!=NULL
	if (1 < anchors[ix].tick < 654000)
	  {
	    printf("i: %d, instructticks: %d  ", ix, anchors[ix].tick);
	    printf("instructiontype: %d ", anchors[ix].instruct->type());
	    if (anchors[ix].start==true) printf(" true "); else printf(" false ");
	    printf("elementticks: %d \n", anchors[ix].anchor->tick());
	  }
      }
    printf("end of ankerlistetest\n");
}  


//---------------------------------------------------------
//   symbol
//---------------------------------------------------------

void ExportLy::symbol(Symbol* sym, int stav)
      {
      QString name = symbols[sym->sym()].name();
      if (name == "pedal ped")
	os << " \\sustainOn ";
      else if (name == "pedalasterisk")
	os << " \\sustainOff ";
      else {
            printf("ExportLy::symbol(): %s not supported", name.toLatin1().data());
            return;
            }
      }


//---------------------------------------------------------
//   tempoText
//---------------------------------------------------------

void ExportLy::tempoText(TempoText* text, int stav)
      {
	os << "^\\markup {" << text->getText() << "}";
      }



//---------------------------------------------------------
//   words
//---------------------------------------------------------

void ExportLy::words(Text* text, int stav)
      {
	os << "^\\markup {" << text->getText() << "}";
      }



//---------------------------------------------------------
//   hairpin
//---------------------------------------------------------

void ExportLy::hairpin(Hairpin* hp, int stav, int tick)
      {
	int art=2;
	art=hp->subtype();
	if (hp->tick() == tick)
	  {
	    if (art == 0) //diminuendo
	      os << "\\< ";
	    if (art == 1) //crescendo
	      os << "\\> ";
	  }
	else   os << "\\! ";
      }
//---------------------------------------------------------
//   ottava
// <octave-shift type="down" size="8" relative-y="14"/>
// <octave-shift type="stop" size="8"/>
//---------------------------------------------------------

void ExportLy::ottava(Ottava* ot, int stav, int tick)
      {
      int st = ot->subtype();
      if (ot->tick() == tick) {
            const char* sz = 0;
            switch(st) {
                  case 0:
                        sz = "-1";
                        break;
                  case 1:
                        sz = "-2";
                        break;
                  case 2:
                        sz = "1";
                        break;
                  case 3:
                        sz = "2";
                        break;
                  default:
                        printf("ottava subtype %d not understood\n", st);
                  }
            if (sz)
	      os << "#(set-octaviation " << sz << ") ";
            }
      else {
	if (st == 0)
	  os << "#(set-octaviation 0)";
            }
      }

//---------------------------------------------------------
//   pedal
//---------------------------------------------------------

void ExportLy::pedal(Pedal* pd, int stav, int tick)
      {
      if (pd->tick() == tick)
	{
	  os << "\\override Staff.pedalSustainStyle #\'bracket ";
	  os << "\\sustainDown ";
	}
      else
	os << "\\sustainUp ";
      }



//---------------------------------------------------------
//   dynamic
//---------------------------------------------------------
void ExportLy::dynamic(Dynamic* dyn, int stav)
{  
  QString t = dyn->getText();
  if (t == "p" || t == "pp" || t == "ppp" || t == "pppp" || t == "ppppp" || t == "pppppp"
      || t == "f" || t == "ff" || t == "fff" || t == "ffff" || t == "fffff" || t == "ffffff"
      || t == "mp" || t == "mf" || t == "sf" || t == "sfp" || t == "sfpp" || t == "fp"
      || t == "rf" || t == "rfz" || t == "sfz" || t == "sffz" || t == "fz") 
    {
      os << "\\" << t.toLatin1().data() << " ";
    }
  else if (t == "m" || t == "z") 
    {
      os << "\\"<< t.toLatin1().data() << " ";
    }
  //  else
  // words(t.toLatin1().data(), stav);
}//end dynamic
 


//---------------------------------------------------------
//   textLine
//---------------------------------------------------------

void ExportLy::textLine(TextLine* tl, int stav, int tick)
      {
	printf("textline\n");
//       QString rest;
//       QPointF p;

//       QString lineEnd = "none";
//       QString type;
//       int offs;
//       int n = 0;
//       if (tl->tick() == tick) {
//             QString lineType;
//             switch (tl->lineStyle()) {
//                   case Qt::SolidLine:
//                         lineType = "solid";
//                         break;
//                   case Qt::DashLine:
//                         lineType = "dashed";
//                         break;
//                   case Qt::DotLine:
//                         lineType = "dotted";
//                         break;
//                   default:
//                         lineType = "solid";
//                   }
//             rest += QString(" line-type=\"%1\"").arg(lineType);
//             p = tl->lineSegments().first()->userOff();
//             offs = tl->mxmlOff();
//             type = "start";
//             }
//       else {
//             if (tl->hook()) {
//                   lineEnd = tl->hookUp() ? "up" : "down";
//                   rest += QString(" end-length=\"%1\"").arg(tl->hookHeight().val() * 10);
//                   }
//             // userOff2 is relative to userOff in MuseScore
//             p = tl->lineSegments().last()->userOff2() + tl->lineSegments().first()->userOff();
//             offs = tl->mxmlOff2();
//             type = "stop";
//             }

//       n = findBracket(tl);
//       if (n >= 0)
//             bracket[n] = 0;
//       else {
//             n = findBracket(0);
//             bracket[n] = tl;
//             }

//       if (p.x() != 0)
//             rest += QString(" default-x=\"%1\"").arg(p.x() * 10);
//       if (p.y() != 0)
//             rest += QString(" default-y=\"%1\"").arg(p.y() * -10);

//       attr.doAttr(xml, false);
//       xml.stag(QString("direction placement=\"%1\"").arg((p.y() > 0.0) ? "below" : "above"));
//       if (tl->hasText()) {
//             xml.stag("direction-type");
//             xml.tag("words", tl->text());
//             xml.etag();
//             }
//       xml.stag("direction-type");
//       xml.tagE(QString("bracket type=\"%1\" number=\"%2\" line-end=\"%3\"%4").arg(type, QString::number(n + 1), lineEnd, rest));
//       xml.etag();
//       if (offs)
//             xml.tag("offset", offs);
//       if (staff)
//             xml.tag("staff", staff);
//       xml.etag();
      }


//--------------------------------------------------------
//  initAnchors
//--------------------------------------------------------
void ExportLy::initAnchors()
{ 
  int i;
  for (i=0; i<256; i++)
    resetAnchor(anchors[i]);
}



//--------------------------------------------------------
//   resetAnchor
//--------------------------------------------------------
void ExportLy::resetAnchor(struct InstructionAnchor &ank)
{
  ank.instruct=0;
  ank.anchor=0;
  ank.start=false;
  ank.tick=0;
}

//---------------------------------------------------------
//   deleteAnchor
//---------------------------------------------------------
void ExportLy::removeAnchor(int ankind)
{
  int i;
  resetAnchor(anchors[ankind]);
  for (i=ankind; i<=nextAnchor; i++)
    anchors[i]=anchors[i+1];
  resetAnchor(anchors[nextAnchor]);
  nextAnchor=nextAnchor-1;
}

//---------------------------------------------------------
//   storeAnchor
//---------------------------------------------------------

void ExportLy::storeAnchor(struct InstructionAnchor a)
      {
      if (nextAnchor < 256)
	{
	  anchors[nextAnchor++] = a;
	}
      else
            printf("InstructionHandler: too many instructions\n");
      resetAnchor(anker);
      }


//---------------------------------------------------------
//   handleElement -- handle all instructions attached to one specific element
//---------------------------------------------------------

void ExportLy::handleElement(Element* el, int sstaff, bool start)
{
  int i = 0;
  for (i = 0; i<=nextAnchor; i++)//run thru filled part of list
    {
      //anchored to start of this element:
      if (anchors[i].anchor != 0 and anchors[i].anchor==el && anchors[i].start == start) 
       {
	  Element* instruction = anchors[i].instruct; 
	  ElementType instructiontype = instruction->type();

	  switch(instructiontype) 
	    {
	    case SYMBOL:
	      printf("SYMBOL\n");
	      symbol((Symbol *) instruction, sstaff);
	      break;
	    case TEMPO_TEXT:
	      printf("TEMPOTEXT MEASURE\n");
	      tempoText((TempoText*) instruction, sstaff);
	      break;
	    case STAFF_TEXT:
	    case TEXT:
	      printf("TEXT\n");
	      words((Text*) instruction, sstaff);
	      break;
	    case DYNAMIC:
	      printf("funnet DYNAMIC i ankerliste\n");
	      dynamic((Dynamic*) instruction, sstaff);
	      break;
	    case HAIRPIN:
	      hairpin((Hairpin*) instruction, sstaff, anchors[i].tick);
	      break; 
	    case OTTAVA:
	      ottava((Ottava*) instruction, sstaff, anchors[i].tick);
	      break;
	    case PEDAL:
	      pedal((Pedal*) instruction, sstaff, anchors[i].tick);
	      break;
	    case TEXTLINE:
	      textLine((TextLine*) instruction, sstaff, anchors[i].tick);
	      break;
	    default:
	      printf("InstructionHandler::handleElement: direction type %s at tick %d not implemented\n",
		     elementNames[instruction->type()], anchors[i].tick);
	      break;
	    }
	  removeAnchor(i);
	  //	  resetAnchor(anchors[i]); //discard used anchors to avoid unnecessary doubles. 
	}
    } //foreach position i anchor-array.
}


//---------------------------------------------------------
//   findSpecificMatchInMeasure -- find chord or rest in measure
//     starting or ending at tick
//---------------------------------------------------------

bool ExportLy::findSpecificMatchInMeasure(int tick, Staff* stf, Measure* m, int strack, int etrack)
{
  bool found=false;
  for (int st = strack; st < etrack; ++st) 
    {
      for (Segment* seg = m->first(); seg; seg = seg->next()) 
	{
	  Element* el = seg->element(st);
	  if (!el)
	    continue;
	  if ((el->isChordRest() && el->staff() == stf) and ((el->tick() == tick) or ((el->tick() + el->tickLen()) == tick)))
	    {
	      anker.anchor=el;
	      found=true;
	      anker.tick=tick;
	      anker.start=true;
	      continue;
	}
    }
}
if (found=true) return true; else return false;
}


//---------------------------------------------------------
//   findMatchInMeasure -- find chord or rest in measure
//---------------------------------------------------------

bool ExportLy::findMatchInMeasure(int tick, Staff* st, Measure* m, int strack, int etrack)
      {
	bool found=false;
	found = findSpecificMatchInMeasure(tick, st, m, strack, etrack);
	return found;
      }

//---------------------------------------------------------
//   findSpecificMatchInPart -- find chord or rest in part
//     starting or ending at tick
//---------------------------------------------------------

bool ExportLy::findSpecificMatchInPart(int tick, Staff* st, bool start, Score* sc, int strack, int etrack)
{
  bool found=false;
  for (MeasureBase* mb = sc->measures()->first(); mb; mb = mb->next()) 
    {
      if (mb->type() != MEASURE)
	continue;
      Measure* m = (Measure*)mb;
      found= findSpecificMatchInMeasure(tick, st, m, strack, etrack);
    }
  return found;
}

//---------------------------------------------------------
//   findMatchInPart -- find chord or rest in part
//---------------------------------------------------------

bool ExportLy::findMatchInPart(int tick, Staff* st, Score* sc, Part* p, int strack, int etrack)
      {
	bool found=false;
	found = findSpecificMatchInPart(tick, st, true, sc, strack, etrack);
	found =	findSpecificMatchInPart(tick, st, false, sc, strack, etrack);
	return found;
      //      return (st->part() == p) ? new InstructionAnchor(tick) : 0;
      }

//---------------------------------------------------------
//   buildInstructionList -- associate instruction (measure relative elements)
//     with elements in segments to enable writing at the correct position
//     in the output stream. Called once for every part to handle all part-level elements.
//---------------------------------------------------------

void ExportLy::buildInstructionList(Part* p, int strack, int etrack)
{
  // part-level elements stored in the score layout

  foreach(Element* instruction, *(score->gel())) 
    {
      bool found=false;
      switch(instruction->type()) {
      case HAIRPIN:
      case OTTAVA:
      case PEDAL:
      case TEXTLINE:
	{ 
	  SLine* sl = (SLine*) instruction;
	  found=findMatchInPart(sl->tick(), sl->staff(), score, p, strack, etrack);
	  if (found)
	    { 
	      anker.instruct=instruction;  
	      storeAnchor(anker);
	      printf("found match in part at tick1: %d %d Type: %d\n", sl->tick(), anchors[nextAnchor].tick, instruction->type());
	    }
	  found=findMatchInPart(sl->tick2(), sl->staff(), score, p, strack, etrack);
	  if (found) 
	    { 
	      anker.instruct=instruction;
	      storeAnchor(anker);
	      printf("found match in part at tick2: %d %d Type: %d\n", sl->tick2(), anchors[nextAnchor].tick, instruction->type());
	    }
	}
	break;
      default:
	// all others silently ignored
	// printf("InstructionHandler::buildInstructionList: direction type %s not implemented\n",
	//        elementNames[instruction->type()]);
	break;
      }
    }// end foreach element....
  
  // part-level elements stored in measures
  for (MeasureBase* mb = score->measures()->first(); mb; mb = mb->next()) 
    {
      if (mb->type() != MEASURE)
	continue;
      Measure* m = (Measure*)mb;
      buildInstructionList(m, true, p, strack, etrack);
    }
}//end: buildInstructionList


//---------------------------------------------------------
//   buildInstructionList -- associate instruction (measure relative elements)
//     with elements in segments to enable writing at the correct position
//     in the output stream. Called once for every measure to handle either
//     part-level or measure-level elements.
//---------------------------------------------------------

void ExportLy::buildInstructionList(Measure* m, bool dopart, Part* p, int strack, int etrack)
{
  bool found=false;
  // loop over all measure relative elements in this measure
  for (ciElement ci = m->el()->begin(); ci != m->el()->end(); ++ci) 
    {
      Element* instruction = *ci;
      switch(instruction->type()) 
	{
	case DYNAMIC:
	case SYMBOL:
	case TEMPO_TEXT:
	case TEXT:
	case STAFF_TEXT:
	  found = findMatchInMeasure(instruction->tick(), instruction->staff(), m, strack, etrack);
	  if (found) 
	    {
	      anker.instruct=instruction;
	      printf("measure anker.instruct: %d\n", anker.instruct->type());
	      storeAnchor(anker);
	      found=false;
	    }
	  break;
	default:
	  printf("Intet measurerelevant anker\n");
	  break;
	}
    }
}

//---------------------------------------------------------
//   indent
//---------------------------------------------------------

void ExportLy::indent()
{
  for (int i = 0; i < level; ++i)
    os << "    ";
}


//-------------------------------------
// Find tuplets
//-------------------------------------

void ExportLy::findTuplets(Note* note)

{

  Tuplet* t = note->chord()->tuplet();
  int actNotes = 1;
  int nrmNotes = 1;

  if (t)
    {
      if (tupletcount == 0)
	{
	  actNotes = t->actualNotes();
	  nrmNotes = t->normalNotes();
	  tupletcount=actNotes;
	  os << "\\times " <<  nrmNotes << "/" << actNotes << "{" ;
	}
      else if (tupletcount>1)
	{
	  tupletcount--;
	}
    }
}//end of find-tuplets


//-----------------------------------------------------
//  voltaCheckBar
//
// supplements findVolta and called from there: check barlinetypes in
// addition to endings
//------------------------------------------------------
int ExportLy::voltaCheckBar(Measure* meas, int i)
{

  int blt = meas->endBarLineType();

  switch(blt)
    {
    case START_REPEAT:
      i++;
      voltarray[i].voltart=startrepeat;
      voltarray[i].barno=taktnr;
      break;
    case END_REPEAT:
      i++;
      voltarray[i].voltart=endrepeat;
      voltarray[i].barno=taktnr;
      break;
    case END_START_REPEAT:
      i++;
      voltarray[i].voltart=bothrepeat;
      voltarray[i].barno=taktnr;
      break;
    case END_BAR:
      i++;
      voltarray[i].voltart=endbar;
      voltarray[i].barno=taktnr;
      break;
    case DOUBLE_BAR:
      i++;
      voltarray[i].voltart=doublebar;
      voltarray[i].barno=taktnr;
    case BROKEN_BAR:
      i++;
      voltarray[i].voltart=brokenbar;
      voltarray[i].barno=taktnr;
    default:
      break;
    }//switch
  return i;
}//end voltacheckbarline



//------------------------------------------------------------------
//   findVolta -- find and register volta and repeats in entire piece
//   register them in voltarray for later use in writeVolta.
//------------------------------------------------------------------

void  ExportLy::findVolta()

{
  taktnr=0;
  lastind=0;
  int i=0;

  for (i=0; i<255; i++)
    {
      voltarray[i].voltart=none;
      voltarray[i].barno=0;
    }

  i=0;

  for (MeasureBase * m=score->layout()->first(); m; m=m->next())
    {// for all measures
      if (m->type() !=MEASURE )
	continue;
      ++taktnr;
      foreach(Element* el, *(m->score()->gel()))
	//walk thru all elements of measure
	{
	  if (el->type() == VOLTA)
	    {
	      Volta* v = (Volta*) el;

	      if (v->tick() == m->tick()) //If we are at the beginning of the measure
		{
		  i++;
		  //  if (v->subtype() == Volta::VOLTA_CLOSED)
		  // 		    {//lilypond developers have "never seen" last ending closed.
		  // 		      //So they are reluctant to implement it. Final ending is always "open" in lilypond.
		  // 		    }
		  // 		  else if (v->subtype() == Volta::VOLTA_OPEN)
		  // 		    {
		  // 		    }
		  voltarray[i].voltart = startending;
		  voltarray[i].barno=taktnr-1; //register as last element i previous measure
		}

	      if (v->tick2() == m->tick() + m->tickLen()) // if it is at the end of measure
		{
		  i++;
		  voltarray[i].voltart = endending;
		  voltarray[i].barno=taktnr;//last element of this measure
		  // 		  if (v->subtype() == Volta::VOLTA_CLOSED)
		  // 		    {// se comment above.
		  // 		    }
		  // 		  else if (v->subtype() == Volta::VOLTA_OPEN)
		  // 		    {// se comment above.
		  // 		    }

		}
	    }
	}// for all elements

      i=voltaCheckBar((Measure *) m, i);

    }//for all measures

  lastind=i;

}// end findvolta


//---------------------------------------------------------
//   exportLilypond
//---------------------------------------------------------

bool Score::saveLilypond(const QString& name)
{
  ExportLy em(this);
  return em.write(name);
}


//---------------------------------------------------------
//   writeClef
//---------------------------------------------------------

void ExportLy::writeClef(int clef)
{
  os << "\\clef ";
  switch(clef) {
  case CLEF_G:      os << "treble";       break;
  case CLEF_F:      os << "bass";         break;
  case CLEF_G1:     os << "\"treble^8\""; break;
  case CLEF_G2:     os << "\"treble^15\"";break;
  case CLEF_G3:     os << "\"treble_8\""; break;
  case CLEF_F8:     os << "\"bass_8\"";   break;
  case CLEF_F15:    os << "\"bass_15\"";  break;
  case CLEF_F_B:    os << "bass";         break;
  case CLEF_F_C:    os << "bass";         break;
  case CLEF_C1:     os <<  "soprano";     break;
  case CLEF_C2:     os <<  "mezzo-soprano";break;
  case CLEF_C3:     os <<  "alto";        break;
  case CLEF_C4:     os <<  "tenor";       break;
  case CLEF_TAB:    os <<  "tab";         break;
  case CLEF_PERC:   os <<  "percussion";  break;
  }
  os << " ";
}

//---------------------------------------------------------
//   writeTimeSig
//---------------------------------------------------------

void ExportLy::writeTimeSig(TimeSig* sig)
{
  sig->getSig(&n, &z1, &z2, &z3, &z4);

  timebelow=n;
  os << "\\time " << z1 << "/" << n << " ";
}

//---------------------------------------------------------
//   writeKeySig
//---------------------------------------------------------

void ExportLy::writeKeySig(int st)
{
  st = char(st & 0xff);
  if (st == 0)
    return;
  os << "\\key ";
  switch(st) {
  case 6:  os << "fis"; break;
  case 5:  os << "h";   break;
  case 4:  os << "e";   break;
  case 3:  os << "a";   break;
  case 2:  os << "d";   break;
  case 1:  os << "g";   break;
  case 0:  os << "c";   break;
  case -7: os << "ces"; break;
  case -6: os << "ges"; break;
  case -5: os << "des"; break;
  case -4: os << "as";  break;
  case -3: os << "es";  break;
  case -2: os << "bes"; break;
  case -1: os << "f";   break;
  default:
    printf("illegal key %d\n", st);
    break;
  }
  os << " \\major ";
}

//---------------------------------------------------------
//   tpc2name
//---------------------------------------------------------

QString ExportLy::tpc2name(int tpc)
{
  const char names[] = "fcgdaeb";
  int acc   = ((tpc+1) / 7) - 2;
  QString s(names[(tpc + 1) % 7]);
  switch(acc) {
  case -2: s += "eses"; break;
  case -1: s += "es";  break;
  case  1: s += "is";  break;
  case  2: s += "isis"; break;
  case  0: break;
  default: s += "??"; break;
  }
  return s;
}


//---------------------------------------------------------
//   tpc2purename
//---------------------------------------------------------

QString ExportLy::tpc2purename(int tpc)
{
  const char names[] = "fcgdaeb";
  QString s(names[(tpc + 1) % 7]);
  return s;
}


//--------------------------------------------------------
//  Slur functions, stolen from exportxml.cpp:
//
//---------------------------------------------------------
//   findSlur -- get index of slur in slur table
//   return -1 if not found
//---------------------------------------------------------

int ExportLy::findSlur(const Slur* s) const
{
  for (int i = 0; i < 8; ++i)
    if (slurre[i] == s) return i;
  return -1;
}

//---------------------------------------------------------
//   doSlurStart
//---------------------------------------------------------

void ExportLy::doSlurStart(Chord* chord)
{
  foreach(const Slur* s, chord->slurFor())
    {
      int i = findSlur(s);
      if (i >= 0) {
	slurre[i] = 0;
	started[i] = false;
	if (s->slurDirection() == UP) os << "^";
	os << "(";
      }
      else {
	i = findSlur(0);
	if (i >= 0) {
	  slurre[i] = s;
	  started[i] = true;
	  os << "(";
	}
	else
	  printf("no free slur slot");
      }
    }
}


//---------------------------------------------------------
//   doSlurStop
//   From exportxml.cpp:
//-------------------------------------------
void ExportLy::doSlurStop(Chord* chord)
{
  foreach(const Slur* s, chord->slurBack())
    {
      // check if on slur list
      int i = findSlur(s);
      if (i < 0) {
	// if not, find free slot to store it
	i = findSlur(0);
	if (i >= 0) {
	  slurre[i] = s;
	  started[i] = false;
	  os << ")";
	}
	else
	  printf("no free slur slot");
      }
    }
  for (int i = 0; i < 8; ++i)
    {
      if (slurre[i])
	{
	  if  (slurre[i]->endElement() == chord)
	    {
	      if (started[i]) {
		slurre[i] = 0;
		started[i] = false;
		os << ")";
	      }
	    }
	}
    }
}

//-------------------------
// checkSlur
//-------------------------
void ExportLy::checkSlur(Chord* chord)
{
  //init array:
  for (int i = 0; i < 8; ++i)
    {
      slurre[i] = 0;
      started[i] = false;
    }
  doSlurStop(chord);
  doSlurStart(chord);
}


//-----------------------------------
// helper routine for writeScore
// -- called from there
//-----------------------------------

void ExportLy::writeArticulation(Chord* c)
{
  foreach(Articulation* a, *c->getArticulations())
    {
      switch(a->subtype())
	{
	case UfermataSym:
	  os << "\\fermata";
	  break;
	case DfermataSym:
	  os << "_\\fermata";
	  break;
	case ThumbSym:
	  os << "\\thumb";
	  break;
	case SforzatoaccentSym:
	  os << "->";
	  break;
	case EspressivoSym:
	  os << "\\espressivo";
	  break;
	case StaccatoSym:
	  os << "-.";
	  break;
	case UstaccatissimoSym:
	  os << "-|";
	  break;
	case DstaccatissimoSym:
	  os << "_|";
	  break;
	case TenutoSym:
	  os << "--";
	  break;
	case UportatoSym:
	  os << "-_";
	  break;
	case DportatoSym:
	  os << "__";
	  break;
	case UmarcatoSym:
	  os << "-^";
	  break;
	case DmarcatoSym:
	  os << "_^";
	  break;
	case OuvertSym:
	  os << "\\open";
	  break;
	case PlusstopSym:
	  os << "-+";
	  break;
	case UpbowSym:
	  os << "\\upbow";
	  break;
	case DownbowSym:
	  os << "\\downbow";
	  break;
	case ReverseturnSym:
	  os << "\\reverseturn";
	  break;
	case TurnSym:
	  os << "\\turn";
	  break;
	case TrillSym:
	  os << "\\trill";
	  break;
	case PrallSym:
	  os << "\\prall";
	  break;
	case MordentSym:
	  os << "\\mordent";
	  break;
	case PrallPrallSym:
	  os << "\\prallprall";
	  break;
	case PrallMordentSym:
	  os << "\\prallmordent";
	  break;
	case UpPrallSym:
	  os << "\\prallup";
	  break;
	case DownPrallSym:
	  os << "\\pralldown";
	  break;
	case UpMordentSym:
	  os << "\\upmordent";
	  break;
	case DownMordentSym:
	  os << "\\downmordent";
	  break;
#if 0 // TODO: this are now Repeat() elements
	case SegnoSym:
	  os << "\\segno";
	  break;
	case CodaSym:
	  os << "\\coda";
	  break;
	case VarcodaSym:
	  os << "\\varcoda";
	  break;
#endif
	default:
	  printf("unsupported note attribute %d\n", a->subtype());
	  break;
	}// end switch
    }// end foreach
}// end writeArticulation();



//---------------------------------------------------------
//   writeChord
//---------------------------------------------------------

void ExportLy::writeChord(Chord* c)
{
  bool graceslur=false;
  int  purepitch;
  QString purename;

  // only the stem direction of the first chord in a beamed chord
  // group is relevant OG: -- for Mscore that is, causes trouble for
  // lily when tested on inv1.msc. So we only export stem directions
  // for gracenotes.

  if (c->beam() == 0 || c->beam()->elements().front() == c)
    {
      Direction d = c->stemDirection();
      if (d != stemDirection)
	{
	  stemDirection = d;
	  if ((d == UP) and (graceswitch == true))
	    os << "\\stemUp ";
	  else if ((d == DOWN)  and (graceswitch == true))
	    os << "\\stemDown ";
	  else if (d == AUTO)
	    {
	      if (graceswitch == true) os << "] ";
	      os << "\\stemNeutral ";
	    }
	}
    }

  bool tie=false;
  NoteList* nl = c->noteList();

  if (nl->size() > 1)
    os << "<"; //start of chord

  for (iNote i = nl->begin();;)
    {
      Note* n = i->second;
      NoteType gracen;

      gracen = n->noteType();
      switch(gracen)
	{
	case NOTE_INVALID:
	case NOTE_NORMAL: if (graceswitch==true)
	    {
	      graceswitch=false;
	      graceslur=true;
	      os << " } "; //end of grace
	    }
	  break;
	case NOTE_ACCIACCATURA:
	case NOTE_APPOGGIATURA:
	case NOTE_GRACE4:
	case NOTE_GRACE16:
	case NOTE_GRACE32:
	  if (graceswitch==false)
	    {
	      os << "\\grace{\\stemUp "; //as long as general stemdirecton is unsolved: graces always stemUp.
	      graceswitch=true;
	    }
	  else
	    if (graceswitch==true)
	      {
		os << "[( "; //grace always beamed and slurred
	      }
	  break;
	} //end of switch(gracen)


      findTuplets(n);

      os << tpc2name(n->tpc());
      if (n->tieFor()) tie=true;
      purepitch = n->pitch();
      purename = tpc2name(n->tpc());  //with -es or -is
      prevnote=cleannote;             //without -es or -is
      cleannote=tpc2purename(n->tpc());//without -es or -is

      if (purename.contains("eses")==1)  purepitch=purepitch+2;
      else if (purename.contains("es")==1)  purepitch=purepitch+1;
      else if (purename.contains("isis")==1) purepitch=purepitch-2;
      else if (purename.contains("is")==1) purepitch=purepitch-1;

      oktavdiff=prevpitch - purepitch;
      int oktreit=numval(oktavdiff);

      while (oktreit > 0)
	{
	  if ((oktavdiff < -6) or ((prevnote=="b") and (oktavdiff < -5)))
	    { //up
		os << "'";
		oktavdiff=oktavdiff+12;
	    }
	    else if ((oktavdiff > 6)  or ((prevnote=="f") and (oktavdiff > 5)))
	      {//down
		os << ",";
		oktavdiff=oktavdiff-12;
	      }
	  oktreit=oktreit-12;
	}

      prevpitch=purepitch;

      if (i == nl->begin()) chordpitch=prevpitch;
      //^^^^^^remember pitch of first chordnote to write next chord-or-note relative to.

      ++i; //number of notes in chord, we progress to next chordnote
      if (i == nl->end())
	break;
      os << " ";
    } //end of notelist = end of chord

  if (nl->size() > 1)
    os << ">"; //endofchord sign
  prevpitch=chordpitch;
  writeLen(c->tickLen());
  if (tie)
    {
      os << "~";
      tie=false;
    }

  if (tupletcount==1) {os << " } "; tupletcount=0; }
  if (graceslur==true)
    {
      os << " ) ";
      graceslur=false;
    }

  writeArticulation(c);
  checkSlur(c);

  os << " ";
}// end of writechord


//---------------------------------------------------------
//   getLen
//---------------------------------------------------------

int ExportLy::getLen(int l, int* dots)
{
  int len  = 4;

  if      (l == 6 * division)
    {
      len  = 1;
      *dots = 1;
    }
  else if (l == 5 * division)
    {
      len = 1;
      *dots = 2;
    }
  else if (l == 4 * division)
    len = 1;
  else if (l == 3 * division) // dotted half
    {
      len = 2;
      *dots = 1;
    }
  else if (l == 2 * division)
    len = 2;
  else if (l == division)
    len = 4;
  else if (l == division *3 /2)
    {
      len=4;
      *dots=1;
    }
  else if (l == division / 2)
    len = 8;
  else if (l == division*3 /4) //dotted 8th
    {
      len = 8;
      *dots=1;
    }
  else if (l == division / 4)
    len = 16;
  else if (l == division / 8)
    len = 32;
  else if (l == division * 3 /8) //dotted 16th.
    {
      len = 16;
      *dots = 1;
    }
  else if (l == division / 16)
    len = 64;
  else if (l == division /32)
    len = 128;
  //triplets, lily uses nominal value surrounded by \times 2/3 {  }
  //so we set len equal to nominal value
  else if (l == division * 4 /3)
    len = 2;
  else if (l == division * 2 /3)
    len = 4;
  else if (l == division /3)
    len = 8;
  else if (l == division /3*2)
    len = 16;
  else if (l == division /3*4)
    len = 32;
  else printf("unsupported len %d (%d,%d)\n", l, l/division, l % division);
  return len;
}

//---------------------------------------------------------
//   writeLen
//---------------------------------------------------------

void ExportLy::writeLen(int ticks)
{
  int dots = 0;
  int len = getLen(ticks, &dots);
  if (ticks != curTicks) {
    os << len;
    for (int i = 0; i < dots; ++i)
      os << ".";
    curTicks = ticks;
     }
}

//---------------------------------------------------------
//   writeRest
//    type = 0    normal rest
//    type = 1    whole measure rest
//    type = 2    spacer rest
//---------------------------------------------------------

void ExportLy::writeRest(int tick, int l, int type)
{
  if (type == 1) {
    // write whole measure rest
    int z, n;
    score->sigmap->timesig(tick, z, n);
    os << "R1*" << z << "/" << n;
    curTicks = -1;
  }
  else if (type == 2) {
    os << "s";
    writeLen(l);
  }
  else {
    os << "r";
    writeLen(l);
  }
  os << " ";
}


//--------------------------------------------------------
//   writeVolta
//--------------------------------------------------------
void ExportLy::writeVolta(int measurenumber, int lastind)
{
  bool utgang=false;
  int i=0;

  while ((voltarray[i].barno < measurenumber) and (i<=lastind))
    {
      //find the present measure
      i++;
    }

  if (measurenumber==voltarray[i].barno)
    {
      while (utgang==false)
	{
	  switch(voltarray[i].voltart)
	    {
	    case startrepeat:
	      indent();
	      os << "\\repeat volta 2 {";
	      firstalt=false;
	      secondalt=false;
	      break;
	    case endrepeat:
	      if ((repeatactive==true) and (secondalt==false))
		{
		  os << "} % end of repeatactive\n";
		  // repeatactive=false;
		}
	      indent();
	      break;
	    case bothrepeat:
	      if (firstalt==false)
		{
		  os << "} % end of repeat\n";
		  indent();
		  os << "\\repeat volta 2 {";
		  firstalt=false;
		  secondalt=false;
		  repeatactive=true;
		}
	      break;
	    case doublebar:
	      indent();
	      os << "\\bar \"||\"";
	      break;
	      // 	    case brokenbar:
	      // 	      indent();
	      // 	      os << "\\bar \"| |:\"";
	      // 	      break;
	    case startending:
	      if (firstalt==false)
		{
		  os << "} % end of repeat except alternate endings\n";
		  indent();
		  os << "\\alternative{ {  ";
		  firstalt=true;
		}
	      else
		{
		  os << "{ ";
		  indent();
		  firstalt=false;
		  secondalt=true;
		}
	      break;
	    case endending:
	      if (firstalt)
		{
		  os << "} %close alt1\n";
		  secondalt=true;
		  repeatactive=true;
		}
	      else
		{
		  os << "} } %close alternatives\n";
		  secondalt=false;
		  firstalt=true;
		  repeatactive=false;
		}
	      break;
	    case endbar:
	      os << "\\bar \"|.\"";
	      break;
          default:
	    case none: printf("strange voltarraycontents?\n");
	      break;
	    }//end switch

	  if (voltarray[i+1].barno==measurenumber)
	    {
	      i++;
	    }
	  else utgang=true;
	}// end of while utgang false;
    }// if barno=measurenumber
}// end writevolta



//---------------------------------------------------------
//   writeVoiceMeasure
//---------------------------------------------------------
void ExportLy::writeVoiceMeasure(Measure* m, Staff* staff, int staffInd, int voice)
{
  int i=0;
  char cvoicenum;
  measurenumber=m->no()+1;

  if (measurenumber==1)
    {
      level=0;
      indent();
      cvoicenum=voice+65;
      //there must be more elegant ways to do this, but whatever...
      voicename[voice] = partshort[staffInd];
      voicename[voice].append("voice");
      voicename[voice].append(cvoicenum);
      voicename[voice].prepend("A");
      voicename[voice].remove(QRegExp("[0-9]"));
      voicename[voice].remove(QChar('.'));
      voicename[voice].remove(QChar(' '));

      os << voicename[voice];
      os << " = \\relative c" << relativ <<" \n";
      indent();
      os << "{\n";
      level++;
      indent();
      if (voice==0)
	{
	  os <<"\\set Staff.instrumentName = #\"" << partname[staffInd] << "\"\n";
	  indent();
	  os << "\\set Staff.shortInstrumentName = #\"" <<partshort[staffInd] << "\"\n";
	  indent();
	  writeClef(staff->clef(0));
	  writeKeySig(staff->keymap()->key(0));
	  os << "\n";
	  indent();
	  score->sigmap->timesig(0, z1, n);
	  os << "\\time " << z1<< "/" << n << " \n";
	  indent();
	  os << "\n\n";
	}

      switch(voice)
	{
	case 0: break;
	case 1:
	  os <<"\\voiceTwo" <<"\n\n";
	  break;
	case 2:
	  os <<"\\voiceThree" <<"\n\n";
	  break;
	case 3:
	  os <<"\\voiceFour" <<"\n\n";
	  break;
	}
      //check for implicit startrepeat before first measure:
      i=0;
      while ((voltarray[i].voltart != startrepeat) and (voltarray[i].voltart != endrepeat)
	     and (voltarray[i].voltart !=bothrepeat) and (i<=lastind))
	{
	  i++;
	}

      if (i<=lastind)
	{
	  if ((voltarray[i].voltart==endrepeat) or (voltarray[i].voltart==bothrepeat))
	    {
	      indent();
	      os << "\\repeat volta 2 { \n";
	      repeatactive=true;
	    }
	}
    }// if start of first measure

  indent();
  int tick = m->tick();
  barlen = m->tickLen();
  int measuretick=0;
  Element* e;

  for(Segment* s = m->first(); s; s = s->next())
    {
      // for all segments in measure.
      e = s->element(staffInd * VOICES + voice);

      if (!(e == 0 || e->generated()))  voiceActive[voice] = true;
      else  continue;

      switch(e->type())
	{
	case CLEF:
	  writeClef(e->subtype());
	  break;
	case TIMESIG:
	  {
	    writeTimeSig((TimeSig*)e);
	    os << "\n\n";
	    barlen=m->tickLen();
	    int nombarlen=z1*division;
	    if (n==8) nombarlen=nombarlen/2;
	    if ((barlen<nombarlen) and (measurenumber==1))
	      {
		pickup=true;
		int punkt=0;
		int partial=getLen(barlen, &punkt);
		indent();
		switch(punkt)
		  {
		  case 0: os << "\\partial " << partial << "\n";
		    break;
		  case 1:
		    {
		      partial=(partial*2);
		      os << "\\partial " << partial << "*3 \n";
		    }
		    break;
		  case 2:
		    {
		      partial=partial*4;
		      os << "\\partial " << partial << "*7 \n";
		      break;
		    }
		  } //end switch (punkt)
	      }
	    indent();
	    break;	  }
	case KEYSIG:
	  writeKeySig(e->subtype());
	  break;
	case CHORD:
	  {
	    int ntick = e->tick() - tick;
	    if (ntick > 0)
	      {
		writeRest(tick, ntick, 2);
		curTicks=-1;
	      }
	    tick += ntick;
	    measuretick=measuretick+ntick;
	    // }
	    writeChord((Chord*)e);
	    tick += e->tickLen();
	    measuretick=measuretick+e->tickLen();
	  }
	  break;
	case REST:
	  {
	    int l = e->tickLen();
	    if (l == 0) {
	      l = ((Rest*)e)->segment()->measure()->tickLen();
	      writeRest(e->tick(), l, 1);
	    }
	    else
	      writeRest(e->tick(), l, 0);
	    tick += l;
	    measuretick=measuretick+l;
	  }
	  break;
	case BAR_LINE: //We never arrive here!!??
	  break;
	case BREATH:
	  os << "\\breathe ";
	  break;
	default:
	  printf("Export Lilypond: unsupported element <%s>\n", e->name());
	  break;
	}
      handleElement(e, staffInd, true); //check for instructions anchored to element e.
      //check for instructions anchored to start of next element
      Element * nextel;
      Segment* nextseg;
      nextseg=s->next();
      if(nextseg) 
	{
	  nextel=nextseg->element(staffInd * VOICES + voice);
	  handleElement(nextel, staffInd, false); 
	}
    } //end for all segments


  if (voiceActive[voice] == false)  //fill empty  bar with silent rest
    {
      if ((pickup) and (measurenumber==1))
	{
	  int punkt=0;
	  int partial=getLen(barlen, &punkt);

	  switch(punkt)
	    {
	    case 0: os << "s" << partial << " ";
	      break;
	    case 1:
	      {
		partial=(partial*2);
		os << "s" << partial << "*3 ";
	      }
	      break;
	    case 2:
	      {
		partial=partial*4;
		os << "s" << partial << "*7 ";
		break;
	      }
	    } //end switch (punkt)
	}//end if pickup
      else
	{
	  os << "s1";
	  curTicks=-1;
	}
    }//end voice not active
  else  if ((measuretick < barlen) and (measurenumber>1))
      {
	printf("underskudd i activvoice measure %d\n", measurenumber);
	int negative=barlen-measuretick;
	writeRest(tick, negative, 2);
	curTicks=-1;
      }

  writeVolta(measurenumber, lastind);
  os << " | % " << m->no()+1 << "\n" ; //barcheck and barnumber
} //end write VoiceMeasure


//---------------------------------------------------------
//   writeScore
//---------------------------------------------------------

void ExportLy::writeScore()
{
  firstalt=false;
  secondalt=false;
  tupletcount=0;
  char  cpartnum;
  chordpitch=41;
  repeatactive=false;
  int staffInd = 0;
  int np = score->parts()->size();
  graceswitch=false;
  int voice=0;
  cleannote="c";
  prevnote="c";
  nextAnchor=0;
  initAnchors();
  resetAnchor(anker);

  if (np > 1)
    {
      //Number of staffs in each part, to be used when we implement braces and brackets.
    }

  foreach(Part* part, *score->parts())
    {
      int n = part->staves()->size();
      partname[staffInd]  = part->longName();
      partshort[staffInd] = part->shortName();
      curTicks=-1;
      pickup=false;
      if (n > 1)
	{//number of staffs per instrument.
	  pianostaff=true;
	}
      
      int staves = part->nstaves();
      printf("staves: %d, np: %d\n", staves, np);
      int strack = score->staffIdx(part) * VOICES;
      printf("strack: %d\n", strack);
      int etrack = strack + np * VOICES;
      printf("etrack: %d\n", etrack);
      buildInstructionList(part, strack, etrack);
      
      //     anchorList(); 
      
      foreach(Staff* staff, *part->staves())
	{

	  os << "\n";

	  switch(staff->clef(0))
	    {
	    case CLEF_G:
	      relativ="'";
	      staffpitch=12*5;
	      break;
	    case CLEF_TAB:
	    case CLEF_PERC:
	    case CLEF_G3:
	    case CLEF_F:
	      relativ="";
	      staffpitch=12*4;
	      break;
	    case CLEF_G1:
	    case CLEF_G2:
	      relativ="''";
	      staffpitch=12*6;
	      break;
	    case CLEF_F_B:
	    case CLEF_F_C:
	    case CLEF_F8:
	      relativ=",";
	      staffpitch=12*3;
	      break;
	    case CLEF_F15:
	      relativ=",,";
	      staffpitch=12*2;
	      break;
	    case CLEF_C1:
	    case CLEF_C2:
	    case CLEF_C3:
	    case CLEF_C4:
	      relativ="'";
	      staffpitch=12*5;
	      break;
	    }

	  staffrelativ=relativ;

	  cpartnum = staffInd + 65;
	  staffid[staffInd] = partshort[staffInd];
	  staffid[staffInd].append("part");
	  staffid[staffInd].append(cpartnum);
	  staffid[staffInd].prepend("A");
	  staffid[staffInd].remove(QRegExp("[0-9]"));
	  staffid[staffInd].remove(QChar('.'));
	  staffid[staffInd].remove(QChar(' '));

	  findVolta();

	  for (voice = 0; voice < VOICES; ++voice)  voiceActive[voice] = false;

	  for (voice = 0; voice < VOICES; ++voice)
	    {
	      prevpitch=staffpitch;
	      relativ=staffrelativ;
	      for (MeasureBase* m = score->layout()->first(); m; m = m->next())
		{
		  if (m->type() != MEASURE)
		    continue;
		  writeVoiceMeasure((Measure*)m, staff, staffInd, voice);
		}
	      level=0;
	      indent();
	      os << "}% end of last bar in partorvoice\n\n";
	    }

	  int voiceno=0;

	  for (voice = 0; voice < VOICES; ++voice)
	    if (voiceActive) voiceno++;

	  if (voiceno>1)
	    {
	      level=0;
	      indent();
	      os << staffid[staffInd] << " = \\simultaneous{\n";
	      level++;
	      indent();
	      os << "\\override Staff.NoteCollision  #'merge-differently-headed = ##t\n";
	      indent();
              os << "\\override Staff.NoteCollision  #'merge-differently-dotted = ##t\n";
	      ++level;
	      for (voice = 0; voice < VOICES; ++voice)
		{
		  if (voiceActive[voice])
		    {
		      indent();
		      os << "\\context Voice = \"" << voicename[voice] << "\" \\" << voicename[voice]  << "\n";
		    }
		}
	      os << "} \n";
	      level=0;
	      indent();
	    }

	  ++staffInd;
	}// end of foreach staff
      staffid[staffInd]="laststaff";
      if (n > 1) {
	--level;
	indent();
      }
    }
}// end of writeScore


//-------------------
// write score-block: combining parts and voices, drawing brackets and braces, at end of lilypond file
//-------------------
void ExportLy::writeScoreBlock()
{
  level=0;
  os << "\n\\score { \n";
  level++;
  indent();
  os << "\\relative << \n";

  if (pianostaff)
    {
      ++level;
      indent();
      os << "\\context PianoStaff <<\n";
      indent();
      os << "\\set PianoStaff.instrumentName=\"Piano\" \n";
    }

  indx=0;

  while (staffid[indx]!="laststaff")
    {
      ++level;
      indent();
      os << "\\context Staff = O" << staffid[indx] << "G" << "  << \n";
      ++level;
      indent();
      os << "\\context Voice = O" << staffid[indx] << "G \\" << staffid[indx] << "\n";
      if (pianostaff)
	{
	  indent();
	  os << "\\set Staff.instrumentName = #\"\"\n";
	  indent();
	  os << "\\set Staff.shortInstrumentName = #\"\"\n";
	}
      --level;
      indent();
      os << ">>\n";
      ++indx;
    }

  if (pianostaff)
    {
      --level;
      indent();
      os << ">>\n";
      pianostaff=false;
    }

  indent();
  os << "\\set Score.hairpinToBarline = ##t\n";
  --level;
  indent();
  os << ">>\n";
  --level;
  indent();
  os <<"}\n";

}// end scoreblock


//---------------------------------------------------------
//   write
//---------------------------------------------------------

bool ExportLy::write(const QString& name)
{
  pianostaff=false;
  f.setFileName(name);
  if (!f.open(QIODevice::WriteOnly))
    return false;
  os.setDevice(&f);
  os << "%=============================================\n"
    "%   created by MuseScore Version: " << VERSION << "\n"
    "%=============================================\n"
    "\n"
    "\\version \"2.10.5\"\n\n";     // target lilypond version

  //---------------------------------------------------
  //    Page format
  //---------------------------------------------------

  PageFormat* pf = score->pageFormat();
  os << "#(set-default-paper-size ";
  switch(pf->size) {
  default:
  case  0: os << "\"a4\""; break;
  case  2: os << "\"letter\""; break;
  case  8: os << "\"a3\""; break;
  case  9: os << "\"a5\""; break;
  case 10: os << "\"a6\""; break;
  case 29: os << "\"11x17\""; break;
  }

  if (pf->landscape) os << " 'landscape";

  os << ")\n\n";

  // TODO/O.G.: better choose between standard formats and specified paper
  // dimensions. We normally don't need both.

  double lw = pf->width() - pf->evenLeftMargin - pf->evenRightMargin;
  os << "\\paper {\n"
    "  line-width    = " << lw * INCH << "\\mm\n"
    "  left-margin   = " << pf->evenLeftMargin * INCH << "\\mm\n"
    "  top-margin    = " << pf->evenTopMargin * INCH << "\\mm\n"
    "  bottom-margin = " << pf->evenBottomMargin * INCH << "\\mm\n"
    "  }\n\n";


  //---------------------------------------------------
  //    score
  //---------------------------------------------------

  os << "\\header {\n";

  ++level;
  const MeasureBase* m = score->layout()->first();
  foreach(const Element* e, *m->el()) {
    if (e->type() != TEXT)
      continue;
    QString s = ((Text*)e)->getText();
    indent();
    switch(e->subtype()) {
    case TEXT_TITLE:
      os << "title = ";
      break;
    case TEXT_SUBTITLE:
      os << "subtitle = ";
      break;
    case TEXT_COMPOSER:
      os << "composer = ";
      break;
    case TEXT_POET:
      os << "poet = ";
      break;
    default:
      printf("text-type %d not supported\n", e->subtype());
      os << "subtitle = ";
      break;
    }
    os << "\"" << s << "\"\n";
  }
  indent();
  os << "}\n";

  writeScore();

  writeScoreBlock();

  f.close();
  return f.error() == QFile::NoError;
}// end of function "write"





/*----------------------- NEWS and HISTORY:--------------------  */
/* NEW 24. nov. 2008:
   -- added dynamic signs and staff-text, by stealing from exportxml.cpp.

/* NEW 1. nov. 2008
   --pickupbar (But not irregular bars in general.)
   --ties
   --management of incomplete bars in voices 2-4.
*/

/* NEW 26. oct. 2008
  - voice separation and recombination in score-block for easier editing of Lilypondfile.
    todo/unresolved: writes voices 2-4 in Lilypond file even if voice is empty.
  - better finding of correct octave when jumping intervals of fifths or more.
*/


/* NEW 10.oct.2008:
   - rudimentary handling of slurs.
   - voltas and endings
   - dotted 8ths and 4ths.
   - triplets, but not general tuplets.
   - PianoStaff reactivated.*/


/*NEW:
  1. dotted 16th notes
  2. Relative pitches
  3. More grace-note types
  4. Slurs and beams in grace-notes
  5. Some adjustments in articulations
  6. separation of staffs/voices from score-block. Unfinished for pianostaffs/GrandStaffs and voices on one staff.
  7. completed the clef-secton.
  Points 2 and 6, and also in a smaller degree 5, enhances human readability of lilypond-code.
*/


/*----------------------TODOS------------------------------------*/

/* TODO: PROJECTS

   1  avoid empty output in voices 2-4. Does not affect visual endresult. 
        Pre-check for empty voices? --- or buffering or back-patching?
   2. Segno etc.                -----"-------
   3. Piano staffs/GrandStaffs, system brackets and braces.
   - Lyrics
   -. Collisions in crowded multi-voice staffs (e.g. cello-suite).
   4. General tuplets
   -  Use linked list instead of static array for dynamics etcs.
   
   - etc.etc.etc.etc........
*/

/*TODO: BUGS
  
  - still problems with finding the correct octave in the case of
    large intervals involving chords.

  - Piano-staff only works for piano alone, and messes things up if
  piano is part of a larger score. Solution: Awaiting implementaton of braces
  and brackets.

   - \stemUp \stemDown : sometimes correct sometimes not??? Maybe I
  have not understood Lily's rules for the use of these commands?
  Lily's own stem direction algorithms are good enough. Until a better
  idea comes along: drop export of stem direction and leave it to
  LilyPond.
 */
