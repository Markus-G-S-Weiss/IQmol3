/*******************************************************************************

  Copyright (C) 2022 Andrew Gilbert

  This file is part of IQmol, a free molecular visualization program. See
  <http://iqmol.org> for more details.

  IQmol is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

  IQmol is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along
  with IQmol.  If not, see <http://www.gnu.org/licenses/>.

********************************************************************************/

#include "AtomLayer.h"
#include "Util/Preferences.h"
#include "Viewer/Viewer.h"
#include "Viewer/PovRayGen.h"
#include "Util/GLShape.h"
#include <openbabel/elements.h>
#include <openbabel/data.h>
#include <QColor>
#include <QMenu>
#include <QActionGroup>
#include <vector>
#include <string>
#define _USE_MATH_DEFINES
#include <cmath>

#include <QtDebug>


using namespace qglviewer;
using namespace OpenBabel;

namespace IQmol {
namespace Layer {

// Static Data
GLfloat Atom::s_radiusBallsAndSticks       = 0.40;
GLfloat Atom::s_radiusWireFrame            = 0.50;  // pixels
GLfloat Atom::s_radiusTubes                = 0.10;
 
GLfloat Atom::s_vibrationVectorColor[]     = { 0.0f, 0.0f, 0.0f, 0.0f };
GLfloat Atom::s_vibrationAmplitude         = 0.0f;
GLfloat Atom::s_vibrationVectorScale       = 1.0f;
bool    Atom::s_vibrationDisplayVector     = false;
bool    Atom::s_vibrationColorInitialized  = false;  // There must be a better way



// Static functions
double Atom::distance(Atom* A, Atom* B) 
{
   return (A->getPosition() - B->getPosition()).norm();
}


double Atom::angle(Atom* A, Atom* B, Atom* C) 
{
  if ( (A == B) || (B == C)) return 0.0;
  Vec u(B->getPosition() - A->getPosition());
  Vec v(B->getPosition() - C->getPosition());
  double theta(u * v / (u.norm() * v.norm())); 
  return std::acos(theta) * 180.0 / M_PI; 
}


double Atom::torsion(Atom* A, Atom* B, Atom* C , Atom* D) 
{
   Vec a(B->getPosition() - A->getPosition());
   Vec b(C->getPosition() - B->getPosition());
   Vec c(D->getPosition() - C->getPosition());
   double x((b.norm() * a) * cross(b,c));
   double y(cross(a,b) * cross(b,c));
   return std::atan2(x,y) * 180.0 / M_PI;
}


void Atom::setVibrationVectorColor(QColor const& color) 
{
   s_vibrationVectorColor[0] = color.redF();
   s_vibrationVectorColor[1] = color.greenF();
   s_vibrationVectorColor[2] = color.blueF();
   s_vibrationVectorColor[3] = color.alphaF();
   s_vibrationColorInitialized = true;
}


Atom::Atom(int Z, QString const& label) 
 : Primitive(label.isEmpty() ? "Atom" : label), 
   m_charge(0.0), 
   m_spin(0.0),
   m_nmr(0.0),
   m_label(label),
   m_smallerHydrogens(true), 
   m_hideHydrogens(false), 
   m_haveNmrShift(false), 
   m_reorderIndex(0), 
   m_hybridization(0)
{
   setAtomicNumber(Z);
   if (!s_vibrationColorInitialized) {
      setVibrationVectorColor(Preferences::VibrationVectorColor());
   }

   // We don't allow changes to the valency at the moment as it seems to be unstable
   // Need to check the OpenBabel code again.
   return;
   QActionGroup* hybrids(new QActionGroup(this));
   QStringList labels;
   labels << "sp" << "sp2" << "sp3" << "Square Planar" 
           << "Trigonal Bipyramid" << "Octahedral";

   for (int i = 0; i < labels.size() ; ++i) {
       QAction* action(newAction(labels[i]));
       action->setData(i+1);
       action->setCheckable(true);
       action->setChecked(false);
       hybrids->addAction(action);
       connect(action, SIGNAL(triggered()), this, SLOT(updateHybridization()));
   }
}


void Atom::updateHybridization()
{
   qDebug() << "Hybridization update called";
   QAction* action(qobject_cast<QAction*>(sender()));
   if (action) {
      m_hybridization = action->data().toInt();
      switch (m_hybridization) {
         case 6:  m_valency = 6;  break;
         case 5:  m_valency = 5;  break;
         case 4:  m_valency = 4;  break;
         case 3:  m_valency = 4;  break;
         case 2:  m_valency = 3;  break;
         case 1:  m_valency = 2;  break;

         default: 
            m_hybridization = 0;  
            break;
      }
   }

   qDebug() << "Hybridization/valency updated" << m_hybridization << m_valency;
}


void Atom::resetMass()
{
   m_mass = OpenBabel::OBElements::GetMass(m_atomicNumber);
}


void Atom::setAtomicNumber(unsigned int const Z) 
{
   // Little bit of a hack to allow allpha sorting to draw atoms after
   // bonds in case of ordering problems
   // setAlpha(0.999);
   m_atomicNumber = Z;
   m_vdwRadius = OpenBabel::OBElements::GetVdwRad(Z);
   m_mass      = OpenBabel::OBElements::GetMass(Z);
   m_symbol    = QString(OpenBabel::OBElements::GetSymbol(Z));
   m_valency   = OpenBabel::OBElements::GetMaxBonds(Z);

   double r, g, b;
   OpenBabel::OBElements::GetRGB(Z, &r, &g, &b);
   m_color[0] = r;
   m_color[1] = g;
   m_color[2] = b;
   m_color[3] = m_alpha;
}


void Atom::setIndex(int const index)
{ 
   m_index = index; 
   if (m_label.isEmpty()) {
      setText(m_symbol + QString::number(index));
   }else {
      setText(m_label);
   }
}


void Atom::povray(PovRayGen& povray)
{
   QColor color;
   color.setRgbF(m_color[0], m_color[1], m_color[2], m_color[3]);
   double radius(getRadius());
   if (m_drawMode == Primitive::WireFrame) radius = 0.02;
   povray.writeAtom(getPosition(), color, radius);
}


void Atom::draw()
{
   glColor4fv(m_color);
   drawPrivate(getRadius(false));
   if (s_vibrationDisplayVector) drawDisplacement();
}


void Atom::drawFast()
{
   glColor4fv(m_color);
   drawPrivate(getRadius(false));
}

void Atom::drawFlat()
{
   glDisable(GL_LIGHTING);
   drawPrivate(getRadius(false));
   glEnable(GL_LIGHTING);
}



void Atom::drawSelected()
{
   glColor4fv(Primitive::s_selectColor);
   drawPrivate(getRadius(true));
}


void Atom::drawPrivate(double const radius)
{
   if (hideHydrogens()) return;
   glPushMatrix();
   glMultMatrixd(m_frame.matrix());
   
   glTranslatef(s_vibrationAmplitude * m_displacement.x, 
                s_vibrationAmplitude * m_displacement.y, 
                s_vibrationAmplitude * m_displacement.z);

   if (m_drawMode == Primitive::WireFrame) {
      glDisable(GL_LIGHTING);
      glPointSize(radius);
      glBegin(GL_POINTS);
         glVertex3f(0.0, 0.0, 0.0);
      glEnd();
      glEnable(GL_LIGHTING);
   }else {
      GLUquadric* quad = gluNewQuadric();
      gluSphere(quad, radius, Primitive::s_resolution, Primitive::s_resolution);
      gluDeleteQuadric(quad); 
   }

   glPopMatrix();
}


double Atom::getRadius(bool const selected)
{
   double r(0.0);

   switch (m_drawMode) {
      case Primitive::Plastic:
         r = s_radiusBallsAndSticks;
         if (m_smallerHydrogens && m_atomicNumber == 1) r *= 0.7;
      case Primitive::BallsAndSticks:
         r = s_radiusBallsAndSticks * m_scale;
         if (m_smallerHydrogens && m_atomicNumber == 1) r *= 0.7;
         if (selected) r += Primitive::s_selectOffset; 
         break;
      case Primitive::SpaceFilling:
         r = m_vdwRadius * m_scale;
         if (selected) r += Primitive::s_selectOffset; 
         break;
      case Primitive::Tubes:
         r =  s_radiusTubes * m_scale;
         if (selected) r += Primitive::s_selectOffset; 
         break;
      case Primitive::WireFrame:
         r = s_radiusWireFrame * m_scale;
         if (selected) r += Primitive::s_selectOffsetWireFrame; 
         break;
   }

   return  r;
}


void Atom::drawLabel(Viewer& viewer, LabelType const type, QFontMetrics& fontMetrics) 
{
bool print(false);
   Vec pos(getPosition());
   Vec cam(viewer.camera()->position());
   Vec shift = viewer.camera()->position() - pos;

if (print) {
   qDebug() << "Atom coordinates: " << pos[0] << pos[1] << pos[2];
   qDebug() << "Camera coords:    " << cam[0] << cam[1] << cam[2];
   qDebug() << "    Shift vector: " << shift[0] << shift[1] << shift[2];
}
   shift.normalize();
   QString label(getLabel(type));

   pos = pos + 1.05 * shift * getRadius(true);
if (print) {
   qDebug() << "Shifted coords:   " << pos[0] << pos[1] << pos[2] << " r=" << getRadius(true);
}

   pos = viewer.camera()->projectedCoordinatesOf(pos);
   pos.x -= fontMetrics.horizontalAdvance(label)/2.0;
   pos.y += fontMetrics.height()/4.0;
   pos = viewer.camera()->unprojectedCoordinatesOf(pos);

   glPushMatrix();
      glColor3f(0.1, 0.1, 0.1);
      viewer.renderText(pos.x, pos.y, pos.z, label, viewer.labelFont());
   glPopMatrix();

if (print) {
    qDebug() << "Rendering text:   " << pos[0] << pos[1] << pos[2] << label;
    qDebug() << "";
    glDisable(GL_LIGHTING);
    glPointSize(3);
    glBegin(GL_POINTS);
       glVertex3f(pos.x, pos.y, pos.z);
    glEnd();
    glEnable(GL_LIGHTING);
}
}


QString Atom::getLabel(LabelType const type) const
{
   QString label;
   switch (type) {
      case None:      label = "";
         break;
      case Index:     label = QString::number(m_index);
         break;
      case Element:   label = m_symbol;
         break;
      case Charge:    label = QString::number(m_charge, 'f', 2);
         break;
      case Mass:      label = QString::number(m_mass, 'f', 3);
         break;
      case Spin:      label = QString::number(m_spin, 'f', 2);
         break;
      case Reindex:   label = isSelected() ? QString::number(m_reorderIndex) : "";
         break;
      case NmrShift:  label = QString::number(m_nmr, 'f', 2);
         break;
      case Label:     label = text();
         break;
   }
   return label;
}


void Atom::drawDisplacement() 
{
   if (s_vibrationDisplayVector) {
      Vec from(getPosition());
      Vec shift(s_cameraPosition - from);
      shift.normalize();
      from = from + 1.10 * shift * getRadius(true) - 
          0.5*m_displacement*s_vibrationVectorScale;
      Vec to(from + m_displacement*s_vibrationVectorScale);
      drawArrow(from, to);
   }
}


void Atom::drawArrow(Vec const& from, Vec const& to)
{
   glPushMatrix();
   glTranslatef(from[0],from[1],from[2]);
   Vec dir(to-from);
   glMultMatrixd(Quaternion(Vec(0,0,1), dir).matrix());
   drawArrow(dir.norm());
   glPopMatrix();
}


void Atom::drawArrow(float length)
{
   static GLUquadric* quadric = gluNewQuadric();
   static const GLfloat radius(0.04f);
   GLfloat coneRadius(3.20f*radius);
   GLfloat headLength(1.80f*coneRadius);
   GLfloat tubeLength(length-headLength);
   glColor4fv(s_vibrationVectorColor);

   if (2.0*length < headLength) {
      // Zero out very small vectors to tydy things up.
      return;

   }else if (length < headLength) {
      coneRadius *= length/headLength;
      headLength  = length;
      tubeLength  = 0.0f;
   }else {
      // Only Vectors longer than headLength have a tube.
      gluQuadricOrientation(quadric, GLU_OUTSIDE);
      gluCylinder(quadric, radius, radius, tubeLength, Primitive::s_resolution, 1);
      gluQuadricOrientation(quadric, GLU_INSIDE);
      gluDisk(quadric, 0.0, radius, Primitive::s_resolution, 1);
   }

   glTranslatef(0.0, 0.0, tubeLength);
   gluQuadricOrientation(quadric, GLU_OUTSIDE);
   gluCylinder(quadric, coneRadius, 0.0, headLength, Primitive::s_resolution, 1);
   gluQuadricOrientation(quadric, GLU_INSIDE);
   gluDisk(quadric, 0.0, coneRadius, Primitive::s_resolution, 1);
}

} } // end namespace IQmol::Layer
