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


namespace IQmol {

namespace Data {
   class Nmr;
}

namespace Configurator {
   class Nmr;
}

namespace Layer {

   class Molecule;

   class Nmr : public Base {

      Q_OBJECT 

      public:
         Nmr(Data::Nmr&);
         ~Nmr();

         void setMolecule(Molecule* molecule) { m_molecule = molecule; }

      public Q_SLOTS:
         void configure();

      private:
         Molecule*  m_molecule;
         Data::Nmr& m_data;
         Configurator::Nmr* m_configurator;
   };

} } // end namespace IQmol::Layer
