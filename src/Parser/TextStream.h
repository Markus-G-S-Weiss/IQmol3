#pragma once
/*******************************************************************************

  Copyright (C) 2022-2025 Andrew Gilbert

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

#include "Util/QtVersionHacks.h"
//#include "Util/QsLog.h"
#include <QStringList>
#include <QTextStream>
#include <QIODevice>
#include <QRegularExpression>



namespace IQmol {
namespace Parser {

   /// A wrapper around an input QTextStream that adds some useful
   /// functionality like line counting, seeking and tokenization.
   class TextStream : public QTextStream {

      public:
         TextStream(QIODevice* device) : QTextStream(device), m_lineCount(0) 
         {
            if (!device->isOpen() || !device->isReadable()) {
               throw std::runtime_error("TextStream unable to read device");
            }
         }

         TextStream(QString* string) : QTextStream(string, QIODevice::ReadOnly), 
            m_lineCount(0) { }

         QString const& nextLine() 
         {
            ++m_lineCount;
            m_previousLine = readLine().trimmed();
            return m_previousLine;
         }

         QString const& nextLineNonTrimmed() 
         {
            ++m_lineCount;
            m_previousLine = readLine();
            return m_previousLine;
         }

         QString const& previousLine() 
         {
            return m_previousLine;
         }

        void skipBack(int n = 1)
         {
            //QLOG_DEBUG() << " Linecount is " << m_lineCount;
            int oldlinecount = m_lineCount - n;

            QTextStream::seek(0);
            //QLOG_DEBUG() << readLine();
            m_lineCount = 0;
            //QLOG_DEBUG() << " Linecount is " << m_lineCount;
            while((m_lineCount < oldlinecount )){
               nextLine();
            }
         }

         QString const& nextNonEmptyLine() 
         {
            nextLine();
            while (!atEnd() && m_previousLine.isEmpty()) {
               nextLine();
            }
            return m_previousLine;
         }

         QStringList nextLineAsTokens() 
         {
            return tokenize(nextLine());
         }

         QStringList nextNonEmptyLineAsTokens() 
         {
            return tokenize(nextNonEmptyLine());
         }

         QList<double> nextLineAsDoubles() 
         {
             QStringList tokens(nextLineAsTokens());
             QList<double> values;
             double x;  bool ok;

             for (int i = 0; i < tokens.size(); ++i) {
                 x = tokens[i].toDouble(&ok);                 
                 if (ok) values.append(x);
             }
             return values;
         }

         // Returns the next line that contains the given string.
         QString const& seek(QString const& str, 
            Qt::CaseSensitivity caseSensitive = Qt::CaseSensitive) 
         {
            nextLine();
            while (!atEnd() && !m_previousLine.contains(str, caseSensitive)) {
               nextLine();
            }
            return m_previousLine;
         }

         QString const& seek(QRegularExpression const& regExp) 
         {
            nextLine();
            while (!atEnd() && !m_previousLine.contains(regExp)) {
               nextLine();
            }
            return m_previousLine;
         }

         QStringList seekAndSplit(QString const& str,
            Qt::CaseSensitivity caseSensitive = Qt::CaseSensitive) 
         {
            return tokenize(seek(str, caseSensitive));
         }

         QStringList seekAndSplit(QRegularExpression const& regExp) 
         {
            return tokenize(seek(regExp));
         }

         void skipLine(int n = 1) 
         {
             for (int i = 0; i < n; ++i) { nextLine(); }
         }

         int  lineNumber() const { return m_lineCount; }

		 /// This is useful if the TextStream is part of another TextStream and
		 /// line numbers need to be referenced to the parent TextStream.
         void setOffset(int const offset) { m_lineCount = offset; }

         static QStringList tokenize(QString const& str) 
         {
            return str.split(QRegularExpression("\\s+"), IQmolSkipEmptyParts);
         }

         QString const& nextBlock(QChar const open = '{', QChar const close = '}') 
         {
             int nested(0);
             QChar c;
             QString block;
             
             // keep going until we find our starting brace
             while (!atEnd() && !nested) {
                this->operator>>(c); 
                block += c;
                if (c == open) ++nested;

             }

             while (!atEnd() && nested) {
                this->operator>>(c); 
                block += c;
                if (c == open)  ++nested;
                if (c == close) --nested;
             }
   
             // Not strictly correct, but conceptually consistent.
             m_previousLine = block;
             return m_previousLine;
         }

      private:
         void rewind() { 
            m_lineCount = 0;
            seek(0); 
         }

         int m_lineCount;
         QString m_previousLine;
   };

} } // end namespace IQmol::Parser
