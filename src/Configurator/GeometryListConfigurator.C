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

#include "Layer/MoleculeLayer.h"
#include "Layer/GeometryLayer.h"
#include "Layer/GeometryListLayer.h"
#include "GeometryListConfigurator.h"
#include "Constraint.h"
#include <QHeaderView>

#include "CustomPlot.h"


using namespace qglviewer;

namespace IQmol {
namespace Configurator {


GeometryList::GeometryList(Layer::GeometryList& geometryList) : m_geometryList(geometryList),
   m_customPlot(0)
{
   m_configurator.setupUi(this);
   m_configurator.energyTable->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);

   m_pen.setColor(Qt::blue);
   m_pen.setStyle(Qt::SolidLine);
   m_pen.setWidthF(1);
   
   m_selectedPen.setColor(Qt::red);
   m_selectedPen.setStyle(Qt::SolidLine);
   m_selectedPen.setWidthF(3);

   initPlot();
}


GeometryList::~GeometryList()
{
   if (m_customPlot) delete m_customPlot;
}


void GeometryList::initPlot()
{
   m_customPlot = new CustomPlot();
   m_customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
   m_customPlot->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
   m_customPlot->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);

   m_customPlot->xAxis->setSelectableParts(QCPAxis::spNone);
   m_customPlot->xAxis->setLabel("Geometry");
   m_customPlot->yAxis->setLabel("Energy");

   connect(m_customPlot, SIGNAL(mousePress(QMouseEvent*)), 
      this, SLOT(setSelectionRectMode(QMouseEvent*)));
   
   QFrame* frame(m_configurator.plotFrame);
   QVBoxLayout* layout(new QVBoxLayout());
   frame->setLayout(layout);
   layout->addWidget(m_customPlot);
}


void GeometryList::load()
{
   QTableWidget* table(m_configurator.energyTable);
   table->clearContents();
   QList<Layer::Geometry*> 
      geometries(m_geometryList.findLayers<Layer::Geometry>(Layer::Children));

   unsigned rowCount(0);
   for (auto iter = geometries.begin(); iter != geometries.end(); ++iter) {
       double e((*iter)->energy());
       if (std::abs(e) >= 0.000001)  ++rowCount;
   }
   table->setRowCount(rowCount);
   m_rawData.clear();

   QTableWidgetItem* energy;

   if (geometries.size() < 2) {
      m_configurator.playButton->setEnabled(false);
      m_configurator.forwardButton->setEnabled(false);
      m_configurator.backButton->setEnabled(false);
      m_configurator.bounceButton->setEnabled(false);
      m_configurator.updateBondsButton->setEnabled(false);
      m_configurator.speedSlider->setEnabled(false);
      m_configurator.speedLabel->setEnabled(false);
      return;      
   }

   int row(0);
   bool property(false);
   for (auto iter = geometries.begin(); iter != geometries.end(); ++iter) {
       double e((*iter)->energy());
       if (std::abs(e) < 0.000001)  continue;
       double x(row+1);
       energy = new QTableWidgetItem( (*iter)->text() );
       energy->setTextAlignment(Qt::AlignCenter|Qt::AlignVCenter);
       table->setItem(row, 0, energy);
       Data::Geometry& geom((*iter)->geomData());

       if (geom.hasProperty<Data::Constraint>()) {
          x = geom.getProperty<Data::Constraint>().value();
          property = true;
       }

       m_rawData.append(qMakePair(x, e));
       ++row;
   }

   if (property) {
      m_customPlot->xAxis->setLabel("Geometric Parameter");
   }else {
      m_customPlot->xAxis->setLabel("Geometry");
   }

   plotEnergies();
}


void GeometryList::plotEnergies()
{
   m_customPlot->clearGraphs();

   double xmax(m_rawData.first().first),  xmin(m_rawData.first().first);
   double ymax(m_rawData.first().second), ymin(m_rawData.first().second);
   QVector<double> xx(m_rawData.size()), yy(m_rawData.size());

   for (int i = 0; i < m_rawData.size(); ++i) {
       xx[i] = m_rawData[i].first;
       yy[i] = m_rawData[i].second;
       xmin = std::min(xmin, xx[i]);
       xmax = std::max(xmax, xx[i]);
       ymin = std::min(ymin, yy[i]);
       ymax = std::max(ymax, yy[i]);
   }

   // Note this means the first graph is the line plot
   QCPGraph* graph(m_customPlot->addGraph());
   graph->setData(xx, yy);
   graph->setPen(m_pen);
   graph->selectionDecorator()->setPen(m_selectedPen);
   graph->setSelectable(QCP::stNone);
   graph->setLineStyle(QCPGraph::lsLine);

   QVector<double> x(1), y(1);
   for (int geom = 0; geom < m_rawData.size(); ++geom) {
       x[0] = m_rawData[geom].first;
       y[0] = m_rawData[geom].second;

       QCPGraph* graph(m_customPlot->addGraph());
       graph->setData(x, y);
       graph->setName(QString::number(geom));
       graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle));
       graph->setPen(m_pen);
       graph->selectionDecorator()->setPen(m_selectedPen);
       connect(graph, SIGNAL(selectionChanged(bool)), this, SLOT(plotSelectionChanged(bool)));
   }

   m_customPlot->xAxis->setRange(0, m_rawData.size());
   m_customPlot->yAxis->setRange(ymin, ymax);
   m_customPlot->xAxis->setRange(xmin, xmax);
   m_customPlot->replot();
}



void GeometryList::plotSelectionChanged(bool tf)
{
   QCPGraph* graph(qobject_cast<QCPGraph*>(sender()));
   if (!graph) return;

   if (tf) {
      graph->setPen(m_selectedPen);
      graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc));
   }else {
      graph->setPen(m_pen);
      graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle));
      return;
   }   
       
   if (!tf) return;
       
   bool ok;
   int geom(graph->name().toInt(&ok));
   // table cells are index
   if (!ok) return;

   QTableWidget* table(m_configurator.energyTable);
   table->setCurrentCell(geom, 0, 
      QItemSelectionModel::Rows | QItemSelectionModel::ClearAndSelect);
   table->scrollToItem(table->item(geom,0));
}


void GeometryList::on_energyTable_itemSelectionChanged()
{
   QList<QTableWidgetItem*> selection = m_configurator.energyTable->selectedItems();
   if (selection.isEmpty()) return;

   int index(selection.first()->row());
   int nGraphs(m_customPlot->graphCount()); 

   if (index < 0 || index >= nGraphs) {
      qDebug() << "Unmatched graph requested" << index;
      return;
   }

   // skip the (first) line graph that connects the dots
   for (int i = 1; i < nGraphs; ++i) { 
       QCPGraph* graph(m_customPlot->graph(i));
       if (i-1 == index) {
          QCPDataSelection selection(QCPDataRange(0, graph->dataCount()));
          graph->setSelection(selection);
       }else {
          graph->setSelection(QCPDataSelection());  // Empty Selection
       }
   }

   m_customPlot->replot();
   m_geometryList.setCurrentGeometry(index);
}


void GeometryList::reset()
{
   m_configurator.playButton->setChecked(false);
   m_configurator.backButton->setEnabled(true);
   m_configurator.forwardButton->setEnabled(true);
}


void GeometryList::on_playButton_clicked(bool play)
{
   if (play) {
      QTableWidget* table(m_configurator.energyTable);
      table->setCurrentCell(-1, 0,  // clear the current selection 
         QItemSelectionModel::Rows | QItemSelectionModel::ClearAndSelect);
   }

   m_configurator.backButton->setEnabled(!play);
   m_configurator.forwardButton->setEnabled(!play);
   m_geometryList.setPlay(play);
}


void GeometryList::on_backButton_clicked(bool)
{
   QTableWidget* table(m_configurator.energyTable);
   int currentRow(table->currentRow());
   if (currentRow > 0) {
      table->setCurrentCell(currentRow-1, 0, 
         QItemSelectionModel::Rows | QItemSelectionModel::ClearAndSelect);
   }
}


void GeometryList::on_forwardButton_clicked(bool)
{
   QTableWidget* table(m_configurator.energyTable);
   int currentRow(table->currentRow());
   if (currentRow < table->rowCount()-1) {
      table->setCurrentCell(currentRow+1, 0, 
         QItemSelectionModel::Rows | QItemSelectionModel::ClearAndSelect);
   }
}


void GeometryList::on_speedSlider_valueChanged(int value)
{
   // Default is 25 => 0.125 in GeometryList ctor
   m_geometryList.setSpeed(value/200.0);
}


void GeometryList::on_bounceButton_clicked(bool tf)
{
   m_geometryList.setBounce(tf);
}


void GeometryList::on_loopButton_clicked(bool tf)
{
   m_geometryList.setLoop(tf);
}


void GeometryList::on_updateBondsButton_clicked(bool tf)
{
   m_geometryList.setReperceiveBonds(tf);
   on_energyTable_itemSelectionChanged(); 
}


void GeometryList::on_resetViewButton_clicked()
{
   plotEnergies();
}


void GeometryList::setSelectionRectMode(QMouseEvent* e)
{
   if (e->modifiers() == Qt::ShiftModifier) {
      m_customPlot->setSelectionRectMode(QCP::srmZoom);
   }else {
      m_customPlot->setSelectionRectMode(QCP::srmNone);
   }
}


void GeometryList::closeEvent(QCloseEvent* e)
{
   on_playButton_clicked(false);
   m_geometryList.resetGeometry();
   Base::closeEvent(e);
}

} } // end namespace IQmol::Configurator
