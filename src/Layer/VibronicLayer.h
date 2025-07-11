#pragma once
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

#include "Layer.h"
#include "Configurator/VibronicConfigurator.h"


namespace IQmol {

namespace Data {
   class Vibronic;
}

namespace Layer {

   class Frequencies;

   class Vibronic: public Base {

      Q_OBJECT 

      public:
         Vibronic(Data::Vibronic const&);
         Data::Vibronic const& vibronicData() const { return m_vibronic; }
         void setFrequencyLayers(QList<Layer::Frequencies*> const& frequencyLayers);

      Q_SIGNALS:
         void update();
         void playMode(int mode); // -ve => stop animation

      public Q_SLOTS:
         void configure();
         void connectInitialFrequencies(bool);
         void connectFinalFrequencies(bool);

      private:
         Data::Vibronic const& m_vibronic;
         Configurator::Vibronic m_configurator;
         Layer::Frequencies* m_initialFrequencies;
         Layer::Frequencies* m_finalFrequencies;
   };


} } // end namespace IQmol::Layer
