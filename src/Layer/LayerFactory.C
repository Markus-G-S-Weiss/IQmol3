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

#include "Data/Bank.h"
#include "Data/CubeData.h"
#include "Data/Data.h"
#include "Data/EfpFragment.h"
#include "Data/ExcitedStates.h"
#include "Data/File.h"
#include "Data/Frequencies.h"
#include "Data/GeminalOrbitals.h"
#include "Data/MacroMolecule.h"
#include "Data/Mesh.h"
#include "Data/NmrData.h"
#include "Data/PointCharge.h"
#include "Data/ProteinChain.h"
#include "Data/OrbitalsList.h"
#include "Data/Surface.h"
#include "Data/Solvent.h"
#include "Data/Vibronic.h"
#include "Data/ResidueName.h"

#include "LayerFactory.h"
#include "AtomLayer.h"
#include "BondLayer.h"
#include "CanonicalOrbitalsLayer.h"
#include "ChargeLayer.h"
#include "CubeDataLayer.h"
#include "DipoleLayer.h"
#include "DysonOrbitalsLayer.h"
#include "FileLayer.h"
#include "EfpFragmentListLayer.h"
#include "EfpFragmentLayer.h"
#include "ExcitedStatesLayer.h"
#include "FrequenciesLayer.h"
#include "GeometryLayer.h"
#include "GeometryListLayer.h"
#include "GeminalOrbitalsLayer.h"
#include "MacroMoleculeLayer.h"
#include "MoleculeLayer.h"
#include "NaturalBondOrbitalsLayer.h"
#include "NaturalTransitionOrbitalsLayer.h"
#include "OrbitalsLayer.h"
#include "ProteinChainLayer.h"
#include "SolventLayer.h"
#include "NmrLayer.h"
#include "RemLayer.h"
#include "TagLayer.h"
#include "VibronicLayer.h"

#include "Util/QsLog.h"

#include "openbabel/obiter.h"
#include "openbabel/mol.h"
#include "openbabel/atom.h"
#include "openbabel/bond.h"

#include <typeinfo>   // for std::bad_cast

#include <QDebug>


namespace IQmol {
namespace Layer {

Factory* Factory::s_instance = 0;


Factory& Factory::instance()
{
   if (s_instance == 0) {
      s_instance = new Factory();
      atexit(Factory::destroy);
   }
   return *s_instance;
}


void Factory::destroy()
{
   delete s_instance;
}


Layer::List Factory::toLayers(Data::Base& data)
{
   Layer::List layers;

   qDebug() << "Layer::Factory converting" << Data::Type::toString(data.typeID());

   try {

      switch (data.typeID()) {
   
         case Data::Type::Bank: {
            Data::Bank& bank(dynamic_cast<Data::Bank&>(data));
            layers << convert(bank);
         } break;

         case Data::Type::GeometryList: {
            Data::GeometryList& list(dynamic_cast<Data::GeometryList&>(data));
            layers << convert(list);
         } break;

         case Data::Type::Geometry: {
            Data::Geometry& geometry(dynamic_cast<Data::Geometry&>(data));
            layers << convert(geometry);
         } break;

         case Data::Type::PointChargeList: {
            Data::PointChargeList&  charges(dynamic_cast<Data::PointChargeList&>(data));
            layers << convert(charges);
         } break;

         case Data::Type::OrbitalsList: {
            Data::OrbitalsList& list(dynamic_cast<Data::OrbitalsList&>(data));
            layers << convert(list);
         } break;

         case Data::Type::Orbitals: {
            Data::Orbitals& orbitals(dynamic_cast<Data::Orbitals&>(data));
            layers.append(new Orbitals(orbitals));
         } break;

         case Data::Type::GeminalOrbitals: {
            Data::GeminalOrbitals& 
               geminalOrbitals(dynamic_cast<Data::GeminalOrbitals&>(data));
            layers.append(new GeminalOrbitals(geminalOrbitals));
         } break;

         case Data::Type::ExcitedStates: {
            Data::ExcitedStates& 
               states(dynamic_cast<Data::ExcitedStates&>(data));
            layers.append(new ExcitedStates(states));
         } break;

         case Data::Type::Frequencies: {
            Data::Frequencies& 
               frequencies(dynamic_cast<Data::Frequencies&>(data));
            layers.append(new Frequencies(frequencies));
         } break;

         case Data::Type::FileList: {
            IQmol::Data::FileList& fileList(dynamic_cast<IQmol::Data::FileList&>(data));
            layers << convert(fileList);
         } break;

         case Data::Type::GridData: {
            QLOG_WARN() << "Data::GridData passed to LayerFactory";
            //Data::GridData& grid(dynamic_cast<Data::GridData&>(data));
            //layers.append(new CubeData(grid));
         } break;

         case Data::Type::CubeData: {
            Data::CubeData& cube(dynamic_cast<Data::CubeData&>(data));
            layers.append(new CubeData(cube));
         } break;

         case Data::Type::EfpFragment: {
            Data::EfpFragment& efp(dynamic_cast<Data::EfpFragment&>(data));
            layers.append(new EfpFragment(efp));
         } break;

         case Data::Type::EfpFragmentList: {
            Data::EfpFragmentList& 
               efpList(dynamic_cast<Data::EfpFragmentList&>(data));
            layers << convert(efpList);
         } break;

         case Data::Type::Mesh: {
            // Note we create  surface on the heap as otherwise it will
            // go out of scope which can lead to seg faults
            Data::Mesh&  meshData(dynamic_cast<Data::Mesh&>(data));
            Data::Surface* surface(new Data::Surface(meshData));
            Layer::Surface* surfaceLayer(new Surface(*surface));
            surfaceLayer->setCheckState(Qt::Checked);
            layers.append(surfaceLayer);
         } break;

         case Data::Type::Surface: {
            Data::Surface&  surfaceData(dynamic_cast<Data::Surface&>(data));
            Layer::Surface* surfaceLayer(new Surface(surfaceData));
            surfaceLayer->setCheckState(surfaceData.isVisible() ? Qt::Checked : Qt::Unchecked);
            layers.append(surfaceLayer);
         } break;

         case Data::Type::Nmr: {
            Data::Nmr&  nmrData(dynamic_cast<Data::Nmr&>(data));
            Layer::Nmr* nmrLayer(new Nmr(nmrData));
            layers.append(nmrLayer);
         } break;

         case Data::Type::RemSection: {
            // These are re-parsed from the input file, so no need 
            //Data::RemSection&  remSection(dynamic_cast<Data::RemSection&>(data));
            //Layer::Rem* remLayer(new Rem(remSection));
            //layers.append(remLayer);
         } break;

         case Data::Type::Vibronic: {
            Data::Vibronic& 
               vibronic(dynamic_cast<Data::Vibronic&>(data));
            layers.append(new Vibronic(vibronic));
         } break;

         case Data::Type::ProteinChain: {
            Data::ProteinChain& 
               proteinChain(dynamic_cast<Data::ProteinChain&>(data));
            layers.append(new ProteinChain(proteinChain));
         } break;

         case Data::Type::MacroMolecule: {
            Data::MacroMolecule& 
               macroMolecule(dynamic_cast<Data::MacroMolecule&>(data));
            layers.append(new MacroMolecule(macroMolecule));
         } break;

         case Data::Type::Solvent: {
            Data::Solvent& 
               solvent(dynamic_cast<Data::Solvent&>(data));
            layers.append(new Solvent(solvent));
         } break;

         default:
            QLOG_WARN() << "Unimplemented data type in Layer::Factory"
                        <<  Data::Type::toString(data.typeID());
            break;
      }

   } catch (const std::bad_cast& e) {
       QLOG_ERROR() << "Data cast in Layer::Factory failed"
                    << Data::Type::toString(data.typeID());
   }

   return layers;
}


List Factory::convert(Data::Bank& bank) 
{
   List list;

   if (!bank.isEmpty()){
      Data::Bank::iterator iter;
      for (iter = bank.begin(); iter != bank.end(); ++iter) {
          list << toLayers(**iter);
      }
   }

   return list;
}


List Factory::convert(Data::Geometry& geometry)
{
   List list;
   Atoms* atoms(new Atoms());
   Bonds* bonds(new Bonds());
   list.append(atoms);
   list.append(bonds);

   unsigned nAtoms(geometry.nAtoms());
   OpenBabel::OBMol obMol;
   obMol.BeginModify();
   AtomMap atomMap;
   
   for (unsigned i = 0; i < nAtoms; ++i) {
       unsigned Z(geometry.atomicNumber(i));
       qglviewer::Vec position(geometry.position(i));

       Atom* atom(new Atom(geometry.atomicNumber(i), geometry.atomicLabel(i)));
       atom->setPosition(geometry.position(i));
       atoms->appendLayer(atom);

       OpenBabel::OBAtom* obAtom(obMol.NewAtom());
       obAtom->SetAtomicNum(Z);
       obAtom->SetVector(position.x, position.y, position.z);
       atomMap.insert(obAtom, atom);
   }

   obMol.SetTotalCharge(geometry.charge());
   obMol.SetTotalSpinMultiplicity(geometry.multiplicity());
   obMol.EndModify();
   obMol.ConnectTheDots();
   obMol.PerceiveBondOrders();

   for (OpenBabel::OBMolBondIter obBond(&obMol); obBond; ++obBond) {
       Atom* begin(atomMap.value(obBond->GetBeginAtom()));
       Atom* end(atomMap.value(obBond->GetEndAtom()));
       Bond* bond(new Bond(begin, end));
       bond->setOrder(obBond->GetBondOrder());
       bonds->appendLayer(bond);
   }

   if (geometry.hasProperty<Data::ResidueName>()) {
      Tag* tag(new Tag(geometry.getProperty<Data::ResidueName>().name()));
      list.append(tag);
   }

   return list;
}


List Factory::convert(Data::GeometryList& geometryList)
{
   List list;
   unsigned nGeometries(geometryList.size());

   if (nGeometries > 0) {
      list <<  convert(*(geometryList.first()));  // Atom and Bond lists
      list.append(new GeometryList(geometryList));
   }

   return list;
}


List Factory::convert(Data::PointChargeList& pointCharges)
{
   Charges* charges(new Charges());
   unsigned nCharges(pointCharges.size());
   
   for (unsigned i = 0; i < nCharges; ++i) {
       double q(pointCharges[i]->value());
       qglviewer::Vec position(pointCharges[i]->position());
       Charge* charge(new Charge(q,position));
       charges->appendLayer(charge);
   }

   List list;
   list.append(charges);
   return list;
}



List Factory::convert(Data::OrbitalsList& orbitalsList)
{
   List list;

   Data::OrbitalsList::iterator iter;
   for (iter = orbitalsList.begin(); iter != orbitalsList.end(); ++iter) {

       Data::CanonicalOrbitals* canonical =
          dynamic_cast<Data::CanonicalOrbitals*>(*iter);

       Data::NaturalTransitionOrbitals* ntos =
          dynamic_cast<Data::NaturalTransitionOrbitals*>(*iter);

       Data::NaturalBondOrbitals* nbos =
          dynamic_cast<Data::NaturalBondOrbitals*>(*iter);

       Data::DysonOrbitals* dyson =
          dynamic_cast<Data::DysonOrbitals*>(*iter);

       if (canonical) {
          list.append(new CanonicalOrbitals(*canonical));
       }else if (ntos) {
          list.append(new NaturalTransitionOrbitals(*ntos));
       }else if (nbos) {
          list.append(new NaturalBondOrbitals(*nbos));
       }else if (dyson) {
          list.append(new DysonOrbitals(*dyson));
       }else {
          list.append(new Orbitals(**iter));
       }
   }

   return list;
}



List Factory::convert(Data::EfpFragmentList& efpList) 
{
   List list;

   if (!efpList.isEmpty()){
      EfpFragments* efpFragments( new EfpFragments );
      Data::EfpFragmentList::iterator iter;
      for (iter = efpList.begin(); iter != efpList.end(); ++iter) {
          efpFragments->appendLayer(new EfpFragment(**iter));
      }
      list.append(efpFragments);
   }

   return list;
}


List Factory::convert(Data::FileList& fileList) 
{
   List list;

   if (!fileList.isEmpty()){
      Data::FileList::iterator iter;
      Files* files( new Files );
      for (iter = fileList.begin(); iter != fileList.end(); ++iter) {
          files->appendLayer(new File(**iter));
      }
      list.append(files);
   }

   return list;
}

} } // end namespace IQmol::Layer
