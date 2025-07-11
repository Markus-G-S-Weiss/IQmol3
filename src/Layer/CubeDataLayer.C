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

#include "CubeDataLayer.h"
#include "SurfaceLayer.h"
#include "MoleculeLayer.h"
#include "Grid/MarchingCubes.h"
#include "Grid/MeshDecimator.h"
#include "Data/SurfaceInfo.h"
#include "Grid/Property.h" 
#include "Data/CubeData.h"
#include "Util/QsLog.h"

#include <QDebug>


namespace IQmol {
namespace Layer {

CubeData::CubeData(Data::CubeData const& cube) : Base("Cube Data"), m_configurator(*this),
   m_cube(cube), m_molecule(0)
{
   connect(&m_configurator, SIGNAL(calculateSurface(Data::SurfaceInfo const&)),
      this, SLOT(calculateSurface(Data::SurfaceInfo const&)));
   setConfigurator(&m_configurator);
   setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
   setText("Cube Data (" + m_cube.label() + ")");
}


Data::Geometry const& CubeData::geometry() const 
{ 
   return m_cube.geometry(); 
}


void CubeData::setMolecule(Molecule* molecule) 
{
   m_molecule = molecule;
   // Actions for the context menu
   connect(newAction("Surface Animator"), SIGNAL(triggered()),
      m_molecule, SLOT(openSurfaceAnimator()));
}


Property::GridBased* CubeData::createProperty() const
{
   return new Property::GridBased(text(), m_cube);
}


Surface* CubeData::calculateSurface(Data::SurfaceInfo const& surfaceInfo)
{
   if (!m_molecule) {
      QLOG_WARN() << "Failed to find parent molecule for CubeData surface";
      return 0;
   }

   MarchingCubes mc(m_cube);
   qDebug() << "The Data::Surface needs to be appended to the molecule's Data::Bank";
   Data::Surface* surfaceData(new Data::Surface(surfaceInfo));

   qglviewer::Vec d(m_cube.delta());
   double delta((d.x+d.y+d.z)/3.0);

   mc.generateMesh(surfaceInfo.isovalue(), surfaceData->meshPositive());
   if (surfaceInfo.simplifyMesh()) {
      MeshDecimator decimator(surfaceData->meshPositive());
      if (!decimator.decimate(delta)) {
         QLOG_ERROR() << "Mesh decimation failed:" << decimator.error();
      }
   }

   if (surfaceInfo.isSigned()) {
      mc.generateMesh(-surfaceInfo.isovalue(), surfaceData->meshNegative());
      if (surfaceInfo.simplifyMesh()) {
         MeshDecimator decimator(surfaceData->meshNegative());
         if (!decimator.decimate(delta)) {
            QLOG_ERROR() << "Mesh decimation failed:" << decimator.error();
         }
      }
   }
   
   Layer::Surface* surfaceLayer(new Layer::Surface(*surfaceData));
   surfaceLayer->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable |
      Qt::ItemIsEnabled | Qt::ItemIsEditable);

   // Get the frame from the parent molecule in case it has been reoriented.
   if (m_molecule) {
      connect(surfaceLayer, SIGNAL(updated()), m_molecule, SIGNAL(softUpdate()));
      surfaceLayer->setFrame(m_molecule->getReferenceFrame());
   }

   appendRow(surfaceLayer);
   surfaceLayer->setCheckState(Qt::Checked);

   return surfaceLayer;
}

} } // end namespace IQmol::Layer
