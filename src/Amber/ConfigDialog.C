/*******************************************************************************
       
  Copyright (C) 2023 Andrew Gilbert
           
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

#include "Util/QMsgBox.h"
#include "Util/Preferences.h"
#include "Amber/ConfigDialog.h"

#include <QDir>

namespace IQmol {
namespace Amber { 

ConfigDialog::ConfigDialog(QWidget* parent) : QDialog(parent)
{
   m_dialog.setupUi(this);
   m_dialog.directory->setText(Preferences::AmberDirectory());
}

QString ConfigDialog::getDirectory() const
{
   return m_dialog.directory->text();
}

void ConfigDialog::accept()
{
   // check if the directory exists
   QString directory = m_dialog.directory->text();
   if (!directory.isEmpty() && !QDir(directory).exists()) {
      QMessageBox::warning(this, tr("Amber directory"),
         tr("The directory %1 does not exist.").arg(directory));
      return;
   }
   // check if the directory is valid
   if (!directory.isEmpty() && !QDir(directory).exists("amber.sh")) {
      QMessageBox::warning(this, tr("Amber directory"),
         tr("The directory %1 does not contain the Amber executable.").arg(directory));
      return;
   }
   QDialog::accept();
}

} } // end namespace IQmol::Amber
